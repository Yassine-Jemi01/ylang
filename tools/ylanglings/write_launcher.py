#!/usr/bin/env python3
"""Write the POSIX launcher without Make interpreting its runtime $@."""

from __future__ import annotations

import argparse
import os
import shlex
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python", required=True)
    parser.add_argument("--datadir", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    interpreter = shlex.quote(args.python)
    script = shlex.quote(str(Path(args.datadir) / "ylanglings.py"))
    destination = Path(args.output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        f"#!/bin/sh\nexec {interpreter} {script} \"$@\"\n",
        encoding="utf-8",
        newline="\n",
    )
    destination.chmod(0o755)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
