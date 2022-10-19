from __future__ import annotations

import shlex
import re
import subprocess
from sys import stderr
from itertools import chain
from pathlib import Path
from typing import Optional
from dataclasses import dataclass

from diopter.compiler import (
    CompileError,
    ClangTool,
    ClangToolMode,
    SourceProgram,
    CompilerExe,
    CompilationSetting,
)


@dataclass(frozen=True, kw_only=True)
class InstrumentedProgram(SourceProgram):
    markers: tuple[str, ...] = tuple()
    marker_prefix: str = "DCEMarker"

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
        return tuple(alive_markers)

    def find_dead_markers(
        self, compilation_setting: CompilationSetting
    ) -> tuple[str, ...]:
        return tuple(
            set(self.markers) - set(self.find_alive_markers(compilation_setting))
        )

    def disable_markers(self, markers: tuple[str, ...]) -> InstrumentedProgram:
        new_macro_defs: list[str] = []
        for marker in markers:
            assert marker in self.markers, f"{marker} not in {self.markers}"
            macro = "Delete" + marker
            assert macro in self.available_macros
            new_macro_defs.append(macro)

        return InstrumentedProgram(
            code=self.code,
            language=self.language,
            available_macros=self.available_macros,
            defined_macros=tuple(set(self.defined_macros) | set(new_macro_defs)),
            include_paths=self.include_paths,
            system_include_paths=self.system_include_paths,
            flags=self.flags,
            markers=self.markers,
        )

    def disable_markers_and_blocks(
        self, markers: tuple[str, ...]
    ) -> InstrumentedProgram:
        new_macro_defs: list[str] = []
        for marker in markers:
            assert marker in self.markers
            # XXX: an instrumented program does not necesarily have DeleteBLock macros
            # the code should check this (it's harmless but why define additional macros)
            macro1 = "DeleteBlock" + marker
            macro2 = "Delete" + marker
            assert macro1 in self.available_macros
            assert macro2 in self.available_macros
            new_macro_defs.extend((macro1, macro2))

        return InstrumentedProgram(
            code=self.code,
            language=self.language,
            available_macros=self.available_macros,
            defined_macros=tuple(set(self.defined_macros) | set(new_macro_defs)),
            include_paths=self.include_paths,
            system_include_paths=self.system_include_paths,
            flags=self.flags,
            markers=self.markers,
        )

    def disable_all_markers(self) -> InstrumentedProgram:
        return InstrumentedProgram(
            code=self.code,
            language=self.language,
            available_macros=self.available_macros,
            defined_macros=tuple(
                set(self.defined_macros)
                | set("Delete" + marker for marker in self.markers)
            ),
            include_paths=self.include_paths,
            system_include_paths=self.system_include_paths,
            flags=self.flags,
            markers=self.markers,
        )


def find_all_markers(
    code: str,
    marker_prefix: str,
) -> tuple[str, ...]:
    marker_regex = re.compile(rf".*void.*{marker_prefix}([0-9]+)_\(void\);.*")
    all_markers = set()
    for line in code.split("\n"):
        m = marker_regex.match(line.strip())
        if m:
            all_markers.add(f"{marker_prefix}{m.group(1)}_")
    return tuple(all_markers)


def get_instrumenter(
    instrumenter: Optional[ClangTool] = None, clang: Optional[CompilerExe] = None
) -> ClangTool:
    if not instrumenter:
        if not clang:
            clang = CompilerExe.get_system_clang()
        instrumenter = ClangTool.init_with_paths_from_llvm(
            Path(__file__).parent / "dead-instrument", clang
        )
    return instrumenter


def instrument_program(
    program: SourceProgram,
    emit_disable_macros: bool = False,
    instrumenter: Optional[ClangTool] = None,
    clang: Optional[CompilerExe] = None,
) -> InstrumentedProgram:
    """Instrument a given file i.e. put markers in the file.

    Args:
        program (Source): The program to be instrumented.
        emit_disable_macros (bool): Whether to include disabling macros in the instrumented program (only for InstrumenterMode.DCEMarkers)
        instrumenter (ClangTool): The instrumenter
        clang (CompilerExe): Which clang to use for searching the standard include paths
    Returns:
        str: Marker prefix. Here: 'DCEMarker'
    """

    instrumenter_resolved = get_instrumenter(instrumenter, clang)

    flags = []
    prefix = "DCEMarker"
    flags.append("--mode=instrument")
    if emit_disable_macros:
        flags.append("--emit-disable-macros")

    try:
        instrumented_code = instrumenter_resolved.run_on_program(
            program, flags, ClangToolMode.READ_MODIFIED_FILE
        )
    except CompileError as e:
        print(e, file=stderr)
        exit(1)

    markers = find_all_markers(instrumented_code, prefix)
    macros = ["DeleteBlock" + marker for marker in markers] + [
        "Delete" + marker for marker in markers
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

    modified_source = instrumenter_resolved.run_on_program(
        program, flags, ClangToolMode.READ_MODIFIED_FILE
    )

    return SourceProgram(
        code=modified_source,
        language=program.language,
        available_macros=program.available_macros,
        defined_macros=program.defined_macros,
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
    )
