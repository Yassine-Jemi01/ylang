#!/usr/bin/env python3
"""Offline, compiler-backed exercises for learning YLang."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXERCISES = ROOT / "exercises"
MANIFEST = ROOT / "manifest.json"
CONFIG = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "ylanglings"
PROGRESS = CONFIG / "progress.json"


def load_manifest():
    with MANIFEST.open(encoding="utf-8") as handle:
        return json.load(handle)


def load_progress():
    try:
        return json.loads(PROGRESS.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"completed": []}


def save_progress(progress):
    CONFIG.mkdir(parents=True, exist_ok=True)
    PROGRESS.write_text(json.dumps(progress, indent=2) + "\n", encoding="utf-8")


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
    completed = set(progress.get("completed", []))
    for number, item in enumerate(manifest, 1):
        mark = "✓" if number in completed else " "
        print(f"[{mark}] {number:02d}  {item['title']:<27} {item['category']}")
    print(f"\n{len(completed)}/{len(manifest)} completed")


def command_run(number, manifest):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    print_exercise(number, manifest[number - 1])


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
    progress["completed"] = [n for n in progress.get("completed", []) if n != number]
    save_progress(progress)
    print(f"Restored starter: {target}")


def command_verify(number, manifest, compiler):
    if number < 1 or number > len(manifest):
        raise SystemExit("Unknown exercise number. Run 'ylanglings list'.")
    item = manifest[number - 1]
    source = exercise_path(item)
    if not source.is_file():
        raise SystemExit(f"Exercise file is missing: {source}")

    check = subprocess.run([compiler, "check", str(source)], text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check.returncode:
        print(check.stdout, end="")
        print("\nNot quite. Fix the compiler errors, then run verify again.")
        return 1

    if item.get("mode") == "check":
        progress = load_progress()
        completed = set(progress.get("completed", []))
        completed.add(number)
        progress["completed"] = sorted(completed)
        save_progress(progress)
        print(f"Exercise {number:02d} passed compiler checking: {item['title']}")
        print("This API exercise is check-only; runtime integration is covered separately.")
        return 0

    with tempfile.TemporaryDirectory(prefix="ylanglings-") as directory:
        binary = Path(directory) / ("exercise.exe" if os.name == "nt" else "exercise")
        build = subprocess.run([compiler, "build", str(source), "-o", str(binary)],
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build.returncode:
            print(build.stdout, end="")
            print("\nThe source passed check but failed to build.")
            return 1
        try:
            result = subprocess.run([str(binary)], text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=10)
        except subprocess.TimeoutExpired:
            print("The program exceeded the 10-second exercise limit.")
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

    progress = load_progress()
    completed = set(progress.get("completed", []))
    completed.add(number)
    progress["completed"] = sorted(completed)
    save_progress(progress)
    print(f"Exercise {number:02d} passed: {item['title']}")
    print("Progress saved locally. Continue with: ylanglings next")
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
    hint = sub.add_parser("hint", help="Show a hint without revealing a solution")
    hint.add_argument("number", type=int)
    verify = sub.add_parser("verify", help="Compile and run your solution")
    verify.add_argument("number", type=int)
    reset = sub.add_parser("reset", help="Restore the original broken exercise")
    reset.add_argument("number", type=int)
    sub.add_parser("next", help="Open the first incomplete exercise")

    args = parser.parse_args()
    manifest = load_manifest()
    progress = load_progress()
    if args.action == "list":
        command_list(manifest, progress)
    elif args.action == "run":
        command_run(args.number, manifest)
    elif args.action == "hint":
        command_hint(args.number, manifest)
    elif args.action == "reset":
        command_reset(args.number, manifest)
    elif args.action == "verify":
        raise SystemExit(command_verify(args.number, manifest, compiler_path(args.compiler)))
    elif args.action == "next":
        completed = set(progress.get("completed", []))
        number = next((i for i, item in enumerate(manifest, 1) if i not in completed and not item.get("optional", False)), None)
        if number is None:
            print("All core exercises completed. Try optional challenges with: ylanglings list")
        else:
            command_run(number, manifest)


if __name__ == "__main__":
    main()
