import pytest
from diopter.compiler import Language
from program_markers.instrumenter import InstrumentedProgram
from program_markers.markers import DCEMarker, FunctionCallStrategy


def make_marker(i: int) -> DCEMarker:
    return DCEMarker(f"{DCEMarker.prefix()}{i}_", i)


def test_no_duplicate_marker_ids() -> None:
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1), make_marker(1)),
        )
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1),),
            disabled_markers=(make_marker(1),),
        )
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1),),
            unreachable_markers=(make_marker(1),),
        )
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1),),
            aborted_markers=(make_marker(1),),
        )
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1),),
            tracked_markers=(make_marker(1),),
        )
    with pytest.raises(AssertionError):
        InstrumentedProgram(
            marker_strategy=FunctionCallStrategy(),
            language=Language.C,
            code="bla bla",
            enabled_markers=(make_marker(1),),
            tracked_for_refinement_markers=(make_marker(1),),
        )
