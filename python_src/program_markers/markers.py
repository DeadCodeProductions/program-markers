from __future__ import annotations

import re
from abc import ABC, abstractmethod
from dataclasses import dataclass
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
        return f"""#define {self.macro()} \
                    {self.marker_statement_prefix()} \
                    __builtin_printf("{self.name}\\n"); \
                    {self.marker_statement_postfix()}
                """

    def emit_tracking_directive_for_refinement(self) -> str:
        return f"#define {self.macro()}"

    def parse_tracked_output_for_refinement(self, lines: list[str]) -> Marker:
        for line in lines:
            assert line.strip() == self.name
        return self


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


@dataclass(frozen=True)
class VRMarker(Marker):
    """
    A value range marker VRMarkerX_, where X is an
    integer. VR markers enable range checks via dead
    code elimination:

    if (!( LowerBound <= var  LowerBound && var <= UpperBound))
        VRMarkerX_();

    The marker is dead if `var`  in [LowerBound, UpperBound].


    Attributes:
        marker(str): the marker in the VRMarkerX_ form
        id (int): the id of the marker
        variable_type (str): the type of the instrumented variable
        lower_bound (int): the lower bound of the range (inclusive)
        upper_bound (int): the upper bound of the range (inclusive)
    """

    variable_type: str
    lower_bound: int = 0
    upper_bound: int = 0

    def __post_init__(self) -> None:
        assert self.lower_bound <= self.upper_bound

    @classmethod
    def prefix(cls) -> str:
        return "VRMarker"

    @staticmethod
    def from_str(marker_str: str, variable_type: str) -> VRMarker:
        """Parsers a string of the form VRMarkerX_

        Returns:
            VRMarker:
                the parsed marker
        """
        assert marker_str.startswith(VRMarker.prefix())
        marker_id = int(marker_str[len(VRMarker.prefix()) : -1])
        return VRMarker(marker_str, marker_id, variable_type)

    def macro(self) -> str:
        """Returns the preprocessor macro that can
        be defined before compiling the program

        Returns:
            str:
                VRMARKERMACROX_(VAR, TYPE)
        """
        return f"{self.macro_without_arguments()}(VAR, TYPE)"

    def macro_without_arguments(self) -> str:
        """Returns the preprocessor macro that can
        be defined before compiling the program
        without its arguments

        Returns:
            str:
                VRMARKERMACROX_
        """
        return f"VRMARKERMACRO{self.id}_"

    def marker_statement_prefix(self) -> str:
        return (
            f"if (!(((VAR) >= {self.lower_bound}) && ((VAR) <= {self.upper_bound}))) "
            "{ "
        )

    def marker_statement_postfix(self) -> str:
        return " }"

    def emit_tracking_directive_for_refinement(self) -> str:
        format_specifier = {
            "short": "%hd",
            "int": "%d",
            "long": "%ld",
            "unsigned short": "%hu",
            "unsigned int": "%u",
            "unsigned long": "%lu",
        }[self.variable_type]
        return f""" #define {self.macro()} \
                        __builtin_printf("{self.name}:{format_specifier}\\n", (VAR));
                """

    def parse_tracked_output_for_refinement(self, lines: list[str]) -> Marker:
        values = []
        for line in lines:
            assert line.startswith(self.name)
            values.append(int(line.strip().split(":")[1]))
        return VRMarker(
            self.name, self.id, self.variable_type, min(values), max(values)
        )

    def get_variable_name_and_type(self, instrumented_code: str) -> tuple[str, str]:
        """Returns the name and type of the instrumented variable.
        The marker macro must appear only once in the instrumented code.

        Args:
            instrumented_code (str):
                the instrumented code containing the marker

        Returns:
            tuple[str, str]:
                the name and type
        """
        macro = self.macro_without_arguments()
        reg = re.compile(rf"{macro}\((?P<name>[^,]+),\s*(?P<type>[^)]+)\)")
        matches = [match for match in reg.finditer(instrumented_code)]
        assert (
            len(matches) == 1
        ), f"Expected exactly one match for {macro} in {instrumented_code}"
        match = matches[0]
        return match.group("name"), match.group("type")

    def number_occurences_in_code(self, instrumented_code: str) -> int:
        """Returns number of types a marker appears in the instrumented code.

        This is useful, e.g., when reducing a program and a marker is duplicated.

        Args:
            instrumented_code (str):
                the instrumented code containing the marker

        Returns:
            int:
                the number of occurences
        """
        macro = self.macro_without_arguments()
        reg = re.compile(f"{macro}")
        return len(reg.findall(instrumented_code))


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
