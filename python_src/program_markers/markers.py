from __future__ import annotations

import re
from abc import ABC, abstractmethod
from dataclasses import dataclass, fields, replace
from re import Pattern
from typing import Any, Sequence


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

    @abstractmethod
    def macro_without_arguments(self) -> str:
        """Returns the preprocessor macro that can
        be defined before compiling the program
        without its arguments (if any)
        """
        raise NotImplementedError

    def marker_statement_prefix(self) -> str:
        return ""

    def marker_statement_postfix(self) -> str:
        return ""

    def parse_tracked_output_for_refinement(self, output: Sequence[str]) -> Marker:
        raise RuntimeError("This should never be called, DCEMarkers cannot be refined")

    @abstractmethod
    def to_json_dict(self) -> dict[str, Any]:
        raise NotImplementedError

    @staticmethod
    def from_json_dict(j: dict[str, Any]) -> Marker:
        match j["kind"]:
            case "DCEMarker":
                return DCEMarker.from_json_dict(j)
            case "VRMarker":
                return VRMarker.from_json_dict(j)
            case _:
                raise ValueError(f"Unknown marker kind {j['kind']}")

    def update_id(self, new_id: int) -> Marker:
        return replace(
            self, name=self.name.replace(str(self.id), str(new_id)), id=new_id
        )


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

    def macro_without_arguments(self) -> str:
        """Same as self.macro()"""
        return self.macro()

    def to_json_dict(self) -> dict[str, Any]:
        j = {"kind": "DCEMarker", "name": self.name, "id": self.id}
        assert set(j.keys()) == set(field.name for field in fields(self)) | set(
            ("kind",)
        )
        return j

    @staticmethod
    def from_json_dict(j: dict[str, Any]) -> DCEMarker:
        assert j["kind"] == "DCEMarker"
        return DCEMarker(name=j["name"], id=j["id"])


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

    @classmethod
    def macroprefix(cls) -> str:
        return "VRMARKERMACRO"

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
        return f"{VRMarker.macroprefix()}{self.id}_"

    def marker_statement_prefix(self) -> str:
        return (
            f"if (!(((VAR) >= {self.lower_bound}) && ((VAR) <= {self.upper_bound}))) "
            "{ "
        )

    def marker_statement_postfix(self) -> str:
        return " }"

    def parse_tracked_output_for_refinement(self, output: Sequence[str]) -> Marker:
        lbs = []
        ubs = []
        for line in output:
            line = line.strip()
            assert line.startswith(self.name)
            lb, ub = line.split(":")[1].split("/")
            lbs.append(int(lb))
            ubs.append(int(ub))
        assert lbs
        assert ubs
        return VRMarker(self.name, self.id, self.variable_type, min(lbs), max(ubs))

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

    def to_json_dict(self) -> dict[str, Any]:
        j = {
            "kind": "VRMarker",
            "name": self.name,
            "id": self.id,
            "variable_type": self.variable_type,
            "lower_bound": self.lower_bound,
            "upper_bound": self.upper_bound,
        }
        assert set(j.keys()) == set(field.name for field in fields(self)) | set(
            ("kind",)
        )
        return j

    @staticmethod
    def from_json_dict(j: dict[str, Any]) -> VRMarker:
        assert j["kind"] == "VRMarker"
        return VRMarker(
            name=j["name"],
            id=j["id"],
            variable_type=j["variable_type"],
            lower_bound=j["lower_bound"],
            upper_bound=j["upper_bound"],
        )


MarkerTypes = (DCEMarker, VRMarker)


