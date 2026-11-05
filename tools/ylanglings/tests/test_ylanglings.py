#!/usr/bin/env python3
"""Fast, offline contract tests for the YLanglings learning tool."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import subprocess
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
        solutions = set()
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
                self.assertEqual(
                    exercise.read_bytes(), starter.read_bytes(),
                    "reset must restore the exact initial exercise source",
                )
                self.assertNotIn(item["file"], files)
                files.add(item["file"])
                self.assertNotIn(item["solution"], solutions)
                solutions.add(item["solution"])

                if item.get("mode", "run") == "run":
                    self.assertIsInstance(item.get("expected_output"), str)

    def test_core_exercises_are_listed_before_optional_challenges(self):
        optional_seen = False
        for item in self.manifest:
            if item.get("optional", False):
                optional_seen = True
            else:
                self.assertFalse(optional_seen, "core exercise appears after optional track")

    def test_every_exercise_has_a_distinct_maintainer_reference_solution(self):
        for item in self.manifest:
            with self.subTest(exercise=item["title"]):
                exercise = (ylanglings.EXERCISES / item["file"]).read_text(encoding="utf-8")
                solution = ylanglings.solution_path(item).read_text(encoding="utf-8")
                self.assertNotEqual(
                    exercise, solution,
                    "the editable exercise must not already contain its reference solution",
                )

    def test_manifest_rejects_paths_outside_exercise_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "exercises"
            root.mkdir()
            manifest = Path(directory) / "manifest.json"
            manifest.write_text(json.dumps([{
                "title": "Unsafe", "category": "test", "file": "../outside.yl",
                "starter": "starter.yl", "description": "test", "hint": "test",
                "solution": "safe.yl", "expected_output": "",
            }]), encoding="utf-8")
            with mock.patch.object(ylanglings, "EXERCISES", root), mock.patch.object(
                ylanglings, "MANIFEST", manifest
            ):
                with self.assertRaisesRegex(SystemExit, "unsafe file path"):
                    ylanglings.load_manifest()


    def test_manifest_rejects_unsafe_reference_solution_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "exercises"
            root.mkdir()
            (root / "exercise.yl").write_text("", encoding="utf-8")
            (root / "starter.yl").write_text("", encoding="utf-8")
            manifest = Path(directory) / "manifest.json"
            manifest.write_text(json.dumps([{
                "title": "Unsafe solution", "category": "test", "file": "exercise.yl",
                "starter": "starter.yl", "description": "test", "hint": "test",
                "solution": "../outside.yl", "expected_output": "",
            }]), encoding="utf-8")
            with mock.patch.object(ylanglings, "EXERCISES", root), mock.patch.object(
                ylanglings, "MANIFEST", manifest
            ):
                with self.assertRaisesRegex(SystemExit, "unsafe solution path"):
                    ylanglings.load_manifest()


class ReferenceSolutionTests(unittest.TestCase):
    def test_solution_paths_are_confined_to_private_solutions_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "solutions"
            root.mkdir()
            (root / "valid.yl").write_text("fn main() -> int { return 0; }\\n", encoding="utf-8")
            outside = Path(directory) / "outside.yl"
            outside.write_text("not a solution\\n", encoding="utf-8")
            with mock.patch.object(ylanglings, "SOLUTIONS", root):
                self.assertEqual(
                    ylanglings.solution_path({"title": "Valid", "solution": "valid.yl"}),
                    root / "valid.yl",
                )
                with self.assertRaisesRegex(SystemExit, "Unsafe solution path"):
                    ylanglings.solution_path({"title": "Escape", "solution": "../outside.yl"})

    def test_missing_reference_solution_has_actionable_error(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            ylanglings, "SOLUTIONS", Path(directory)
        ):
            with self.assertRaisesRegex(SystemExit, "missing solution for Missing"):
                ylanglings.solution_path({"title": "Missing", "solution": "missing.yl"})



class CliStateTests(unittest.TestCase):
    def test_corrupt_progress_recovers_to_empty_state(self):
        with tempfile.TemporaryDirectory() as directory:
            progress_path = Path(directory) / "progress.json"
            progress_path.write_text("{broken", encoding="utf-8")
            with mock.patch.object(ylanglings, "PROGRESS", progress_path):
                self.assertEqual(ylanglings.load_progress(), {"completed": []})

    def test_malformed_progress_shape_and_identifiers_are_sanitized(self):
        with tempfile.TemporaryDirectory() as directory:
            progress_path = Path(directory) / "progress.json"
            with mock.patch.object(ylanglings, "PROGRESS", progress_path):
                progress_path.write_text("null", encoding="utf-8")
                self.assertEqual(ylanglings.load_progress(), {"completed": []})
                progress_path.write_text(
                    json.dumps({"completed": [1, 1, True, -2, "3", 4]}),
                    encoding="utf-8",
                )
                self.assertEqual(ylanglings.load_progress(), {"completed": [1, 4]})
                progress_path.write_bytes(b"\xff\xfe")
                self.assertEqual(ylanglings.load_progress(), {"completed": []})
                progress_path.write_bytes(b" " * (1024 * 1024 + 1))
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
                self.assertEqual(list(config.glob(".progress-*.tmp")), [])

    def test_list_shows_core_and_optional_exercises(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            ylanglings.command_list(self.manifest, {"completed": [1, 17]})
        rendered = output.getvalue()
        self.assertIn("01", rendered)
        self.assertIn("Optional: nested loops", rendered)
        self.assertIn("2/26 completed", rendered)

    def test_verify_all_continues_and_reports_every_failure(self):
        manifest = ylanglings.load_manifest()
        output = io.StringIO()
        results = [0, 1] + [0] * (len(manifest) - 2)
        original_config = ylanglings.CONFIG
        original_progress = ylanglings.PROGRESS
        with mock.patch.object(ylanglings, "command_verify", side_effect=results) as verify, contextlib.redirect_stdout(output):
            status = ylanglings.command_verify_all(manifest, "ylang")
        self.assertEqual(status, 1)
        self.assertEqual(verify.call_count, 26)
        self.assertTrue(all(call.kwargs.get("persist_progress") is False for call in verify.call_args_list))
        self.assertIn("02", output.getvalue())
        self.assertIn("YLanglings verification failed", output.getvalue())
        self.assertNotIn("All 26", output.getvalue())
        self.assertEqual(ylanglings.CONFIG, original_config)
        self.assertEqual(ylanglings.PROGRESS, original_progress)

    def test_verify_all_reports_complete_success(self):
        manifest = ylanglings.load_manifest()
        output = io.StringIO()
        with mock.patch.object(ylanglings, "command_verify", return_value=0) as verify, contextlib.redirect_stdout(output):
            status = ylanglings.command_verify_all(manifest, "ylang")
        self.assertEqual(status, 0)
        self.assertEqual(verify.call_count, 26)
        self.assertTrue(all(call.kwargs.get("persist_progress") is False for call in verify.call_args_list))
        expected_sources = [str(ylanglings.solution_path(item)) for item in manifest]
        self.assertEqual(
            [call.kwargs.get("source_override") for call in verify.call_args_list],
            expected_sources,
        )
        self.assertIn("All 26 YLanglings reference solutions passed.", output.getvalue())

    def test_compiler_timeout_is_reported_without_hanging(self):
        output = io.StringIO()
        timeout = ylanglings.subprocess.TimeoutExpired(cmd=["ylang"], timeout=60)
        with mock.patch.object(ylanglings.subprocess, "run", side_effect=timeout):
            with contextlib.redirect_stdout(output):
                self.assertIsNone(ylanglings.run_compiler("ylang", "check", "exercise.yl"))
        self.assertIn("60-second", output.getvalue())

    def test_next_optional_selects_first_optional_challenge(self):
        with mock.patch.object(sys, "argv", ["ylanglings", "next", "--optional"]), mock.patch.object(
            ylanglings, "load_progress", return_value={"completed": []}
        ), mock.patch.object(ylanglings, "command_run") as command:
            ylanglings.main()
        command.assert_called_once_with(17, ylanglings.load_manifest())

    def test_edit_uses_configured_editor_without_a_shell(self):
        completed = type("Result", (), {"returncode": 0})()
        with mock.patch.dict(os.environ, {"EDITOR": "nano -w", "VISUAL": ""}), mock.patch.object(
            ylanglings.subprocess, "run", return_value=completed
        ) as run:
            self.assertEqual(ylanglings.command_edit(1, self.manifest), 0)
        args = run.call_args.args[0]
        self.assertEqual(args[:2], ["nano", "-w"])
        self.assertEqual(args[2], str(ylanglings.EXERCISES / self.manifest[0]["file"]))

    def test_invalid_exercise_number_has_actionable_error(self):
        for command in (ylanglings.command_run, ylanglings.command_hint):
            with self.subTest(command=command.__name__):
                with self.assertRaises(SystemExit) as raised:
                    command(0, ylanglings.load_manifest())
                self.assertIn("ylanglings list", str(raised.exception))

    def test_user_install_and_uninstall_smoke(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "prefix with spaces"
            config = Path(directory) / "config"
            environment = {**os.environ, "XDG_CONFIG_HOME": str(config)}
            subprocess.run(
                ["make", "-C", str(TOOL_DIR.parents[1]), "install-ylanglings", f"PREFIX={prefix}", f"PYTHON={'python' if os.name == 'nt' else sys.executable}"],
                check=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True,
            )
            executable = prefix / "bin" / "ylanglings"
            self.assertTrue(executable.is_file())
            launcher_text = executable.read_text(encoding="utf-8")
            self.assertIn('"$@"', launcher_text, "installed launcher must forward CLI arguments")
            command = ["bash", str(executable), "list"] if os.name == "nt" else [str(executable), "list"]
            listing = subprocess.run(
                command, check=True, capture_output=True,
                text=True, encoding="utf-8", env=environment,
            )
            self.assertIn("26", listing.stdout)
            subprocess.run(
                ["make", "-C", str(TOOL_DIR.parents[1]), "uninstall-ylanglings", f"PREFIX={prefix}"],
                check=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True,
            )
            self.assertFalse(executable.exists())
            self.assertFalse((prefix / "share" / "ylanglings").exists())


if __name__ == "__main__":
    unittest.main()
