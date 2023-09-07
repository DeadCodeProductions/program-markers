import pytest
from diopter.compiler import Language, Source, SourceProgram
from program_markers.instrumenter import InstrumentedProgram
from program_markers.markers import (
    AsmCommentEmptyOperandsStrategy,
    AsmCommentGlobalOutOperandStrategy,
    AsmCommentLocalOutOperandStrategy,
    AsmCommentStaticVolatileGlobalOutOperandStrategy,
    AsmCommentStrategy,
    AsmCommentVolatileGlobalOutOperandStrategy,
    DCEMarker,
    FunctionCallStrategy,
    GlobalIntStrategy,
    GlobalVolatileIntStrategy,
    LocalVolatileIntStrategy,
    Marker,
    MarkerStrategy,
    StaticVolatileGlobalIntStrategy,
    VRMarker,
)


def make_dce_marker(i: int) -> DCEMarker:
    return DCEMarker(name=f"DCEMarker{i}_", id=i)


def make_vr_marker(i: int, lower: int, upper: int) -> VRMarker:
    return VRMarker(
        name=f"VRMarker{i}_",
        id=i,
        variable_type="int",
        lower_bound=lower,
        upper_bound=upper,
    )


def test_serialize_marker() -> None:
    dce = make_dce_marker(123)
    assert DCEMarker.from_json_dict(dce.to_json_dict()) == dce
    assert Marker.from_json_dict(dce.to_json_dict()) == dce

    vr = make_vr_marker(123, 1, 10)
    assert VRMarker.from_json_dict(vr.to_json_dict()) == vr
    assert Marker.from_json_dict(vr.to_json_dict()) == vr

    with pytest.raises(AssertionError):
        VRMarker.from_json_dict(dce.to_json_dict())

    with pytest.raises(AssertionError):
        DCEMarker.from_json_dict(vr.to_json_dict())


def test_serialize_strategy() -> None:
    strategies = (
        AsmCommentStrategy(),
        AsmCommentEmptyOperandsStrategy(),
        AsmCommentLocalOutOperandStrategy(),
        AsmCommentGlobalOutOperandStrategy(),
        AsmCommentVolatileGlobalOutOperandStrategy(),
        AsmCommentStaticVolatileGlobalOutOperandStrategy(),
        StaticVolatileGlobalIntStrategy(),
        GlobalIntStrategy(),
        GlobalVolatileIntStrategy(),
        LocalVolatileIntStrategy(),
        FunctionCallStrategy(),
    )
    for strategy in strategies:
        assert MarkerStrategy.from_json_dict(strategy.to_json_dict()) == strategy

    assert MarkerStrategy.from_json_dict(
        AsmCommentStrategy().to_json_dict()
    ) != MarkerStrategy.from_json_dict(GlobalVolatileIntStrategy().to_json_dict())


def test_instrumented_program() -> None:
    p0 = InstrumentedProgram(
        language=Language.C,
        defined_macros=("M1", "M2"),
        include_paths=("a", "b"),
        system_include_paths=("sa", "sb"),
        flags=("-fPIC", "-fno-omit-frame-pointer"),
        code="bla bla",
        enabled_markers=(make_dce_marker(1), make_vr_marker(2, 1, 10)),
        marker_strategy=FunctionCallStrategy(),
    )

    assert p0 == InstrumentedProgram.from_json_dict(p0.to_json_dict())

    with pytest.raises(ValueError):
        Source.from_json_dict(p0.to_json_dict())

    with pytest.raises(AssertionError):
        SourceProgram.from_json_dict(p0.to_json_dict())

    p1 = InstrumentedProgram(
        language=Language.CPP,
        defined_macros=("M1",),
        system_include_paths=("sa",),
        code="bla bla blac",
        enabled_markers=(make_dce_marker(1), make_vr_marker(10, 1, 10)),
        disabled_markers=(make_dce_marker(2), make_vr_marker(11, 4, 10)),
        unreachable_markers=(make_dce_marker(3), make_vr_marker(12, 1, 40)),
        tracked_markers=(make_dce_marker(4), make_vr_marker(13, 1, 11)),
        tracked_for_refinement_markers=(make_dce_marker(5),),
        aborted_markers=(
            make_dce_marker(6),
            make_vr_marker(7, 1, 12),
            make_dce_marker(8),
            make_vr_marker(9, 1, 13),
        ),
        marker_strategy=AsmCommentStrategy(),
    )

    assert p1 == InstrumentedProgram.from_json_dict(p1.to_json_dict())
