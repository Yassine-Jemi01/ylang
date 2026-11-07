#!/usr/bin/env python3
"""Generate a sitemap for the final GitHub Pages output."""

from __future__ import annotations

from pathlib import Path
import sys
from urllib.parse import quote
from xml.etree import ElementTree as ET

BASE_URL = "https://yassine-jemi01.github.io/ylang/"
SITEMAP_NS = "http://www.sitemaps.org/schemas/sitemap/0.9"
XHTML_NS = "http://www.w3.org/1999/xhtml"
ET.register_namespace("", SITEMAP_NS)
ET.register_namespace("xhtml", XHTML_NS)


def public_url(relative: str) -> str:
    if relative == "index.html":
        return BASE_URL
    if relative.endswith("/index.html"):
        return BASE_URL + quote(relative[:-len("index.html")], safe="/-._~")
    return BASE_URL + quote(relative, safe="/-._~")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "site-dist").resolve()
    if not root.is_dir():
        raise SystemExit(f"site output directory does not exist: {root}")

    urlset = ET.Element(f"{{{SITEMAP_NS}}}urlset")
    pages = sorted(
        page for page in root.rglob("*.html")
        if page.name not in {"print.html", "404.html"}
        and "progress" not in page.relative_to(root).parts
    )

    for page in pages:
        relative = page.relative_to(root).as_posix()
        entry = ET.SubElement(urlset, f"{{{SITEMAP_NS}}}url")
        ET.SubElement(entry, f"{{{SITEMAP_NS}}}loc").text = public_url(relative)
        if relative in {"index.html", "ar/index.html"}:
            for language, target in (
                ("en", "index.html"),
                ("ar", "ar/index.html"),
                ("x-default", "index.html"),
            ):
                ET.SubElement(
                    entry,
                    f"{{{XHTML_NS}}}link",
                    {"rel": "alternate", "hreflang": language, "href": public_url(target)},
                )
        if relative in {"docs.html", "docs-ar.html"}:
            for language, target in (
                ("en", "docs.html"),
                ("ar", "docs-ar.html"),
                ("x-default", "docs.html"),
            ):
                ET.SubElement(
                    entry,
                    f"{{{XHTML_NS}}}link",
                    {"rel": "alternate", "hreflang": language, "href": public_url(target)},
                )

    tree = ET.ElementTree(urlset)
    ET.indent(tree, space="  ")
    tree.write(root / "sitemap.xml", encoding="utf-8", xml_declaration=True)
    print(f"Generated sitemap.xml with {len(pages)} public HTML pages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
