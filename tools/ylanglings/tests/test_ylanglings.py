#!/usr/bin/env python3
"""Fast, offline contract tests for the YLanglings learning tool."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOL_DIR = Path(__file__).resolve().parents[1]
SCRIPT = TOOL_DIR / "ylanglings.py"
SPEC = importlib.util.spec_from_file_location("ylanglings", SCRIPT)
ylanglings = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules[SPEC.name] = ylanglings
SPEC.loader.exec_module(ylanglings)


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.manifest = ylanglings.load_manifest()

    def test_expected_track_sizes(self):
        core = [item for item in self.manifest if not item.get("optional", False)]
        optional = [item for item in self.manifest if item.get("optional", False)]
        self.assertEqual(len(core), 16)
        self.assertEqual(len(optional), 10)

    def test_entries_have_unique_existing_files_and_valid_metadata(self):
        files = set()
        for item in self.manifest:
            with self.subTest(exercise=item.get("title")):
                self.assertTrue(item.get("title"))
                self.assertTrue(item.get("category"))
                self.assertTrue(item.get("description"))
                self.assertTrue(item.get("hint"))
                self.assertIn(item.get("mode", "run"), {"run", "check"})

                exercise = (ylanglings.EXERCISES / item["file"]).resolve()
                starter = (ylanglings.EXERCISES / item["starter"]).resolve()
                self.assertTrue(exercise.is_relative_to(ylanglings.EXERCISES.resolve()))
                self.assertTrue(starter.is_relative_to(ylanglings.EXERCISES.resolve()))
                self.assertTrue(exercise.is_file(), f"missing exercise: {exercise}")
                self.assertTrue(starter.is_file(), f"missing starter: {starter}")
                self.assertNotIn(item["file"], files)
                files.add(item["file"])

                if item.get("mode", "run") == "run":
                    self.assertIsInstance(item.get("expected_output"), str)

    def test_core_exercises_are_listed_before_optional_challenges(self):
        optional_seen = False
        for item in self.manifest:
            if item.get("optional", False):
                optional_seen = True
            else:
                self.assertFalse(optional_seen, "core exercise appears after optional track")


class CliStateTests(unittest.TestCase):
    def test_corrupt_progress_recovers_to_empty_state(self):
        with tempfile.TemporaryDirectory() as directory:
            progress_path = Path(directory) / "progress.json"
            progress_path.write_text("{broken", encoding="utf-8")
            with mock.patch.object(ylanglings, "PROGRESS", progress_path):
                self.assertEqual(ylanglings.load_progress(), {"completed": []})

    def test_progress_is_saved_and_loaded(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "config"
            progress_path = config / "progress.json"
            with mock.patch.object(ylanglings, "CONFIG", config), mock.patch.object(
                ylanglings, "PROGRESS", progress_path
            ):
                ylanglings.save_progress({"completed": [1, 2]})
                self.assertEqual(ylanglings.load_progress(), {"completed": [1, 2]})
                self.assertTrue(progress_path.is_file())

    def test_list_shows_core_and_optional_exercises(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            ylanglings.command_list(self.manifest, {"completed": [1, 17]})
        rendered = output.getvalue()
        self.assertIn("01", rendered)
        self.assertIn("Optional: nested loops", rendered)
        self.assertIn("2/26 completed", rendered)

    def test_invalid_exercise_number_has_actionable_error(self):
        for command in (ylanglings.command_run, ylanglings.command_hint):
            with self.subTest(command=command.__name__):
                with self.assertRaises(SystemExit) as raised:
                    command(0, ylanglings.load_manifest())
                self.assertIn("ylanglings list", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
