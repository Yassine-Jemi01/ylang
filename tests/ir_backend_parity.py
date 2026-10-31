#!/usr/bin/env python3
"""Compare reference AST-to-C, verified IR-to-C, and optimized IR-to-C."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[1]
YLANG = ROOT / "build" / ("ylang.exe" if os.name == "nt" else "ylang")
FIXTURES = (
    "tests/emit-ir-c.yl",
    "tests/emit-mir-opt.yl",
    "tests/emit-mir-opt-cfg.yl",
    "tests/emit-ir-c-arithmetic.yl",
    "tests/emit-ir-c-div-zero.yl",
    "tests/emit-ir-c-div-overflow.yl",
    "tests/emit-ir-c-neg-overflow.yl",
    "tests/emit-ir-c-modulo-edge.yl",
    "tests/emit-ir-c-f64.yl",
    "tests/emit-ir-c-f64-ieee.yl",
    "tests/emit-ir-c-conversions.yl",
    "tests/emit-ir-c-conversions-overflow.yl",
    "tests/emit-ir-c-conversions-nonfinite.yl",
    "tests/emit-ir-c-opt-f64-div-zero.yl",
    "tests/emit-ir-c-short-circuit.yl",
    "tests/emit-mir-calls.yl",
    "tests/emit-mir-control-flow.yl",
    "tests/ir-module-calls/main.yl",
)


def fail(label: str, message: str, stdout: bytes = b"",
         stderr: bytes = b"") -> None:
    print(f"IR backend parity: {label}: FAIL: {message}", file=sys.stderr)
    if stdout:
        print(f"stdout: {stdout.decode(errors='replace')!r}", file=sys.stderr)
    if stderr:
        print(f"stderr: {stderr.decode(errors='replace')!r}", file=sys.stderr)
    raise SystemExit(1)


def run_checked(label: str, command: Sequence[str]) -> None:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, check=False)
    if result.returncode != 0:
        fail(label, f"command exited with status {result.returncode}",
             result.stdout, result.stderr)


def run_program(executable: Path) -> tuple[int, bytes, bytes]:
    result = subprocess.run([str(executable)], cwd=ROOT,
                            capture_output=True, check=False)
    return result.returncode, result.stdout, result.stderr


def main() -> int:
    compiler_args = shlex.split(os.environ.get("CC", "cc"))
    if not compiler_args:
        compiler_args = ["cc"]
    compiler_name = " ".join(compiler_args).lower()
    ylang_cc = "clang" if "clang" in compiler_name else "gcc"
    executable_suffix = ".exe" if os.name == "nt" else ""

    if not YLANG.is_file():
        fail("setup", f"compiler binary not found: {YLANG}")

    with tempfile.TemporaryDirectory(prefix="ylang-ir-parity-") as temporary:
        work = Path(temporary)
        for index, fixture_name in enumerate(FIXTURES):
            fixture = ROOT / fixture_name
            if not fixture.is_file():
                fail(fixture_name, "fixture does not exist")

            stem = f"case-{index}"
            ast_executable = work / f"{stem}-ast{executable_suffix}"
            ir_source = work / f"{stem}-ir.c"
            ir_executable = work / f"{stem}-ir{executable_suffix}"
            optimized_ir_source = work / f"{stem}-ir-opt.c"
            optimized_ir_executable = work / f"{stem}-ir-opt{executable_suffix}"

            run_checked(
                fixture_name,
                [str(YLANG), "build", str(fixture), "-o",
                 str(ast_executable), "--cc", ylang_cc],
            )
            run_checked(
                fixture_name,
                [str(YLANG), "emit-ir-c", str(fixture), "-o",
                 str(ir_source)],
            )
            run_checked(
                fixture_name,
                [*compiler_args, "-std=c17", "-Wall", "-Wextra",
                 "-Wpedantic", str(ir_source), "-o", str(ir_executable)],
            )
            run_checked(
                fixture_name,
                [str(YLANG), "emit-ir-c-opt", str(fixture), "-o",
                 str(optimized_ir_source)],
            )
            run_checked(
                fixture_name,
                [*compiler_args, "-std=c17", "-Wall", "-Wextra",
                 "-Wpedantic", str(optimized_ir_source), "-o",
                 str(optimized_ir_executable)],
            )

            ast_result = run_program(ast_executable)
            ir_result = run_program(ir_executable)
            optimized_ir_result = run_program(optimized_ir_executable)
            if ast_result != ir_result:
                fail(
                    fixture_name,
                    "AST-to-C and IR-to-C results differ "
                    f"(AST status={ast_result[0]}, IR status={ir_result[0]})",
                    stdout=(b"AST: " + ast_result[1] + b"\nIR: " + ir_result[1]),
                    stderr=(b"AST: " + ast_result[2] + b"\nIR: " + ir_result[2]),
                )
            if ast_result != optimized_ir_result:
                fail(
                    fixture_name,
                    "AST-to-C and optimized IR-to-C results differ "
                    f"(AST status={ast_result[0]}, optimized IR status={optimized_ir_result[0]})",
                    stdout=(b"AST: " + ast_result[1] +
                            b"\nOptimized IR: " + optimized_ir_result[1]),
                    stderr=(b"AST: " + ast_result[2] +
                            b"\nOptimized IR: " + optimized_ir_result[2]),
                )
            print(f"IR backend parity: {fixture_name}: PASS")

    print(f"IR backend parity: {len(FIXTURES)} fixtures passed through all three paths")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
