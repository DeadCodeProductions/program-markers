from __future__ import annotations

import re
from dataclasses import dataclass, replace
from enum import Enum
from functools import cache
from itertools import chain
from pathlib import Path
from typing import ClassVar, Optional, Sequence, TypeAlias

from diopter.compiler import (
    ClangTool,
    ClangToolMode,
    CompilationSetting,
    CompilerExe,
    SourceProgram,
)

# TODO: The various hardcoded strings, e.g., "//MARKER_DIRECTIVES\n"
# should not be manually copied, instead either have some common file
# where they are defined or read them from some kind of info output,
# e.g., instrumenter --info


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
        if (var <= VRMarkerConstantLEX_)
            VRMarkerLEX_

    or a Greater-Equal(GE):
        if (var >= VRMarkerConstantGEX_)
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
        if (var <= VRMarkerConstantLEX_)
            VRMarkerLEX_
    or:
        if (var >= VRMarkerConstantGEX_)
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


Marker: TypeAlias = DCEMarker | VRMarker


def find_alive_markers_impl(asm: str) -> tuple[Marker, ...]:
    """Finds alive markers in asm, i.e., markers that have not been eliminated.

    The markers are found by searching for calls or jumps to functions with
    names starting with a known marker prefix (e.g., DCEMarker123_)

    Returns:
        tuple[Marker, ...]:
            The alive markers for the given compilation setting.
    """
    dce_alive_regex = re.compile(f".*[call|jmp].*{DCEMarker.prefix}([0-9]+)_.*")
    vr_alive_regex = re.compile(f".*[call|jmp].*{VRMarker.prefix}([G|L])E([0-9]+)_.*")
    alive_markers: set[Marker] = set()
    for line in asm.split("\n"):
        if m := dce_alive_regex.match(line.strip()):
            alive_markers.add(DCEMarker(f"{DCEMarker.prefix}{m.group(1)}_"))
        elif m := vr_alive_regex.match(line.strip()):
            alive_markers.add(
                VRMarker.from_str(f"{VRMarker.prefix}{m.group(1)}E{m.group(2)}_")
            )
    return tuple(alive_markers)


