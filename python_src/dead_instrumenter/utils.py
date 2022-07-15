from __future__ import annotations

import json
import os
import shlex
import shutil
import subprocess
from contextlib import contextmanager
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Iterator, Optional

from dead_instrumenter import TARGET_VERSION


class DeadInstrumenterConfigError(Exception):
    pass


class DeadInstrumenterVersionError(Exception):
    pass


def check_executable(path: str) -> None:
    if not shutil.which(path):
        raise DeadInstrumenterConfigError(f"{path} does not exist or is not executable")


def check_version(dead_path: str) -> None:
    version = (
        subprocess.run(shlex.split(f"{dead_path} --version"), capture_output=True)
        .stdout.decode("utf-8")
        .strip()
    )
    if version != TARGET_VERSION:
        raise DeadInstrumenterVersionError(
            f"Expected {TARGET_VERSION} but found {version}"
        )


class Config:
    def __init__(self, instrumenter: str, clang: str) -> None:
        self.instrumenter_path: str = instrumenter
        self.clang_path: str = clang
        self.validate()

    @staticmethod
    def from_dict(fields: dict[str, str]) -> Config:
        config = Config(fields["instrumenter_path"], fields["clang_path"])
        config.validate()
        return config

    def to_dict(self) -> dict[str, str]:
        return {
            "instrumenter_path": self.instrumenter_path,
            "clang_path": self.clang_path,
        }

    def validate(self) -> None:
        check_executable(self.instrumenter_path)
        check_executable(self.clang_path)

    @staticmethod
    def load(config_path: Path) -> Config:
        assert config_path.exists()
        with open(str(config_path), "r") as f:
            return Config.from_dict(json.load(f))

    def store(self, config_path: Path) -> None:
        config_path.parent.mkdir(parents=True, exist_ok=True)
        with open(str(config_path), "w") as f:
            json.dump(self.to_dict(), f)


def binary_question(txt: str) -> bool:
    answer = ""
    while answer not in ["y", "n"]:
        answer = input(txt + " [y/n] ")
    return answer == "y"


@contextmanager
def pushd(new_dir: str) -> Iterator[None]:
    previous_dir = os.getcwd()
    os.chdir(new_dir)
    try:
        yield
    finally:
        os.chdir(previous_dir)


def download_and_build(no_questions: bool = False) -> str:
    print("Downloanding and building dead-instrumenter")
    print(f"It will be installed under {Path.home()/'.local/bin'}")
    if not no_questions and not binary_question(
        "Build Prequisites: cmake, make, clang/llvm 13/14, git. Are they satisfied?"
    ):
        raise DeadInstrumenterConfigError(
            "Please install the necessary build prerequisites."
        )

    with TemporaryDirectory() as tdir:
        with pushd(tdir):
            subprocess.run(
                shlex.split(
                    f"git clone --depth 1 --single-branch --branch {TARGET_VERSION}"
                    " https://github.com/DeadCodeProductions/dead_instrumenter.git"
                ),
                check=True,
            )
            os.mkdir("dead_instrumenter/build")
            os.chdir("dead_instrumenter/build")
            subprocess.run(shlex.split("cmake .."), check=True)
            prefix = Path().home() / ".local"
            subprocess.run(shlex.split("cmake --build . --parallel"), check=True)
            subprocess.run(
                shlex.split(f"cmake --install . --prefix={str(prefix)}"),
                check=True,
            )
            return str(prefix / "bin/dead-instrument")


def make_config(config_path: Path, no_questions: bool = False) -> Config:
    assert not config_path.exists()
    print(f"{config_path} does not exist")
    if (not no_questions) and not binary_question("Shall I create one?"):
        raise DeadInstrumenterConfigError("No config for dead-instrumenter available")

    clang = (
        None
        if no_questions
        else input(
            "Path to clang? (leave blank to use whatever is available): "
        ).strip()
    )
    if not clang:
        clang_default = shutil.which("clang")
        clang = clang_default if clang_default else ""
    check_executable(clang)

    instrumenter = (
        None
        if no_questions
        else input(
            "Path to dead-instrumenter? (leave blank to download and build): "
        ).strip()
    )
    if not instrumenter:
        instrumenter = download_and_build(no_questions=no_questions)
    check_executable(instrumenter)

    config = Config(instrumenter, clang)
    config.store(config_path)
    return config


class Binary(Enum):
    INSTRUMENTER = 1
    CLANG = 2


def find_binary(binary: Binary, no_questions: bool = False) -> str:
    """Looks up a binary in $home/.config/dead/instrumenter.json
    if the config file does not exists, the user is prompted
    Args:
        name (str): the name of the binary to find: clang or dead-instrumenter
    Returns:
        str: the full path to that binary
    """

    config_path = Path.home() / ".config/dead/instrumenter.json"
    if config_path.exists():
        config = Config.load(config_path)
    else:
        config_path = Path.home() / ".config/dead/instrumenter.json"
        config = make_config(config_path, no_questions)

    if binary == Binary.CLANG:
        return config.clang_path
    else:
        assert binary == Binary.INSTRUMENTER
        try:
            check_version(config.instrumenter_path)
        except DeadInstrumenterVersionError as E:
            print(
                f"{E}, update or delete {config_path} and/or {config.instrumenter_path}"
            )
            exit(1)

        return config.instrumenter_path
