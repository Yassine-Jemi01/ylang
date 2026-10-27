#!/usr/bin/env python3
"""Deterministic localhost HTTP fixture for YLang standard-library tests."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import sys


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, _format, *args):
        return

    def send_bytes(self, status, body, content_type="text/plain; charset=utf-8"):
        payload = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_connection_close()
        self.end_headers()
        self.wfile.write(payload)

    def send_connection_close(self):
        self.send_header("Connection", "close")

    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/redirect-target")
            self.send_header("Content-Length", "0")
            self.send_connection_close()
            self.end_headers()
        elif self.path == "/redirect-target":
            self.send_bytes(200, "redirect-target-reached")
        elif self.path == "/slow":
            import time
            time.sleep(0.2)
            self.send_bytes(200, "slow-response")
        elif self.path == "/hello":
            self.send_bytes(200, "hello")
        elif self.path == "/auth":
            if self.headers.get("Authorization") == "Bearer test-token":
                self.send_bytes(200, "authorized")
            else:
                self.send_bytes(401, "missing-auth")
        elif self.path == "/missing":
            self.send_bytes(404, "not-found")
        else:
            self.send_bytes(404, "unknown-path")

    def read_body(self):
        size = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(size).decode("utf-8")

    def do_POST(self):
        body = self.read_body()
        if self.path == "/echo":
            if self.headers.get("Content-Type") != "application/json":
                self.send_bytes(415, "wrong-content-type")
            else:
                self.send_bytes(200, body, "application/json; charset=utf-8")
        elif self.path == "/custom":
            self.send_bytes(200, self.headers.get("X-Test", "") + "|" + body)
        else:
            self.send_bytes(404, "unknown-path")

    def do_PUT(self):
        body = self.read_body()
        if self.path == "/echo" and self.headers.get("Content-Type") == "application/json":
            self.send_bytes(200, body, "application/json; charset=utf-8")
        else:
            self.send_bytes(404, "unknown-path")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: http_test_server.py PORT_FILE")
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    Path(sys.argv[1]).write_text(str(server.server_address[1]), encoding="ascii")
    try:
        server.serve_forever(poll_interval=0.1)
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
