from pathlib import Path

from diopter.compiler import CompilationSetting, CompilerExe, OptLevel


def get_system_gcc_O0() -> CompilationSetting:
    exe = CompilerExe.from_path(Path("gcc"))
    return CompilationSetting(
        compiler=exe,
        opt_level=OptLevel.O0,
        flags=(),
        include_paths=(),
        system_include_paths=(),
    )


def get_system_gcc_O3() -> CompilationSetting:
    exe = CompilerExe.from_path(Path("gcc"))
    return CompilationSetting(
        compiler=exe,
        opt_level=OptLevel.O3,
        flags=(),
        include_paths=(),
        system_include_paths=(),
    )
