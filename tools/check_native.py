#!/usr/bin/env python3
"""Exercise overflow tracking without Unity binaries or an Android device."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        raise RuntimeError("A C++20 compiler is required")
    with tempfile.TemporaryDirectory(prefix="legacy-tracker-test-") as folder:
        binary = Path(folder) / "tracker-test"
        subprocess.run(
            [
                compiler,
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                str(ROOT / "tests/overflow_tracker_test.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
    print("Overflow tracking: sparse regions, ownership and address reuse verified")


if __name__ == "__main__":
    main()
