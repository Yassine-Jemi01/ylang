"""Keep the YLanglings tool, exercises, hints, and CLI text English-only."""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARABIC = re.compile(r"[\u0600-\u06ff]")
TEXT_SUFFIXES = {".md", ".py", ".json", ".toml", ".yl", ".txt"}


class EnglishOnlyTests(unittest.TestCase):
    def test_y_langlings_tree_contains_no_arabic_prose(self) -> None:
        checked = 0
        for path in sorted(ROOT.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            if "__pycache__" in path.parts:
                continue
            content = path.read_text(encoding="utf-8")
            self.assertIsNone(
                ARABIC.search(content),
                f"YLanglings must remain English-only; Arabic text found in {path.relative_to(ROOT)}",
            )
            checked += 1
        self.assertGreater(checked, 0, "no YLanglings text files were checked")


if __name__ == "__main__":
    unittest.main()
