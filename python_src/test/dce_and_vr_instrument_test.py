from diopter.compiler import Language, SourceProgram
from program_markers.instrumenter import InstrumenterMode, instrument_program
from program_markers.markers import DCEMarker, VRMarker

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
        mode=InstrumenterMode.DCE_AND_VR,
    )

    all_markers = set(
        (
            VRMarker.from_str("VRMarker0_", "int"),
            DCEMarker.from_str("DCEMarker1_"),
            DCEMarker.from_str("DCEMarker2_"),
        )
    )

    assert all_markers == set(iprogram.enabled_markers())

    gcc = get_system_gcc_O0()
    assert all_markers == set(iprogram.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram.find_eliminated_markers(gcc))

    eliminated_markers = iprogram.find_eliminated_markers(gcc)
    non_eliminated_markers = iprogram.find_non_eliminated_markers(gcc)
    assert set() == set(eliminated_markers)
    assert all_markers == set(non_eliminated_markers)
