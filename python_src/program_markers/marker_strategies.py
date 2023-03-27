import re
from re import Pattern

from program_markers.instrumenter import DCEMarker, MarkerStrategy


class FunctionCallStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Function call"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        return f"{marker.marker}();\n void {marker.marker}(void);"

    @staticmethod
    def marker_detection_regex() -> re.Pattern[str]:
        return re.compile(f".*(call|jmp).*{DCEMarker.prefix}([0-9]+)_.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 2


class AsmCommentStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        return f'asm("# {marker.marker}");'

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(f".*\\#.*{DCEMarker.prefix}([0-9]+)_.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1


class AsmCommentEmptyOperandsStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Empty Operands"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        return f'asm("# {marker.marker}" :::);'


class AsmCommentLocalOutOperandStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Local Out Operand"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        return (
            f"{{volatile int {variable_name};"
            + f'asm("# {marker.marker}" : "=r" ({variable_name}));}}'
        )


class AsmCommentGlobalOutOperandStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Global Out Operand"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        return (
            f'asm("# {marker.marker}" : "=r" ({variable_name}));\n'
            + f"int {variable_name};"
        )


class AsmCommentVolatileGlobalOutOperandStrategy(AsmCommentStrategy):
    @staticmethod
    def name() -> str:
        return "Asm Comment Volatile Global Out Operand"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        return (
            f'asm("# {marker.marker}" : "=r" ({variable_name}));\n'
            + f"volatile int {variable_name};"
        )


class LocalVolatileIntStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Local Volatile Int"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        magic_int = f"1234123{id}"
        return f"volatile int {variable_name} = {magic_int};"

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(".*movl.*\\$1234123([0-9]+),.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1


class GlobalVolatileIntStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Global Volatile Int"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        magic_int = f"1234123{id}"
        return f"{variable_name} = {magic_int};\n volatile int {variable_name};"

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(".*movl.*\\$1234123([0-9]+),.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1


class GlobalIntStrategy(MarkerStrategy):
    @staticmethod
    def name() -> str:
        return "Global Int"

    @staticmethod
    def make_preprocessor_code(marker: DCEMarker) -> str:
        id = marker.marker[len(marker.prefix) : -1]
        variable_name = f"DCE_JUNK_VAR{id}_"
        magic_int = f"1234123{id}"
        return f"{variable_name} = {magic_int};\n int {variable_name};"

    @staticmethod
    def marker_detection_regex() -> Pattern[str]:
        return re.compile(".*movl.*\\$1234123([0-9]+),.*")

    @staticmethod
    def regex_marker_id_group_index() -> int:
        return 1
