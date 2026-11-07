#!/usr/bin/env python3
"""Generate and test the Tree-sitter grammar without touching compiler sources.

`tree-sitter generate` writes `src/parser.c` into the current grammar
directory. In this repository that path is the YLang compiler's own
hand-written parser, so generating at the repository root would overwrite it.
This helper copies the grammar into a temporary directory, runs generation and
the corpus tests there, and leaves the checkout untouched.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GRAMMAR_PATHS = (
    "grammar.js",
    "tree-sitter.json",
    "package.json",
)
CORPUS_DIR = "test"


def run(command: list[str], cwd: Path) -> int:
    print(f"+ {' '.join(command)} (in {cwd})", flush=True)
    try:
        return subprocess.run(command, cwd=cwd, check=False).returncode
    except FileNotFoundError:
        print(f"error: '{command[0]}' was not found on PATH", file=sys.stderr)
        return 127


def main() -> int:
    missing = [name for name in GRAMMAR_PATHS if not (ROOT / name).is_file()]
    corpus = ROOT / CORPUS_DIR / "corpus"
    if not corpus.is_dir():
        missing.append(str(corpus.relative_to(ROOT)))
    if missing:
        print(
            "error: missing Tree-sitter input(s): " + ", ".join(missing),
            file=sys.stderr,
        )
        return 1

    with tempfile.TemporaryDirectory(prefix="ylang-tree-sitter-") as temporary:
        workspace = Path(temporary)
        for name in GRAMMAR_PATHS:
            shutil.copy2(ROOT / name, workspace / name)
        shutil.copytree(ROOT / CORPUS_DIR, workspace / CORPUS_DIR)

        status = run(["tree-sitter", "generate"], workspace)
        if status != 0:
            print("error: tree-sitter generate failed", file=sys.stderr)
            return status
        if not (workspace / "src" / "parser.c").is_file():
            print("error: tree-sitter generate produced no src/parser.c", file=sys.stderr)
            return 1

        status = run(["tree-sitter", "test"], workspace)
        if status != 0:
            print("error: tree-sitter corpus tests failed", file=sys.stderr)
            return status

    print("Tree-sitter grammar generated and corpus tests passed in isolation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
