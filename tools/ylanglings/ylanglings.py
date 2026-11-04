#!/usr/bin/env python3
"""Offline, compiler-backed exercises for learning YLang."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXERCISES = ROOT / "exercises"
MANIFEST = ROOT / "manifest.json"
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME") or (Path.home() / ".config")) / "ylanglings"
PROGRESS = CONFIG / "progress.json"


def load_manifest():
    try:
        with MANIFEST.open(encoding="utf-8") as handle:
            manifest = json.load(handle)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise SystemExit(f"Could not read YLanglings manifest: {error}") from error
    if not isinstance(manifest, list) or not manifest:
        raise SystemExit("Invalid YLanglings manifest: expected a non-empty exercise list.")
    seen_files = set()
    seen_starters = set()
    seen_titles = set()
    core_count = optional_count = 0
    root = EXERCISES.resolve()
    for number, item in enumerate(manifest, 1):
        if not isinstance(item, dict):
            raise SystemExit(f"Invalid manifest entry {number}: expected an object.")
        required = ("title", "category", "file", "starter", "description", "hint")
        missing = [field for field in required if not isinstance(item.get(field), str) or not item[field].strip()]
        if missing:
            raise SystemExit(f"Invalid manifest entry {number}: missing {', '.join(missing)}.")
        for field in required:
            if any(ord(character) < 32 or ord(character) == 127 for character in item[field]):
                raise SystemExit(f"Invalid manifest entry {number}: {field} contains control characters.")
        mode = item.get("mode", "run")
        if not isinstance(mode, str) or mode not in {"run", "check"}:
            raise SystemExit(f"Invalid mode in manifest entry {number}: {mode!r}.")
        optional = item.get("optional", False)
        if not isinstance(optional, bool):
            raise SystemExit(f"Invalid optional flag in manifest entry {number}.")
        if mode == "run" and not isinstance(item.get("expected_output"), str):
            raise SystemExit(f"Runnable exercise {number} needs expected_output.")
        resolved_paths = {}
        for field in ("file", "starter"):
            if "\0" in item[field]:
                raise SystemExit(f"Manifest entry {number} has an unsafe {field} path.")
            try:
                candidate = (root / item[field]).resolve()
                contained = candidate.is_relative_to(root)
                exists = candidate.is_file()
            except (OSError, RuntimeError, ValueError) as error:
                raise SystemExit(f"Invalid {field} path in manifest entry {number}: {error}") from error
            if not contained:
                raise SystemExit(f"Manifest entry {number} has an unsafe {field} path.")
            if not exists:
                raise SystemExit(f"Manifest entry {number} references missing {field}: {item[field]}")
            resolved_paths[field] = candidate
        exercise_path_resolved = str(resolved_paths["file"])
        starter_path_resolved = str(resolved_paths["starter"])
        if exercise_path_resolved == starter_path_resolved:
            raise SystemExit(f"Manifest entry {number} uses the same file as its starter and editable exercise.")
        if exercise_path_resolved in seen_files:
            raise SystemExit(f"Duplicate exercise file in manifest entry {number}: {item['file']}")
        if starter_path_resolved in seen_starters:
            raise SystemExit(f"Duplicate starter file in manifest entry {number}: {item['starter']}")
        if item["title"] in seen_titles:
            raise SystemExit(f"Duplicate exercise title in manifest entry {number}: {item['title']}")
        seen_files.add(exercise_path_resolved)
        seen_starters.add(starter_path_resolved)
        seen_titles.add(item["title"])
        if optional:
            optional_count += 1
        else:
            core_count += 1
    if core_count != 16 or optional_count != 10:
        raise SystemExit(
            f"Invalid 2.3 exercise tracks: expected 16 core and 10 optional, "
            f"found {core_count} core and {optional_count} optional."
        )
    return manifest


def load_progress():
    try:
        if PROGRESS.stat().st_size > 1024 * 1024:
            return {"completed": []}
        progress = json.loads(PROGRESS.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return {"completed": []}

    if not isinstance(progress, dict):
        return {"completed": []}
    completed = progress.get("completed", [])
    if not isinstance(completed, list):
        return {"completed": []}
    valid = sorted({
        number for number in completed
        if isinstance(number, int) and not isinstance(number, bool) and number > 0
    })
    return {"completed": valid}


def save_progress(progress):
    CONFIG.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=CONFIG,
            prefix=".progress-", suffix=".tmp", delete=False,
        ) as temporary:
            temporary_path = Path(temporary.name)
            json.dump(progress, temporary, indent=2)
            temporary.write("\n")
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temporary_path, PROGRESS)
    except OSError as error:
        raise SystemExit(f"Could not save YLanglings progress: {error}") from error
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def completed_exercises(progress, manifest):
    return {
        number for number in progress.get("completed", [])
        if isinstance(number, int) and not isinstance(number, bool)
        and 1 <= number <= len(manifest)
    }


def compiler_path(value):
    found = shutil.which(value)
    if found:
        return found
    candidate = Path(value).expanduser()
    if candidate.is_file():
        return str(candidate.resolve())
    raise SystemExit(f"YLang compiler not found: {value}\nPass --compiler /path/to/ylang or add ylang to PATH.")


def exercise_path(item):
    return EXERCISES / item["file"]


def print_exercise(number, item):
    print(f"\n{number:02d}. {item['title']} [{item['category']}]")
    print(f"   {item['description']}")
    print(f"   File: {exercise_path(item)}")
    print(f"   Hint: ylanglings hint {number:02d}")
    print(f"   Verify: ylanglings verify {number:02d}\n")


def command_list(manifest, progress):
    completed = completed_exercises(progress, manifest)
    core = [(number, item) for number, item in enumerate(manifest, 1) if not item.get("optional", False)]
    optional = [(number, item) for number, item in enumerate(manifest, 1) if item.get("optional", False)]
    print("CORE TRACK")
    for number, item in core:
        mark = "✓" if number in completed else " "
        print(f"[{mark}] {number:02d}  {item['title']:<27} {item['category']}")
    core_done = sum(number in completed for number, _ in core)
    print(f"\nCore progress: {core_done}/{len(core)}")
    print("\nOPTIONAL CHALLENGES")
    for number, item in optional:
        mark = "✓" if number in completed else " "
        print(f"[{mark}] {number:02d}  {item['title']:<27} {item['category']}")
    optional_done = sum(number in completed for number, _ in optional)
    print(f"Optional progress: {optional_done}/{len(optional)}")
    print(f"Total: {len(completed)}/{len(manifest)} completed")


def command_run(number, manifest):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    print_exercise(number, manifest[number - 1])


def command_edit(number, manifest):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    editor = os.environ.get("VISUAL") or os.environ.get("EDITOR")
    if not editor:
        raise SystemExit("No editor configured. Set EDITOR (for example: export EDITOR=nano).")
    command = shlex.split(editor, posix=(os.name != "nt"))
    if os.name == "nt":
        command = [
            token[1:-1] if len(token) >= 2 and token[0] == token[-1] and token[0] in {"'", '"'}
            else token
            for token in command
        ]
    if not command:
        raise SystemExit("EDITOR is empty. Set EDITOR to an editor command.")
    target = exercise_path(manifest[number - 1])
    try:
        return subprocess.run([*command, str(target)], check=False).returncode
    except OSError as error:
        raise SystemExit(f"Could not launch editor {command[0]!r}: {error}") from error


def command_hint(number, manifest):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    item = manifest[number - 1]
    print(f"{number:02d}. {item['title']}\nHint: {item['hint']}")


def command_reset(number, manifest):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    item = manifest[number - 1]
    target = exercise_path(item)
    starter = EXERCISES / item["starter"]
    shutil.copyfile(starter, target)
    progress = load_progress()
    progress["completed"] = sorted(completed_exercises(progress, manifest) - {number})
    save_progress(progress)
    print(f"Restored starter: {target}")


def run_compiler(compiler, *arguments):
    try:
        return subprocess.run(
            [compiler, *arguments],
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=60,
        )
    except subprocess.TimeoutExpired:
        print("The YLang compiler exceeded the 60-second command limit.")
        return None
    except OSError as error:
        raise SystemExit(f"Could not run YLang compiler: {error}") from error


def command_verify(number, manifest, compiler, persist_progress=True):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    item = manifest[number - 1]
    source = exercise_path(item)
    if not source.is_file():
        raise SystemExit(f"Exercise file is missing: {source}")

    check = run_compiler(compiler, "check", str(source))
    if check is None:
        return 1
    if check.returncode:
        print(check.stdout, end="")
        print("\nNot quite. Fix the compiler errors, then run verify again.")
        return 1

    if item.get("mode") == "check":
        if persist_progress:
            progress = load_progress()
            completed = completed_exercises(progress, manifest)
            completed.add(number)
            progress["completed"] = sorted(completed)
            save_progress(progress)
        print(f"Exercise {number:02d} passed compiler checking: {item['title']}")
        print("This API exercise is check-only; runtime integration is covered separately.")
        return 0

    with tempfile.TemporaryDirectory(prefix="ylanglings-") as directory:
        binary = Path(directory) / ("exercise.exe" if os.name == "nt" else "exercise")
        build = run_compiler(compiler, "build", str(source), "-o", str(binary))
        if build is None:
            return 1
        if build.returncode:
            print(build.stdout, end="")
            print("\nThe source passed check but failed to build.")
            return 1
        try:
            result = subprocess.run(
                [str(binary)], cwd=directory, text=True, encoding="utf-8",
                errors="replace", stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, timeout=10,
            )
        except subprocess.TimeoutExpired:
            print("The program exceeded the 10-second exercise limit.")
            return 1
        except OSError as error:
            print(f"Could not launch the exercise program: {error}")
            return 1

    if result.returncode != 0:
        print(result.stdout, end="")
        print(f"\nProgram exited with status {result.returncode}; expected success.")
        return 1
    expected = item.get("expected_output", "")
    if result.stdout != expected:
        print("Output does not match the exercise target.")
        print("--- expected ---")
        print(expected, end="" if expected.endswith("\n") else "\n")
        print("--- received ---")
        print(result.stdout, end="" if result.stdout.endswith("\n") else "\n")
        return 1

    if persist_progress:
        progress = load_progress()
        completed = completed_exercises(progress, manifest)
        completed.add(number)
        progress["completed"] = sorted(completed)
        save_progress(progress)
    print(f"Exercise {number:02d} passed: {item['title']}")
    if persist_progress:
        print("Progress saved locally. Continue with: ylanglings next")
    return 0


def command_verify_all(manifest, compiler):
    failures = []
    for number, item in enumerate(manifest, 1):
        print(f"\n=== {number:02d}/{len(manifest):02d}: {item['title']} ===")
        if command_verify(number, manifest, compiler, persist_progress=False) != 0:
            failures.append(number)

    if failures:
        failed = ", ".join(f"{number:02d}" for number in failures)
        print(f"\nYLanglings verification failed for exercise(s): {failed}")
        return 1
    print(f"\nAll {len(manifest)} YLanglings exercises passed.")
    return 0


def main():
    parser = argparse.ArgumentParser(
        prog="ylanglings",
        description="Offline, compiler-backed YLang exercises."
    )
    parser.add_argument("--compiler", default="ylang", help="YLang compiler executable")
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("list", help="List all exercises")
    run = sub.add_parser("run", help="Show an exercise and its source path")
    run.add_argument("number", type=int)
    edit = sub.add_parser("edit", help="Open an exercise in $VISUAL or $EDITOR")
    edit.add_argument("number", type=int)
    hint = sub.add_parser("hint", help="Show a hint without revealing a solution")
    hint.add_argument("number", type=int)
    verify = sub.add_parser("verify", help="Compile and run one solution")
    verify.add_argument("number", type=int)
    sub.add_parser("verify-all", help="Verify every core exercise and optional challenge")
    reset = sub.add_parser("reset", help="Restore the original broken exercise")
    reset.add_argument("number", type=int)
    next_parser = sub.add_parser("next", help="Show the first incomplete exercise")
    next_parser.add_argument("--optional", action="store_true", help="Choose from optional challenges")

    args = parser.parse_args()
    manifest = load_manifest()
    progress = load_progress()
    if args.action == "list":
        command_list(manifest, progress)
    elif args.action == "run":
        command_run(args.number, manifest)
    elif args.action == "edit":
        raise SystemExit(command_edit(args.number, manifest))
    elif args.action == "hint":
        command_hint(args.number, manifest)
    elif args.action == "reset":
        command_reset(args.number, manifest)
    elif args.action == "verify":
        raise SystemExit(command_verify(args.number, manifest, compiler_path(args.compiler)))
    elif args.action == "verify-all":
        raise SystemExit(command_verify_all(manifest, compiler_path(args.compiler)))
    elif args.action == "next":
        completed = completed_exercises(progress, manifest)
        want_optional = args.optional
        number = next((
            i for i, item in enumerate(manifest, 1)
            if i not in completed and bool(item.get("optional", False)) == want_optional
        ), None)
        if number is None and want_optional:
            print("All optional challenges completed.")
        elif number is None:
            print("All core exercises completed. Continue with: ylanglings next --optional")
        else:
            command_run(number, manifest)


if __name__ == "__main__":
    main()
