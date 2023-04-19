import pytest
from diopter.compiler import Language, SourceProgram
from program_markers.instrumenter import (
    InstrumenterMode,
    find_non_eliminated_markers_impl,
    instrument_program,
)
from program_markers.markers import (
    AsmCommentEmptyOperandsStrategy,
    AsmCommentGlobalOutOperandStrategy,
    AsmCommentLocalOutOperandStrategy,
    AsmCommentStrategy,
    AsmCommentVolatileGlobalOutOperandStrategy,
    FunctionCallStrategy,
    GlobalIntStrategy,
    GlobalVolatileIntStrategy,
    LocalVolatileIntStrategy,
    VRMarker,
)

from .utils import get_system_gcc_O0, get_system_gcc_O3


def test_asm_parsing() -> None:
    asm = """
    call VRMarker0_@PLT
.L2:
    cmpl	$0, -4(%rbp)
    js	.L3
    call	VRMarker1_@PLT
    """
    markers = (VRMarker.from_str("VRMarkerLE0_"), VRMarker.from_str("VRMarkerGE1_"))
    assert (
        set(find_non_eliminated_markers_impl(asm, markers, FunctionCallStrategy()))
    ) == set(
        (
            VRMarker.from_str("VRMarkerGE1_"),
            VRMarker.from_str("VRMarkerLE0_"),
        )
    )

    asm = """
    js	.L2
    call	VRMarker1_@PLT
"""
    assert (
        set(find_non_eliminated_markers_impl(asm, markers, FunctionCallStrategy()))
    ) == set((VRMarker.from_str("VRMarkerGE1_"),))


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
        mode=InstrumenterMode.VR,
    )
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram.enabled_markers)

    gcc = get_system_gcc_O0()
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram.find_eliminated_markers(gcc))


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
        mode=InstrumenterMode.VR,
    )

    gcc = get_system_gcc_O0()

    with pytest.raises(AssertionError):
        iprogram.disable_markers((VRMarker.from_str("VRMarkerLE1_"),))

    iprogram0 = iprogram.disable_markers((VRMarker.from_str("VRMarkerLE0_"),))
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(iprogram0.disabled_markers)
    assert set((VRMarker.from_str("VRMarkerGE1_"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers((VRMarker.from_str("VRMarkerLE0_"),))

    iprogram1 = iprogram.disable_markers((VRMarker.from_str("VRMarkerGE1_"),))
    assert set((VRMarker.from_str("VRMarkerGE1_"),)) == set(iprogram1.disabled_markers)
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram1.find_non_eliminated_markers(gcc)
    )
    assert set((VRMarker.from_str("VRMarkerGE1_"),)) == set(
        iprogram1.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram1.find_eliminated_markers(gcc, include_all_markers=False)
    )

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram1_1.disabled_markers)
    assert set() == set(iprogram1_1.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
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
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram2.disabled_markers)
    assert set(()) == set(iprogram2.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram2.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram2.find_eliminated_markers(gcc, include_all_markers=False)
    )


def test_unreachable() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a){
        if (a){
            return 1;
        }
        return 0;
    }
    """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )
    gcc = get_system_gcc_O0()

    with pytest.raises(AssertionError):
        iprogram.make_markers_unreachable((VRMarker.from_str("VRMarkerLE1_"),))

    iprogram0 = iprogram.make_markers_unreachable((VRMarker.from_str("VRMarkerLE0_"),))
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram0.unreachable_markers
    )
    assert set((VRMarker.from_str("VRMarkerGE1_"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    # Making the same marker unreachable twice shouldn't have any effect
    assert iprogram0 == iprogram0.make_markers_unreachable(
        (VRMarker.from_str("VRMarkerLE0_"),)
    )


def test_disable_and_unreachable() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a){
        if (a){
            return 1;
        }
        return 0;
    }
    """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )
    gcc = get_system_gcc_O0()

    iprogram = iprogram.make_markers_unreachable((VRMarker.from_str("VRMarkerLE0_"),))
    with pytest.raises(AssertionError):
        # We can't disable a marker that was already make unreachable
        iprogram.disable_markers((VRMarker.from_str("VRMarkerLE0_"),))

    iprogram = iprogram.disable_markers((VRMarker.from_str("VRMarkerGE1_"),))
    with pytest.raises(AssertionError):
        # We can't make ureachable a marker that was already disabled
        iprogram = iprogram.make_markers_unreachable(
            (VRMarker.from_str("VRMarkerGE1_"),)
        )
    assert set((VRMarker.from_str("VRMarkerLE0_"),)) == set(
        iprogram.unreachable_markers
    )
    assert set((VRMarker.from_str("VRMarkerGE1_"),)) == set(iprogram.disabled_markers)

    assert set(()) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarkerLE0_"),
            VRMarker.from_str("VRMarkerGE1_"),
        )
    ) == set(iprogram.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram.find_eliminated_markers(gcc, include_all_markers=False)
    )

    # All markers have already been disabled or made unreachable
    assert iprogram == iprogram.disable_remaining_markers()


def test_strategies() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
        static int foo(int a, int b){
            return a + b;
        }
        int main()
        {
            return foo(-1,1);
        }
        """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )

    all_marker_strategies = (
        FunctionCallStrategy(),
        AsmCommentStrategy(),
        AsmCommentEmptyOperandsStrategy(),
        AsmCommentLocalOutOperandStrategy(),
        AsmCommentGlobalOutOperandStrategy(),
        AsmCommentVolatileGlobalOutOperandStrategy(),
        LocalVolatileIntStrategy(),
        GlobalVolatileIntStrategy(),
        GlobalIntStrategy(),
    )
    gcc = get_system_gcc_O3()

    for strategy in all_marker_strategies:
        ip = iprogram.with_marker_strategy(strategy)
        markers = ip.find_non_eliminated_markers(gcc)

        assert VRMarker.from_str("VRMarkerGE3_") in markers
        assert VRMarker.from_str("VRMarkerLE0_") in markers
