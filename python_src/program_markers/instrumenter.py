from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass, replace
from enum import Enum
from functools import cache
from pathlib import Path
from typing import Optional, Sequence

from diopter.compiler import (
    ASMCompilationOutput,
    ClangTool,
    ClangToolMode,
    CompilationSetting,
    CompilerExe,
    ExeCompilationOutput,
    SourceProgram,
)
from program_markers.markers import (
    DCEMarker,
    FunctionCallStrategy,
    Marker,
    MarkerStrategy,
    VRMarker,
)

# TODO: The various hardcoded strings, e.g., "//MARKER_DIRECTIVES\n"
# should not be manually copied, instead either have some common file
# where they are defined or read them from some kind of info output,
# e.g., instrumenter --info


def find_non_eliminated_markers_impl(
    asm: str, program_markers: Sequence[Marker], marker_strategy: MarkerStrategy
) -> tuple[Marker, ...]:
    """Finds non-eliminated markers using the `marker_strategy` in `asm`.

    Args:
        asm (str):
            assembly code where to do the search
        program_markers (Sequence[Marker, ...]):
            the markers that we are looking for in the assembly code
        marker_strategy (MarkerStrategy):
            the strategy to use to find markers in the assembly code

    Returns:
        tuple[Marker, ...]:
            The markers detected in the assembly code
    """
    non_eliminated_markers: set[Marker] = set()
    marker_id_map = {marker.id: marker for marker in program_markers}
    for line in asm.split("\n"):
        marker_id = marker_strategy.detect_marker_id(line)
        if marker_id is None:
            continue
        non_eliminated_markers.add(marker_id_map[marker_id])

    return tuple(non_eliminated_markers)


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    marker_strategy: MarkerStrategy
    enabled_markers: tuple[Marker, ...] = tuple()
    disabled_markers: tuple[Marker, ...] = tuple()
    unreachable_markers: tuple[Marker, ...] = tuple()
    tracked_markers: tuple[Marker, ...] = tuple()
    tracked_for_refinement_markers: tuple[Marker, ...] = tuple()

    def __post_init__(self) -> None:
        """Sanity checks"""

        # There is no overlap between enabled, disabled,
        # unreachable, and tracked markers
        enabled_markers = set(self.enabled_markers)
        disabled_markers = set(self.disabled_markers)
        unreachable_markers = set(self.unreachable_markers)
        tracked_markers = set(self.tracked_markers)
        tracked_for_refinement_markers = set(self.tracked_for_refinement_markers)
        assert enabled_markers.isdisjoint(disabled_markers)
        assert enabled_markers.isdisjoint(unreachable_markers)
        assert enabled_markers.isdisjoint(tracked_markers)
        assert enabled_markers.isdisjoint(tracked_for_refinement_markers)
        assert disabled_markers.isdisjoint(unreachable_markers)
        assert disabled_markers.isdisjoint(tracked_markers)
        assert disabled_markers.isdisjoint(tracked_for_refinement_markers)
        assert unreachable_markers.isdisjoint(tracked_markers)
        assert unreachable_markers.isdisjoint(tracked_for_refinement_markers)
        assert tracked_markers.isdisjoint(tracked_for_refinement_markers)

        # All markers ids are unique
        marker_ids = set()
        for marker in self.all_markers():
            assert marker.id not in marker_ids
            marker_ids.add(marker.id)

    def generate_preprocessor_directives(self) -> str:
        return (
            "\n".join(
                marker.emit_enabled_directive(self.marker_strategy)
                for marker in self.enabled_markers
            )
            + "\n"
            + "\n".join(
                marker.emit_disabling_directive() for marker in self.disabled_markers
            )
            + "\n"
            + "\n".join(
                marker.emit_unreachable_directive()
                for marker in self.unreachable_markers
            )
            + "\n"
            + "\n".join(
                marker.emit_tracking_directive() for marker in self.tracked_markers
            )
            + "\n"
            + "\n".join(
                marker.emit_tracking_directive_for_refinement()
                for marker in self.tracked_for_refinement_markers
            )
        )

    def get_modified_code(self) -> str:
        """Returns the necessary preprocessor directives for markers + self.code.

        Only directives for the enabled, disabled and made unreachable markers
        are added.

        If any markers have not been enabled, disabled, or made unreachable,
        then the code not compilable but it can be preprocessed.

        Returns:
            str:
                the source code including the necessary preprocessor directives
        """

        return self.generate_preprocessor_directives() + "\n" + self.code

    def all_markers(self) -> tuple[Marker, ...]:
        """Return all of this program's markers.

        Returns:
            tuple[Marker, ...]:
                All markers
        """
        return (
            self.enabled_markers
            + self.disabled_markers
            + self.unreachable_markers
            + self.tracked_markers
            + self.tracked_for_refinement_markers
        )

    def find_non_eliminated_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[Marker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        the non-eliminated markers.

        The DCE markers are found by matching the regex pattern specified by the used
        marker strategy.
        The VR markers are found by searching for calls or jumps to functions with
        names starting with a known marker prefix (e.g., VRMarkerE123_)

        Args:
            compilation_setting (CompilationSetting):
                the setting used to compile the program
        Returns:
            tuple[Marker, ...]:
                The non_eliminated markers for the given compilation setting.
        """
        asm = compilation_setting.compile_program(
            self, ASMCompilationOutput()
        ).output.read()
        non_eliminated_markers = find_non_eliminated_markers_impl(
            asm, self.enabled_markers, self.marker_strategy
        )
        assert set(non_eliminated_markers) <= set(self.all_markers())
        return non_eliminated_markers

    def find_eliminated_markers(
        self, compilation_setting: CompilationSetting, include_all_markers: bool = False
    ) -> tuple[Marker, ...]:
        """Compiles the program to ASM with `compilation_setting` and finds
        the eliminated markers.

        Args:
            compilation_setting (CompilationSetting):
                the setting used to compile the program
            include_all_markers (bool):
                if true disabled and unreachable markers are included

        Returns:
            tuple[Marker, ...]:
                The eliminated markers for the given compilation setting.
        """
        eliminated_markers = set(self.all_markers()) - set(
            self.find_non_eliminated_markers(compilation_setting)
        )
        if not include_all_markers:
            eliminated_markers = eliminated_markers & set(self.enabled_markers)
        return tuple(eliminated_markers)

    def replace_markers(self, new_markers: tuple[Marker, ...]) -> InstrumentedProgram:
        """Replaces the markers in the program with the new ones.
        Each original marker whose id matches of the new ones is replaced,
        any markers whose id is not included in `new_markers` is maintained.

        Args:
            new_markers (tuple[Marker, ...]):
                the new markers to replace the old ones
        Returns:
            InstrumentedProgram:
                the new program with the new markers
        """
        new_markers_dict = {marker.id: marker for marker in new_markers}
        new_enabled_markers = tuple(
            new_markers_dict.get(marker.id, marker) for marker in self.enabled_markers
        )
        new_disabled_markers = tuple(
            new_markers_dict.get(marker.id, marker) for marker in self.disabled_markers
        )
        new_unreachable_markers = tuple(
            new_markers_dict.get(marker.id, marker)
            for marker in self.unreachable_markers
        )
        assert len(self.tracked_markers) == 0
        assert len(self.tracked_for_refinement_markers) == 0
        return replace(
            self,
            enabled_markers=new_enabled_markers,
            disabled_markers=new_disabled_markers,
            unreachable_markers=new_unreachable_markers,
        )

    def track_reachable_markers(
        self,
        args: tuple[str, ...],
        setting: CompilationSetting,
        timeout: int | None = None,
    ) -> tuple[Marker, ...]:
        """Runs the program and tracks which markers are reachable(executed).
        Ureachable and disabled markers are ignored.

        Ars:
            args (tuple[str,...]):
                arguments to pass to the program
            setting (CompilationSetting):
                the compiler used to compile to program
            timeout (int | None):
                if not None, abort after `timeout` seconds
        Returns:
            tuple[Marker, ...]:
                the markers that were "encountered" during execution
        """

        tracked_program = replace(
            self, enabled_markers=tuple(), tracked_markers=tuple(self.enabled_markers)
        )
        result = setting.compile_program(tracked_program, ExeCompilationOutput())
        output = result.output.run(args, timeout=timeout)
        return tuple(
            marker for marker in self.enabled_markers if marker.name in output.stdout
        )

    def refine_markers_with_runtime_information(
        self,
        args: tuple[str, ...],
        setting: CompilationSetting,
        timeout: int | None = None,
    ) -> tuple[InstrumentedProgram, tuple[Marker, ...]]:
        """Runs the program and tracks which markers are reachable(executed).
        The markers' emit_tracking_directive_for_refinement are used to print
        runtime information relevant to each marker. The output is then parsed
        with parse_tracked_output_for_refinement and each refined marker is
        updated.

        One example usage of this is to update the bounds of VRMarkers based
        on the values encountered during execution.


        Ars:
            args (tuple[str,...]):
                arguments to pass to the program
            setting (CompilationSetting):
                the compiler used to compile to program
            timeout (int | None):
                if not None, abort after `timeout` seconds
        Returns:
            tuple[InstrumentedProgram, tuple[Marker, ...]]:
                the refined program with the updated
                markers and the refined markers
        """

        tracked_program = replace(
            self,
            enabled_markers=tuple(),
            tracked_for_refinement_markers=tuple(self.enabled_markers),
        )
        result = setting.compile_program(tracked_program, ExeCompilationOutput())
        output = result.output.run(args, timeout=timeout)
        marker_names = {marker.name: marker for marker in self.enabled_markers}
        marker_lines: dict[Marker, list[str]] = defaultdict(list)
        for output_line in output.stdout.splitlines():
            for marker_name, marker in marker_names.items():
                if marker_name in output_line:
                    marker_lines[marker].append(output_line)

        refined_markers = tuple(
            marker.parse_tracked_output_for_refinement(lines)
            for marker, lines in marker_lines.items()
        )
        return self.replace_markers(refined_markers), refined_markers

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
        assert dmarkers_set <= set(self.enabled_markers + self.disabled_markers)

        dmarkers_set |= set(self.disabled_markers)
        new_enabled_markers = set(self.enabled_markers) - dmarkers_set

        return replace(
            self,
            enabled_markers=tuple(new_enabled_markers),
            disabled_markers=tuple(dmarkers_set),
        )

    def make_markers_unreachable(
        self, umarkers: Sequence[Marker]
    ) -> InstrumentedProgram:
        """Makes the given markers unreachable by setting the relevant macros.

        Markers that have already been made unreachable are ignored. If any of
        the markers are not in self.enabled_markers an AssertionError will be raised.
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
        assert umarkers_set <= set(self.enabled_markers + self.unreachable_markers)
        assert umarkers_set.isdisjoint(self.disabled_markers)

        umarkers_set |= set(self.unreachable_markers)
        new_enabled_markers = set(self.enabled_markers) - umarkers_set
        return replace(
            self,
            unreachable_markers=tuple(umarkers_set),
            enabled_markers=tuple(new_enabled_markers),
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
        if not self.enabled_markers:
            return self

        new_disabled_markers = set(self.disabled_markers) | (
            set(self.enabled_markers) - set(do_not_disable)
        )
        new_enabled_markers = set(self.enabled_markers) - new_disabled_markers

        return replace(
            self,
            disabled_markers=tuple(new_disabled_markers),
            enabled_markers=tuple(new_enabled_markers),
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
                a program with only the originally enabled markers, the
                preprocessor directives of the other ones have been expanded
        """

        # Preprocess a program that does not contain the enabled markers and
        # their directives. It still includes the macros in the code, e.g.,
        # DCEMarker0_, but since their directives are missing they won't be
        # expanded.
        program_with_markers_removed = replace(
            self,
            enabled_markers=tuple(),
            disabled_markers=self.disabled_markers,
            unreachable_markers=self.unreachable_markers,
        )
        pprogram = setting.preprocess_program(
            program_with_markers_removed, make_compiler_agnostic=make_compiler_agnostic
        )

        return replace(
            pprogram,
            enabled_markers=self.enabled_markers,
            disabled_markers=tuple(),
            unreachable_markers=tuple(),
        )

    def with_marker_strategy(
        self, marker_strategy: MarkerStrategy
    ) -> InstrumentedProgram:
        """Returns a new program using the new `marker_strategy`.

        Args:
            marker_strategy (MarkerStrategy):
                the strategy the program uses to find non eliminated markers

        Returns:
            InstrumentedProgram:
                the new program
        """
        return replace(
            self,
            marker_strategy=marker_strategy,
        )


def __variable_type_of_VRMarker(vr_marker_macro: str, instrumented_code: str) -> str:
    """Finds the variable type for the given VRMarker macro
    Args:
        vr_marker_macro (str):
            a marker marker in the form VRMarkerX_,
        instrumented_code (str):
            the output of the instrumenter (the variable types
            of VRMarkers are parsed from the instrumented_code)
    Returns:
        str:
            the instrumented variable type
    """
    dummy_marker = VRMarker.from_str(vr_marker_macro, "dummy")
    instr_macro = dummy_marker.macro().split("(")[0]
    for line in instrumented_code.splitlines():
        if instr_macro in line:
            line = line[line.index(instr_macro) :]
            line = line[: line.find(")")]
            variable_type = (line.split(",")[1].replace('"', "")).strip()
            if variable_type == "uint16_t":
                variable_type = "unsigned short"
            elif variable_type == "int16_t":
                variable_type = "short"
            elif variable_type == "uint32_t":
                variable_type = "unsigned int"
            elif variable_type == "uint64_t":
                variable_type = "unsigned long"
            elif variable_type == "uint8_t":
                variable_type = "unsigned int"
            elif variable_type == "int32_t":
                variable_type = "int"
            elif variable_type == "int64_t":
                variable_type = "long"
            elif variable_type == "int8_t":
                variable_type = "int"

            assert variable_type in [
                "short",
                "int",
                "long",
                "unsigned short",
                "unsigned int",
                "unsigned long",
            ], f"Unexpected variable type for VRMarker: {variable_type} in {line}"
            return variable_type
    raise ValueError(f"Could not find variable type for {vr_marker_macro}")


def __str_to_marker(marker_macro: str, instrumented_code: str) -> Marker:
    """Converts the `marker_macro` string into a `Marker.
    Args:
        marker_macro (str):
            a marker in the form PrefixMarkerX_, e.g., DCEMarker32_.
        instrumented_code (str):
            the output of the instrumenter (the variable types
            of VRMarkers are parsed from the instrumented_code)
    Returns:
        Marker:
            a `Marker` object
    """
    if marker_macro.startswith(DCEMarker.prefix()):
        return DCEMarker.from_str(marker_macro)
    else:
        assert marker_macro.startswith(VRMarker.prefix())
        return VRMarker.from_str(
            marker_macro, __variable_type_of_VRMarker(marker_macro, instrumented_code)
        )


def __split_marker_directives(
    directives: str, instrumented_code: str
) -> dict[Marker, str]:
    """Maps each set of preprocessor directive in `directives` to the
    appropriate markers.

    Args:
        directives (str):
            the marker preprocessor directives added by the instrumenter
        instrumented_code (str):
            the output of the instrumenter (the variable types
            of VRMarkers are parsed from the instrumented_code)
    Returns:
        dict[Marker, str]:
            a mapping from each marker to its preprocessor directives
    """
    directives_map = {}
    for directive in directives.split("//MARKER_DIRECTIVES:")[1:]:
        marker_macro = directive[: directive.find("\n")]
        directives_map[__str_to_marker(marker_macro, instrumented_code)] = directive[
            len(marker_macro) :
        ]
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
            Path(__file__).parent / "program-markers", clang
        )
    return instrumenter


class InstrumenterMode(Enum):
    DCE = 0
    VR = 1
    DCE_AND_VR = 2


def instrument_program(
    program: SourceProgram,
    ignore_functions_with_macros: bool = False,
    mode: InstrumenterMode = InstrumenterMode.DCE,
    instrumenter: Optional[ClangTool] = None,
    clang: Optional[CompilerExe] = None,
    timeout: int | None = None,
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
        timeout (int | None):
            Optional timeout in seconds for the instrumenter
    Returns:
        InstrumentedProgram: The instrumented version of program
    """

    instrumenter_resolved = get_instrumenter(instrumenter, clang)

    flags = []
    if ignore_functions_with_macros:
        flags.append("--ignore-functions-with-macros=1")
    else:
        flags.append("--ignore-functions-with-macros=0")

    def get_code_and_markers(mode: str) -> tuple[str, list[Marker]]:
        result = instrumenter_resolved.run_on_program(
            program,
            flags + [f"--mode={mode}"],
            ClangToolMode.READ_MODIFIED_FILE,
            timeout=timeout,
        )
        assert result.modified_source_code
        directives, instrumented_code = __split_to_marker_directives_and_code(
            result.modified_source_code
        )

        directives_map = __split_marker_directives(directives, instrumented_code)
        return instrumented_code, list(directives_map.keys())

    match mode:
        case InstrumenterMode.DCE:
            instrumented_code, markers = get_code_and_markers("dce")
        case InstrumenterMode.VR:
            instrumented_code, markers = get_code_and_markers("vr")
        case InstrumenterMode.DCE_AND_VR:
            result = instrumenter_resolved.run_on_program(
                program,
                flags + ["--mode=dce"],
                ClangToolMode.READ_MODIFIED_FILE,
                timeout=timeout,
            )
            assert result.modified_source_code
            program_dce = replace(program, code=result.modified_source_code)
            result = instrumenter_resolved.run_on_program(
                program_dce,
                flags + ["--mode=vr"],
                ClangToolMode.READ_MODIFIED_FILE,
                timeout=timeout,
            )
            assert result.modified_source_code
            (
                vr_directives,
                dce_directives_and_instrumented_code,
            ) = __split_to_marker_directives_and_code(result.modified_source_code)
            dce_directives, instrumented_code = __split_to_marker_directives_and_code(
                dce_directives_and_instrumented_code
            )
            vr_markers = list(
                __split_marker_directives(vr_directives, instrumented_code).keys()
            )
            dce_markers = list(
                __split_marker_directives(dce_directives, instrumented_code).keys()
            )
            if len(vr_markers) > 0 and len(dce_markers) > 0:
                # There will be overlap between the two sets of marker ids
                max_vr_id = max(vr_marker.id for vr_marker in vr_markers)
                replacements = []
                new_dce_markers = []
                for dce_marker in dce_markers:
                    old_macro = dce_marker.macro()
                    old_id = dce_marker.id
                    new_id = old_id + max_vr_id + 1
                    new_marker = replace(
                        dce_marker,
                        id=new_id,
                        name=dce_marker.name.replace(str(old_id), str(new_id)),
                    )
                    new_macro = new_marker.macro()
                    replacements.append((old_macro, new_macro))
                    new_dce_markers.append(new_marker)
                dce_markers = new_dce_markers
                for old_macro, new_macro in replacements:
                    instrumented_code = instrumented_code.replace(
                        old_macro, new_macro, 1
                    )
                markers = dce_markers + vr_markers

    return InstrumentedProgram(
        code=instrumented_code,
        marker_strategy=FunctionCallStrategy(),
        language=program.language,
        defined_macros=tuple(),
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        enabled_markers=tuple(markers),
    )
