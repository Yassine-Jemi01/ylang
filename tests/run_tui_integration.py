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


def read_pty_until(fd, token, timeout):
    """Drain a PTY until token arrives or the bounded wait expires."""
    output = bytearray()
    deadline = time.monotonic() + timeout
    while token not in output:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        ready, _, _ = select.select([fd], [], [], min(remaining, 0.1))
        if not ready:
            continue
        try:
            chunk = os.read(fd, 4096)
        except OSError as error:
            if error.errno == errno.EIO:
                break
            raise
        if not chunk:
            break
        output.extend(chunk)
    return bytes(output)


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
            try:
                signal_process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                ready, _, _ = select.select([signal_master], [], [], 0.2)
                signal_output = bytearray()
                if ready:
                    try:
                        signal_output.extend(os.read(signal_master, 4096))
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                signal_process.kill()
                signal_process.wait(timeout=2)
                raise SystemExit(f"TUI SIGINT did not exit; signal output: {signal_output!r}")
            if signal_process.returncode != 128 + signal.SIGINT:
                raise SystemExit(f"TUI SIGINT exit status mismatch: {signal_process.returncode}")
            signal_output = read_pty_until(signal_master, b"\x1b[?25h", 2.0)
            if b"\x1b[?25h" not in signal_output:
                raise SystemExit(
                    "TUI signal handler did not restore cursor visibility within 2 seconds: "
                    f"{signal_output!r}"
                )
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
        # Exercise the real menu at wide, medium, and narrow widths, then navigate it.
        menu_executable = ROOT / "build" / "tui-menu-integration"
        subprocess.run(
            [compiler, "build", "examples/tui-menu.yl", "-o",
             str(menu_executable.relative_to(ROOT))],
            cwd=ROOT,
            check=True,
        )
        menu_master, menu_slave = pty.openpty()
        menu_process = None
        menu_output = bytearray()
        try:
            def resize_menu(columns):
                fcntl.ioctl(menu_slave, termios.TIOCSWINSZ,
                            struct.pack("HHHH", 24, columns, 0, 0))

            resize_menu(80)
            menu_original = termios.tcgetattr(menu_slave)
            menu_process = subprocess.Popen(
                [str(menu_executable)],
                cwd=ROOT,
                stdin=menu_slave,
                stdout=menu_slave,
                stderr=menu_slave,
                close_fds=True,
            )

            def wait_for_menu_frame(predicate, start_at, timeout, description):
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    ready, _, _ = select.select([menu_master], [], [], 0.05)
                    if ready:
                        try:
                            menu_output.extend(os.read(menu_master, 8192))
                        except OSError as error:
                            if error.errno != errno.EIO:
                                raise
                    new_output = bytes(menu_output[start_at:])
                    frame_bytes = new_output.rsplit(b"\x1b[2J", 1)[-1]
                    frame = re.sub(
                        r"\x1b\[[0-?]*[ -/]*[@-~]",
                        "",
                        frame_bytes.decode("utf-8", errors="replace"),
                    )
                    if predicate(frame):
                        return frame
                    if menu_process.poll() is not None:
                        break
                raise SystemExit(f"TUI menu did not render {description}")

            wide_start = len(menu_output)
            wide = wait_for_menu_frame(
                lambda frame: "+--------------------------------------------------+" in frame and "GET STARTED" in frame,
                wide_start, 3.0, "the wide layout",
            )
            resize_menu(50)
            medium_start = len(menu_output)
            medium = wait_for_menu_frame(
                lambda frame: "GET STARTED" in frame and "UP / DOWN" in frame,
                medium_start, 3.0, "the medium-width layout",
            )
            if "+--------------------------------------------------+" in medium:
                raise SystemExit("TUI menu kept the wide box after resizing to 50 columns")

            resize_menu(40)
            narrow_start = len(menu_output)
            narrow = wait_for_menu_frame(
                lambda frame: "UP/DOWN Move" in frame and "ENTER Choose" in frame,
                narrow_start, 3.0, "the narrow-width layout",
            )
            os.write(menu_master, b"\x1b[B")
            selected_start = len(menu_output)
            wait_for_menu_frame(
                lambda frame: "  >  Open workspace" in frame,
                selected_start, 2.0, "the keyboard selection update",
            )
            os.write(menu_master, b"\r")
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline and menu_process.poll() is None:
                ready, _, _ = select.select([menu_master], [], [], 0.05)
                if ready:
                    try:
                        menu_output.extend(os.read(menu_master, 8192))
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
            if menu_process.poll() is None:
                menu_process.kill()
                menu_process.wait(timeout=2)
                raise SystemExit("TUI menu did not exit after Enter")
            # Drain bytes written immediately before process exit.
            for _ in range(5):
                ready, _, _ = select.select([menu_master], [], [], 0.02)
                if not ready:
                    break
                try:
                    menu_output.extend(os.read(menu_master, 8192))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
            if menu_process.returncode != 0:
                raise SystemExit(f"TUI menu exited with status {menu_process.returncode}")
            if b"Open workspace selected." not in menu_output:
                raise SystemExit("TUI menu did not activate the selected workspace item")
            menu_restored = termios.tcgetattr(menu_slave)
            if (menu_restored[3] & mask) != (menu_original[3] & mask):
                raise SystemExit("TUI menu did not restore terminal canonical/echo settings")
        finally:
            if menu_process is not None and menu_process.poll() is None:
                menu_process.kill()
                menu_process.wait(timeout=2)
            os.close(menu_master)
            os.close(menu_slave)

        print("YLang TUI raw-key, signal-restoration, resize, menu-navigation, and redirected-output tests passed.")
    finally:
        os.close(master)
        os.close(slave)


if __name__ == "__main__":
    main()