@dataclass(frozen=True, kw_only=True)
class MarkerStatus:
    dead_markers: tuple[Marker, ...]
    alive_markers: tuple[Marker, ...]


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    marker_preprocessor_directives: dict[Marker, str]
    # XXX: markers: tuple[Marker, ...] = tuple()
    # instead of separate markers
    dce_markers: tuple[DCEMarker, ...] = tuple()
    # XXX: should the constants be part of the marker?
    vr_markers: tuple[VRMarker, ...] = tuple()
    vr_marker_defined_constants: tuple[tuple[VRMarker, int], ...] = tuple()
    disabled_markers: tuple[Marker, ...] = tuple()
    unreachable_markers: tuple[Marker, ...] = tuple()
    disable_prefix: ClassVar[str] = "Disable"
    unreachable_prefix: ClassVar[str] = "Unreachable"

    def __post_init__(self) -> None:
        """Sanity checks"""

        # The relevant directives for each marker should be present
        for marker in self.all_markers():
            assert marker in self.marker_preprocessor_directives

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

    def get_modified_code(self) -> str:
        """Returns self.marker_preprocessor_directives + self.code.

        Only directives for the enabled, disabled and made unreachable markers
        are added.

        If any markers have not been enabled, disabled, or made unreachable,
        then the code not compilable but it can be preprocessed.

        Returns:
            str:
                the source code including the necessary preprocessor directives
        """
        return (
            "\n".join(
                self.marker_preprocessor_directives[marker]
                for marker in self.all_markers()
            )
            + "\n"
            + self.code
        )

    def all_markers(self) -> tuple[Marker, ...]:
        """Return all of this program's markers.

        Returns:
            tuple[DCEMarker| VRMarker, ...]:
                All markers
        """
        return self.dce_markers + self.vr_markers

    # XXX: 1) merge these two methods, 2) should they
    # optionally ignore enabled/disabled/unreachable?
    def find_alive_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[Marker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        alive markers, i.e., markers that have not been eliminated.

        The markers are found by searching for calls or jumps to functions with
        names starting with a known marker prefix (e.g., DCEMarker123_)

        Returns:
            tuple[Marker, ...]:
                The alive markers for the given compilation setting.
        """
        asm = compilation_setting.get_asm_from_program(self)
        alive_markers = find_alive_markers_impl(asm)
        assert set(alive_markers) <= set(self.all_markers())
        return alive_markers

    def find_dead_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[Marker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        dead markers, i.e., markers that have been eliminated.

        Returns:
            tuple[Marker, ...]:
                The dead markers for the given compilation setting.
        """
        dead_markers = set(self.all_markers()) - set(
            self.find_alive_markers(compilation_setting)
        )
        return tuple(dead_markers)

    def find_dead_and_alive_markers(
        self, compilation_setting: CompilationSetting
    ) -> MarkerStatus:
        """Compiles the program to ASM with `compilation_setting` and finds
        dead and alive markers, i.e., markers that have been eliminated or not

        Returns:
            MarkerStatus:
                The dead and alive markers for the given compilation setting.
        """
        alive_markers = self.find_alive_markers(compilation_setting)
        dead_markers = set(self.all_markers()) - set(alive_markers)
        return MarkerStatus(
            dead_markers=tuple(dead_markers), alive_markers=alive_markers
        )

    @staticmethod
    def __make_disable_macro(marker: Marker) -> str:
        return InstrumentedProgram.disable_prefix + marker.to_macro()

    @staticmethod
    def __make_unreachable_macro(marker: Marker) -> str:
        return InstrumentedProgram.unreachable_prefix + marker.to_macro()

    def disable_markers(self, dmarkers: Sequence[Marker]) -> InstrumentedProgram:
        """Disables the given markers by setting the relevant macros.

        Markers that have already been disabled are ignored. If any of the
        markers are not in self.markers an AssertionError will be raised.  An
        AssertionError error is also raised if markers have been previously
        made unreachable.

        Args:
            markers (Sequence[Marker]):
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
        self, umarkers: Sequence[Marker]
    ) -> InstrumentedProgram:
        """Makes the given markers unreachable by setting the relevant macros.

        Markers that have already been made unreachable are ignored. If any of
        the markers are not in self.markers an AssertionError will be raised.
        An AssertionError error is also raised if markers have been previously
        disabled.


        Args:
            markers (Sequence[Marker]):
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

    def disable_remaining_markers(
        self, do_not_disable: Sequence[Marker] = tuple()
    ) -> InstrumentedProgram:
        """Disable all remaining markers by setting the relevant macros.

        Args:
            do_not_disable (Sequence[Marker]):
                markers that will not be modified

        The following are unaffected:
        - already disabled markers
        - markers already made unreachable
        - markers in `do_not_disable` (optional argument)

        Returns:
            InstrumentedProgram:
                A similar InstrumentedProgram as self but with all remaining
                markers disabled and the corresponding macros defined.
        """

        remaining_markers = tuple(
            set(self.all_markers())
            - set(self.disabled_markers)
            - set(self.unreachable_markers)
            - set(do_not_disable)
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

    def with_preprocessed_code(self, preprocessed_code: str) -> InstrumentedProgram:
        """Returns a new program with its code replaced with `preprocessed_code`

        Any markers that were previously disabled or made unreachable are removed.

        Returns:
            InstrumentedProgram:
                the new program
        """

        return replace(
            self,
            code=preprocessed_code,
            marker_preprocessor_directives={},
            dce_markers=tuple(),
            vr_markers=tuple(),
            vr_marker_defined_constants=tuple(),
            disabled_markers=tuple(),
            unreachable_markers=tuple(),
            defined_macros=tuple(),
            include_paths=tuple(),
            system_include_paths=tuple(),
        )

    def preprocess_disabled_and_unreachable_markers(
        self, setting: CompilationSetting, make_compiler_agnostic: bool = False
    ) -> InstrumentedProgram:
        """Preprocesses `self.code` with `setting` and makes disabled markers
        and unreachable markers permanent.

        All disabled and unreachable markers are "committed" and their
        corresponding preprocessor directives are expanded. The resulting
        program contains only the remaining markers (and their corresponding
        directives).

        Args:
            setting (CompilationSetting):
                the compiler setting used to to preprocess the program
            make_compiler_agnostic (bool):
                if True, various compiler specific attributes, types and function
                declarations will be removed from the preprocessed code
        Returns:
            InstrumentedProgram:
                a program with only the non-disabled/unreachable markers, the
                preprocessor directives of the other ones have been expanded
        """
        preserved_markers = set(self.all_markers()).difference(
            self.disabled_markers, self.unreachable_markers
        )

        def remove_markers(old_markers: Sequence[Marker]) -> tuple[Marker, ...]:
            return tuple(set(old_markers) - preserved_markers)

        # Preprocess a program that does not contain the preserved markers and
        # their directives. It still included the macros in the code, e.g.,
        # DCEMarker0_, but since their directives are missing they won't be
        # expanded.
        program_with_markers_removed = replace(
            self,
            marker_preprocessor_directives={
                m: d
                for m, d in self.marker_preprocessor_directives.items()
                if m not in preserved_markers
            },
            dce_markers=remove_markers(self.dce_markers),
            vr_markers=remove_markers(self.vr_markers),
            vr_marker_defined_constants=tuple(
                constant
                for constant in self.vr_marker_defined_constants
                if constant[0] not in preserved_markers
            ),
            disabled_markers=remove_markers(self.disabled_markers),
            unreachable_markers=remove_markers(self.unreachable_markers),
        )
        pprogram = setting.preprocess_program(
            program_with_markers_removed, make_compiler_agnostic=make_compiler_agnostic
        )

        # Add the preserved markers and directives back
        preserved_directives = {
            m: d
            for m, d in self.marker_preprocessor_directives.items()
            if m in preserved_markers
        }
        preserved_dce_markers = tuple(set(self.dce_markers) & preserved_markers)
        preserved_vr_markers = tuple(set(self.vr_markers) & preserved_markers)
        preserved_vr_marker_constants = tuple(
            constant
            for constant in self.vr_marker_defined_constants
            if constant[0] in preserved_markers
        )
        return replace(
            pprogram,
            marker_preprocessor_directives=preserved_directives,
            dce_markers=preserved_dce_markers,
            vr_markers=preserved_vr_markers,
            vr_marker_defined_constants=preserved_vr_marker_constants,
        )


def __str_to_marker(marker_macro: str) -> Marker:
    """Converts the `marker_macro` string into a `Marker.
    Args:
        marker_macro (str):
            a marker in the form PrefixMarkerX_, e.g., DCEMarker32_.
    Returns:
        Marker:
            a `Marker` object
    """
    if marker_macro.startswith(DCEMarker.prefix):
        return DCEMarker(marker_macro)
    else:
        assert marker_macro.startswith(VRMarker.prefix)
        return VRMarker.from_str(marker_macro)


def __split_marker_directives(directives: str) -> dict[Marker, str]:
    """Maps each set of preprocessor directive in `directives` to the
    appropriate markers.

    Args:
        directives (str):
            the marker preprocessor directives added by the instrumenter
    Returns:
        dict[Marker, str]:
            a mapping from each marker to its preprocessor directives
    """
    directives_map = {}
    for directive in directives.split("//MARKER_DIRECTIVES:")[1:]:
        marker_macro = directive[: directive.find("\n")]
        directives_map[__str_to_marker(marker_macro)] = directive[len(marker_macro) :]
    return directives_map


def __split_to_marker_directives_and_code(instrumented_code: str) -> tuple[str, str]:
    """Splits the instrumented code into the marker preprocessor
    directives and the actual code.

    Args:
        instrumented_code (str): the output of the instrumenter

    Returns:
        tuple[str,str]:
            marker preprocessor directives, instrumented code
    """
    markers_start = "//MARKERS START\n"
    markers_end = "//MARKERS END\n"
    assert instrumented_code.startswith(markers_start), instrumented_code
    markers_end_idx = instrumented_code.find(markers_end)
    assert markers_end_idx != -1, instrumented_code
    code_idx = markers_end_idx + len(markers_end)
    directives = instrumented_code[len(markers_start) : markers_end_idx]
    code = instrumented_code[code_idx:]
    return directives, code


@cache
def get_instrumenter(
    instrumenter: Optional[ClangTool] = None, clang: Optional[CompilerExe] = None
) -> ClangTool:
    if not instrumenter:
        if not clang:
            # TODO: move this to diopter
            try:
                clang = CompilerExe.get_system_clang()
            except:  # noqa: E722
                pass
            if not clang:
                try:
                    clang = CompilerExe.from_path(Path("clang-15"))
                except:  # noqa: E722
                    pass
            if not clang:
                clang = CompilerExe.from_path(Path("clang-14"))

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
        program, flags, ClangToolMode.READ_MODIFIED_FILE
    )
    assert result.modified_source_code

    directives, instrumented_code = __split_to_marker_directives_and_code(
        result.modified_source_code
    )

    directives_map = __split_marker_directives(directives)
    dce_markers = tuple(
        marker for marker in directives_map.keys() if isinstance(marker, DCEMarker)
    )
    vr_markers = tuple(
        marker for marker in directives_map.keys() if isinstance(marker, VRMarker)
    )

    return InstrumentedProgram(
        code=instrumented_code,
        language=program.language,
        defined_macros=program.defined_macros,
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        marker_preprocessor_directives=directives_map,
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
