import pytest
from diopter.compiler import Language, ObjectCompilationOutput, SourceProgram
from program_markers.instrumenter import InstrumenterMode, instrument_program
from program_markers.iprogram import find_non_eliminated_markers_impl, rename_markers
from program_markers.markers import (
    AsmCommentDetectionStrategy,
    AsmCommentEmptyOperandsDetectionStrategy,
    AsmCommentGlobalOutOperandDetectionStrategy,
    AsmCommentLocalOutOperandDetectionStrategy,
    AsmCommentVolatileGlobalOutOperandDetectionStrategy,
    FunctionCallDetectionStrategy,
    GlobalIntDetectionStrategy,
    GlobalVolatileIntDetectionStrategy,
    LocalVolatileIntDetectionStrategy,
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
    markers = (
        VRMarker.from_str("VRMarker0_", "int"),
        VRMarker.from_str("VRMarker1_", "int"),
    )
    assert (
        set(
            find_non_eliminated_markers_impl(
                asm, markers, FunctionCallDetectionStrategy()
            )
        )
    ) == set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    )

    asm = """
    js	.L2
    call	VRMarker1_@PLT
"""
    assert (
        set(
            find_non_eliminated_markers_impl(
                asm, markers, FunctionCallDetectionStrategy()
            )
        )
    ) == set((VRMarker.from_str("VRMarker1_", "int"),))


def test_instrumentation() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a, int b){
        if (a+b)
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
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram.enabled_markers())

    gcc = get_system_gcc_O0()
    assert set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram.find_eliminated_markers(gcc))


def test_disable_markers() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a, int b){
        if (a+b)
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
        iprogram.disable_markers((VRMarker.from_str("VRMarker2_", "int"),))

    iprogram0 = iprogram.disable_markers((VRMarker.from_str("VRMarker0_", "int"),))
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.disabled_markers()
    )
    assert set((VRMarker.from_str("VRMarker1_", "int"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers(
        (VRMarker.from_str("VRMarker0_", "int"),)
    )

    iprogram1 = iprogram.disable_remaining_markers()
    assert set(
        (VRMarker.from_str("VRMarker0_", "int"), VRMarker.from_str("VRMarker1_", "int"))
    ) == set(iprogram1.disabled_markers())
    assert set() == set(iprogram1.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram1.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram1.find_eliminated_markers(gcc, include_all_markers=False)
    )

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram1_1.disabled_markers())
    assert set() == set(iprogram1_1.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram1_1.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram1_1.find_eliminated_markers(gcc, include_all_markers=False)
    )

    iprogram2 = iprogram.disable_remaining_markers()
    # disabling all remaining markers twice shouldn't have any effect
    assert iprogram2 == iprogram2.disable_remaining_markers()
    assert set(
        (VRMarker.from_str("VRMarker0_", "int"), VRMarker.from_str("VRMarker1_", "int"))
    ) == set(iprogram2.disabled_markers())
    assert set(()) == set(iprogram2.find_non_eliminated_markers(gcc))
    assert set(
        (VRMarker.from_str("VRMarker0_", "int"), VRMarker.from_str("VRMarker1_", "int"))
    ) == set(iprogram2.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram2.find_eliminated_markers(gcc, include_all_markers=False)
    )


def test_unreachable() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a, int b){
        if (a+b){
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
        iprogram.make_markers_unreachable((VRMarker.from_str("VRMarker2_", "int"),))

    iprogram0 = iprogram.make_markers_unreachable(
        (VRMarker.from_str("VRMarker0_", "int"),)
    )
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.unreachable_markers()
    )
    assert set((VRMarker.from_str("VRMarker1_", "int"),)) == set(
        iprogram0.find_non_eliminated_markers(gcc)
    )
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=True)
    )
    assert set() == set(
        iprogram0.find_eliminated_markers(gcc, include_all_markers=False)
    )
    # Making the same marker unreachable twice shouldn't have any effect
    assert iprogram0 == iprogram0.make_markers_unreachable(
        (VRMarker.from_str("VRMarker0_", "int"),)
    )


def test_disable_and_unreachable() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a, int b){
        if (a+b){
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

    iprogram = iprogram.make_markers_unreachable(
        (VRMarker.from_str("VRMarker0_", "int"),)
    )
    with pytest.raises(AssertionError):
        # We can't disable a marker that was already make unreachable
        iprogram.disable_markers((VRMarker.from_str("VRMarker0_", "int"),))

    iprogram = iprogram.disable_markers((VRMarker.from_str("VRMarker1_", "int"),))
    with pytest.raises(AssertionError):
        # We can't make ureachable a marker that was already disabled
        iprogram = iprogram.make_markers_unreachable(
            (VRMarker.from_str("VRMarker1_", "int"),)
        )
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram.unreachable_markers()
    )
    assert set((VRMarker.from_str("VRMarker1_", "int"),)) == set(
        iprogram.disabled_markers()
    )

    assert set(()) == set(iprogram.find_non_eliminated_markers(gcc))
    assert set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram.find_eliminated_markers(gcc, include_all_markers=True))
    assert set() == set(
        iprogram.find_eliminated_markers(gcc, include_all_markers=False)
    )

    # All markers have already been disabled or made unreachable
    assert iprogram == iprogram.disable_remaining_markers()


