from __future__ import annotations

import re
from dataclasses import dataclass, replace
from functools import cache
from itertools import chain
from pathlib import Path
from typing import Optional

from diopter.compiler import (
    ClangTool,
    ClangToolMode,
    CompilationSetting,
    CompilerExe,
    SourceProgram,
)


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    markers: tuple[str, ...] = tuple()
    disabled_markers: tuple[str, ...] = tuple()
    unreachable_markers: tuple[str, ...] = tuple()
    marker_prefix: str = "DCEMarker"

    def __post_init__(self) -> None:
        """Sanity checks"""

        # Markers have the correct prefix
        markers = set(self.markers)
        for marker in markers:
            assert marker.startswith(self.marker_prefix)

        # There is no overlap between disabled and unreachable markers
        disabled_markers = set(self.disabled_markers)
        unreachable_markers = set(self.unreachable_markers)
        assert disabled_markers.isdisjoint(unreachable_markers)

        # Disabled and unreachable markers are subsets of all markers
        assert set(self.disabled_markers) <= markers
        assert set(self.unreachable_markers) <= markers

        # Macros have been included for all disabled and unreachable markers
        macros = set(self.defined_macros)
        for marker in chain(
            map(InstrumentedProgram.__make_disable_macro, self.disabled_markers),
            map(InstrumentedProgram.__make_unreachable_macro, self.unreachable_markers),
        ):
            assert marker in macros
            macros.remove(marker)

        # No disable or unreachable marcos have been defined for other markers
        dprefix = InstrumentedProgram.__make_disable_macro(self.marker_prefix)
        uprefix = InstrumentedProgram.__make_unreachable_macro(self.marker_prefix)
        for macro in macros:
            assert not macro.startswith(dprefix) and not macro.startswith(uprefix)

    def find_alive_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[str, ...]:
        alive_regex = re.compile(f".*[call|jmp].*{self.marker_prefix}([0-9]+)_.*")
        asm = compilation_setting.get_asm_from_program(self)
        alive_markers = set()
        for line in asm.split("\n"):
            m = alive_regex.match(line.strip())
            if m:
                alive_markers.add(f"{self.marker_prefix}{m.group(1)}_")
        assert alive_markers <= set(self.markers)
        return tuple(alive_markers)

    def find_dead_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[str, ...]:
        dead_markers = set(self.markers) - set(
            self.find_alive_markers(compilation_setting)
        )
        assert dead_markers <= set(self.markers)
        return tuple(dead_markers)

    @staticmethod
    def __make_disable_macro(marker: str) -> str:
        return "Disable" + marker

    @staticmethod
    def __make_unreachable_macro(marker: str) -> str:
        return "Unreachable" + marker

    def disable_markers(self, dmarkers: tuple[str, ...]) -> InstrumentedProgram:
        """Disables the given markers by setting the relevant macros.

        Markers that have already been disabled are ignored. If any of the
        markers are not in self.markers an AssertionError will be raised.  An
        AssertionError error is also raised if markers have been previously
        made unreachable.

        Args:
            markers (tuple[str, ...]):
                The markers that will be disabled

        Returns:
            InstrumentedProgram:
                A copy of self with the additional disabled markers
        """

        dmarkers_set = set(dmarkers)
        assert dmarkers_set <= set(self.markers)
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
        self, umarkers: tuple[str, ...]
    ) -> InstrumentedProgram:
        """Makes the given markers unreachable by setting the relevant macros.

        Markers that have already been made unreachable are ignored. If any of
        the markers are not in self.markers an AssertionError will be raised.
        An AssertionError error is also raised if markers have been previously
        disabled.


        Args:
            markers (tuple[str, ...]):
                The markers that will be made unreachable

        Returns:
            InstrumentedProgram:
                A copy of self with the additional unreachable markers
        """

        umarkers_set = set(umarkers)
        assert umarkers_set <= set(self.markers)
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
            set(self.markers)
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


def find_all_markers(
    instrumenter_output: str,
) -> tuple[str, ...]:
    # XXX: maybe this should also extract the marker prefix?
    _, _, instrumenter_output = instrumenter_output.partition("MARKERS START")
    assert instrumenter_output
    instrumenter_output, _, _ = instrumenter_output.partition("MARKERS END")
    return tuple(instrumenter_output.strip().splitlines())


@cache
def get_instrumenter(
    instrumenter: Optional[ClangTool] = None, clang: Optional[CompilerExe] = None
) -> ClangTool:
    if not instrumenter:
        if not clang:
            clang = CompilerExe.get_system_clang()
        instrumenter = ClangTool.init_with_paths_from_clang(
            Path(__file__).parent / "dead-instrument", clang
        )
    return instrumenter


def instrument_program(
    program: SourceProgram,
    ignore_functions_with_macros: bool = False,
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
    prefix = "DCEMarker"
    flags.append("--mode=instrument")
    if ignore_functions_with_macros:
        flags.append("--ignore-functions-with-macros")

    result = instrumenter_resolved.run_on_program(
        program, flags, ClangToolMode.CAPTURE_OUT_ERR_AND_READ_MODIFIED_FILED
    )
    assert result.modified_source_code
    assert result.stdout

    instrumented_code = result.modified_source_code

    markers = find_all_markers(result.stdout)
    macros = ["Disable" + marker for marker in markers] + [
        "Unreachable" + marker for marker in markers
    ]

    return InstrumentedProgram(
        code=instrumented_code,
        language=program.language,
        available_macros=program.available_macros + tuple(macros),
        defined_macros=program.defined_macros,
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        markers=markers,
        marker_prefix=prefix,
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

    return SourceProgram(
        code=result.modified_source_code,
        language=program.language,
        available_macros=program.available_macros,
        defined_macros=program.defined_macros,
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
    )
