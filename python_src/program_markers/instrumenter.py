from __future__ import annotations

from dataclasses import replace
from enum import Enum
from functools import cache
from itertools import dropwhile, takewhile
from pathlib import Path

from diopter.compiler import ClangTool, ClangToolMode, CompilerExe, SourceProgram
from program_markers.iprogram import InstrumentedProgram
from program_markers.markers import (
    DCEMarker,
    EnableEmitter,
    FunctionCallDetectionStrategy,
    Marker,
    VRMarker,
)

# TODO: The various hardcoded strings, e.g., "//MARKER_DIRECTIVES\n"
# should not be manually copied, instead either have some common file
# where they are defined or read them from some kind of info output,
# e.g., instrumenter --info


def __get_vr_macro_type_map(instrumented_code: str) -> dict[str, str]:
    """Finds the variable type for each VRMarker instance
    in the instrumented code.
    Args:
        instrumented_code (str):
            the output of the instrumenter (the variable types
            of VRMarkers are parsed from the instrumented_code)
    Returns:
        dict[str,str]:
            the variable type for each instance of VRMarkerX_
            in the instrumented code
    """

    def convert_variable_type(variable_type: str) -> str:
        type_map = {
            "_Bool": "bool",
            "signed char": "char",
            "uint8_t": "unsigned int",
            "int8_t": "int",
            "uint16_t": "unsigned short",
            "int16_t": "short",
            "uint32_t": "unsigned int",
            "int32_t": "int",
            "uint64_t": "unsigned long",
            "int64_t": "long",
        }
        if variable_type in type_map:
            variable_type = type_map[variable_type]
        assert variable_type in [
            "bool",
            "char",
            "short",
            "int",
            "long",
            "long long",
            "unsigned char",
            "unsigned short",
            "unsigned int",
            "unsigned long",
            "unsigned long long",
        ], f"Unexpected variable type for VRMarker: {variable_type} in {line}"
        return variable_type

    macroprefix = VRMarker.macroprefix()
    vr_macro_type_map = {}
    for line in instrumented_code.splitlines():
        if macroprefix not in line:
            continue
        line = line[line.index(macroprefix) :]
        line = line[: line.find(")")]
        macro = line.split("(")[0]
        variable_type = convert_variable_type(
            (line.split(",")[1].replace('"', "")).strip()
        )
        vr_macro_type_map[macro.replace(macroprefix, VRMarker.prefix())] = variable_type
    return vr_macro_type_map


def __str_to_marker(marker_macro: str, vr_macro_type_map: dict[str, str]) -> Marker:
    """Converts the `marker_macro` string into a `Marker.
    Args:
        marker_macro (str):
            a marker in the form PrefixMarkerX_, e.g., DCEMarker32_.
        vr_macro_type_map (dict[str,str]):
            a mapping from VRMarker macros to their variable types
            e.g., {"VRMarker0_": "int", "VRMarker1_": "short"}
    Returns:
        Marker:
            a `Marker` object
    """
    if marker_macro.startswith(DCEMarker.prefix()):
        return DCEMarker.from_str(marker_macro)
    else:
        assert marker_macro.startswith(VRMarker.prefix()), marker_macro
        return VRMarker.from_str(marker_macro, vr_macro_type_map[marker_macro])


class NoInstrumentationAddedError(Exception):
    pass


@cache
def get_instrumenter(
    instrumenter: ClangTool | None = None, clang: CompilerExe | None = None
) -> ClangTool:
    if not instrumenter:
        if not clang:
            # TODO: move this to diopter
            try:
                clang = CompilerExe.get_system_clang()
            except:  # noqa: E722
                pass
            if not clang:
                for clang_path in ("clang-17", "clang-16", "clang-15", "clang-14"):
                    try:
                        clang = CompilerExe.from_path(Path(clang_path))
                        break
                    except:  # noqa: E722
                        pass
        assert clang, "Could not find clang"

        instrumenter = ClangTool.init_with_paths_from_clang(
            Path(__file__).parent / "program-markers", clang
        )
    return instrumenter


def parse_marker_names(instrumenter_output: str) -> list[str]:
    markers_start = "//MARKERS START\n"
    markers_end = "//MARKERS END\n"
    lines: list[str] = list(
        dropwhile(
            lambda x: x.startswith(markers_start),
            instrumenter_output.strip().splitlines(),
        )
    )
    if not lines:
        raise NoInstrumentationAddedError
    return list(takewhile(lambda x: not x.startswith(markers_end), lines[1:]))[:-1]