def test_refinement() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    static int foo(int a, int b){
        if (a+b){
            return 1;
        }
        return 0;
    }
    int main(){
        return foo(1,1) - foo(1,2);
    }
    """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )
    gcc = get_system_gcc_O0()
    rprogram, reachable_markers = iprogram.refine_markers_with_runtime_information(
        (), gcc
    )
    assert len(reachable_markers) == 2, reachable_markers
    refined_markers = list(sorted(reachable_markers, key=lambda m: m.id))
    assert isinstance(refined_markers[0], VRMarker)
    assert refined_markers[0].lower_bound == 1
    assert refined_markers[0].upper_bound == 1
    assert isinstance(refined_markers[1], VRMarker)
    assert refined_markers[1].lower_bound == 1
    assert refined_markers[1].upper_bound == 2
    rprogram = iprogram.replace_markers(tuple(refined_markers))
    gcc = get_system_gcc_O3()
    assert len(iprogram.find_eliminated_markers(gcc)) == 0
    assert len(rprogram.find_eliminated_markers(gcc)) == 2

    reachable_markers2 = rprogram.track_reachable_markers((), gcc)
    # The refined markers should be unreachable since their new bounds are tight
    assert len(reachable_markers2) == 0


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
    marker0 = iprogram.enabled_markers()[0]
    marker0 = VRMarker(marker0.name, marker0.id, "int", -10, 10)
    marker1 = iprogram.enabled_markers()[1]
    iprogram = iprogram.replace_markers((marker0,))

    all_marker_strategies = (
        FunctionCallDetectionStrategy(),
        AsmCommentDetectionStrategy(),
        AsmCommentEmptyOperandsDetectionStrategy(),
        AsmCommentLocalOutOperandDetectionStrategy(),
        AsmCommentGlobalOutOperandDetectionStrategy(),
        AsmCommentVolatileGlobalOutOperandDetectionStrategy(),
        LocalVolatileIntDetectionStrategy(),
        GlobalVolatileIntDetectionStrategy(),
        GlobalIntDetectionStrategy(),
    )
    gcc = get_system_gcc_O3()

    for strategy in all_marker_strategies:
        ip = iprogram.with_marker_strategy(strategy)
        markers = ip.find_non_eliminated_markers(gcc)

        assert marker0 not in markers
        assert marker1 in markers


def test_variable_name_and_type() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
        static int foo(long a, int b){
            return a + b;
        }
        """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )

    markers = [marker for marker in iprogram.markers if isinstance(marker, VRMarker)]
    assert len(markers) == 2
    assert {marker.get_variable_name_and_type(iprogram.code) for marker in markers} == {
        ("a", '"long"'),
        ("b", '"int"'),
    }


def test_number_occurences() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
        static int foo(long a, int b){
            return a + b;
        }
        """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )

    markers = [marker for marker in iprogram.markers if isinstance(marker, VRMarker)]
    assert len(markers) == 2
    for marker in markers:
        assert marker.number_occurences_in_code(iprogram.code) == 1

    code = """
        static int foo(long a, int b){
            VRMARKERMACRO0_(a, "long")
            VRMARKERMACRO0_(a, "long")
            VRMARKERMACRO1_(b, "int")
            return a + b;
        }
        """

    markers = sorted(markers, key=lambda m: m.id)
    assert markers[0].number_occurences_in_code(code) == 2
    assert markers[1].number_occurences_in_code(code) == 1


def test_marker_renaming() -> None:
    iprogram_0 = instrument_program(
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
    iprogram_1 = instrument_program(
        SourceProgram(
            code="""
    int foo(int a, int b){
        if (a+b)
            return 1;
        return 0;
    }
    """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )

    iprogram0, iprogram1 = rename_markers((iprogram_0, iprogram_1))

    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(
        iprogram0.enabled_markers()
    )
    assert set(
        (
            VRMarker.from_str("VRMarker1_", "int"),
            VRMarker.from_str("VRMarker2_", "int"),
        )
    ) == set(iprogram1.enabled_markers())

    assert (
        VRMarker.from_str("VRMarker0_", "int").macro_without_arguments()
        in iprogram0.code
    ), iprogram0.code

    assert (
        VRMarker.from_str("VRMarker1_", "int").macro_without_arguments()
        in iprogram1.code
    ), iprogram1.code
    assert (
        VRMarker.from_str("VRMarker2_", "int").macro_without_arguments()
        in iprogram1.code
    ), iprogram1.code

    gcc = get_system_gcc_O0()
    gcc.compile_program(iprogram1, ObjectCompilationOutput())
