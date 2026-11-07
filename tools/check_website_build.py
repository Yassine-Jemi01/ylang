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
        self.references: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        for attribute in ("href", "src", "poster"):
            value = values.get(attribute)
            if value:
                self.references.append(value)
        srcset = values.get("srcset")
        if srcset:
            for candidate in srcset.split(","):
                url = candidate.strip().split()
                if url:
                    self.references.append(url[0])


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

    english_book_path = root / "learn" / "index.html"
    arabic_book_path = root / "learn" / "ar" / "index.html"
    english_book = english_book_path.read_text(encoding="utf-8")
    arabic_book = arabic_book_path.read_text(encoding="utf-8")
    if 'lang="en"' not in english_book or 'dir="ltr"' not in english_book:
        fail("English book root language or direction is incorrect")
    if 'lang="ar"' not in arabic_book or 'dir="rtl"' not in arabic_book:
        fail("Arabic book root language or direction is incorrect")
    if "المسار العربي — تعلّم YLang" in english_book:
        fail("Arabic chapters leaked into the English book navigation")
    if "Part I — Start Here" in arabic_book:
        fail("English chapters leaked into the Arabic book navigation")

    nav_script = (root / "learn" / "theme" / "ylang-nav.js")
    if not nav_script.is_file():
        fail("the shared book navigation script is missing")
    nav_source = nav_script.read_text(encoding="utf-8")
    for required_mapping in ("'start-here.html': 'index.html'", "'index.html': 'start-here.html'"):
        if required_mapping not in nav_source:
            fail(f"book language switching is missing its home-page mapping: {required_mapping}")

    if "relativeBookPath().startsWith('ar/') ? ARABIC_DOCS_URL : HOME_URL" not in nav_source:
        fail("Arabic book home link does not preserve the Arabic documentation route")

    arabic_book_css = (root / "learn" / "css" / "ylang-book.css")
    if not arabic_book_css.is_file():
        fail("the shared book stylesheet is missing")
    arabic_css_source = arabic_book_css.read_text(encoding="utf-8")
    if ".ylang-ar #sidebar .chapter" not in arabic_css_source or "direction: rtl" not in arabic_css_source:
        fail("Arabic chapter navigation is missing its RTL layout rules")

    arabic_portal = (root / "docs-ar.html").read_text(encoding="utf-8")
    if 'href="https://yassine-jemi01.github.io/ylang/"' in arabic_portal:
        fail("Arabic documentation portal contains a homepage link that switches to English")

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
        for reference in parser.references:
            parsed = urlsplit(reference)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            target_path = unquote(parsed.path)
            if not target_path:
                continue
            target = (root / target_path.lstrip("/")) if target_path.startswith("/") else (page_path.parent / target_path)
            target = target.resolve()
            if target.is_dir():
                target = target / "index.html"
            if not target.exists():
                broken.append(f"{page_path.relative_to(root)} -> {reference}")
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
