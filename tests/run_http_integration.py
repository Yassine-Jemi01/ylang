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
        # Pull-based streaming: partial chunks, final metadata, EOF, 404 bodies,
        # empty responses, and a payload larger than the buffered HTTP body cap.
        stream_source = ROOT / "build" / "http-streaming.yl"
        stream_source.write_text(
            "function main() -> int {\\n"
            "    let string port = io.read_line();\\n"
            '    let string base = string.concat("http://127.0.0.1:", port);\\n'
            '    let http_stream stream = http.stream_open("GET", string.concat(base, "/stream"), "", "");\\n'
            "    print(http.stream_read(stream, 3));\\n"
            "    print(http.stream_read(stream, 3));\\n"
            "    print(http.stream_read(stream, 3));\\n"
            "    print(http.stream_read(stream, 3));\\n"
            "    print(http.stream_read(stream, 3));\\n"
            "    print(http.stream_read(stream, 3));\\n"
            "    while (!http.stream_done(stream)) {\\n"
            "        let owned_string rest = http.stream_read(stream, 16);\\n"
            "        if (string.length(rest) > 0) { print(rest); }\\n"
            "    }\\n"
            "    print(http.stream_status(stream));\\n"
            '    print(string.contains(http.stream_headers(stream), "Content-Type: text/plain; charset=utf-8"));\\n'
            "    print(string.length(http.stream_error(stream)) == 0);\\n"
            "    http.stream_close(stream);\\n"
            '    let http_stream missing = http.stream_open("GET", string.concat(base, "/missing"), "", "");\\n'
            "    print(http.stream_read(missing, 32));\\n"
            "    while (!http.stream_done(missing)) {\\n"
            "        let owned_string rest = http.stream_read(missing, 32);\\n"
            "        if (string.length(rest) > 0) { print(rest); }\\n"
            "    }\\n"
            "    print(http.stream_status(missing));\\n"
            "    http.stream_close(missing);\\n"
            '    let http_stream empty = http.stream_open("GET", string.concat(base, "/stream-empty"), "", "");\\n'
            "    print(string.length(http.stream_read(empty, 8)));\\n"
            "    print(http.stream_done(empty));\\n"
            "    print(http.stream_status(empty));\\n"
            "    http.stream_close(empty);\\n"
            '    let http_stream large = http.stream_open("GET", string.concat(base, "/stream-large"), "", "");\\n'
            "    let int total = 0;\\n"
            "    while (!http.stream_done(large)) {\\n"
            "        let owned_string chunk = http.stream_read(large, 8192);\\n"
            "        total = total + string.length(chunk);\\n"
            "    }\\n"
            "    print(total);\\n"
            "    print(http.stream_status(large));\\n"
            "    http.stream_close(large);\\n"
            "    return 0;\\n"
            "}\\n",
            encoding="utf-8",
        )
        stream_output = "build/test-http-streaming.exe" if os.name == "nt" else "build/test-http-streaming"
        run_checked([compiler, "check", str(stream_source.relative_to(ROOT))])
        run_checked([compiler, "build", str(stream_source.relative_to(ROOT)), "-o", stream_output])
        stream_result = subprocess.run(
            [str(ROOT / stream_output)],
            cwd=ROOT,
            input=f"{port}\\n",
            text=True,
            capture_output=True,
            timeout=30,
        )
        expected_stream = "fir\\nst-\\nsec\\nond\\nthir\\nd\\n200\\ntrue\\ntrue\\nnot-found\\n404\\n0\\ntrue\\n200\\n9437184\\n200\\n"
        if stream_result.returncode != 0 or stream_result.stdout != expected_stream:
            sys.stderr.write("--- expected streaming output ---\\n" + expected_stream)
            sys.stderr.write("--- actual streaming output ---\\n" + stream_result.stdout)
            sys.stderr.write(stream_result.stderr)
            raise SystemExit("HTTP streaming integration test failed")
        # Binary streaming must preserve embedded NUL and high-bit bytes.
        binary_source = ROOT / "build" / "http-stream-binary.yl"
        binary_source.write_text(
            "function main() -> int {\\n"
            "    let string port = io.read_line();\\n"
            '    let string url = string.concat("http://127.0.0.1:", port);\\n'
            '    let http_stream stream = http.stream_open("GET", string.concat(url, "/stream-binary"), "", "");\\n'
            "    let int buffer = io.buffer_new(0);\\n"
            "    let int count = http.stream_read_buffer(stream, buffer, 32);\\n"
            "    print(count);\\n"
            "    let int index = 0;\\n"
            "    while (index < count) {\\n"
            "        print(io.buffer_get(buffer, index));\\n"
            "        index = index + 1;\\n"
            "    }\\n"
            "    while (!http.stream_done(stream)) {\\n"
            "        http.stream_read_buffer(stream, buffer, 32);\\n"
            "    }\\n"
            "    print(http.stream_status(stream));\\n"
            "    io.buffer_free(buffer);\\n"
            "    http.stream_close(stream);\\n"
            "    return 0;\\n"
            "}\\n",
            encoding="utf-8",
        )
        binary_output = "build/test-http-stream-binary.exe" if os.name == "nt" else "build/test-http-stream-binary"
        run_checked([compiler, "check", str(binary_source.relative_to(ROOT))])
        run_checked([compiler, "build", str(binary_source.relative_to(ROOT)), "-o", binary_output])
        binary_result = subprocess.run(
            [str(ROOT / binary_output)],
            cwd=ROOT,
            input=f"{port}\\n",
            text=True,
            capture_output=True,
            timeout=20,
        )
        expected_binary = "8\\n0\\n1\\n255\\n65\\n66\\n67\\n0\\n254\\n200\\n"
        if binary_result.returncode != 0 or binary_result.stdout != expected_binary:
            sys.stderr.write("--- expected binary stream output ---\\n" + expected_binary)
            sys.stderr.write("--- actual binary stream output ---\\n" + binary_result.stdout)
            sys.stderr.write(binary_result.stderr)
            raise SystemExit("binary HTTP streaming integration test failed")

        # If SDL2_mixer is installed, verify the compiler can hand an HTTP
        # stream directly to the audio decoder without staging a local file.
        mixer_available = subprocess.run(
            ["pkg-config", "--exists", "SDL2_mixer"],
            cwd=ROOT,
            check=False,
        ).returncode == 0
        if mixer_available:
            voice_source = ROOT / "build" / "http-voice-stream.yl"
            voice_source.write_text(
                "function main() -> int {\\n"
                "    let string port = io.read_line();\\n"
                '    let string base = string.concat("http://127.0.0.1:", port);\\n'
                '    let http_stream stream = http.stream_open("GET", string.concat(base, "/stream-wav"), "", "");\\n'
                "    if (!voice.play_stream(stream)) { print(voice.last_error()); return 1; }\\n"
                "    while (!voice.is_finished()) { tui.sleep_ms(5); }\\n"
                "    print(voice.is_finished());\\n"
                "    print(voice.last_error());\\n"
                "    voice.shutdown();\\n"
                "    return 0;\\n"
                "}\\n",
                encoding="utf-8",
            )
            voice_output = "build/test-http-voice-stream.exe" if os.name == "nt" else "build/test-http-voice-stream"
            run_checked([compiler, "check", str(voice_source.relative_to(ROOT))])
            run_checked([compiler, "build", str(voice_source.relative_to(ROOT)), "-o", voice_output])
            voice_env = os.environ.copy()
            voice_env["SDL_AUDIODRIVER"] = "dummy"
            voice_result = subprocess.run(
                [str(ROOT / voice_output)],
                cwd=ROOT,
                input=f"{port}\\n",
                text=True,
                capture_output=True,
                env=voice_env,
                timeout=20,
            )
            expected_voice = "true\\nNo error\\n"
            if voice_result.returncode != 0 or voice_result.stdout != expected_voice:
                sys.stderr.write("--- expected HTTP voice output ---\\n" + expected_voice)
                sys.stderr.write("--- actual HTTP voice output ---\\n" + voice_result.stdout)
                sys.stderr.write(voice_result.stderr)
                raise SystemExit("HTTP-to-Voice streaming integration test failed")
        else:
            print("SKIP: SDL2_mixer is unavailable; HTTP-to-Voice integration was not run.")
        print("YLang HTTP integration tests passed.")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
