import shlex
import subprocess
from itertools import chain
from pathlib import Path
from typing import Optional

from dead_instrumenter.utils import Binary, find_binary


def find_include_paths(clang: str, file: Path, flags: list[str]) -> list[str]:
    cmd = [clang, str(file), "-c", "-o/dev/null", "-v", *flags]
    result = subprocess.run(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True
    )
    assert result.returncode == 0
    output = result.stdout.decode("utf-8").split("\n")
    start = (
        next(
            i
            for i, line in enumerate(output)
            if "#include <...> search starts here:" in line
        )
        + 1
    )
    end = next(i for i, line in enumerate(output) if "End of search list." in line)
    return [output[i].strip() for i in range(start, end)]


def instrument_program(
    file: Path,
    flags: list[str] = [],
    emit_disable_macros: bool = False,
    instrumenter: Optional[Path] = None,
    clang: Optional[Path] = None,
) -> str:
    """Instrument a given file i.e. put markers in the file.

    Args:
        file (Path): Path to code file to be instrumented.
        flags (list[str]): list of user provided clang flags
        emit_disable_macros (bool): Whether to include disabling macros in the
        instrumented program
        instrumenter (Path): Path to the instrumenter executable., if not
                             provided will use what's specified in
        clang (Path): Path to the clang executable.
    Returns:
        str: Marker prefix. Here: 'DCEMarker'
    """

    instrumenter_resolved = (
        find_binary(Binary.INSTRUMENTER) if not instrumenter else instrumenter
    )

    clang_resolved = find_binary(Binary.CLANG) if not clang else str(clang)
    includes = find_include_paths(clang_resolved, file, flags)

    cmd = [str(instrumenter_resolved), str(file)]
    for path in includes:
        cmd.append(f"--extra-arg=-isystem{str(path)}")
    if emit_disable_macros:
        cmd.append("--emit-disable-macros")
    cmd.append("--")

    subprocess.run(cmd, capture_output=True, check=True)

    return "DCEMarker"


def annotate_with_static(
    file: Path,
    flags: list[str] = [],
    instrumenter: Optional[Path] = None,
    clang: Optional[Path] = None,
) -> None:
    """Turn all globals in the given file into static globals.

    Args:
        file (Path): Path to code file to be instrumented.
        flags (list[str]): list of user provided clang flags
        instrumenter (Path): Path to the instrumenter executable., if not
                             provided will use what's specified in
        clang (Path): Path to the clang executable.
    Returns:
        None:
    """

    instrumenter_resolved = (
        find_binary(Binary.INSTRUMENTER) if not instrumenter else instrumenter
    )

    clang_resolved = find_binary(Binary.CLANG) if not clang else str(clang)
    includes = find_include_paths(clang_resolved, file, flags)

    cmd = [str(instrumenter_resolved), "--mode", "globals", str(file)]
    for path in includes:
        cmd.append(f"--extra-arg=-isystem{str(path)}")
    cmd.append("--")

    subprocess.run(cmd, capture_output=True, check=True)

    return
