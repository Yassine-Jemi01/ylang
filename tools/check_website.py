#!/usr/bin/env python3
"""Static checks for the English-only YLang website; no third-party packages required."""

from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "website"
TEXT_SUFFIXES = {".html", ".css", ".js", ".md", ".svg"}
ARABIC = re.compile(r"[\u0600-\u06FF]")
LEGACY_MARKERS = (
    "lang-toggle",
    "IBM Plex Sans Arabic",
    "Noto Sans Arabic",
    "العربية",
)


class PageLinks(HTMLParser):
    """Collect the language declaration, local anchors, and referenced assets."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.ids: list[str] = []
        self.fragments: list[str] = []
        self.local_paths: list[str] = []
        self.targets: list[str] = []
        self.lang_ok = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "html":
            self.lang_ok = values.get("lang") == "en" and values.get("dir") == "ltr"
        if values.get("id"):
            self.ids.append(str(values["id"]))

        for attr in ("href", "src"):
            target = values.get(attr)
            if not target:
                continue
            target = str(target).strip()
            self.targets.append(target)
            if target.startswith("#"):
                if len(target) > 1:
                    self.fragments.append(target[1:])
                continue

            parsed = urlparse(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            self.local_paths.append(parsed.path)


def check_book_links(errors: list[str]) -> None:
    """Validate local links and fragments across the generated mdBook HTML."""
    book_root = SITE / "learn"
    if not book_root.is_dir():
        # CI builds the book before running this checker. Keep the standalone
        # marketing-site check usable when the generated book is absent.
        return

    pages = sorted(path for path in book_root.rglob("*.html") if path.is_file())
    if not pages:
        errors.append("website/learn contains no generated HTML book pages.")
        return

    parsed_pages: dict[Path, PageLinks] = {}

    def parse_page(path: Path) -> PageLinks:
        resolved = path.resolve()
        if resolved not in parsed_pages:
            parser = PageLinks()
            parser.feed(resolved.read_text(encoding="utf-8"))
            parsed_pages[resolved] = parser
        return parsed_pages[resolved]

    for page in pages:
        parse_page(page)

    site_root = SITE.resolve()
    for page in pages:
        parser = parse_page(page)
        page_name = page.relative_to(SITE).as_posix()

        duplicates = sorted({item for item in parser.ids if parser.ids.count(item) > 1})
        if duplicates:
            errors.append(f"{page_name} has duplicate IDs: {', '.join(duplicates)}.")

        for target in parser.targets:
            parsed = urlparse(target)
            if parsed.scheme or parsed.netloc:
                continue

            path_part = unquote(parsed.path)
            if not path_part:
                if parsed.fragment and parsed.fragment not in parser.ids:
                    errors.append(f"{page_name} refers to missing in-page anchor: {parsed.fragment}.")
                continue

            if path_part.startswith("/"):
                local_target = (site_root / path_part.lstrip("/")).resolve()
            else:
                local_target = (page.parent / path_part).resolve()

            try:
                local_target.relative_to(site_root)
            except ValueError:
                errors.append(f"{page_name} link escapes the website root: {target}.")
                continue

            if local_target.is_dir():
                local_target = local_target / "index.html"
            if not local_target.is_file():
                # The Pages deployment stages this PDF from docs/book/ after building the book.
                if local_target == (SITE / "YLang-Book.pdf").resolve() and (ROOT / "docs/book/YLang-Book.pdf").is_file():
                    continue
                errors.append(f"{page_name} refers to missing local target: {target}.")
                continue

            if parsed.fragment and local_target.suffix.lower() == ".html":
                target_parser = parse_page(local_target)
                if parsed.fragment not in target_parser.ids:
                    errors.append(
                        f"{page_name} refers to missing anchor {parsed.fragment!r} "
                        f"in {local_target.relative_to(site_root).as_posix()}."
                    )


def check_site() -> list[str]:
    errors: list[str] = []
    files = sorted(p for p in SITE.rglob("*") if p.is_file() and p.suffix.lower() in TEXT_SUFFIXES)

    for path in files:
        text = path.read_text(encoding="utf-8")
        if ARABIC.search(text):
            errors.append(f"{path.relative_to(ROOT)} contains Arabic-script text.")
        if path.suffix.lower() in {".html", ".js", ".css"}:
            for marker in LEGACY_MARKERS:
                if marker in text:
                    errors.append(f"{path.relative_to(ROOT)} contains legacy localization marker {marker!r}.")
                    break

    for relative in ("website/index.html", "website/docs.html"):
        page_path = ROOT / relative
        parser = PageLinks()
        parser.feed(page_path.read_text(encoding="utf-8"))

        if not parser.lang_ok:
            errors.append(f"{relative} must declare <html lang=\"en\" dir=\"ltr\">.")

        duplicates = sorted({item for item in parser.ids if parser.ids.count(item) > 1})
        if duplicates:
            errors.append(f"{relative} has duplicate IDs: {', '.join(duplicates)}.")

        missing_fragments = sorted(set(parser.fragments) - set(parser.ids))
        if missing_fragments:
            errors.append(f"{relative} has missing in-page anchors: {', '.join(missing_fragments)}.")

        for target in sorted(set(parser.local_paths)):
            local_target = (SITE / target).resolve()
            if local_target.is_file():
                continue
            # deploy-website.yml copies the book from docs/book into the Pages artifact.
            if target == "YLang-Book.pdf" and (ROOT / "docs/book/YLang-Book.pdf").is_file():
                continue
            errors.append(f"{relative} refers to missing local file: {target}.")

    check_book_links(errors)

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return errors

    print("PASS: English-only website, navigation anchors, local assets, and links.")
    return []


if __name__ == "__main__":
    raise SystemExit(1 if check_site() else 0)
