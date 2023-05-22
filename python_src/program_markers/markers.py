from __future__ import annotations

import re
from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum
from re import Pattern


@dataclass(frozen=True)
class Marker(ABC):
    name: str
    id: int

    def __post_init__(self) -> None:
        """Check that self.marker is a valid DCEMarker"""
        assert self.name.startswith(type(self).prefix())
        assert self.name[-1] == "_"
        assert self.id >= 0

    @classmethod
    @abstractmethod
    def prefix(cls) -> str:
        raise NotImplementedError

    @abstractmethod
    def macro(self) -> str:
        raise NotImplementedError

    def marker_statement_prefix(self) -> str:
        return ""

    def marker_statement_postfix(self) -> str:
        return ""

    def emit_disabling_directive(self) -> str:
        return f"#define {self.macro()} ;"

    def emit_unreachable_directive(self) -> str:
        return f"""#define {self.macro()} \
                {self.marker_statement_prefix()}__builtin_unreachable();{self.marker_statement_postfix()}
                """

    def emit_enabled_directive(self, strategy: MarkerStrategy) -> str:
        return f"""{strategy.definitions_and_declarations(self)}
                #define {self.macro()} \
                {self.marker_statement_prefix()} \
                {strategy.make_macro_definition(self)} \
                {self.marker_statement_postfix()}
                """

    def emit_tracking_directive(self) -> str:
        return f"""void {self.name}(void){{__builtin_printf("{self.name}");}}
                #define {self.macro()} \
                {self.marker_statement_prefix()}{self.name}();{self.marker_statement_postfix()}
                """


@dataclass(frozen=True)
class DCEMarker(Marker):
    """A dead code elimination marker DCEMarkerX_, where X is an
    integer.

    """

    @staticmethod
    def from_str(marker_str: str) -> DCEMarker:
        """Parsers a string of the form DCEMarkerX_

        Returns:
            DCEMarker:
                the parsed marker
        """
        assert marker_str.startswith(DCEMarker.prefix())
        marker_id = int(marker_str[len(DCEMarker.prefix()) : -1])
        return DCEMarker(marker_str, marker_id)

    @classmethod
    def prefix(cls) -> str:
        return "DCEMarker"

    def macro(self) -> str:
        """Returns the preprocessor macro that can
        be defined before compiling the program

        Returns:
            str:
                DCEMARKERMACROX_
        """
        return f"DCEMARKERMACRO{self.name[len(DCEMarker.prefix()):]}"


class VRMarkerKind(Enum):
    """
    A VRMarker can either be a LE-Equal(LE):
        if (var <= VRMarkerConstantLEX_)
            VRMarkerLEX_

    or a Greater-Equal(GE):
        if (var >= VRMarkerConstantGEX_)
            VRMarkerGEX_
    """

    LE = 0
    GE = 1

    @staticmethod
    def from_str(kind: str) -> VRMarkerKind:
        match kind:
            case "LE":
                return VRMarkerKind.LE
            case "GE":
                return VRMarkerKind.GE
            case _:
                raise ValueError(f"{kind} is not a valid VRMarkerKind")

    def operator(self) -> str:
        """Returns:
        str:
            "<=" or ">="
        """
        if self == VRMarkerKind.LE:
            return "<="
        else:
            return ">="


@dataclass(frozen=True)
class VRMarker(Marker):
    """A value range marker VRMarkerX_, where X is an
    integer, corresponds to a marker macro and a constant macro:
    - (VRMarkerLEX_, VRMarkerConstantLEX_) for LE markers
    - (VRMarkerGEX_ VRMarkerConstantGEX_) for GE markers

    These appear in the source code as:
        if (var <= VRMarkerConstantLEX_)
            VRMarkerLEX_
    or:
        if (var >= VRMarkerConstantGEX_)
            VRMarkerGEX_


    Attributes:
        marker(str): the marker in the VRMarkerX_ form
        kind (VRMarkerKind): LE or GE
    """

    kind: VRMarkerKind
    constant: int = 0

    @classmethod
    def prefix(cls) -> str:
        return "VRMarker"

    @staticmethod
    def from_str(marker_str: str) -> VRMarker:
        """Parsers a string of the form VRMarkerLEX_ | VRMarkerGEX_

        Returns:
            VRMarker:
                the parsed marker
        """
        assert marker_str.startswith(VRMarker.prefix())
        kind = VRMarkerKind.from_str(
            marker_str[len(VRMarker.prefix()) : len(VRMarker.prefix()) + 2]
        )
        marker_id = int(marker_str[len(VRMarker.prefix()) + len(kind.name) : -1])
        marker_name = VRMarker.prefix() + str(marker_id) + "_"

        return VRMarker(marker_name, marker_id, kind)

    def macro(self) -> str:
        """Returns the preprocessor macro that can
        be defined before compiling the program

        Returns:
            str:
                VRMARKERMACRO{LE,GE}X_(VAR)
        """
        return f"VRMARKERMACRO{self.kind.name}{self.name[len(VRMarker.prefix()):]}(VAR)"

    def marker_statement_prefix(self) -> str:
        return f"if ((VAR) {self.kind.operator()} {self.constant}) " "{ "

    def marker_statement_postfix(self) -> str:
        return " }"


