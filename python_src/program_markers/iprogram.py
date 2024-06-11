from __future__ import annotations

import json
import re
from collections import defaultdict
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, Sequence

from diopter.compiler import (
    ASMCompilationOutput,
    CompilationOutputType,
    CompilationResult,
    CompilationSetting,
    ExeCompilationOutput,
    Language,
    SourceFile,
    SourceProgram,
)
from program_markers.markers import (
    AbortEmitter,
    DisableEmitter,
    EnableEmitter,
    Marker,
    MarkerDetectionStrategy,
    MarkerDirectiveEmitter,
    NoEmitter,
    TrackingEmitter,
    TrackingForRefinementEmitter,
    UnreachableEmitter,
)


def find_non_eliminated_markers_impl(
    asm: str,
    program_markers: Sequence[Marker],
    marker_strategy: MarkerDetectionStrategy,
) -> tuple[Marker, ...]:
    """Finds non-eliminated markers using the `marker_strategy` in `asm`.

    Args:
        asm (str):
            assembly code where to do the search
        program_markers (Sequence[Marker, ...]):
            the markers that we are looking for in the assembly code
        marker_strategy (MarkerDetectionStrategy):
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


def rename_markers(
    programs: Sequence[InstrumentedProgram],
) -> list[InstrumentedProgram]:
    """Rename all the markers in `programs` to be unique.
    Args:
        programs (Sequence[InstrumentedProgram]):
            the programs whose markers to rename
    Returns:
        list[InstrumentedProgram]:
            the programs with renamed markers
    """
    current_marker_id = len(programs[0].markers)

    def collect_markers(
        markers: tuple[Marker, ...],
        directive_emitters: dict[Marker, MarkerDirectiveEmitter],
        replacements: dict[tuple[str, int], str],
    ) -> tuple[tuple[Marker, ...], dict[Marker, MarkerDirectiveEmitter]]:
        nonlocal current_marker_id
        new_markers = []
        new_directive_emitters = {}
        for marker in markers:
            assert (marker.macro_without_arguments(), marker.id) not in replacements
            new_marker = marker.update_id(current_marker_id)
            current_marker_id += 1
            new_markers.append(new_marker)
            new_directive_emitters[new_marker] = directive_emitters[marker]
            replacements[(marker.macro_without_arguments(), marker.id)] = (
                new_marker.macro_without_arguments()
            )
        return tuple(new_markers), new_directive_emitters

    new_programs = [programs[0]]
    for program in programs[1:]:
        replacements: dict[tuple[str, int], str] = {}
        new_markers, new_directive_emitters = collect_markers(
            program.markers, program.directive_emitters, replacements
        )
        new_code = program.code
        for (old, marker_id), new in sorted(
            replacements.items(), key=lambda x: x[0][1], reverse=True
        ):
            new_code = new_code.replace(old, new)
        new_programs.append(
            replace(
                program,
                code=new_code,
                markers=new_markers,
                directive_emitters=new_directive_emitters,
            )
        )
    return new_programs


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    marker_strategy: MarkerDetectionStrategy
    markers: tuple[Marker, ...]
    directive_emitters: dict[Marker, MarkerDirectiveEmitter]

    def __post_init__(self) -> None:
        # All markers ids are unique
        marker_ids = set()
        for marker in self.markers:
            assert marker.id not in marker_ids
            marker_ids.add(marker.id)

        for marker in self.markers:
            assert marker in self.directive_emitters

    def to_file(
        self, filename: Path, include_file: Path, json_file: Path
    ) -> SourceFile:
        """The instrumented code is returned as a SourceFile object.
        The marker directives are emitted in a separate file (include_file).
        This file is included in the main file (filename) using the -include
        command line flag. All marker information and directive emitters are
        stored in a json file (json_file).

        Can be reparsed via from source_file.

        """
        with open(filename, "w") as f:
            f.write(self.code)
        with open(include_file, "w") as f:
            f.write(self.generate_preprocessor_directives())
        with open(json_file, "w") as f:
            json.dump(self.marker_stuff_to_json_dict(), f)
        return SourceFile(
            filename=filename,
            language=self.language,
            defined_macros=self.defined_macros,
            include_paths=self.include_paths,
            system_include_paths=self.system_include_paths,
            flags=self.flags + (f"-include {include_file.resolve()}",),
        )

    @staticmethod
    def from_source_file(
        source_file: SourceFile, include_file: Path, json_file: Path
    ) -> InstrumentedProgram:
        with open(json_file) as f:
            d = json.load(f)
        directive_emitters = {
            Marker.from_json_dict(m): MarkerDirectiveEmitter.from_json_dict(directive)
            for m, directive in d["directive_emitters"]
        }
        markers = tuple(Marker.from_json_dict(m) for m in d["markers"])
        marker_strategy = MarkerDetectionStrategy.from_json_dict(d["marker_strategy"])

        return InstrumentedProgram(
            language=source_file.language,
            defined_macros=source_file.defined_macros,
            include_paths=source_file.include_paths,
            system_include_paths=source_file.system_include_paths,
            flags=tuple(f for f in source_file.flags if str(include_file) not in f),
            code=source_file.filename.read_text(),
            markers=markers,
            directive_emitters=directive_emitters,
            marker_strategy=marker_strategy,
        )

    def enabled_markers(self) -> tuple[Marker, ...]:
        """Returns the enabled markers."""

        return tuple(
            marker
            for marker, emitter in self.directive_emitters.items()
            if isinstance(emitter, EnableEmitter)
        )

    def disabled_markers(self) -> tuple[Marker, ...]:
        """Returns the disabled markers."""

        return tuple(
            marker
            for marker, emitter in self.directive_emitters.items()
            if isinstance(emitter, DisableEmitter)
        )

    def unreachable_markers(self) -> tuple[Marker, ...]:
        """Returns the unreachable markers."""

        return tuple(
            marker
            for marker, emitter in self.directive_emitters.items()
            if isinstance(emitter, UnreachableEmitter)
        )

    def generate_preprocessor_directives(self) -> str:
        """Returns the necessary preprocessor directives for markers."""
        return "\n".join(
            emitter.emit_directive(marker)
            for marker, emitter in self.directive_emitters.items()
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
            asm, self.enabled_markers(), self.marker_strategy
        )
        assert set(non_eliminated_markers) <= set(self.markers)
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
        eliminated_markers = set(self.markers) - set(
            self.find_non_eliminated_markers(compilation_setting)
        )
        if not include_all_markers:
            eliminated_markers = eliminated_markers & set(self.enabled_markers())
        return tuple(eliminated_markers)

    def replace_markers(self, new_markers: tuple[Marker, ...]) -> InstrumentedProgram:
        """Replaces the markers in the program with the new ones.
        Each original marker whose id matches one of the new ones is replaced,
        any markers whose id is not included in `new_markers` is maintained.

        Args:
            new_markers (tuple[Marker, ...]):
                the new markers to replace the old ones
        Returns:
            InstrumentedProgram:
                the new program with the new markers
        """
        new_markers_dict = {marker.id: marker for marker in new_markers}
        old_markers_dict = {marker.id: marker for marker in self.markers}
        unchanged_markers = tuple(
            old_markers_dict[id_]
            for id_ in set(old_markers_dict) - set(new_markers_dict)
        )
        new_markers = unchanged_markers + new_markers
        new_directive_emitters = {}
        for marker in unchanged_markers:
            new_directive_emitters[marker] = self.directive_emitters[marker]
        for marker in new_markers:
            new_directive_emitters[marker] = self.directive_emitters[
                old_markers_dict[marker.id]
            ]
        return replace(
            self,
            markers=new_markers,
            directive_emitters=new_directive_emitters,
        )

    def generate_tracking_program(self) -> InstrumentedProgram:
        new_emitters = self.directive_emitters.copy()
        tracked_markers = self.enabled_markers()
        te = TrackingEmitter()
        for marker in tracked_markers:
            new_emitters[marker] = te

        return replace(self, directive_emitters=new_emitters)

    def compile_program_for_tracking(
        self,
        setting: CompilationSetting,
        output: CompilationOutputType,
        timeout: int | None = None,
    ) -> CompilationResult[CompilationOutputType]:
        tracked_program = self.generate_tracking_program()
        return setting.compile_program(tracked_program, output, timeout=timeout)

    def process_tracking_reachable_markers_output(
        self, output: str
    ) -> tuple[Marker, ...]:
        return tuple(
            marker for marker in self.enabled_markers() if marker.name in output
        )

    def track_reachable_markers(
        self,
        args: tuple[str, ...],
        setting: CompilationSetting,
        timeout: int | None = None,
    ) -> tuple[Marker, ...]:
        """Runs the program and tracks which markers are reachable(executed).
        Only enabled markers (EnableEmitter) are tracked.
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
        result = self.compile_program_for_tracking(
            setting, ExeCompilationOutput(), timeout=timeout
        )
        output = result.output.run(args, timeout=timeout)
        return self.process_tracking_reachable_markers_output(output.stdout)

    def generate_tracking_program_for_refinement(
        self,
    ) -> InstrumentedProgram:
        new_emitters = self.directive_emitters.copy()
        tfre = TrackingForRefinementEmitter()
        for marker in self.enabled_markers():
            new_emitters[marker] = tfre
        return replace(
            self,
            directive_emitters=new_emitters,
        )

    def compile_program_for_refinement(
        self,
        setting: CompilationSetting,
        output: CompilationOutputType,
        timeout: int | None = None,
    ) -> CompilationResult[CompilationOutputType]:
        """Compiles the program with tracking code for markers.
        Only enabled markers (EnableEmitter) are tracked.
        The markers' emit_tracking_directive_for_refinement are used to print
        runtime information relevant to each marker.

        Ars:
            setting (CompilationSetting):
                the compiler used to compile to program
            output (CompilationOutputType):
                the type of output to use
            timeout (int | None):
                if not None, abort after `timeout` seconds
        Returns:
            CompilationOutputType:
                the output of the compilation
        """
        tracked_program = self.generate_tracking_program_for_refinement()
        return setting.compile_program(tracked_program, output, timeout=timeout)

    def process_tracked_output_for_refinement(
        self,
        output: str,
    ) -> tuple[InstrumentedProgram, tuple[Marker, ...]]:
        """Processes the output of a program compiled with tracking code for
        markers.  The output is parsed with parse_tracked_output_for_refinement
        and each refined marker is updated.


        Ars:
            output (str):
                the output (printed in stdout) of the program
        Returns:
            tuple[InstrumentedProgram, tuple[Marker, ...]]:
                the refined program with the updated
                markers and the refined markers
        """

        marker_names = {marker.name: marker for marker in self.enabled_markers()}
        marker_lines: dict[Marker, list[str]] = defaultdict(list)
        pattern = r"<MarkerTracking>(.*?)<\/MarkerTracking>"
        matches = re.findall(pattern, output)
        for match in matches:
            # Maybe I should move the parsing/splitting to the Marker class?
            # else I'd have to keep this code in sync with whatever changes
            # in the printing in Marker.emit_tracking_directive_for_refinement
            marker_name = match.split(":")[0]
            if marker_name not in marker_names:
                continue
            marker_lines[marker_names[marker_name]].append(match)

        refined_markers = tuple(
            marker.parse_tracked_output_for_refinement(lines)
            for marker, lines in marker_lines.items()
        )
        return self.replace_markers(refined_markers), refined_markers

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

        output = self.compile_program_for_refinement(
            setting, ExeCompilationOutput(), timeout
        ).output.run(args, timeout=timeout)
        return self.process_tracked_output_for_refinement(output.stdout)

    def disable_markers(self, dmarkers: Sequence[Marker]) -> InstrumentedProgram:
        """Disables the given markers by switching them to the DisableEmitter.

        Only enabled markers (EnableEmitter) can be disabled.
        Markers that have already been disabled are ignored. If any of the
        markers are not in self.enabled() an AssertionError will be raised.

        Args:
            markers (Sequence[Marker]):
                The markers that will be disabled

        Returns:
            InstrumentedProgram:
                A copy of self with the additional disabled markers
        """

        dmarkers_set = set(dmarkers) - set(self.disabled_markers())
        assert dmarkers_set <= set(self.enabled_markers())

        new_directive_emitters = self.directive_emitters.copy()
        de = DisableEmitter()
        for marker in dmarkers_set:
            new_directive_emitters[marker] = de

        return replace(
            self,
            directive_emitters=new_directive_emitters,
        )

    def make_markers_unreachable(
        self, umarkers: Sequence[Marker]
    ) -> InstrumentedProgram:
        """Makes the given markers unreachable by setting the relevant macros.

        Markers that have already been made unreachable are ignored. If any of
        the markers are not in self.enabled_markers() an AssertionError will be raised.

        Args:
            markers (Sequence[Marker]):
                The markers that will be made unreachable

        Returns:
            InstrumentedProgram:
                A copy of self with the additional unreachable markers
        """

        umarkers_set = set(umarkers) - set(self.unreachable_markers())
        assert umarkers_set <= set(self.enabled_markers())
        new_directive_emitters = self.directive_emitters.copy()
        ue = UnreachableEmitter()
        for marker in umarkers_set:
            new_directive_emitters[marker] = ue

        return replace(self, directive_emitters=new_directive_emitters)

    def make_markers_aborted(self, markers: Sequence[Marker]) -> InstrumentedProgram:
        """Make `markers` calls to abort().

        Args:
            markes (Sequence[Marker]):
                the markers to turn into abort() calls

        Returns:
            InstrumentedProgram:
                A similar InstrumentedProgram as self but with `markers` make
                aborted
        """

        aborted_markers = set(
            marker
            for marker, emitter in self.directive_emitters.items()
            if isinstance(emitter, AbortEmitter)
        )
        abort_set = set(markers) - aborted_markers
        assert abort_set <= set(self.enabled_markers())
        new_directive_emitters = self.directive_emitters.copy()
        ae = AbortEmitter()
        for marker in abort_set:
            new_directive_emitters[marker] = ae

        return replace(self, directive_emitters=new_directive_emitters)

    def disable_remaining_markers(
        self, do_not_disable: Sequence[Marker] = tuple()
    ) -> InstrumentedProgram:
        """Disable all remaining enabled markers by setting the relevant macros.

        Args:
            do_not_disable (Sequence[Marker]):
                markers that will not be modified

        Returns:
            InstrumentedProgram:
                A similar InstrumentedProgram as self but with all remaining
                markers disabled and the corresponding macros defined.
        """

        new_disabled_markers = (
            set(self.enabled_markers())
            - set(do_not_disable)
            - set(self.disabled_markers())
        )

        new_directive_emitters = self.directive_emitters.copy()
        de = DisableEmitter()
        for marker in new_disabled_markers:
            new_directive_emitters[marker] = de

        return replace(
            self,
            directive_emitters=new_directive_emitters,
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

        ne = NoEmitter()
        new_directive_emitters = self.directive_emitters.copy()
        for marker in self.enabled_markers():
            new_directive_emitters[marker] = ne

        program_with_markers_removed = replace(
            self,
            directive_emitters=new_directive_emitters,
        )
        pprogram = setting.preprocess_program(
            program_with_markers_removed, make_compiler_agnostic=make_compiler_agnostic
        )

        ee = EnableEmitter(self.marker_strategy)
        return replace(
            pprogram,
            markers=self.enabled_markers(),
            directive_emitters={marker: ee for marker in self.enabled_markers()},
        )

    def with_marker_strategy(
        self, marker_strategy: MarkerDetectionStrategy
    ) -> InstrumentedProgram:
        """Returns a new program using the new `marker_strategy`.

        Args:
            marker_strategy (MarkerDetectionStrategy):
                the strategy the program uses to find non eliminated markers

        Returns:
            InstrumentedProgram:
                the new program
        """
        new_emitters = self.directive_emitters.copy()
        ee = EnableEmitter(marker_strategy)
        for marker, emitter in new_emitters.items():
            if isinstance(emitter, EnableEmitter):
                new_emitters[marker] = ee
            elif hasattr(emitter, "strategy"):
                raise ValueError(
                    f"emitter {emitter} has a strategy attribute "
                    "but I don't know how to handle this"
                )
        return replace(
            self,
            marker_strategy=marker_strategy,
            directive_emitters=new_emitters,
        )

    def marker_stuff_to_json_dict(self) -> dict[str, Any]:
        j: dict[str, Any] = {}
        j["marker_strategy"] = self.marker_strategy.to_json_dict()
        j["directive_emitters"] = [
            (m.to_json_dict(), e.to_json_dict())
            for m, e in self.directive_emitters.items()
        ]
        j["markers"] = [m.to_json_dict() for m in self.markers]
        return j

    def to_json_dict_impl(self) -> dict[str, Any]:
        """Serializes the InstrumentedProgram specific attributes to a
        JSON-serializable dict.

        Returns:
            dict[str, Any]:
                the serialized InstrumentedProgram
        """
        j = SourceProgram.to_json_dict_impl(self)
        j["kind"] = "InstrumentedProgram"
        j.update(self.marker_stuff_to_json_dict())
        return j

    @staticmethod
    def from_json_dict_impl(
        d: dict[str, Any],
        language: Language,
        defined_macros: tuple[str, ...],
        include_paths: tuple[str, ...],
        system_include_paths: tuple[str, ...],
        flags: tuple[str, ...],
    ) -> InstrumentedProgram:
        """Returns an instrumented program  parsed from a json dictionary.
        Args:
           d (dict[str, Any]):
               the dictionary
           language (Language):
               the program's language
           defined_macros (tuple[str,...]):
               macros that will be defined when compiling this program
           include_paths (tuple[str,...]):
               include paths which will be passed to the compiler (with -I)
           system_include_paths (tuple[str,...]):
               system include paths which will be passed to the compiler (with -isystem)
           flags (tuple[str,...]):
               flags, prefixed with a dash ("-") that will be passed to the compiler
        """
        assert d["kind"] == "InstrumentedProgram"

        directive_emitters = {
            Marker.from_json_dict(m): MarkerDirectiveEmitter.from_json_dict(directive)
            for m, directive in d["directive_emitters"]
        }

        markers = tuple(Marker.from_json_dict(m) for m in d["markers"])

        return InstrumentedProgram(
            language=language,
            defined_macros=defined_macros,
            include_paths=include_paths,
            system_include_paths=system_include_paths,
            flags=flags,
            code=d["code"],
            marker_strategy=MarkerDetectionStrategy.from_json_dict(
                d["marker_strategy"]
            ),
            markers=markers,
            directive_emitters=directive_emitters,
        )
