import pytest
from diopter.compiler import Language, SourceProgram

from dead_instrumenter.instrumenter import (
    InstrumenterMode,
    VRMarker,
    VRMarkerKind,
    find_alive_markers_impl,
    instrument_program,
)

from .utils import get_system_gcc_O0


def test_asm_parsing() -> None:
    asm = """
    call VRMarkerLE0_@PLT
.L2:
    cmpl	$0, -4(%rbp)
    js	.L3
    call	VRMarkerGE0_@PLT
    """
    assert (set(find_alive_markers_impl(asm))) == set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.GE),
            VRMarker("VRMarker0_", VRMarkerKind.LE),
        )
    )

    asm = """
    js	.L2
    call	VRMarkerGE0_@PLT
"""
    assert (set(find_alive_markers_impl(asm))) == set(
        (VRMarker("VRMarker0_", VRMarkerKind.GE),)
    )


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
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram.vr_markers)

    gcc = get_system_gcc_O0()
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram.find_alive_markers(gcc))
    assert set() == set(iprogram.find_dead_markers(gcc))


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
        iprogram.disable_markers((VRMarker("VRMarker1_", VRMarkerKind.LE),))

    iprogram0 = iprogram.disable_markers((VRMarker("VRMarker0_", VRMarkerKind.LE),))
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram0.disabled_markers
    )
    assert set(iprogram0.defined_macros) == set(("DisableVRMarkerLE0_",))
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(
        iprogram0.find_alive_markers(gcc)
    )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram0.find_dead_markers(gcc)
    )
    # disabling the same marker twice shouldn't have any effect
    assert iprogram0 == iprogram0.disable_markers(
        (VRMarker("VRMarker0_", VRMarkerKind.LE),)
    )

    iprogram1 = iprogram.disable_markers((VRMarker("VRMarker0_", VRMarkerKind.GE),))
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(
        iprogram1.disabled_markers
    )
    assert set(iprogram1.defined_macros) == set(("DisableVRMarkerGE0_",))
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram1.find_alive_markers(gcc)
    )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(
        iprogram1.find_dead_markers(gcc)
    )

    iprogram1_1 = iprogram1.disable_remaining_markers()
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram1_1.disabled_markers)
    assert set() == set(iprogram1_1.find_alive_markers(gcc))
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram1_1.find_dead_markers(gcc))

    iprogram2 = iprogram.disable_remaining_markers()
    # disabling all remaining markers twice shouldn't have any effect
    assert iprogram2 == iprogram2.disable_remaining_markers()
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram2.disabled_markers)
    assert set(()) == set(iprogram2.find_alive_markers(gcc))
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram2.find_dead_markers(gcc))


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
        iprogram.make_markers_unreachable((VRMarker("VRMarker1_", VRMarkerKind.LE),))

    iprogram0 = iprogram.make_markers_unreachable(
        (VRMarker("VRMarker0_", VRMarkerKind.LE),)
    )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram0.unreachable_markers
    )
    assert set(iprogram0.defined_macros) == set(("UnreachableVRMarkerLE0_",))
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(
        iprogram0.find_alive_markers(gcc)
    )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram0.find_dead_markers(gcc)
    )
    # Making the same marker unreachable twice shouldn't have any effect
    assert iprogram0 == iprogram0.make_markers_unreachable(
        (VRMarker("VRMarker0_", VRMarkerKind.LE),)
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

    iprogram = iprogram.make_markers_unreachable(
        (VRMarker("VRMarker0_", VRMarkerKind.LE),)
    )
    with pytest.raises(AssertionError):
        # We can't disable a marker that was already make unreachable
        iprogram.disable_markers((VRMarker("VRMarker0_", VRMarkerKind.LE),))

    iprogram = iprogram.disable_markers((VRMarker("VRMarker0_", VRMarkerKind.GE),))
    with pytest.raises(AssertionError):
        # We can't make ureachable a marker that was already disabled
        iprogram = iprogram.make_markers_unreachable(
            (VRMarker("VRMarker0_", VRMarkerKind.GE),)
        )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.LE),)) == set(
        iprogram.unreachable_markers
    )
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(
        iprogram.disabled_markers
    )

    assert set(()) == set(iprogram.find_alive_markers(gcc))
    assert set(
        (
            VRMarker("VRMarker0_", VRMarkerKind.LE),
            VRMarker("VRMarker0_", VRMarkerKind.GE),
        )
    ) == set(iprogram.find_dead_markers(gcc))

    # All markers have already been disabled or made unreachable
    assert iprogram == iprogram.disable_remaining_markers()
