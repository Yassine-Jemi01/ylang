#!/usr/bin/env python3
"""Static checks for the English-only YLang website; no third-party packages required."""

from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlparse
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
            if target.startswith("#"):
                if len(target) > 1:
                    self.fragments.append(target[1:])
                continue

            parsed = urlparse(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            self.local_paths.append(parsed.path)


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

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return errors

    print("PASS: English-only website, navigation anchors, local assets, and links.")
    return []


if __name__ == "__main__":
    raise SystemExit(1 if check_site() else 0)
