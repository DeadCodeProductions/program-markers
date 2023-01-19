from diopter.compiler import Language, SourceProgram

from dead_instrumenter.instrumenter import DCEMarker, instrument_program

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
    m0 = DCEMarker("DCEMarker0_")
    m1 = DCEMarker("DCEMarker1_")
    assert set((m0, m1)) == set(iprogram.all_markers())
    gcc = get_system_gcc_O0()

    # preprocess without any disabled or unreachable markers
    iprogram_p = iprogram.preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m0, m1)) == set(iprogram_p.all_markers())
    assert set((m0, m1)) == set(iprogram_p.find_alive_markers(gcc))
    assert set() == set(iprogram_p.find_dead_markers(gcc))

    # preprocess with an unreachable marker
    iprogram_u = iprogram.make_markers_unreachable(
        [m0]
    ).preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m1,)) == set(iprogram_u.all_markers())
    assert set((m1,)) == set(iprogram_u.find_alive_markers(gcc))
    assert set() == set(iprogram_u.find_dead_markers(gcc))

    # preprocess with a disabled marker
    iprogram_d = iprogram.make_markers_unreachable(
        [m1]
    ).preprocess_disabled_and_unreachable_markers(gcc)
    assert set((m0,)) == set(iprogram_d.all_markers())
    assert set((m0,)) == set(iprogram_d.find_alive_markers(gcc))
    assert set() == set(iprogram_d.find_dead_markers(gcc))

    # preprocess with a disabled and an unreachable marker
    iprogram_ud = (
        iprogram.make_markers_unreachable([m1])
        .disable_markers([m0])
        .preprocess_disabled_and_unreachable_markers(gcc)
    )
    assert set() == set(iprogram_ud.all_markers())
    assert set() == set(iprogram_ud.find_alive_markers(gcc))
    assert set() == set(iprogram_ud.find_dead_markers(gcc))
