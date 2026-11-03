#!/usr/bin/env python3
"""Regression tests for bounded compiler input and safe diagnostics."""
from pathlib import Path
import os
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
COMPILER = BUILD / ("ylang.exe" if os.name == "nt" else "ylang")


def run_check(source: Path, timeout: float = 10.0) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(COMPILER), "check", str(source.relative_to(ROOT))],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )


def make_nested_program(depth: int) -> bytes:
    return (
        b"function main() -> int {\n"
        + b"if (true) {\n" * depth
        + b"return 0;\n"
        + b"}\n" * depth
        + b"return 0;\n"
        + b"}\n"
    )


def make_import_chain(directory: Path, count: int) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    for index in range(count):
        if index + 1 < count:
            imported = f'from "./m{index + 1}.yl" import f{index + 1};\n'
            body = f"function f{index}() -> int {{ return f{index + 1}(); }}\n"
        else:
            imported = ""
            body = f"function f{index}() -> int {{ return 0; }}\n"
        (directory / f"m{index}.yl").write_text(imported + body, encoding="utf-8")
    root = directory / "root.yl"
    root.write_text(
        'from "./m0.yl" import f0;\n'
        "function main() -> int { return f0(); }\n",
        encoding="utf-8",
    )
    return root


def main() -> None:
    BUILD.mkdir(exist_ok=True)

    valid_nested = BUILD / "security-nesting-valid.yl"
    valid_nested.write_bytes(make_nested_program(100))
    valid_result = run_check(valid_nested)
    if valid_result.returncode != 0:
        raise SystemExit("100 nested statements should remain valid:\n" +
                         valid_result.stderr.decode("utf-8", "replace"))

    deep_nested = BUILD / "security-nesting-deep.yl"
    deep_nested.write_bytes(make_nested_program(2048))
    deep_result = run_check(deep_nested)
    if deep_result.returncode != 1 or b"E1018" not in deep_result.stderr:
        raise SystemExit(
            f"deep statement nesting was not rejected safely (exit={deep_result.returncode}):\n"
            + deep_result.stderr.decode("utf-8", "replace")
        )

    valid_chain = make_import_chain(BUILD / "security-imports-valid", 100)
    valid_import_result = run_check(valid_chain, timeout=20.0)
    if valid_import_result.returncode != 0:
        raise SystemExit("100-module import chain should succeed:\n" +
                         valid_import_result.stderr.decode("utf-8", "replace"))
    deep_chain = make_import_chain(BUILD / "security-imports-deep", 300)
    deep_import_result = run_check(deep_chain, timeout=20.0)
    if deep_import_result.returncode != 1 or b"E1027" not in deep_import_result.stderr:
        raise SystemExit(
            f"deep module chain was not rejected safely (exit={deep_import_result.returncode}):\n"
            + deep_import_result.stderr.decode("utf-8", "replace")
        )

    noisy = BUILD / "security-diagnostic-amplification.yl"
    noisy.write_bytes(
        b"function main() -> int {\n    " + b"\x01" * 5000 +
        b"\n    return 0;\n}\n"
    )
    started = time.monotonic()
    noisy_result = run_check(noisy, timeout=5.0)
    elapsed = time.monotonic() - started
    if noisy_result.returncode != 1:
        raise SystemExit("invalid-byte source should fail with a compiler diagnostic")
    if len(noisy_result.stderr) >= 1024 * 1024:
        raise SystemExit(f"diagnostic output was not bounded ({len(noisy_result.stderr)} bytes)")
    if b"E1099" not in noisy_result.stderr or elapsed >= 5.0:
        raise SystemExit(
            f"diagnostic amplification limit was not applied (bytes={len(noisy_result.stderr)}, seconds={elapsed:.2f})"
        )

    hostile_source = (
        b"function main() -> int {\n"
        b"    \x1b]0;PWNED\x07\x1b[2J\xff let int value = true;\n"
        b"    return 0;\n"
        b"}\n"
    )
    hostile_path = BUILD / "security-diag-source.yl"
    hostile_path.write_bytes(hostile_source)
    hostile_result = run_check(hostile_path)
    if hostile_result.returncode != 1:
        raise SystemExit("hostile diagnostic source should be rejected")
    for raw in (b"\x1b", b"\x07", b"\xff", b"\r"):
        if raw in hostile_result.stderr:
            raise SystemExit(f"raw terminal/control byte leaked into diagnostics: {raw!r}")

    if os.name != "nt":
        injected_name = BUILD / "security-name-\x1b]0;NAME\x07.yl"
        injected_name.write_bytes(b"function main() -> int { let int value = true; return 0; }\n")
        name_result = run_check(injected_name)
        if name_result.returncode != 1:
            raise SystemExit("malformed program with hostile filename should fail")
        for raw in (b"\x1b", b"\x07", b"\r"):
            if raw in name_result.stderr:
                raise SystemExit(f"raw filename control byte leaked into diagnostics: {raw!r}")
        if b"\\x1B" not in name_result.stderr:
            raise SystemExit("hostile filename did not appear as an escaped path")

    print("Compiler security regression tests passed.")


if __name__ == "__main__":
    main()
