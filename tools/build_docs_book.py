#!/usr/bin/env python3
"""Prepare mdBook reference pages from the canonical YLang docs.

The repository keeps documentation in docs/*.md as the single source of truth.
This script copies the public reference set into the mdBook source tree before
rendering, and rewrites repository-root-relative Markdown links to GitHub links
so source links remain useful from the generated book.
"""

from __future__ import annotations

from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
BOOK_SRC = DOCS / "online-book" / "src"
REFERENCE_DIR = BOOK_SRC / "reference"
REPOSITORY_URL = "https://github.com/Yassine-Jemi01/ylang"

REFERENCE_DOCS = (
    "language-spec.md",
    "architecture.md",
    "error-codes.md",
    "MODULE-SYSTEM-SPEC.md",
    "STRUCTS.md",
    "ENUMS.md",
    "GUI-API-SPEC.md",
    "GAME2D.md",
    "WINDOWS.md",
    "YSPM.md",
    "BACKEND-ROADMAP.md",
    "OPTION-RESULT-DESIGN.md",
    "YLANG-2.0-DESIGN.md",
    "CHANGELOG-2.0.0.md",
    "MEMORY-MODEL.md",
    "CHANGELOG-2.1.0.md",
    "HTTP.md",
    "DEBUGGER.md",
    "DEBUGGER-SPEC.md",
    "ROADMAP-2.2.0.md",
    "STABLE-RELEASE-CHECKLIST.md",
    "CHANGELOG-2.0-BETA.md",
    "BETA-RELEASE-CHECKLIST.md",
)

LINK = re.compile(r"\]\(([^)]+)\)")


def rewrite_repo_relative_links(content: str) -> str:
    """Turn links such as ../examples/hello.yl into stable GitHub source links."""

    def replace(match: re.Match[str]) -> str:
        target = match.group(1)
        if not target.startswith("../"):
            return match.group(0)

        path, marker, fragment = target.partition("#")
        queryless_path = path.partition("?")[0]
        repo_relative = queryless_path[3:]
        local_target = (ROOT / repo_relative).resolve()
        try:
            local_target.relative_to(ROOT)
        except ValueError:
            return match.group(0)

        if not local_target.exists():
            return match.group(0)

        kind = "tree" if local_target.is_dir() else "blob"
        suffix = f"#{fragment}" if marker else ""
        return f"]({REPOSITORY_URL}/{kind}/main/{repo_relative}{suffix})"

    return LINK.sub(replace, content)


def main() -> int:
    REFERENCE_DIR.mkdir(parents=True, exist_ok=True)

    for name in REFERENCE_DOCS:
        source = DOCS / name
        if not source.is_file():
            raise SystemExit(f"required book reference is missing: {source.relative_to(ROOT)}")
        destination = REFERENCE_DIR / name
        content = source.read_text(encoding="utf-8")
        destination.write_text(rewrite_repo_relative_links(content), encoding="utf-8")

    print(f"Prepared {len(REFERENCE_DOCS)} canonical reference chapters for mdBook.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
