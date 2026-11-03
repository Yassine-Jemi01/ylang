#!/usr/bin/env python3
"""Exercise raw-key TUI input under a pseudo-terminal on POSIX hosts."""
import errno
import os
import re
import pty
import select
import struct
import subprocess
import sys
import termios
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    if os.name == "nt":
        print("SKIP: POSIX pseudo-terminal TUI test is not run on Windows.")
        return

    source = ROOT / "build" / "tui-key-test.yl"
    source.write_text(
        "function main() -> int {\n"
        "    if (!tui.init()) { print(tui.last_error()); return 1; }\n"
        "    print(tui.width());\n"
        "    print(tui.height());\n"
        "    print(tui.read_key());\n"
        "    tui.shutdown();\n"
        "    return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    compiler = "./build/ylang"
    executable = "build/test-tui-key"
    subprocess.run([compiler, "check", str(source.relative_to(ROOT))],
                   cwd=ROOT, check=True)
    subprocess.run([compiler, "build", str(source.relative_to(ROOT)), "-o", executable],
                   cwd=ROOT, check=True)

    master, slave = pty.openpty()
    try:
        fcntl = __import__("fcntl")
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
        original = termios.tcgetattr(slave)
        process = subprocess.Popen(
            [str(ROOT / executable)],
            cwd=ROOT,
            stdin=slave,
            stdout=slave,
            stderr=slave,
            close_fds=True,
        )
        deadline = time.monotonic() + 3.0
        raw_enabled = False
        while time.monotonic() < deadline and process.poll() is None:
            current = termios.tcgetattr(slave)
            if not (current[3] & termios.ICANON) and not (current[3] & termios.ECHO):
                raw_enabled = True
                break
            time.sleep(0.01)
        if not raw_enabled:
            process.kill()
            process.wait(timeout=2)
            raise SystemExit("TUI did not enable raw terminal input")
        os.write(master, b"\x1b[A")

        output = bytearray()
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline and process.poll() is None:
            ready, _, _ = select.select([master], [], [], 0.1)
            if ready:
                try:
                    output.extend(os.read(master, 4096))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
                    break
        if process.poll() is None:
            process.kill()
            process.wait(timeout=2)
            raise SystemExit("TUI key reader did not return")
        ready, _, _ = select.select([master], [], [], 0.1)
        if ready:
            try:
                output.extend(os.read(master, 4096))
            except OSError:
                pass
        if process.returncode != 0:
            raise SystemExit(f"TUI key test exited {process.returncode}: {output!r}")
        text = output.decode("utf-8", errors="replace")
        values = re.findall(r"(?m)^(-?\\d+)$", text)
        if values[-4:] != ["80", "24", "-1", "1001"]:
            raise SystemExit(f"TUI dimensions/key mismatch: {text!r}")
        restored = termios.tcgetattr(slave)
        mask = termios.ICANON | termios.ECHO
        if (restored[3] & mask) != (original[3] & mask):
            raise SystemExit("TUI did not restore terminal canonical/echo settings")
        print("YLang TUI raw-key integration test passed.")
    finally:
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    main()
