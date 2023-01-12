from __future__ import annotations

import re
from dataclasses import dataclass, replace
from enum import Enum
from functools import cache
from itertools import chain
from pathlib import Path
from typing import ClassVar, Optional

from diopter.compiler import (
    ClangTool,
    ClangToolMode,
    CompilationSetting,
    CompilerExe,
    SourceProgram,
)


@dataclass(frozen=True)
class DCEMarker:
    """A dead code elimination marker DCEMarkerX_, where X is an
    integer.

    Attributes:
        marker(str): the marker in the DCEMarkerX_ form

    Class Attributes:
        prefix (str): the prefix of all DCEMarkers
    """

    marker: str
    prefix: ClassVar[str] = "DCEMarker"

    def __post_init__(self) -> None:
        """Check that self.marker is a valid DCEMarker"""
        assert self.marker.startswith(DCEMarker.prefix)
        assert self.marker[-1] == "_"
        assert int(self.marker[len(DCEMarker.prefix) : -1]) >= 0

    def to_macro(self) -> str:
        """Returns the marker macro

        Returns:
            str: DCEMarkerX_
        """
        return self.marker


class VRMarkerKind(Enum):
    """
    A VRMarker can either be a LE-Equal(LE):
        if (var <= VRMarerConstantLEX_)
            VRMarkerLEX_

    or a Greater-Equal(GE):
        if (var >= VRMarerConstantGEX_)
            VRMarkerGEX_
    """

    LE = 0
    GE = 1

    @staticmethod
    def from_str(kind: str) -> VRMarkerKind:
        match kind:
            case "LE":
                return VRMarkerKind.LE
            case "GE":
                return VRMarkerKind.GE
            case _:
                raise ValueError(f"{kind} is not a valid VRMarkerKind")


@dataclass(frozen=True)
class VRMarker:
    """A value range marker VRMarkerX_, where X is an
    integer, corresponds to a marker macro and a constant macro:
    - (VRMarkerLEX_, VRMarkerConstantLEX_) for LE markers
    - (VRMarkerGEX_ VRMarkerConstantGEX_) for GE markers

    These appear in the source code as:
        if (var <= VRMarerConstantLEX_)
            VRMarkerLEX_
    or:
        if (var >= VRMarerConstantGEX_)
            VRMarkerGEX_


    Attributes:
        marker(str): the marker in the VRMarkerX_ form
        kind (VRMarkerKind): LE or GE

    Class Attributes:
        prefix (str): the prefix of all VRMarkers
    """

    marker: str
    kind: VRMarkerKind
    prefix: ClassVar[str] = "VRMarker"
    constant_prefix: ClassVar[str] = "VRMarkerConstant"

    @staticmethod
    def from_str(marker_str: str) -> VRMarker:
        """Parsers a string of the form VRMarkerLEX_ | VRMarkerGEX_

        Returns:
            VRMarker:
                the parsed marker
        """
        assert marker_str.startswith(VRMarker.prefix)
        kind_str = marker_str[len(VRMarker.prefix) : len(VRMarker.prefix) + 2]
        return VRMarker(
            marker_str[: len(VRMarker.prefix)] + marker_str[len(VRMarker.prefix) + 2 :],
            VRMarkerKind.from_str(kind_str),
        )

    def __post_init__(self) -> None:
        """Check that self.marker is a valid VRMarker"""
        assert self.marker.startswith(VRMarker.prefix)
        assert self.marker[-1] == "_"
        assert int(self.marker[len(VRMarker.prefix) : -1]) >= 0

    def to_macro(self) -> str:
        """Extract the marker macro

        Returns:
            str: VRMarkerLEX_ or VRMarkerGEX_
        """
        return self.marker[:8] + self.kind.name + self.marker[8:]

    def get_constant_macro(self) -> str:
        """Extract the LE constant macro

        Returns:
            str: VRMarkerConstantLEX_ or VRMarkerConstantGEX_
        """
        return self.marker[:8] + "Constant" + str(self.kind) + self.marker[8:]


