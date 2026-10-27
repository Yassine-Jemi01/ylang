#!/usr/bin/env python3
"""Run the YLang HTTP integration test against an in-process loopback server."""
from pathlib import Path
import subprocess
import sys
import threading

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from http_test_server import Handler, ThreadingHTTPServer  # noqa: E402


def run_checked(command, *, cwd=ROOT, **kwargs):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, **kwargs)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"command failed ({result.returncode}): {' '.join(map(str, command))}")
    return result


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True)
    thread.start()
    try:
        port = server.server_address[1]
        run_checked(["./build/ylang", "check", "tests/http-client.yl"])
        run_checked(["./build/ylang", "build", "tests/http-client.yl", "-o", "build/test-http-client"])
        import os
        env = os.environ.copy()
        env["YLANG_TEST_API_KEY"] = "test-token"
        env.pop("YLANG_MISSING_TEST_VAR", None)
        result = subprocess.run(
            ["./build/test-http-client"],
            cwd=ROOT,
            input=f"{port}\n",
            text=True,
            capture_output=True,
            env=env,
            timeout=20,
        )
        if result.returncode != 0:
            sys.stderr.write(result.stdout)
            sys.stderr.write(result.stderr)
            raise SystemExit(f"HTTP test program failed with exit {result.returncode}")
        expected = (ROOT / "tests" / "expected-http-client.txt").read_text(encoding="utf-8")
        if result.stdout != expected:
            sys.stderr.write("HTTP test output did not match expected output.\n")
            sys.stderr.write("--- expected ---\n" + expected)
            sys.stderr.write("--- actual ---\n" + result.stdout)
            sys.stderr.write(result.stderr)
            raise SystemExit(1)
        print("YLang HTTP integration tests passed.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
