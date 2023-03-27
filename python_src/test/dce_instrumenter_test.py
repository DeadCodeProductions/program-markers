import program_markers.marker_strategies as ms
import pytest
from diopter.compiler import ASMCompilationOutput, Language, SourceProgram
from program_markers.instrumenter import DCEMarker, instrument_program

from .utils import get_system_gcc_O0, get_system_gcc_O3


def test_instrumentation() -> None:
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
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram.dce_markers
    )

    gcc = get_system_gcc_O0()
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram.find_non_eliminated_markers(gcc)
    )
    assert set() == set(iprogram.find_eliminated_markers(gcc))

    marker_status = iprogram.find_eliminated_and_non_eliminated_markers(gcc)
    assert set() == set(marker_status.eliminated_markers)
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        marker_status.non_eliminated_markers
    )


def test_disable_markers() -> None:
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

    gcc = get_system_gcc_O0()

    with pytest.raises(AssertionError):
        iprogram.disable_markers((DCEMarker("DCEMarker2_"),))

    iprogram0 = iprogram.disable_markers((DCEMarker("DCEMarker1_"),))
    assert set((DCEMarker("DCEMarker1_"),)) == set(iprogram0.disabled_markers)
    assert set((DCEMarker("DCEMarker0_"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((DCEMarker("DCEMarker1_"),)) == set(
        iprogram0.find_eliminated_markers(gcc)
    )
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers((DCEMarker("DCEMarker1_"),))

    iprogram1 = iprogram.disable_markers((DCEMarker("DCEMarker0_"),))
    assert set((DCEMarker("DCEMarker0_"),)) == set(iprogram1.disabled_markers)
    assert set((DCEMarker("DCEMarker1_"),)) == set(
        iprogram1.find_non_eliminated_markers(gcc)
    )
    assert set((DCEMarker("DCEMarker0_"),)) == set(
        iprogram1.find_eliminated_markers(gcc)
    )

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram1_1.disabled_markers)
    assert set() == set(iprogram1_1.find_non_eliminated_markers(gcc))
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram1_1.find_eliminated_markers(gcc))

    iprogram2 = iprogram.disable_remaining_markers()
    # disabling all remaining markers twice shouldn't have any effect
    assert iprogram2 == iprogram2.disable_remaining_markers()
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram2.disabled_markers)
    assert set(()) == set(iprogram2.find_non_eliminated_markers(gcc))
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram2.find_eliminated_markers(gcc)
    )


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
    gcc = get_system_gcc_O0()

    with pytest.raises(AssertionError):
        iprogram.make_markers_unreachable((DCEMarker("DCEMarker2_"),))

    iprogram = iprogram.make_markers_unreachable((DCEMarker("DCEMarker1_"),))
    # Making the same marker unreachable twice shouldn't have any effect
    iprogram == iprogram.make_markers_unreachable((DCEMarker("DCEMarker1_"),))
    assert set((DCEMarker("DCEMarker1_"),)) == set(iprogram.unreachable_markers)

    asm = gcc.compile_program(iprogram, ASMCompilationOutput()).output.read()
    assert "bar" not in asm


def test_disable_and_unreachable() -> None:
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
    gcc = get_system_gcc_O0()

    iprogram = iprogram.make_markers_unreachable((DCEMarker("DCEMarker1_"),))
    with pytest.raises(AssertionError):
        # We can't disable a marker that was already make unreachable
        iprogram.disable_markers((DCEMarker("DCEMarker1_"),))
    iprogram = iprogram.disable_markers((DCEMarker("DCEMarker0_"),))
    with pytest.raises(AssertionError):
        # We can't make ureachable a marker that was already disabled
        iprogram.make_markers_unreachable((DCEMarker("DCEMarker0_"),))
    assert set((DCEMarker("DCEMarker1_"),)) == set(iprogram.unreachable_markers)
    assert set((DCEMarker("DCEMarker0_"),)) == set(iprogram.disabled_markers)

    assert set(()) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram.find_eliminated_markers(gcc)
    )

    asm = gcc.compile_program(iprogram, ASMCompilationOutput()).output.read()
    assert "bar" not in asm

    # All markers have already been disabled or made unreachable
    assert iprogram == iprogram.disable_remaining_markers()


def test_strategies() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
        int g;
        int main()
        {
            g = 5;
            if (g == 5) {
                if (g != 5) {
                }
            }
        }
        """,
            language=Language.C,
        ),
    )

    all_marker_strategies = (
        ms.FunctionCallStrategy(),
        ms.AsmCommentStrategy(),
        ms.AsmCommentEmptyOperandsStrategy(),
        ms.AsmCommentLocalOutOperandStrategy(),
        ms.AsmCommentGlobalOutOperandStrategy(),
        ms.AsmCommentVolatileGlobalOutOperandStrategy(),
        ms.LocalVolatileIntStrategy(),
        ms.GlobalVolatileIntStrategy(),
        ms.GlobalIntStrategy(),
    )
    gcc = get_system_gcc_O3()

    for strategy in all_marker_strategies:
        ip = iprogram.with_marker_directives(strategy)

        assert (
            ip.marker_preprocessor_directives.keys()
            == iprogram.marker_preprocessor_directives.keys()
        )

        markers = ip.find_non_eliminated_markers(gcc)

        assert (DCEMarker(f"{DCEMarker.prefix}1_")) in markers
        assert (DCEMarker(f"{DCEMarker.prefix}2_")) in markers
