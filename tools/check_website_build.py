#!/usr/bin/env python3
"""Smoke-test the assembled YLang website before publishing GitHub Pages."""

from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
import sys
from urllib.parse import unquote, urlsplit
from xml.etree import ElementTree as ET


class LinkParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.hrefs: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag not in {"a", "link", "area"}:
            return
        for key, value in attrs:
            if key == "href" and value:
                self.hrefs.append(value)


BASE_URL = "https://yassine-jemi01.github.io/ylang/"


def fail(message: str) -> None:
    raise SystemExit(f"Website smoke test failed: {message}")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "site-dist").resolve()
    required = (
        "index.html",
        "docs.html",
        "docs-ar.html",
        "learn/index.html",
        "learn/ar/index.html",
        "learn/ar/start-here.html",
        "ylang-mark.svg",
        "assets/ylang-mark.svg",
        "assets/site-theme.css",
        "robots.txt",
        "sitemap.xml",
        "learn/favicon.svg",
        "learn/ar/favicon.svg",
    )
    for relative in required:
        if not (root / relative).is_file():
            fail(f"required output is missing: {relative}")

    canonical = (root / "ylang-mark.svg").read_bytes()
    for relative in ("assets/ylang-mark.svg", "learn/favicon.svg", "learn/ar/favicon.svg"):
        if (root / relative).read_bytes() != canonical:
            fail(f"favicon does not match the canonical YLang mark: {relative}")

    if (root / "progress" / "index.html").exists():
        fail("the retired YLang 3.0 progress page is still being published")

    sitemap = ET.parse(root / "sitemap.xml").getroot()
    sitemap_urls = {
        element.text for element in sitemap.iter()
        if element.tag.endswith("}loc") and element.text
    }
    for expected in (BASE_URL, BASE_URL + "docs.html", BASE_URL + "docs-ar.html", BASE_URL + "learn/"):
        if expected not in sitemap_urls:
            fail(f"sitemap is missing a public page: {expected}")
    if any("/progress/" in url for url in sitemap_urls):
        fail("the retired progress page appears in the sitemap")
    if "Sitemap: " + BASE_URL + "sitemap.xml" not in (root / "robots.txt").read_text(encoding="utf-8"):
        fail("robots.txt does not advertise the generated sitemap")

    english_book = (root / "learn" / "index.html").read_text(encoding="utf-8")
    arabic_book = (root / "learn" / "ar" / "index.html").read_text(encoding="utf-8")
    if "المسار العربي — تعلّم YLang" in english_book:
        fail("Arabic chapters leaked into the English book navigation")
    if "Part I — Start Here" in arabic_book:
        fail("English chapters leaked into the Arabic book navigation")

    for relative in ("docs.html", "docs-ar.html"):
        page = (root / relative).read_text(encoding="utf-8")
        if "assets/site-theme.css" not in page:
            fail(f"shared visual system is not linked from {relative}")

    broken: list[str] = []
    for page_path in root.rglob("*.html"):
        parser = LinkParser()
        try:
            parser.feed(page_path.read_text(encoding="utf-8"))
        except UnicodeDecodeError:
            fail(f"HTML is not UTF-8: {page_path.relative_to(root)}")
        for href in parser.hrefs:
            parsed = urlsplit(href)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            target_path = unquote(parsed.path)
            target = (root / target_path.lstrip("/")) if target_path.startswith("/") else (page_path.parent / target_path)
            target = target.resolve()
            if target.is_dir():
                target = target / "index.html"
            if not target.exists():
                broken.append(f"{page_path.relative_to(root)} -> {href}")
                if len(broken) >= 25:
                    break
        if len(broken) >= 25:
            break

    if broken:
        fail("broken local links detected:\n  " + "\n  ".join(broken))

    print("Website smoke test passed: language split, shared theme, favicons, retired route, and local links.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
