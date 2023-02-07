from diopter.compiler import Language, SourceProgram
from program_markers.instrumenter import (
    DCEMarker,
    InstrumenterMode,
    VRMarker,
    VRMarkerKind,
    instrument_program,
)

from .utils import get_system_gcc_O0


def test_dce_marker_tracking() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a){
        if (a > 1)
            return 1;
        return 0;
    }
    int main(int argc, char* argv[]){
         foo(argc);
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

    executed_markers_without_args = iprogram.track_markers((), gcc)
    assert set((DCEMarker("DCEMarker0_"),)) == set(executed_markers_without_args)

    executed_markers_with_args = iprogram.track_markers(("foo",), gcc)
    assert set((DCEMarker("DCEMarker1_"),)) == set(executed_markers_with_args)


def test_vr_marker_tracking() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int foo(int a){
        if (a > 1)
            return 1;
        return 0;
    }
    int main(int argc, char* argv[]){
         foo(argc);
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
            VRMarker("VRMarker1_", VRMarkerKind.LE),
            VRMarker("VRMarker1_", VRMarkerKind.GE),
        )
    ) == set(iprogram.vr_markers)
    iprogram = iprogram.disable_markers(
        (
            VRMarker("VRMarker1_", VRMarkerKind.LE),
            VRMarker("VRMarker1_", VRMarkerKind.GE),
        )
    )

    gcc = get_system_gcc_O0()

    executed_markers = iprogram.track_markers((), gcc)
    assert set((VRMarker("VRMarker0_", VRMarkerKind.GE),)) == set(executed_markers)
