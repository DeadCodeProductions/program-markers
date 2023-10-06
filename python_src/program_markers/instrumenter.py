from __future__ import annotations

from dataclasses import replace
from enum import Enum
from functools import cache
from pathlib import Path

from diopter.compiler import ClangTool, ClangToolMode, CompilerExe, SourceProgram
from program_markers.iprogram import InstrumentedProgram
from program_markers.markers import (
    DCEMarker,
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
        assert marker_macro.startswith(VRMarker.prefix())
        return VRMarker.from_str(marker_macro, vr_macro_type_map[marker_macro])


def __split_marker_directives(
    directives: str, vr_macro_type_map: dict[str, str]
) -> dict[Marker, str]:
    """Maps each set of preprocessor directive in `directives` to the
    appropriate markers.

    Args:
        directives (str):
            the marker preprocessor directives added by the instrumenter
        vr_macro_type_map (dict[str,str]):
            a mapping from VRMarker macros to their variable types
            e.g., {"VRMarker0_": "int", "VRMarker1_": "short"}
    Returns:
        dict[Marker, str]:
            a mapping from each marker to its preprocessor directives
    """
    directives_map = {}
    for directive in directives.split("//MARKER_DIRECTIVES:")[1:]:
        marker_macro = directive[: directive.find("\n")]
        directives_map[__str_to_marker(marker_macro, vr_macro_type_map)] = directive[
            len(marker_macro) :
        ]
    return directives_map


class NoInstrumentationAddedError(Exception):
    pass


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
    if not instrumented_code.startswith(markers_start):
        raise NoInstrumentationAddedError
    markers_end_idx = instrumented_code.find(markers_end)
    assert markers_end_idx != -1, instrumented_code
    code_idx = markers_end_idx + len(markers_end)
    directives = instrumented_code[len(markers_start) : markers_end_idx]
    code = instrumented_code[code_idx:]
    return directives, code


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
                for clang_path in ("clang-16", "clang-15", "clang-14"):
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


class InstrumenterMode(Enum):
    DCE = 0
    VR = 1
    DCE_AND_VR = 2


def instrument_program(
    program: SourceProgram,
    ignore_functions_with_macros: bool = False,
    mode: InstrumenterMode = InstrumenterMode.DCE,
    instrumenter: ClangTool | None = None,
    clang: CompilerExe | None = None,
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
        if mode == "vr":
            vr_macro_type_map = __get_vr_macro_type_map(instrumented_code)
        else:
            vr_macro_type_map = {}

        directives_map = __split_marker_directives(directives, vr_macro_type_map)
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
                __split_marker_directives(
                    vr_directives, __get_vr_macro_type_map(instrumented_code)
                ).keys()
            )
            dce_markers = list(__split_marker_directives(dce_directives, {}).keys())
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
        marker_strategy=FunctionCallDetectionStrategy(),
        language=program.language,
        defined_macros=tuple(),
        include_paths=program.include_paths,
        system_include_paths=program.system_include_paths,
        flags=program.flags,
        enabled_markers=tuple(markers),
    )