class InstrumenterMode(Enum):
    DCE = 0
    VR = 1
    DCE_AND_VR = 2


def add_temporary_disable_directives(source: str, markers: list[Marker]) -> str:
    temp_directives = ["//TEMP_DIRECTIVES_START\n"]
    for marker in markers:
        assert isinstance(marker, DCEMarker)
        temp_directives.append(f"#define {marker.macro()}")
    temp_directives.append("//TEMP_DIRECTIVES_END\n")
    return "\n".join(temp_directives) + "\n" + source


def remove_temporary_disable_directives(source: str) -> str:
    lines = source.strip().splitlines()
    pre_lines = list(
        takewhile(lambda x: not x.startswith("//TEMP_DIRECTIVES_START"), lines)
    )
    post_lines = list(
        dropwhile(
            lambda x: not x.startswith("//TEMP_DIRECTIVES_END"), lines[len(pre_lines) :]
        )
    )
    return "\n".join(pre_lines) + "\n" + "\n".join(post_lines)


def instrument_program(
    program: SourceProgram,
    ignore_functions_with_macros: bool = False,
    mode: InstrumenterMode = InstrumenterMode.DCE,
    instrumenter: ClangTool | None = None,
    clang: CompilerExe | None = None,
    timeout: int | None = None,
) -> InstrumentedProgram:
    """Instrument a given program i.e. put markers in the source code.

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

    flags = ["--no-preprocessor-directives"]
    if ignore_functions_with_macros:
        flags.append("--ignore-functions-with-macros=1")
    else:
        flags.append("--ignore-functions-with-macros=0")

    def get_code_and_markers(mode: str) -> tuple[str, list[Marker]]:
        result = instrumenter_resolved.run_on_program(
            program,
            flags + [f"--mode={mode}"],
            ClangToolMode.CAPTURE_OUT_ERR_AND_READ_MODIFIED_FILED,
            timeout=timeout,
        )
        assert result.modified_source_code
        if not result.stdout:
            raise NoInstrumentationAddedError
        instrumented_code = result.modified_source_code
        marker_names = parse_marker_names(result.stdout)
        if mode == "vr":
            vr_macro_type_map = __get_vr_macro_type_map(instrumented_code)
        else:
            vr_macro_type_map = {}

        return instrumented_code, [
            __str_to_marker(marker_name, vr_macro_type_map)
            for marker_name in marker_names
        ]

    match mode:
        case InstrumenterMode.DCE:
            instrumented_code, markers = get_code_and_markers("dce")
        case InstrumenterMode.VR:
            instrumented_code, markers = get_code_and_markers("vr")
        case InstrumenterMode.DCE_AND_VR:
            result = instrumenter_resolved.run_on_program(
                program,
                flags + ["--mode=dce"],
                ClangToolMode.CAPTURE_OUT_ERR_AND_READ_MODIFIED_FILED,
                timeout=timeout,
            )
            assert result.modified_source_code
            if not result.stdout:
                raise NoInstrumentationAddedError
            dce_markers = [
                __str_to_marker(name, {}) for name in parse_marker_names(result.stdout)
            ]
            program_dce = replace(
                program,
                code=add_temporary_disable_directives(
                    result.modified_source_code, dce_markers
                ),
            )
            result = instrumenter_resolved.run_on_program(
                program_dce,
                flags + ["--mode=vr"],
                ClangToolMode.CAPTURE_OUT_ERR_AND_READ_MODIFIED_FILED,
                timeout=timeout,
            )
            assert result.modified_source_code
            if not result.stdout:
                # XXX: I don't really nead to raise here,
                # I can just ignore VRMarkers
                raise NoInstrumentationAddedError
            instrumented_code = remove_temporary_disable_directives(
                result.modified_source_code
            )
            vr_macro_type_map = __get_vr_macro_type_map(instrumented_code)
            vr_markers = list(
                __str_to_marker(name, vr_macro_type_map)
                for name in parse_marker_names(result.stdout)
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

    ee = EnableEmitter(FunctionCallDetectionStrategy())
    return InstrumentedProgram(
        code=instrumented_code,
        marker_strategy=FunctionCallDetectionStrategy(),
        language=program.language,
        defined_macros=tuple(),
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        markers=tuple(markers),
        directive_emitters={m: ee for m in markers},
    )


# Instrument_file calls instrument_program and writes the result to a file
# (filename argument) plus an include file for the directives. It can use
# the temp InstrumentedProgram to serialize everything and read it back in.
# I can also add a debug mode that checks if the parsed is the same as the original.
