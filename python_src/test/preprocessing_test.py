from diopter.compiler import Language, SourceProgram
from program_markers.instrumenter import instrument_program
from program_markers.markers import DCEMarker

from .utils import get_system_gcc_O0


def test_preprocessing() -> None:
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
    m0 = DCEMarker.from_str("DCEMarker0_")
    m1 = DCEMarker.from_str("DCEMarker1_")
    assert set((m0, m1)) == set(iprogram.markers)
    gcc = get_system_gcc_O0()

    # preprocess without any disabled or unreachable markers
    iprogram_p = iprogram.preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m0, m1)) == set(iprogram_p.markers)
    assert set((m0, m1)) == set(iprogram_p.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram_p.find_eliminated_markers(gcc))

    # preprocess with an unreachable marker
    iprogram_u = iprogram.make_markers_unreachable(
        [m0]
    ).preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m1,)) == set(iprogram_u.markers)
    assert set((m1,)) == set(iprogram_u.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram_u.find_eliminated_markers(gcc))
    assert "__builtin_unreachable()" in iprogram_u.code
    assert m1.macro() in iprogram_u.code

    # preprocess with a disabled marker
    iprogram_d = iprogram.disable_markers(
        [m1]
    ).preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m0,)) == set(iprogram_d.markers)
    assert set((m0,)) == set(iprogram_d.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram_d.find_eliminated_markers(gcc))
    assert m0.macro() in iprogram_d.code
    assert m1.macro() not in iprogram_d.code

    # preprocess with a disabled and an unreachable marker
    iprogram_ud = (
        iprogram.make_markers_unreachable([m1])
        .disable_markers([m0])
        .preprocess_disabled_and_unreachable_markers(gcc)
    )
    assert set() == set(iprogram_ud.markers)
    assert set() == set(iprogram_ud.find_non_eliminated_markers(gcc))
    assert set() == set(iprogram_ud.find_eliminated_markers(gcc))
    assert m0.macro() not in iprogram_ud.code
    assert "__builtin_unreachable()" in iprogram_ud.code
