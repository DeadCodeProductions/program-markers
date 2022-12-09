from pathlib import Path

import pytest
from diopter.compiler import (
    CompilationSetting,
    CompilerExe,
    CompilerProject,
    Language,
    OptLevel,
    SourceProgram,
)

from dead_instrumenter.instrumenter import instrument_program


def get_system_gcc_O0() -> CompilationSetting:
    exe = CompilerExe(CompilerProject.GCC, Path("gcc"), "system")  # parse the version?
    return CompilationSetting(
        compiler=exe,
        opt_level=OptLevel.O0,
        flags=(),
        include_paths=(),
        system_include_paths=(),
    )


def test_instr() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a){
        if (a)
            return 1;
        return 0;
    }
    """,
            language=Language.C,
        ),
    )
    assert set(("DCEMarker0_", "DCEMarker1_")) == set(iprogram.markers)
    assert set(
        (
            "DisableDCEMarker0_",
            "DisableDCEMarker1_",
            "UnreachableDCEMarker0_",
            "UnreachableDCEMarker1_",
        )
    ) == set(iprogram.available_macros)

    gcc = get_system_gcc_O0()
    assert set(("DCEMarker0_", "DCEMarker1_")) == set(iprogram.find_alive_markers(gcc))
    assert set() == set(iprogram.find_dead_markers(gcc))
    with pytest.raises(AssertionError):
        iprogram.disable_markers(("DCEMarker2_",))
    iprogram0 = iprogram.disable_markers(("DCEMarker1_",))
    assert set(("DCEMarker0_",)) == set(iprogram0.find_alive_markers(gcc))
    assert set(("DCEMarker1_",)) == set(iprogram0.find_dead_markers(gcc))

    iprogram1 = iprogram.make_markers_unreachable(("DCEMarker0_",))
    assert set(("DCEMarker1_",)) == set(iprogram1.find_alive_markers(gcc))
    assert set(("DCEMarker0_",)) == set(iprogram1.find_dead_markers(gcc))

    iprogram2 = iprogram.disable_all_markers()
    assert set(()) == set(iprogram2.find_alive_markers(gcc))
    assert set(("DCEMarker0_", "DCEMarker1_")) == set(iprogram2.find_dead_markers(gcc))


def test_unreachable() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int bar();
    int foo(int a){
        if (a){
            bar();
            return 1;
        }
        return 0;
    }
    """,
            language=Language.C,
        ),
    )
    iprogram = iprogram.make_markers_unreachable(("DCEMarker1_",))
    gcc = get_system_gcc_O0()
    asm = gcc.get_asm_from_program(iprogram)
    assert "bar" not in asm


if __name__ == "__main__":
    test_instr()
