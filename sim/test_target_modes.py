"""Compile and run 3/5-target C policy tests using Windows-native GCC.

PowerShell:
    python sim/test_target_modes.py
"""
from __future__ import annotations

from pathlib import Path
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "sim" / "build" / (
    "test_target_modes.exe" if sys.platform == "win32" else "test_target_modes"
)

def main():
    compiler = os.environ.get("CC", "gcc")
    if not shutil.which(compiler):
        raise SystemExit(f"Missing native compiler: {compiler}")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    command = [
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-Isrc/header",
        "src/target_selector.c", "src/target_tracker.c",
        "tests/test_target_modes.c",
        "-o", str(OUTPUT),
    ]
    print("[compile] " + " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, check=True)
    print("[run] " + str(OUTPUT), flush=True)
    subprocess.run([str(OUTPUT)], cwd=ROOT, check=True)

if __name__ == "__main__":
    main()
