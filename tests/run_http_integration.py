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
        # Fill the synchronous response-header accumulator to exactly 16 KiB,
        # then send one more header. The runtime must reject it safely.
        header_source = ROOT / "build" / "http-header-limit.yl"
        header_source.write_text(
            'function main() -> int {\n'
            '    let string port = io.read_line();\n'
            '    let string path = string.concat("http://127.0.0.1:", string.concat(port, "/oversized-headers"));\n'
            '    let owned_string response = http.get(path);\n'
            '    if (string.length(http.last_error()) == 0) { return 1; }\n'
            '    return 0;\n'
            '}\n',
            encoding="utf-8",
        )
        header_executable = "build/http-header-limit.exe" if os.name == "nt" else "build/http-header-limit"
        header_source_rel = str(header_source.relative_to(ROOT))
        run_checked([compiler, "check", header_source_rel])
        run_checked([compiler, "build", header_source_rel, "-o", header_executable])
        header_result = subprocess.run(
            [header_executable],
            cwd=ROOT,
            input=f"{port}\n",
            text=True,
            capture_output=True,
            env=env,
            timeout=20,
        )
        if header_result.returncode != 0:
            sys.stderr.write(header_result.stdout)
            sys.stderr.write(header_result.stderr)
            raise SystemExit("Oversized HTTP response headers were not rejected safely.")
        print("YLang HTTP header-limit regression passed.")
        print("YLang HTTP integration tests passed.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
