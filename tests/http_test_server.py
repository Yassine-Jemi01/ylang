#!/usr/bin/env python3
"""Local deterministic HTTP fixture for the YLang HTTP runtime tests."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import sys


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def send_body(self, status, body):
        encoded = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(encoded)

    def do_GET(self):
        if self.path == "/hello":
            self.send_body(200, "hello")
        elif self.path == "/redirect":
            encoded = b"redirect"
            self.send_response(302)
            self.send_header("Location", "/hello")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            self.wfile.write(encoded)
        elif self.path == "/secret":
            if self.headers.get("X-API-Key") == "secret":
                self.send_body(200, "secret-ok")
            else:
                self.send_body(403, "unauthorized")
        elif self.path == "/missing":
            self.send_body(404, "missing")
        else:
            self.send_body(404, "unknown")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        if (
            self.path == "/post"
            and self.headers.get("Content-Type", "").startswith("application/json")
            and body == b'{"x":1}'
        ):
            self.send_body(201, "post-ok")
        else:
            self.send_body(400, "bad-post")

    def do_PUT(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        if self.path == "/update" and body == b"updated":
            self.send_body(202, "updated")
        else:
            self.send_body(400, "bad-put")

    def do_PATCH(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        if self.path == "/patch" and body == b"patched":
            self.send_body(203, "patched")
        else:
            self.send_body(400, "bad-patch")

    def do_DELETE(self):
        if self.path == "/delete":
            self.send_body(204, "")
        else:
            self.send_body(404, "unknown")

    def do_HEAD(self):
        if self.path == "/head":
            self.send_body(200, "head")
        else:
            self.send_body(404, "unknown")

    def log_message(self, _format, *_args):
        pass


if __name__ == "__main__":
    ready_file = Path(sys.argv[1])
    with HTTPServer(("127.0.0.1", 0), Handler) as server:
        ready_file.write_text(str(server.server_port), encoding="ascii")
        server.serve_forever()
