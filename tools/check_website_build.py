#!/usr/bin/env python3
"""Validate the fully assembled YLang GitHub Pages website."""

from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
import re
import sys
from urllib.parse import quote, unquote, urlsplit
from xml.etree import ElementTree as ET

BASE_URL = "https://yassine-jemi01.github.io/ylang/"
PROJECT_PREFIX = "/ylang"
CSS_URL = re.compile(r"url\(\s*(['\"]?)(.*?)\1\s*\)", re.IGNORECASE)
MAP_ENTRY = re.compile(r"'([^']+)'\s*:\s*'([^']+)'")

APP_ROUTES = (
    "learn/index.html",
    "docs.html",
    "docs-ar.html",
    "learn/reference/HTTP.html",
    "learn/reference/JSON.html",
    "learn/reference/DEBUGGER.html",
    "learn/reference/CLI.html",
    "learn/reference/CHANGELOG-2.3.0.html",
    "learn/reference/SECURITY.html",
    "learn/reference/PERFORMANCE.html",
    "learn/ylanglings.html",
)


class LinkParser(HTMLParser):
    """Collect local references, element IDs, and document language metadata."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.references: list[str] = []
        self.favicons: list[str] = []
        self.ids: list[str] = []
        self.language: str | None = None
        self.direction: str | None = None

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "html":
            self.language = values.get("lang")
            self.direction = values.get("dir")
        element_id = values.get("id")
        if element_id:
            self.ids.append(element_id)
        for attribute in ("href", "src", "poster"):
            value = values.get(attribute)
            if value:
                self.references.append(value)
        if tag == "link":
            rel_tokens = set((values.get("rel") or "").lower().split())
            icon_href = values.get("href")
            if "icon" in rel_tokens and icon_href:
                self.favicons.append(icon_href)
        srcset = values.get("srcset")
        if srcset:
            for candidate in srcset.split(","):
                parts = candidate.strip().split()
                if parts:
                    self.references.append(parts[0])


def fail(message: str) -> None:
    raise SystemExit(f"Website integrity check failed: {message}")


def public_url(relative: str) -> str:
    if relative == "index.html":
        return BASE_URL
    if relative.endswith("/index.html"):
        return BASE_URL + quote(relative[:-len("index.html")], safe="/-._~")
    return BASE_URL + quote(relative, safe="/-._~")


def parse_mapping(source: str, name: str) -> dict[str, str]:
    match = re.search(rf"const {name}\s*=\s*\{{(.*?)\n  \}};", source, re.DOTALL)
    if not match:
        fail(f"book navigation is missing its {name} route map")
    return dict(MAP_ENTRY.findall(match.group(1)))


def resolve_local_target(reference_path: str, page: Path, root: Path) -> Path:
    decoded = unquote(reference_path)
    if decoded.startswith("/"):
        if decoded == PROJECT_PREFIX:
            decoded = ""
        elif decoded.startswith(PROJECT_PREFIX + "/"):
            decoded = decoded[len(PROJECT_PREFIX) + 1:]
        else:
            fail(
                f"{page.relative_to(root)} uses root-relative path {reference_path!r}; "
                "GitHub project Pages requires relative paths or the /ylang/ prefix"
            )
        target = root / decoded
    else:
        target = page.parent / decoded

    resolved = target.resolve()
    try:
        resolved.relative_to(root)
    except ValueError:
        fail(f"{page.relative_to(root)} link escapes the published site: {reference_path}")
    if resolved.is_dir():
        resolved = resolved / "index.html"
    return resolved


def main() -> int:
    root_arg = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("site-dist")
    root = root_arg.resolve()
    if not root.is_dir():
        fail(f"site output directory does not exist: {root}")

    required = (
        "index.html",
        "docs.html",
        "docs-ar.html",
        "learn/index.html",
        "learn/ar/index.html",
        "learn/ar/start-here.html",
        "learn/ylanglings.html",
        "ylang-mark.svg",
        "assets/ylang-mark.svg",
        "assets/favicon.svg",
        "assets/favicon-home.svg",
        "assets/site-theme.css",
        "robots.txt",
        "sitemap.xml",
        "learn/favicon.svg",
        "learn/ar/favicon.svg",
        "learn/theme/ylang-nav.js",
        "learn/ar/theme/ylang-nav.js",
        "learn/theme/ylang-book.css",
        "learn/ar/theme/ylang-book.css",
    )
    for relative in required:
        if not (root / relative).is_file():
            fail(f"required output is missing: {relative}")

    canonical_mark = (root / "ylang-mark.svg").read_bytes()
    for relative in (
        "assets/ylang-mark.svg",
        "assets/favicon.svg",
        "assets/favicon-home.svg",
        "learn/favicon.svg",
        "learn/ar/favicon.svg",
    ):
        if (root / relative).read_bytes() != canonical_mark:
            fail(f"favicon does not match the canonical YLang mark: {relative}")

    if (root / "progress" / "index.html").exists():
        fail("the retired YLang 3.0 progress page is still being published")

    html_pages = sorted(path for path in root.rglob("*.html") if path.is_file())
    if not html_pages:
        fail("no HTML pages were generated")
    parsed_pages: dict[Path, LinkParser] = {}
    for page in html_pages:
        try:
            content = page.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            fail(f"HTML is not UTF-8: {page.relative_to(root)}")
        parser = LinkParser()
        parser.feed(content)
        parsed_pages[page.resolve()] = parser

        relative = page.relative_to(root).as_posix()
        arabic = relative == "docs-ar.html" or relative.startswith("learn/ar/")
        expected_language, expected_direction = ("ar", "rtl") if arabic else ("en", "ltr")
        if parser.language != expected_language or parser.direction != expected_direction:
            fail(
                f"{relative} must declare lang={expected_language} "
                f"and dir={expected_direction}; got lang={parser.language!r}, dir={parser.direction!r}"
            )
        duplicates = sorted({value for value in parser.ids if parser.ids.count(value) > 1})
        if duplicates:
            fail(f"{relative} contains duplicate HTML IDs: {', '.join(duplicates)}")
        if not parser.favicons:
            fail(f"{relative} does not declare a favicon")
        for favicon in parser.favicons:
            parsed_icon = urlsplit(favicon)
            if parsed_icon.scheme or parsed_icon.netloc or not parsed_icon.path:
                fail(f"{relative} uses a non-local favicon: {favicon}")
            icon_path = resolve_local_target(parsed_icon.path, page, root)
            if not icon_path.is_file():
                fail(f"{relative} favicon file is missing: {favicon}")
            if icon_path.read_bytes() != canonical_mark:
                fail(f"{relative} uses a favicon that differs from the canonical YLang mark: {favicon}")

    homepage = (root / "index.html").read_text(encoding="utf-8")
    english_portal = (root / "docs.html").read_text(encoding="utf-8")
    arabic_portal = (root / "docs-ar.html").read_text(encoding="utf-8")
    if 'href="docs-ar.html"' not in english_portal:
        fail("English documentation portal does not link to the Arabic portal")
    if 'href="docs.html"' not in arabic_portal:
        fail("Arabic documentation portal does not link back to the English portal")
    if 'href="learn/ar/start-here.html"' not in arabic_portal:
        fail("Arabic documentation portal does not lead to the Arabic learning path")
    for page_name, source in (("docs.html", english_portal), ("docs-ar.html", arabic_portal)):
        for language in ('hreflang="en"', 'hreflang="ar"', 'hreflang="x-default"'):
            if language not in source:
                fail(f"{page_name} is missing its {language} alternate declaration")

    homepage_parser = parsed_pages[(root / "index.html").resolve()]
    homepage_scripts = [
        resolve_local_target(urlsplit(reference).path, root / "index.html", root)
        for reference in homepage_parser.references
        if urlsplit(reference).path.endswith(".js")
    ]
    if not homepage_scripts:
        fail("the built homepage does not reference its JavaScript application bundle")
    app_sources: list[str] = []
    for script_path in homepage_scripts:
        if not script_path.is_file():
            fail(f"homepage JavaScript bundle is missing: {script_path.relative_to(root)}")
        app_sources.append(script_path.read_text(encoding="utf-8"))
    app_bundle = "\n".join(app_sources)
    if "docs-ar.html" not in app_bundle:
        fail("homepage Arabic language selector does not link to docs-ar.html")
    if "learn/ar/index.html" in app_bundle:
        fail("homepage Arabic language selector still routes directly to the Arabic book index")
    for route in APP_ROUTES:
        if not (root / route).is_file():
            fail(f"homepage resource route has no generated page: {route}")
        if route not in app_bundle:
            fail(f"homepage resource route is missing from the production bundle: {route}")

    nav_source = (root / "learn" / "theme" / "ylang-nav.js").read_text(encoding="utf-8")
    if "const ARABIC_PORTAL_URL = HOME_URL + 'docs-ar.html';" not in nav_source:
        fail("Arabic book home link does not define the Arabic documentation portal")
    if "relativeBookPath().startsWith('ar/') ? ARABIC_PORTAL_URL : HOME_URL" not in nav_source:
        fail("Arabic book home link does not return to the Arabic documentation portal")
    ar_to_en = parse_mapping(nav_source, "AR_TO_EN")
    en_to_ar = parse_mapping(nav_source, "EN_TO_AR")
    for arabic_path, english_path in ar_to_en.items():
        if not (root / "learn" / "ar" / arabic_path).is_file():
            fail(f"Arabic-to-English switcher source page is missing: {arabic_path}")
        if not (root / "learn" / english_path).is_file():
            fail(f"Arabic-to-English switcher target page is missing: {english_path}")
        if arabic_path != "index.html" and en_to_ar.get(english_path) != arabic_path:
            fail(f"language switcher mapping does not round-trip: ar/{arabic_path} -> {english_path}")
    for english_path, arabic_path in en_to_ar.items():
        if not (root / "learn" / english_path).is_file():
            fail(f"English-to-Arabic switcher source page is missing: {english_path}")
        if not (root / "learn" / "ar" / arabic_path).is_file():
            fail(f"English-to-Arabic switcher target page is missing: {arabic_path}")

    if (root / "learn" / "ar" / "ylanglings.html").exists():
        fail("YLanglings must remain English-only and must not be copied into the Arabic book")
    english_book = (root / "learn" / "index.html").read_text(encoding="utf-8")
    arabic_book = (root / "learn" / "ar" / "index.html").read_text(encoding="utf-8")
    if "المسار العربي — تعلّم YLang" in english_book:
        fail("Arabic chapters leaked into the English book navigation")
    if "Part I — Start Here" in arabic_book:
        fail("English chapters leaked into the Arabic book navigation")
    book_css = (root / "learn" / "theme" / "ylang-book.css").read_text(encoding="utf-8")
    if "direction: rtl" not in book_css or ".ylang-ar #sidebar .chapter" not in book_css:
        fail("shared book stylesheet is missing RTL chapter-navigation rules")

    sitemap_root = ET.parse(root / "sitemap.xml").getroot()
    sitemap_urls = {
        item.text for item in sitemap_root.iter()
        if item.tag.endswith("}loc") and item.text
    }
    expected_urls = {
        public_url(page.relative_to(root).as_posix())
        for page in html_pages
        if page.name not in {"print.html", "404.html"} and "progress" not in page.relative_to(root).parts
    }
    missing_urls = sorted(expected_urls - sitemap_urls)
    extra_urls = sorted(sitemap_urls - expected_urls)
    if missing_urls:
        fail("sitemap is missing published pages: " + ", ".join(missing_urls[:10]))
    if extra_urls:
        fail("sitemap contains non-published pages: " + ", ".join(extra_urls[:10]))
    if any("/progress/" in url for url in sitemap_urls):
        fail("the retired progress page appears in the sitemap")
    if any(url.endswith("/404.html") for url in sitemap_urls):
        fail("generated 404 pages must not be indexed in the sitemap")
    robots = (root / "robots.txt").read_text(encoding="utf-8")
    if "Sitemap: " + BASE_URL + "sitemap.xml" not in robots:
        fail("robots.txt does not advertise the generated sitemap")

    broken: list[str] = []
    for page in html_pages:
        parser = parsed_pages[page.resolve()]
        for reference in parser.references:
            parsed = urlsplit(reference)
            if parsed.scheme or parsed.netloc:
                continue
            target = page if not parsed.path else resolve_local_target(parsed.path, page, root)
            if not target.is_file():
                broken.append(f"{page.relative_to(root)} -> {reference}")
                if len(broken) >= 50:
                    break
                continue
            if parsed.fragment and target.suffix.lower() == ".html":
                target_parser = parsed_pages.get(target.resolve())
                if target_parser is None:
                    target_parser = LinkParser()
                    target_parser.feed(target.read_text(encoding="utf-8"))
                    parsed_pages[target.resolve()] = target_parser
                fragment = unquote(parsed.fragment)
                if fragment not in target_parser.ids:
                    broken.append(
                        f"{page.relative_to(root)} -> missing #{fragment} in {target.relative_to(root)}"
                    )
                    if len(broken) >= 50:
                        break
        if len(broken) >= 50:
            break
    if broken:
        fail("broken local HTML links or fragments detected:\n  " + "\n  ".join(broken))

    css_broken: list[str] = []
    for css_path in root.rglob("*.css"):
        source = css_path.read_text(encoding="utf-8")
        for match in CSS_URL.finditer(source):
            value = match.group(2).strip()
            parsed = urlsplit(value)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            target = resolve_local_target(parsed.path, css_path, root)
            if not target.is_file():
                css_broken.append(f"{css_path.relative_to(root)} -> {value}")
                if len(css_broken) >= 30:
                    break
        if len(css_broken) >= 30:
            break
    if css_broken:
        fail("broken local CSS resources detected:\n  " + "\n  ".join(css_broken))

    print(
        "Website integrity passed: bilingual builds, language routes, switcher mappings, "
        "RTL/LTR, favicons, sitemap, local HTML/CSS assets, and fragments."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