MarkerTypes = (DCEMarker, VRMarker)


class MarkerStrategy(ABC):
    """The base class of all marker strategies used for detecting Markers.

    Subclasses of MarkerStrategy are used to specify what kind of marker preprocessor
    directives should be generated by `Instrumenter.with_marker_strategy`.
    """

    @staticmethod
    @abstractmethod
    def name() -> str:
        """Returns the name of the marker strategy

        Returns:
            str:
                the name
        """
        raise NotImplementedError

    @staticmethod
    @abstractmethod
    def definitions_and_declarations(marker: Marker) -> str:
        """Returns any definitions and declarations
        necessary for this marker strategy

        Args:
            marker (Marker):
                the marker for which the macro definition code should be generated

        Returns:
            str:
                the definitions and declarations
        """
        raise NotImplementedError

    @staticmethod
    @abstractmethod
    def make_macro_definition(marker: Marker) -> str:
        """Generates the macro definition for the marker

        Args:
            marker (Marker):
                the marker for which the macro definition code should be generated

        Returns:
            str:
                the marker macro defition
        """
        raise NotImplementedError

    @staticmethod
    @abstractmethod
    def marker_detection_regex() -> re.Pattern[str]:
        """Returns a pattern of the regex used to detect markers of this strategy

        Returns:
            re.Pattern[str]:
                the regex pattern
        """
        raise NotImplementedError

    @staticmethod
    @abstractmethod
    def regex_marker_id_group_index() -> int:
        """Returns the index of the group in the regex, generated by
        marker_detection_regex(), containing the marker id

        Returns:
            int:
                the index of the id group
        """
        raise NotImplementedError

    def detect_marker_id(self, asm_line: str) -> int | None:
        if m := self.marker_detection_regex().match(asm_line.strip()):
            idx = self.regex_marker_id_group_index()
            return int(m.group(idx))
        return None


def marker_prefixes() -> tuple[str, ...]:
    prefixes = []
    for marker_type in MarkerTypes:
        prefix = marker_type.prefix()  # type: ignore
        assert isinstance(prefix, str)
        prefixes.append(prefix)
    return tuple(prefixes)


class FunctionCallStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Function call"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        return f"void {marker.name}(void);"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        return f"{marker.name}();"

    @staticmethod
    def marker_detection_regex() -> re.Pattern[str]:
        # (call|j[a-z]{1,2}) checks that the instruction either starts with
        # call or j and two letters (thus, including conditional
        # and uncoditional jumps)
        return re.compile(
            f".*(call|j[a-z]{{1,2}}).*({'|'.join(marker_prefixes())})([0-9]+)_.*"
        )

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 3


class AsmCommentStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        return ""

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        return f'asm("# {marker.name}");'

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(f".*\\#.*({'|'.join(marker_prefixes())})([0-9]+)_.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 2


class AsmCommentEmptyOperandsStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Empty Operands"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        return f'asm("# {marker.name}" :::);'


class AsmCommentLocalOutOperandStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Local Out Operand"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return (
            f"{{volatile int {variable_name};"
            + f'asm("# {marker.name}" : "=r" ({variable_name}));}}'
        )


class AsmCommentGlobalOutOperandStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Global Out Operand"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"int {variable_name};"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f'asm("# {marker.name}" : "=r" ({variable_name}));'


class AsmCommentVolatileGlobalOutOperandStrategy(AsmCommentGlobalOutOperandStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Volatile Global Out Operand"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"volatile int {variable_name};"


class AsmCommentStaticVolatileGlobalOutOperandStrategy(
    AsmCommentGlobalOutOperandStrategy
):
    @staticmethod
    def name() -> str:
        return "Asm Comment Static Volatile Global Out Operand"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"static volatile int {variable_name};"


class LocalVolatileIntStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Local Volatile Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        return ""

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        magic_int = f"1234123{marker.id}"
        return f"volatile int {variable_name} = {magic_int};"

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        # need to disambiguate between marker types
        return re.compile(".*movl.*\\$1234123([0-9]+),.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1


class GlobalIntStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Global Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"int {variable_name};"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        magic_int = f"1234123{marker.id}"
        return f"{variable_name} = {magic_int};"

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(".*movl.*\\$1234123([0-9]+),.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1


class GlobalVolatileIntStrategy(GlobalIntStrategy):
    @staticmethod
    def name() -> str:
        return "Global Volatile Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"volatile int {variable_name};"


class StaticVolatileGlobalIntStrategy(GlobalIntStrategy):
    @staticmethod
    def name() -> str:
        return "Static Volatile Global Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"static volatile int {variable_name};"