class MarkerDirectiveEmitter(ABC):
    def emit_directive(self, marker: Marker) -> str:
        raise NotImplementedError

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MarkerDirectiveEmitter):
            return NotImplemented
        return isinstance(other, self.__class__)

    @staticmethod
    def from_json_dict(j: dict[str, Any]) -> MarkerDirectiveEmitter:
        match j["kind"]:
            case "EnableEmitter":
                strategy = MarkerDetectionStrategy.from_json_dict(j["strategy"])
                return EnableEmitter(strategy)
            case "DisableEmitter":
                return DisableEmitter()
            case "UnreachableEmitter":
                return UnreachableEmitter()
            case "AbortEmitter":
                return AbortEmitter()
            case "TrackingEmitter":
                return TrackingEmitter()
            case "TrackingForRefinementEmitter":
                return TrackingForRefinementEmitter()
            case _:
                raise ValueError(f"Unknown kind {j['kind']}")

    def to_json_dict(self) -> dict[str, Any]:
        name = self.__class__.__name__
        assert name in (
            "EnableEmitter",
            "DisableEmitter",
            "UnreachableEmitter",
            "AbortEmitter",
            "TrackingEmitter",
            "TrackingForRefinementEmitter",
        ), self
        j: dict[str, Any] = {"kind": name}
        if isinstance(self, EnableEmitter):
            j["strategy"] = self.strategy.to_json_dict()
        return j


class NoEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        return ""


class EnableEmitter(MarkerDirectiveEmitter):
    def __init__(self, strategy: MarkerDetectionStrategy):
        self.strategy = strategy

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MarkerDirectiveEmitter):
            return NotImplemented
        return isinstance(other, EnableEmitter) and self.strategy == other.strategy

    def emit_directive(self, marker: Marker) -> str:
        return f"""{self.strategy.definitions_and_declarations(marker)}
                #define {marker.macro()} \
                {marker.marker_statement_prefix()} \
                {self.strategy.make_macro_definition(marker)} \
                {marker.marker_statement_postfix()}
                """


class DisableEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        return f"#define {marker.macro()} ;"


class UnreachableEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        return f"""#define {marker.macro()} \
                {marker.marker_statement_prefix()}__builtin_unreachable();{marker.marker_statement_postfix()}
                """


class AbortEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        return f"""#define {marker.macro()} \
                {marker.marker_statement_prefix()} \
                __builtin_printf("BUG\\n"); \
                __builtin_abort(); \
                {marker.marker_statement_postfix()}
                """


class TrackingEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        return f""" int {marker.name}_ENCOUNTERED = 0;
                    __attribute__((destructor))
                    void {marker.name}_print() {{
                        if ({marker.name}_ENCOUNTERED == 1) {{
                        __builtin_printf("{marker.name}\\n");
                        }}
                    }}
                    #define {marker.macro()} \
                    {marker.marker_statement_prefix()} \
                    {marker.name}_ENCOUNTERED  = 1;    \
                    {marker.marker_statement_postfix()}
                """


class TrackingForRefinementEmitter(MarkerDirectiveEmitter):
    def emit_directive(self, marker: Marker) -> str:
        match marker:
            case DCEMarker():
                return f"#define {marker.macro()} "
            case VRMarker():
                format_specifier = {
                    "bool": "%d",
                    "char": "%d",
                    "short": "%hd",
                    "int": "%d",
                    "long": "%ld",
                    "long long": "%lld",
                    "unsigned char": "%u",
                    "unsigned short": "%hu",
                    "unsigned int": "%u",
                    "unsigned long": "%lu",
                    "unsigned long long": "%llu",
                }[marker.variable_type]
                variable_type = (
                    marker.variable_type if marker.variable_type != "bool" else "int"
                )
                return f"""
        int {marker.name}_ENCOUNTERED = 0;
        {variable_type} {marker.name}_LB;
        {variable_type} {marker.name}_UB;

        void track_{marker.name}({variable_type} v) {{
            if (!{marker.name}_ENCOUNTERED) {{
                {marker.name}_LB = v;
                {marker.name}_UB = v;
                {marker.name}_ENCOUNTERED = 1;
                return;
            }}
            if (v < {marker.name}_LB) {{
                {marker.name}_LB = v;
            }}
            if (v > {marker.name}_UB) {{
                {marker.name}_UB = v;
            }}
        }}

        __attribute__((destructor))
        void {marker.name}_print() {{
            if ({marker.name}_ENCOUNTERED == 1) {{
            __builtin_printf(
                "<MarkerTracking>{marker.name}:{format_specifier}/{format_specifier}</MarkerTracking>\\n",
                {marker.name}_LB, {marker.name}_UB);
            }}
        }}

        #define {marker.macro()} \
                track_{marker.name}(VAR);
        """
            case _:
                raise ValueError(f"Unsupported marker type {type(marker)}")


