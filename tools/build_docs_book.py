#!/usr/bin/env python3
"""Prepare mdBook sources from canonical YLang documentation.

The repository keeps technical references in docs/*.md as the single source
of truth. Arabic cross-language links are rewritten in an ignored build copy,
never in the canonical Arabic chapter sources.
"""

from __future__ import annotations

from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
BOOK_ROOT = DOCS / "online-book"
BOOK_SRC = BOOK_ROOT / "src"
REFERENCE_DIR = BOOK_SRC / "reference"
ARABIC_BUILD_SRC = BOOK_ROOT / "build-src" / "ar"
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
    "CHANGELOG-2.2.0.md",
    "ACCEPTANCE-2.2.0.md",
    "HTTP.md",
    "HTTP-STREAMING.md",
    "JSON.md",
    "CHANGELOG-2.3.0.md",
    "ROADMAP-2.3.0.md",
    "PERFORMANCE.md",
    "CLI.md",
    "TERMINAL.md",
    "SECURITY.md",
    "DEBUGGER.md",
    "DEBUGGER-SPEC.md",
    "ROADMAP-2.2.0.md",
    "STABLE-RELEASE-CHECKLIST.md",
    "CHANGELOG-2.0-BETA.md",
    "BETA-RELEASE-CHECKLIST.md",
    "RELEASE-CHECKLIST-2.3.0.md",
    "POST-RELEASE-CHECKLIST-2.3.0.md",
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


def prepare_arabic_build_source() -> int:
    """Create a normalized Arabic build tree without modifying canonical chapters."""
    arabic_source = BOOK_SRC / "ar"
    if not arabic_source.is_dir():
        raise SystemExit(f"required Arabic book source is missing: {arabic_source.relative_to(ROOT)}")

    if ARABIC_BUILD_SRC.exists():
        shutil.rmtree(ARABIC_BUILD_SRC)
    ARABIC_BUILD_SRC.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(arabic_source, ARABIC_BUILD_SRC)

    base = "https://yassine-jemi01.github.io/ylang/learn"
    replacements = {
        "../index.html": f"{base}/index.html",
        "../ylanglings.md": f"{base}/ylanglings.html",
    }
    changed = 0
    for chapter in ARABIC_BUILD_SRC.glob("*.md"):
        original = chapter.read_text(encoding="utf-8")
        content = original
        for old, new in replacements.items():
            content = content.replace(old, new)
        content = re.sub(
            r"\]\(\.\./reference/([^)]+)\.md(#[^)]*)?\)",
            lambda match: f"]({base}/reference/{match.group(1)}.html{match.group(2) or ''})",
            content,
        )
        if content != original:
            chapter.write_text(content, encoding="utf-8")
            changed += 1
    return changed


def main() -> int:
    REFERENCE_DIR.mkdir(parents=True, exist_ok=True)

    for name in REFERENCE_DOCS:
        source = DOCS / name
        if not source.is_file():
            raise SystemExit(f"required book reference is missing: {source.relative_to(ROOT)}")
        destination = REFERENCE_DIR / name
        content = source.read_text(encoding="utf-8")
        destination.write_text(rewrite_repo_relative_links(content), encoding="utf-8")

    print(f"Prepared {len(REFERENCE_DOCS)} canonical English reference chapters for mdBook.")
    changed = prepare_arabic_build_source()
    print(
        f"Prepared the Arabic build copy and normalized cross-language links in {changed} chapters; "
        "canonical Arabic source files were left unchanged."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