def find_alive_markers_impl(asm: str) -> tuple[DCEMarker | VRMarker, ...]:
    """Finds alive markers in asm, i.e., markers that have not been eliminated.

    The markers are found by searching for calls or jumps to functions with
    names starting with a known marker prefix (e.g., DCEMarker123_)

    Returns:
        tuple[DCEMarker | VRMarker, ...]:
            The alive markers for the given compilation setting.
    """
    dce_alive_regex = re.compile(f".*[call|jmp].*{DCEMarker.prefix}([0-9]+)_.*")
    vr_alive_regex = re.compile(f".*[call|jmp].*{VRMarker.prefix}([G|L])E([0-9]+)_.*")
    alive_markers: set[DCEMarker | VRMarker] = set()
    for line in asm.split("\n"):
        if m := dce_alive_regex.match(line.strip()):
            alive_markers.add(DCEMarker(f"{DCEMarker.prefix}{m.group(1)}_"))
        elif m := vr_alive_regex.match(line.strip()):
            alive_markers.add(
                VRMarker.from_str(f"{VRMarker.prefix}{m.group(1)}E{m.group(2)}_")
            )
    return tuple(alive_markers)


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    dce_markers: tuple[DCEMarker, ...] = tuple()
    vr_markers: tuple[VRMarker, ...] = tuple()
    vr_marker_defined_constants: tuple[tuple[VRMarker, int], ...] = tuple()
    disabled_markers: tuple[DCEMarker | VRMarker, ...] = tuple()
    unreachable_markers: tuple[DCEMarker | VRMarker, ...] = tuple()
    disable_prefix: ClassVar[str] = "Disable"
    unreachable_prefix: ClassVar[str] = "Unreachable"

    def __post_init__(self) -> None:
        """Sanity checks"""

        # There is no overlap between disabled and unreachable markers
        disabled_markers = set(self.disabled_markers)
        unreachable_markers = set(self.unreachable_markers)
        assert disabled_markers.isdisjoint(unreachable_markers)

        # Disabled and unreachable markers are subsets of all markers
        all_markers = set(self.all_markers())
        assert disabled_markers <= all_markers
        assert unreachable_markers <= all_markers

        # Macros have been included for all disabled and
        # unreachable markers and defined constants
        macros = set(self.defined_macros)
        for macro in chain(
            map(InstrumentedProgram.__make_disable_macro, self.disabled_markers),
            map(InstrumentedProgram.__make_unreachable_macro, self.unreachable_markers),
            map(
                lambda vrc: vrc[0].get_constant_macro() + f"={vrc[1]}",
                self.vr_marker_defined_constants,
            ),
        ):
            assert macro in macros
            macros.remove(macro)

        # No disable, unreachable, or constant marcos
        # have been defined for other markers
        prefixes = [
            macro_prefix + marker_prefix
            for macro_prefix in (
                InstrumentedProgram.disable_prefix,
                InstrumentedProgram.unreachable_prefix,
            )
            for marker_prefix in (DCEMarker.prefix, VRMarker.prefix)
        ] + [VRMarker.constant_prefix]
        for macro in macros:
            for prefix in prefixes:
                assert not macro.startswith(prefix)

    def all_markers(self) -> tuple[DCEMarker | VRMarker, ...]:
        """Return all of this program's markers.

        Returns:
            tuple[DCEMarker| VRMarker, ...]:
                All markers
        """
        return self.dce_markers + self.vr_markers

    def find_alive_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[DCEMarker | VRMarker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        alive markers, i.e., markers that have not been eliminated.

        The markers are found by searching for calls or jumps to functions with
        names starting with a known marker prefix (e.g., DCEMarker123_)

        Returns:
            tuple[DCEMarker | VRMarker, ...]:
                The alive markers for the given compilation setting.
        """
        asm = compilation_setting.get_asm_from_program(self)
        alive_markers = find_alive_markers_impl(asm)
        assert set(alive_markers) <= set(self.all_markers())
        return alive_markers

    def find_dead_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[DCEMarker | VRMarker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        dead markers, i.e., markers that have been eliminated.

        Returns:
            tuple[DCEMarker | VRMarker, ...]:
                The dead markers for the given compilation setting.
        """
        dead_markers = set(self.all_markers()) - set(
            self.find_alive_markers(compilation_setting)
        )
        return tuple(dead_markers)

    @staticmethod
    def __make_disable_macro(marker: DCEMarker | VRMarker) -> str:
        return InstrumentedProgram.disable_prefix + marker.to_macro()

    @staticmethod
    def __make_unreachable_macro(marker: DCEMarker | VRMarker) -> str:
        return InstrumentedProgram.unreachable_prefix + marker.to_macro()

    def disable_markers(
        self, dmarkers: tuple[DCEMarker | VRMarker, ...]
    ) -> InstrumentedProgram:
        """Disables the given markers by setting the relevant macros.

        Markers that have already been disabled are ignored. If any of the
        markers are not in self.markers an AssertionError will be raised.  An
        AssertionError error is also raised if markers have been previously
        made unreachable.

        Args:
            markers (tuple[DCEMarker|VRMarker, ...]):
                The markers that will be disabled

        Returns:
            InstrumentedProgram:
                A copy of self with the additional disabled markers
        """

        dmarkers_set = set(dmarkers)
        assert dmarkers_set <= set(self.all_markers())
        assert dmarkers_set.isdisjoint(self.unreachable_markers)

        new_dmarkers = tuple(dmarkers_set - set(self.disabled_markers))
        new_macros = tuple(
            InstrumentedProgram.__make_disable_macro(marker) for marker in new_dmarkers
        )

        return replace(
            self,
            disabled_markers=self.disabled_markers + new_dmarkers,
            defined_macros=self.defined_macros + new_macros,
        )

    def make_markers_unreachable(
        self, umarkers: tuple[DCEMarker | VRMarker, ...]
    ) -> InstrumentedProgram:
        """Makes the given markers unreachable by setting the relevant macros.

        Markers that have already been made unreachable are ignored. If any of
        the markers are not in self.markers an AssertionError will be raised.
        An AssertionError error is also raised if markers have been previously
        disabled.


        Args:
            markers (tuple[DCEMarker|VRMarker, ...]):
                The markers that will be made unreachable

        Returns:
            InstrumentedProgram:
                A copy of self with the additional unreachable markers
        """

        umarkers_set = set(umarkers)
        assert umarkers_set <= set(self.all_markers())
        assert umarkers_set.isdisjoint(self.disabled_markers)

        new_umarkers = tuple(umarkers_set - set(self.unreachable_markers))
        new_macros = tuple(
            InstrumentedProgram.__make_unreachable_macro(marker)
            for marker in new_umarkers
        )

        return replace(
            self,
            unreachable_markers=self.unreachable_markers + new_umarkers,
            defined_macros=self.defined_macros + new_macros,
        )

    def disable_remaining_markers(self) -> InstrumentedProgram:
        """Disable all remaining markers by setting the relevant macros.

        Macros that have already been disabled or make unreachable are unchanged.

        Returns:
            InstrumentedProgram:
                A similar InstrumentedProgram as self but with all remaining
                markers disabled and the corresponding macros defined.
        """

        remaining_markers = tuple(
            set(self.all_markers())
            - set(self.disabled_markers)
            - set(self.unreachable_markers)
        )
        new_macros = tuple(
            InstrumentedProgram.__make_disable_macro(marker)
            for marker in remaining_markers
        )
        return replace(
            self,
            disabled_markers=self.disabled_markers + remaining_markers,
            defined_macros=self.defined_macros + new_macros,
        )


def __find_all_markers_with_prefix(
    instrumenter_output: str, prefix: str
) -> tuple[str, ...]:
    """Finds the set of `prefix`MarkerX_ inserted by the instrumenter.

    The instrumenter prints the set of markers in stdout in this format:
    `prefix`MARKERS START\n
    `prefix`Marker1_\n
    `prefix`Marker2_\n
    ...
    `prefix`MARKERS END

    Args:
        instrumenter_output (str):
            The instrumenter's stdout
    Returns:
        tuple[str, ...] the parsed markers
    """
    _, _, instrumenter_output = instrumenter_output.partition(f"{prefix}MARKERS START")
    assert instrumenter_output
    instrumenter_output, _, _ = instrumenter_output.partition(f"{prefix}MARKERS END")
    return tuple(instrumenter_output.strip().splitlines())


def __find_all_dce_markers(
    instrumenter_output: str,
) -> tuple[DCEMarker, ...]:
    """Finds the set of DCE markers inserted by the instrumenter.
    Args:
        instrumenter_output (str):
            The instrumenter's stdout
    Returns:
        tuple[str, ...] the parsed markers
    """
    return tuple(
        DCEMarker(marker)
        for marker in __find_all_markers_with_prefix(instrumenter_output, "DCE")
    )


def __find_all_vr_markers(
    instrumenter_output: str,
) -> tuple[VRMarker, ...]:
    """Finds the set of VR markers inserted by the instrumenter.

    Args:
        instrumenter_output (str):
            The instrumenter's stdout
    Returns:
        tuple[VRMarker, ...] the parsed markers
    """
    return tuple(
        chain.from_iterable(
            (VRMarker(marker, VRMarkerKind.LE), VRMarker(marker, VRMarkerKind.GE))
            for marker in __find_all_markers_with_prefix(instrumenter_output, "VR")
        )
    )


@cache
def get_instrumenter(
    instrumenter: Optional[ClangTool] = None, clang: Optional[CompilerExe] = None
) -> ClangTool:
    if not instrumenter:
        if not clang:
            # TODO: need to check clang version, maybe supply multiple binaries?
            clang = CompilerExe.get_system_clang()
        instrumenter = ClangTool.init_with_paths_from_clang(
            Path(__file__).parent / "dead-instrument", clang
        )
    return instrumenter


class InstrumenterMode(Enum):
    DCE = 0
    VR = 1


def instrument_program(
    program: SourceProgram,
    ignore_functions_with_macros: bool = False,
    mode: InstrumenterMode = InstrumenterMode.DCE,
    instrumenter: Optional[ClangTool] = None,
    clang: Optional[CompilerExe] = None,
) -> InstrumentedProgram:
    """Instrument a given program i.e. put markers in the file.

    Args:
        program (Source):
            The program to be instrumented.
        ignore_functions_with_macros (bool):
            Whether to ignore instrumenting functions that contain macro expansions
        instrumenter (ClangTool):
            The instrumenter
        clang (CompilerExe):
            Which clang to use for searching the standard include paths
    Returns:
        InstrumentedProgram: The instrumented version of program
    """

    instrumenter_resolved = get_instrumenter(instrumenter, clang)

    flags = []
    match mode:
        case InstrumenterMode.DCE:
            flags.append("--mode=dce")
        case InstrumenterMode.VR:
            flags.append("--mode=vr")
    if ignore_functions_with_macros:
        flags.append("--ignore-functions-with-macros=1")
    else:
        flags.append("--ignore-functions-with-macros=0")

    result = instrumenter_resolved.run_on_program(
        program, flags, ClangToolMode.CAPTURE_OUT_ERR_AND_READ_MODIFIED_FILED
    )
    assert result.modified_source_code
    assert result.stdout

    instrumented_code = result.modified_source_code

    match mode:
        case InstrumenterMode.DCE:
            dce_markers = __find_all_dce_markers(result.stdout)
            vr_markers: tuple[VRMarker, ...] = tuple()
        case InstrumenterMode.VR:
            dce_markers = tuple()
            vr_markers = __find_all_vr_markers(result.stdout)

    return InstrumentedProgram(
        code=instrumented_code,
        language=program.language,
        defined_macros=program.defined_macros,
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        dce_markers=dce_markers,
        vr_markers=vr_markers,
    )


def annotate_with_static(
    program: SourceProgram,
    instrumenter: Optional[ClangTool] = None,
    clang: Optional[CompilerExe] = None,
) -> SourceProgram:
    """Turn all globals in the given file into static globals.

    Args:
        file (Path): Path to code file to be instrumented.
        flags (list[str]): list of user provided clang flags
        instrumenter (Path): Path to the instrumenter executable., if not
                             provided will use what's specified in
        clang (Path): Path to the clang executable.
    Returns:
        None:
    """

    instrumenter_resolved = get_instrumenter(instrumenter, clang)
    flags = ["--mode=globals"]

    result = instrumenter_resolved.run_on_program(
        program, flags, ClangToolMode.READ_MODIFIED_FILE
    )
    assert result.modified_source_code

    return replace(program, code=result.modified_source_code)
