#!/usr/bin/env python3
"""Exercise raw-key TUI input under a pseudo-terminal on POSIX hosts."""
import errno
import os
import re
import pty
import select
import signal
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
        values = re.findall(r"(?m)^(-?\d+)\r?$", text)
        if values[-3:] != ["80", "24", "1001"]:
            raise SystemExit(f"TUI dimensions/key mismatch: {text!r}")
        restored = termios.tcgetattr(slave)
        mask = termios.ICANON | termios.ECHO
        if (restored[3] & mask) != (original[3] & mask):
            raise SystemExit("TUI did not restore terminal canonical/echo settings")

        signal_master, signal_slave = pty.openpty()
        try:
            fcntl.ioctl(signal_slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
            signal_original = termios.tcgetattr(signal_slave)
            signal_process = subprocess.Popen(
                [str(ROOT / executable)],
                cwd=ROOT,
                stdin=signal_slave,
                stdout=signal_slave,
                stderr=signal_slave,
                close_fds=True,
            )
            deadline = time.monotonic() + 3.0
            signal_raw_enabled = False
            while time.monotonic() < deadline and signal_process.poll() is None:
                current = termios.tcgetattr(signal_slave)
                if not (current[3] & termios.ICANON) and not (current[3] & termios.ECHO):
                    signal_raw_enabled = True
                    break
                time.sleep(0.01)
            if not signal_raw_enabled:
                signal_process.kill()
                signal_process.wait(timeout=2)
                raise SystemExit("TUI did not enable raw mode before signal test")
            signal_process.send_signal(signal.SIGINT)
            signal_process.wait(timeout=3)
            if signal_process.returncode != 128 + signal.SIGINT:
                raise SystemExit(f"TUI SIGINT exit status mismatch: {signal_process.returncode}")
            signal_output = bytearray()
            ready, _, _ = select.select([signal_master], [], [], 0.2)
            if ready:
                try:
                    signal_output.extend(os.read(signal_master, 4096))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
            if b"\x1b[?25h" not in signal_output:
                raise SystemExit(f"TUI signal handler did not restore cursor visibility: {signal_output!r}")
            signal_restored = termios.tcgetattr(signal_slave)
            if (signal_restored[3] & mask) != (signal_original[3] & mask):
                raise SystemExit("TUI did not restore terminal settings on SIGINT")
        finally:
            os.close(signal_master)
            os.close(signal_slave)
        # TUI must refuse a tty on stdin when stdout is redirected.
        redirected_master, redirected_slave = pty.openpty()
        try:
            redirected_original = termios.tcgetattr(redirected_slave)
            redirected_process = subprocess.run(
                [str(ROOT / executable)],
                cwd=ROOT,
                stdin=redirected_slave,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=3,
            )
            if redirected_process.returncode != 1 or b"interactive stdin and stdout" not in redirected_process.stdout:
                raise SystemExit(
                    "TUI did not reject redirected stdout: "
                    f"status={redirected_process.returncode}, output={redirected_process.stdout!r}"
                )
            redirected_after = termios.tcgetattr(redirected_slave)
            if redirected_after != redirected_original:
                raise SystemExit("TUI changed terminal mode despite redirected stdout")
        finally:
            os.close(redirected_master)
            os.close(redirected_slave)
        print("YLang TUI raw-key, signal-restoration, and redirected-output tests passed.")
    finally:
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    main()
