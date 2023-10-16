import pytest
from diopter.compiler import (
    ASMCompilationOutput,
    Language,
    ObjectCompilationOutput,
    SourceProgram,
)
from program_markers.instrumenter import instrument_program
from program_markers.iprogram import find_non_eliminated_markers_impl, rename_markers
from program_markers.markers import (
    AsmCommentDetectionStrategy,
    AsmCommentEmptyOperandsDetectionStrategy,
    AsmCommentGlobalOutOperandDetectionStrategy,
    AsmCommentLocalOutOperandDetectionStrategy,
    AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy,
    AsmCommentVolatileGlobalOutOperandDetectionStrategy,
    DCEMarker,
    FunctionCallDetectionStrategy,
    GlobalIntDetectionStrategy,
    GlobalVolatileIntDetectionStrategy,
    LocalVolatileIntDetectionStrategy,
    StaticVolatileGlobalIntDetectionStrategy,
)

from .utils import get_system_gcc_O0, get_system_gcc_O3


def test_parsing_with_tailcalls() -> None:
    asm = """
    f:                                      # @f
        xor     edi, edx
        je      DCEMarker0_                     # TAILCALL
        ret
    """
    markers = (DCEMarker.from_str("DCEMarker0_"),)
    assert (
        set(
            find_non_eliminated_markers_impl(
                asm, markers, FunctionCallDetectionStrategy()
            )
        )
    ) == set((DCEMarker.from_str("DCEMarker0_"),))

    asm = """
    f:
        cmp     dl, dil
        je      .L4
        ret
.L4:
        xor     eax, eax
        jmp     DCEMarker0_
    """
    assert (
        set(
            find_non_eliminated_markers_impl(
                asm, markers, FunctionCallDetectionStrategy()
            )
        )
    ) == set((DCEMarker.from_str("DCEMarker0_"),))


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
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram.enabled_markers())

    gcc = get_system_gcc_O0()
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram.find_eliminated_markers(gcc))

    eliminated_markers = iprogram.find_eliminated_markers(gcc)
    non_eliminated_markers = iprogram.find_non_eliminated_markers(gcc)
    assert set() == set(eliminated_markers)
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(non_eliminated_markers)


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
        iprogram.disable_markers((DCEMarker.from_str("DCEMarker2_"),))

    iprogram0 = iprogram.disable_markers((DCEMarker.from_str("DCEMarker1_"),))
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(
        iprogram0.disabled_markers()
    )
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(iprogram0.enabled_markers())
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers((DCEMarker.from_str("DCEMarker1_"),))

    iprogram1 = iprogram.disable_markers((DCEMarker.from_str("DCEMarker0_"),))
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(
        iprogram1.disabled_markers()
    )
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(
        iprogram1.find_non_eliminated_markers(gcc)
    )
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(
        iprogram1.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram1.find_eliminated_markers(gcc, include_all_markers=False)
    )

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            DCEMarker.from_str("DCEMarker0_"),
            DCEMarker.from_str("DCEMarker1_"),
        )
    ) == set(iprogram1_1.disabled_markers())
    assert set() == set(iprogram1_1.find_non_eliminated_markers(gcc))
    assert set(
        (
            DCEMarker.from_str("DCEMarker0_"),
            DCEMarker.from_str("DCEMarker1_"),
        )
    ) == set(iprogram1_1.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram1_1.find_eliminated_markers(gcc, include_all_markers=False)
    )

    iprogram2 = iprogram.disable_remaining_markers()
    # disabling all remaining markers twice shouldn't have any effect
    assert iprogram2 == iprogram2.disable_remaining_markers()
    assert set(
        (
            DCEMarker.from_str("DCEMarker0_"),
            DCEMarker.from_str("DCEMarker1_"),
        )
    ) == set(iprogram2.disabled_markers())
    assert set(()) == set(iprogram2.find_non_eliminated_markers(gcc))
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram2.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram2.find_eliminated_markers(gcc, include_all_markers=False)
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
        iprogram.make_markers_unreachable((DCEMarker.from_str("DCEMarker2_"),))

    iprogram = iprogram.make_markers_unreachable((DCEMarker.from_str("DCEMarker1_"),))
    # Making the same marker unreachable twice shouldn't have any effect
    iprogram == iprogram.make_markers_unreachable((DCEMarker.from_str("DCEMarker1_"),))
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(
        iprogram.unreachable_markers()
    )
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(iprogram.enabled_markers())

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

    iprogram = iprogram.make_markers_unreachable((DCEMarker.from_str("DCEMarker1_"),))
    with pytest.raises(AssertionError):
        # We can't disable a marker that was already make unreachable
        iprogram.disable_markers((DCEMarker.from_str("DCEMarker1_"),))
    iprogram = iprogram.disable_markers((DCEMarker.from_str("DCEMarker0_"),))
    with pytest.raises(AssertionError):
        # We can't make ureachable a marker that was already disabled
        iprogram.make_markers_unreachable((DCEMarker.from_str("DCEMarker0_"),))
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(
        iprogram.unreachable_markers()
    )
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(iprogram.disabled_markers())

    assert set(()) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram.find_eliminated_markers(gcc, include_all_markers=False)
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
        FunctionCallDetectionStrategy(),
        AsmCommentDetectionStrategy(),
        AsmCommentEmptyOperandsDetectionStrategy(),
        AsmCommentLocalOutOperandDetectionStrategy(),
        AsmCommentGlobalOutOperandDetectionStrategy(),
        AsmCommentVolatileGlobalOutOperandDetectionStrategy(),
        AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy(),
        LocalVolatileIntDetectionStrategy(),
        GlobalVolatileIntDetectionStrategy(),
        GlobalIntDetectionStrategy(),
        StaticVolatileGlobalIntDetectionStrategy(),
    )
    gcc = get_system_gcc_O3()

    for strategy in all_marker_strategies:
        ip = iprogram.with_marker_strategy(strategy)

        # assert (
        # ip.marker_preprocessor_directives.keys()
        # == iprogram.marker_preprocessor_directives.keys()
        # )

        markers = ip.find_non_eliminated_markers(gcc)

        assert (DCEMarker.from_str(f"{DCEMarker.prefix()}1_")) in markers
        assert (DCEMarker.from_str(f"{DCEMarker.prefix()}2_")) in markers


def test_marker_renaming() -> None:
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
    iprogram0, iprogram1 = rename_markers((iprogram, iprogram))

    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram0.enabled_markers())
    assert set(
        (DCEMarker.from_str("DCEMarker2_"), DCEMarker.from_str("DCEMarker3_"))
    ) == set(iprogram1.enabled_markers())

    assert DCEMarker.from_str("DCEMarker2_").macro() in iprogram1.code
    assert DCEMarker.from_str("DCEMarker3_").macro() in iprogram1.code

    gcc = get_system_gcc_O0()
    gcc.compile_program(iprogram1, ObjectCompilationOutput())