class MarkerDetectionStrategy(ABC):
    """The base class of all marker strategies used for detecting Markers.

    Subclasses of MarkerDetectionStrategy are used to specify what kind of
    marker preprocessor directives should be generated by
    `Instrumenter.with_marker_strategy`.
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

    def to_json_dict(self) -> dict[str, Any]:
        return {"kind": self.name()}

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MarkerDetectionStrategy):
            return NotImplemented
        return type(self) is type(other)

    @staticmethod
    def from_json_dict(j: dict[str, Any]) -> MarkerDetectionStrategy:
        match j["kind"]:
            case "Function Call":
                return FunctionCallDetectionStrategy()
            case "Asm Comment":
                return AsmCommentDetectionStrategy()
            case "Asm Comment Empty Operands":
                return AsmCommentEmptyOperandsDetectionStrategy()
            case "Asm Comment Local Out Operand":
                return AsmCommentLocalOutOperandDetectionStrategy()
            case "Asm Comment Global Out Operand":
                return AsmCommentGlobalOutOperandDetectionStrategy()
            case "Asm Comment Volatile Global Out Operand":
                return AsmCommentVolatileGlobalOutOperandDetectionStrategy()
            case "Asm Comment Static Volatile Global Out Operand":
                return AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy()
            case "Local Volatile Int":
                return LocalVolatileIntDetectionStrategy()
            case "Global Int":
                return GlobalIntDetectionStrategy()
            case "Static Volatile Global Int":
                return StaticVolatileGlobalIntDetectionStrategy()
            case "Global Volatile Int":
                return GlobalVolatileIntDetectionStrategy()
            case _:
                raise ValueError(f"Unknown marker strategy {j['kind']}")


def marker_prefixes() -> tuple[str, ...]:
    prefixes = []
    for marker_type in MarkerTypes:
        prefix = marker_type.prefix()  # type: ignore
        assert isinstance(prefix, str)
        prefixes.append(prefix)
    return tuple(prefixes)


class FunctionCallDetectionStrategy(MarkerDetectionStrategy):
    @staticmethod
    def name() -> str:
        return "Function Call"

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


class AsmCommentDetectionStrategy(MarkerDetectionStrategy):
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


class AsmCommentEmptyOperandsDetectionStrategy(AsmCommentDetectionStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Empty Operands"

    @staticmethod
    def make_macro_definition(marker: Marker) -> str:
        return f'asm("# {marker.name}" :::);'


class AsmCommentLocalOutOperandDetectionStrategy(AsmCommentDetectionStrategy):
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


class AsmCommentGlobalOutOperandDetectionStrategy(AsmCommentDetectionStrategy):
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


class AsmCommentVolatileGlobalOutOperandDetectionStrategy(
    AsmCommentGlobalOutOperandDetectionStrategy
):
    @staticmethod
    def name() -> str:
        return "Asm Comment Volatile Global Out Operand"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"volatile int {variable_name};"


class AsmCommentStaticVolatileGlobalOutOperandDetectionStrategy(
    AsmCommentGlobalOutOperandDetectionStrategy
):
    @staticmethod
    def name() -> str:
        return "Asm Comment Static Volatile Global Out Operand"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"static volatile int {variable_name};"


class LocalVolatileIntDetectionStrategy(MarkerDetectionStrategy):
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


class GlobalIntDetectionStrategy(MarkerDetectionStrategy):
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


class GlobalVolatileIntDetectionStrategy(GlobalIntDetectionStrategy):
    @staticmethod
    def name() -> str:
        return "Global Volatile Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"volatile int {variable_name};"


class StaticVolatileGlobalIntDetectionStrategy(GlobalIntDetectionStrategy):
    @staticmethod
    def name() -> str:
        return "Static Volatile Global Int"

    @staticmethod
    def definitions_and_declarations(marker: Marker) -> str:
        variable_name = f"{marker.prefix()}_JUNK_VAR{marker.id}_"
        return f"static volatile int {variable_name};"
