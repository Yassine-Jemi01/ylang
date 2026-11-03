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
        # Pull-based HTTP streaming must preserve partial reads, yield delayed
        # chunks incrementally, expose per-stream metadata, and handle empty/404 bodies.
        stream_source = ROOT / "build" / "http-streaming.yl"
        stream_source.write_text(
            "fn main() -> int {\n"
            "    let string port = io.read_line();\n"
            '    let string base = string.concat("http://127.0.0.1:", port);\n'
            '    let http_stream stream = http.stream_open("GET", string.concat(base, "/stream"), "", "");\n'
            "    print(http.stream_read(stream, 3));\n"
            "    print(http.stream_read(stream, 3));\n"
            "    print(http.stream_read(stream, 3));\n"
            "    print(http.stream_read(stream, 3));\n"
            "    print(http.stream_read(stream, 3));\n"
            "    print(http.stream_read(stream, 3));\n"
            "    while (!http.stream_done(stream)) {\n"
            "        let owned_string rest = http.stream_read(stream, 16);\n"
            "        if (string.length(rest) > 0) { print(rest); }\n"
            "    }\n"
            "    print(http.stream_status(stream));\n"
            '    print(string.contains(http.stream_headers(stream), "Content-Type: text/plain; charset=utf-8"));\n'
            "    print(string.length(http.stream_error(stream)) == 0);\n"
            "    http.stream_close(stream);\n"
            '    let http_stream missing = http.stream_open("GET", string.concat(base, "/missing"), "", "");\n'
            "    print(http.stream_read(missing, 32));\n"
            "    while (!http.stream_done(missing)) {\n"
            "        let owned_string rest = http.stream_read(missing, 32);\n"
            "        if (string.length(rest) > 0) { print(rest); }\n"
            "    }\n"
            "    print(http.stream_status(missing));\n"
            "    http.stream_close(missing);\n"
            '    let http_stream empty = http.stream_open("GET", string.concat(base, "/stream-empty"), "", "");\n'
            "    print(string.length(http.stream_read(empty, 8)));\n"
            "    print(http.stream_done(empty));\n"
            "    print(http.stream_status(empty));\n"
            "    http.stream_close(empty);\\n"
            '    let http_stream large = http.stream_open("GET", string.concat(base, "/stream-large"), "", "");\\n'
            "    let mut total: int = 0;\\n"
            "    while (!http.stream_done(large)) {\\n"
            "        let owned_string chunk = http.stream_read(large, 8192);\\n"
            "        total = total + string.length(chunk);\\n"
            "    }\\n"
            "    print(total);\\n"
            "    print(http.stream_status(large));\\n"
            "    http.stream_close(large);\\n"
            "    return 0;\\n"
            "}\n",
            encoding="utf-8",
        )
        stream_executable = "build/test-http-streaming.exe" if os.name == "nt" else "build/test-http-streaming"
        run_checked([compiler, "check", str(stream_source.relative_to(ROOT))])
        run_checked([compiler, "build", str(stream_source.relative_to(ROOT)), "-o", stream_executable])
        stream_result = subprocess.run(
            [str(ROOT / stream_executable)],
            cwd=ROOT,
            input=f"{port}\\n",
            text=True,
            capture_output=True,
            timeout=20,
        )
        expected_stream = "fir\\nst-\\nsec\\nond\\nthir\\nd\\n200\\ntrue\\ntrue\\nnot-found\\n404\\n0\\ntrue\\n200\\n9437184\\n200\\n"
        if stream_result.returncode != 0 or stream_result.stdout != expected_stream:
            sys.stderr.write("--- expected stream output ---\\n" + expected_stream)
            sys.stderr.write("--- actual stream output ---\\n" + stream_result.stdout)
            sys.stderr.write(stream_result.stderr)
            raise SystemExit("HTTP streaming integration test failed")

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
