#!/usr/bin/env python3
"""Build both YLang book editions and assemble the exact GitHub Pages site."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BOOK_ROOT = ROOT / "docs" / "online-book"
ENGLISH_BOOK = BOOK_ROOT / "book"
ARABIC_BOOK = BOOK_ROOT / "book-ar"
ARABIC_BUILD_SOURCE = BOOK_ROOT / "build-src" / "ar"
WEBSITE_SOURCE = ROOT / "website"
HOMEPAGE_DIST = ROOT / "website-app" / "dist"


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def remove_generated(path: Path) -> None:
    if path.is_symlink():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def copy_theme_assets(destination: Path) -> None:
    theme = destination / "theme"
    theme.mkdir(parents=True, exist_ok=True)
    shutil.copy2(BOOK_ROOT / "theme" / "favicon.svg", destination / "favicon.svg")
    shutil.copy2(BOOK_ROOT / "theme" / "ylang-nav.js", theme / "ylang-nav.js")
    shutil.copy2(BOOK_ROOT / "theme" / "ylang-book.css", theme / "ylang-book.css")


def ensure_book_favicons(learn_root: Path) -> None:
    """Give every generated mdBook HTML page the same local project favicon.

    mdBook's special toc.html template does not include the ordinary page head
    partial, so its icon declaration is added after rendering. The path is
    relative to each page, keeping both books usable below GitHub project Pages.
    """
    favicon_pattern = re.compile(
        r"<link\b(?=[^>]*\brel\s*=\s*['\"][^'\"]*\bicon\b[^'\"]*['\"])[^>]*>",
        re.IGNORECASE,
    )
    for page in learn_root.rglob("*.html"):
        relative = page.relative_to(learn_root)
        book_root = learn_root / "ar" if relative.parts[0] == "ar" else learn_root
        content = page.read_text(encoding="utf-8")
        if favicon_pattern.search(content):
            continue
        closing_head = re.search(r"</head\s*>", content, re.IGNORECASE)
        if closing_head is None:
            raise SystemExit(f"generated book page has no </head>: {page.relative_to(learn_root)}")
        favicon = os.path.relpath(book_root / "favicon.svg", page.parent).replace(os.sep, "/")
        tag = f'<link rel="icon" href="{favicon}" type="image/svg+xml">\n'
        content = content[:closing_head.start()] + tag + content[closing_head.start():]
        page.write_text(content, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output",
        nargs="?",
        default="site-dist",
        help="assembled output directory (default: site-dist)",
    )
    args = parser.parse_args()

    output = Path(args.output)
    output = output.resolve() if output.is_absolute() else (ROOT / output).resolve()
    if output == ROOT or ROOT not in output.parents:
        parser.error("output must be a directory inside the repository")

    if not HOMEPAGE_DIST.is_dir() or not (HOMEPAGE_DIST / "index.html").is_file():
        parser.error("homepage build is missing; run npm ci and npm run build in website-app first")
    if not (HOMEPAGE_DIST / "ylang-mark.svg").is_file():
        parser.error("homepage build is missing the canonical ylang-mark.svg")

    # Remove stale outputs so deleted chapters or assets cannot leak into a deployment.
    for generated in (ENGLISH_BOOK, ARABIC_BOOK, BOOK_ROOT / "build-src", output):
        remove_generated(generated)

    run([sys.executable, "tools/build_docs_book.py"])
    run(["mdbook", "build", "docs/online-book"])
    if not ENGLISH_BOOK.is_dir():
        raise SystemExit("mdBook did not create the English book output")

    shutil.copytree(
        WEBSITE_SOURCE,
        output,
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("learn"),
    )
    shutil.copytree(HOMEPAGE_DIST, output, dirs_exist_ok=True)
    learn_root = output / "learn"
    shutil.copytree(ENGLISH_BOOK, learn_root)

    arabic_env = os.environ.copy()
    arabic_env.update(
        {
            "MDBOOK_BOOK__SRC": "build-src/ar",
            "MDBOOK_BOOK__LANGUAGE": "ar",
            "MDBOOK_BOOK__TEXT_DIRECTION": "rtl",
            "MDBOOK_BOOK__TITLE": "كتاب YLang",
            "MDBOOK_BOOK__DESCRIPTION": "المسار التعليمي العربي للغة YLang.",
            "MDBOOK_BUILD__BUILD_DIR": "book-ar",
            "MDBOOK_OUTPUT__HTML__SITE_URL": "/ylang/learn/ar/",
        }
    )
    if not ARABIC_BUILD_SOURCE.is_dir():
        raise SystemExit("Arabic build source was not prepared by tools/build_docs_book.py")
    run(["mdbook", "build", "docs/online-book"], env=arabic_env)
    if not ARABIC_BOOK.is_dir():
        raise SystemExit("mdBook did not create the Arabic book output")

    remove_generated(learn_root / "ar")
    shutil.copytree(ARABIC_BOOK, learn_root / "ar")
    copy_theme_assets(learn_root)
    copy_theme_assets(learn_root / "ar")
    ensure_book_favicons(learn_root)

    run([sys.executable, "tools/build_sitemap.py", str(output)])
    run([sys.executable, "tools/check_website_build.py", str(output)])
    print(f"Complete bilingual website assembled and verified at {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
