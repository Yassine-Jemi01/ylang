#!/usr/bin/env python3
"""Run the YLang HTTP integration test against an in-process loopback server."""
from pathlib import Path
import os
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
        compiler = "./build/ylang.exe" if os.name == "nt" else "./build/ylang"
        executable = "./build/test-http-client.exe" if os.name == "nt" else "./build/test-http-client"
        output_path = "build/test-http-client.exe" if os.name == "nt" else "build/test-http-client"
        run_checked([compiler, "check", "tests/http-client.yl"])
        run_checked([compiler, "build", "tests/http-client.yl", "-o", output_path])
        env = os.environ.copy()
        env["YLANG_TEST_API_KEY"] = "test-token"
        env.pop("YLANG_MISSING_TEST_VAR", None)
        result = subprocess.run(
            [executable],
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
        # The synchronous header callback must reject an append when its
        # accumulator is already exactly at the configured 16 KiB limit.
        header_source = ROOT / "build" / "http-header-overflow.yl"
        header_source.write_text(
            "fn main() -> int {\n"
            f'    let owned_string body = http.get("http://127.0.0.1:{port}/oversized-headers");\n'
            "    print(string.length(body));\n"
            '    print(string.contains(http.last_error(), "response headers exceeded"));\n'
            "    return 0;\n"
            "}\n",
            encoding="utf-8",
        )
        header_executable = "build/test-http-header-overflow.exe" if os.name == "nt" else "build/test-http-header-overflow"
        run_checked([compiler, "check", str(header_source.relative_to(ROOT))])
        run_checked([compiler, "build", str(header_source.relative_to(ROOT)), "-o", header_executable])
        header_result = subprocess.run(
            [str(ROOT / header_executable)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=20,
        )
        if header_result.returncode != 0 or header_result.stdout != "0\ntrue\n":
            sys.stderr.write(header_result.stdout)
            sys.stderr.write(header_result.stderr)
            raise SystemExit("HTTP oversized-header regression test failed")

        print("YLang HTTP integration tests passed.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
