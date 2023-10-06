import pytest
from diopter.compiler import Language
from program_markers.iprogram import InstrumentedProgram
from program_markers.markers import (
    DCEMarker,
    EnableEmitter,
    FunctionCallDetectionStrategy,
)


def make_marker(i: int) -> DCEMarker:
    return DCEMarker(f"{DCEMarker.prefix()}{i}_", i)


def test_no_duplicate_marker_ids() -> None:
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallDetectionStrategy(),
            language=Language.C,
            code="bla bla",
            markers=(make_marker(1), make_marker(1)),
            directive_emitters={
                make_marker(1): EnableEmitter(FunctionCallDetectionStrategy()),
                make_marker(1): EnableEmitter(FunctionCallDetectionStrategy()),
            },
        )
