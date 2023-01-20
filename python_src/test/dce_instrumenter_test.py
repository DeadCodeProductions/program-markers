import pytest
from diopter.compiler import Language, SourceProgram

from dead_instrumenter.instrumenter import DCEMarker, instrument_program

from .utils import get_system_gcc_O0


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
        iprogram.find_alive_markers(gcc)
    )
    assert set() == set(iprogram.find_dead_markers(gcc))

    marker_status = iprogram.find_dead_and_alive_markers(gcc)
    assert set() == set(marker_status.dead_markers)
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        marker_status.alive_markers
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
    assert set((DCEMarker("DCEMarker0_"),)) == set(iprogram0.find_alive_markers(gcc))
    assert set((DCEMarker("DCEMarker1_"),)) == set(iprogram0.find_dead_markers(gcc))
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers((DCEMarker("DCEMarker1_"),))

    iprogram1 = iprogram.disable_markers((DCEMarker("DCEMarker0_"),))
    assert set((DCEMarker("DCEMarker0_"),)) == set(iprogram1.disabled_markers)
    assert set((DCEMarker("DCEMarker1_"),)) == set(iprogram1.find_alive_markers(gcc))
    assert set((DCEMarker("DCEMarker0_"),)) == set(iprogram1.find_dead_markers(gcc))

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram1_1.disabled_markers)
    assert set() == set(iprogram1_1.find_alive_markers(gcc))
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram1_1.find_dead_markers(gcc))

    iprogram2 = iprogram.disable_remaining_markers()
    # disabling all remaining markers twice shouldn't have any effect
    assert iprogram2 == iprogram2.disable_remaining_markers()
    assert set(
        (
            DCEMarker("DCEMarker0_"),
            DCEMarker("DCEMarker1_"),
        )
    ) == set(iprogram2.disabled_markers)
    assert set(()) == set(iprogram2.find_alive_markers(gcc))
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram2.find_dead_markers(gcc)
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

    asm = gcc.get_asm_from_program(iprogram)
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

    assert set(()) == set(iprogram.find_alive_markers(gcc))
    assert set((DCEMarker("DCEMarker0_"), DCEMarker("DCEMarker1_"))) == set(
        iprogram.find_dead_markers(gcc)
    )

    asm = gcc.get_asm_from_program(iprogram)
    assert "bar" not in asm

    # All markers have already been disabled or made unreachable
    assert iprogram == iprogram.disable_remaining_markers()
