from diopter.compiler import Language, SourceProgram
from program_markers.instrumenter import InstrumenterMode, instrument_program
from program_markers.markers import DCEMarker, VRMarker

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
    assert set(
        (DCEMarker.from_str("DCEMarker0_"), DCEMarker.from_str("DCEMarker1_"))
    ) == set(iprogram.enabled_markers)
    gcc = get_system_gcc_O0()

    executed_markers_without_args = iprogram.track_reachable_markers((), gcc)
    assert set((DCEMarker.from_str("DCEMarker0_"),)) == set(
        executed_markers_without_args
    )

    executed_markers_with_args = iprogram.track_reachable_markers(("foo",), gcc)
    assert set((DCEMarker.from_str("DCEMarker1_"),)) == set(executed_markers_with_args)


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
            VRMarker.from_str("VRMarker0_", "int"),
            VRMarker.from_str("VRMarker1_", "int"),
        )
    ) == set(iprogram.enabled_markers)
    iprogram = iprogram.disable_markers((VRMarker.from_str("VRMarker1_", "int"),))

    gcc = get_system_gcc_O0()

    executed_markers = iprogram.track_reachable_markers((), gcc)
    assert set((VRMarker.from_str("VRMarker0_", "int"),)) == set(executed_markers)


def test_vr_marker_int_max() -> None:
    iprogram = instrument_program(
        SourceProgram(
            code="""
    int main(int argc, char* argv[]){
         unsigned long a = 18446744073709551615UL;
         return a-a;
    }
    """,
            language=Language.C,
        ),
        mode=InstrumenterMode.VR,
    )
    assert set((VRMarker.from_str("VRMarker0_", "unsigned long"),)) == set(
        iprogram.enabled_markers
    )
    gcc = get_system_gcc_O0()

    rprogram, reachable_markers = iprogram.refine_markers_with_runtime_information(
        (), gcc
    )
    for marker in reachable_markers:
        assert isinstance(marker, VRMarker)
        assert marker.upper_bound == 18446744073709551615
        assert marker.lower_bound == 18446744073709551615
