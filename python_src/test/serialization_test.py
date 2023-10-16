from pathlib import Path
from tempfile import NamedTemporaryFile

import pytest
from diopter.compiler import Language, Source, SourceProgram
from program_markers.instrumenter import InstrumenterMode, instrument_program
from program_markers.iprogram import InstrumentedProgram
from program_markers.markers import (
    AbortEmitter,
    AsmCommentDetectionStrategy,
    AsmCommentEmptyOperandsDetectionStrategy,
    AsmCommentGlobalOutOperandDetectionStrategy,
    AsmCommentLocalOutOperandDetectionStrategy,
    AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy,
    AsmCommentVolatileGlobalOutOperandDetectionStrategy,
    DCEMarker,
    DisableEmitter,
    EnableEmitter,
    FunctionCallDetectionStrategy,
    GlobalIntDetectionStrategy,
    GlobalVolatileIntDetectionStrategy,
    LocalVolatileIntDetectionStrategy,
    Marker,
    MarkerDetectionStrategy,
    StaticVolatileGlobalIntDetectionStrategy,
    TrackingEmitter,
    TrackingForRefinementEmitter,
    UnreachableEmitter,
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
        AsmCommentDetectionStrategy(),
        AsmCommentEmptyOperandsDetectionStrategy(),
        AsmCommentLocalOutOperandDetectionStrategy(),
        AsmCommentGlobalOutOperandDetectionStrategy(),
        AsmCommentVolatileGlobalOutOperandDetectionStrategy(),
        AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy(),
        StaticVolatileGlobalIntDetectionStrategy(),
        GlobalIntDetectionStrategy(),
        GlobalVolatileIntDetectionStrategy(),
        LocalVolatileIntDetectionStrategy(),
        FunctionCallDetectionStrategy(),
    )
    for strategy in strategies:
        assert (
            MarkerDetectionStrategy.from_json_dict(strategy.to_json_dict()) == strategy
        )

    assert MarkerDetectionStrategy.from_json_dict(
        AsmCommentDetectionStrategy().to_json_dict()
    ) != MarkerDetectionStrategy.from_json_dict(
        GlobalVolatileIntDetectionStrategy().to_json_dict()
    )


def test_instrumented_program() -> None:
    p0 = InstrumentedProgram(
        language=Language.C,
        defined_macros=("M1", "M2"),
        include_paths=("a", "b"),
        system_include_paths=("sa", "sb"),
        flags=("-fPIC", "-fno-omit-frame-pointer"),
        code="bla bla",
        markers=(make_dce_marker(1), make_vr_marker(2, 1, 10)),
        directive_emitters={
            make_dce_marker(1): EnableEmitter(FunctionCallDetectionStrategy()),
            make_vr_marker(2, 1, 10): EnableEmitter(FunctionCallDetectionStrategy()),
        },
        marker_strategy=FunctionCallDetectionStrategy(),
    )

    assert p0 == InstrumentedProgram.from_json_dict(p0.to_json_dict())

    with pytest.raises(ValueError):
        Source.from_json_dict(p0.to_json_dict())

    with pytest.raises(AssertionError):
        SourceProgram.from_json_dict(p0.to_json_dict())

    marker_strategy = AsmCommentDetectionStrategy()
    ee = EnableEmitter(marker_strategy)
    de = DisableEmitter()
    ue = UnreachableEmitter()
    te = TrackingEmitter()
    tfre = TrackingForRefinementEmitter()
    ae = AbortEmitter()
    marker_directives = {
        make_dce_marker(1): ee,
        make_vr_marker(10, 1, 10): ee,
        make_dce_marker(2): de,
        make_vr_marker(11, 4, 10): de,
        make_dce_marker(3): ue,
        make_vr_marker(12, 1, 40): ue,
        make_dce_marker(4): te,
        make_vr_marker(13, 1, 11): te,
        make_dce_marker(5): tfre,
        make_dce_marker(6): ae,
        make_vr_marker(7, 1, 12): ae,
        make_dce_marker(8): ae,
        make_vr_marker(9, 1, 13): ae,
    }

    p1 = InstrumentedProgram(
        language=Language.CPP,
        defined_macros=("M1",),
        system_include_paths=("sa",),
        code="bla bla blac",
        markers=tuple(marker_directives.keys()),
        directive_emitters=marker_directives,
        marker_strategy=AsmCommentDetectionStrategy(),
    )

    assert p1 == InstrumentedProgram.from_json_dict(p1.to_json_dict())


def test_to_file() -> None:
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
    ntfc = NamedTemporaryFile(suffix=".c")
    ntfh = NamedTemporaryFile(suffix=".h")
    ntfj = NamedTemporaryFile(suffix=".json")

    source = iprogram.to_file(Path(ntfc.name), Path(ntfh.name), Path(ntfj.name))
    assert any(str(ntfh.name) in flag for flag in source.flags)
    assert source.filename.read_text() == iprogram.code
    assert Path(ntfh.name).read_text() == iprogram.generate_preprocessor_directives()
    assert iprogram == InstrumentedProgram.from_source_file(
        source, Path(ntfh.name), Path(ntfj.name)
    )
