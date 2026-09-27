#!/usr/bin/env python3
"""Short-lived LAN fixture for issue #22 HTTP/TLS failure-path measurement."""

from __future__ import annotations

import argparse
import socket
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        print(format % args, flush=True)

    def do_GET(self) -> None:
        if self.path == "/timeout":
            time.sleep(12)
        if self.path == "/truncated":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", "200")
            self.end_headers()
            self.wfile.write(b'{"current":')
            self.wfile.flush()
            self.close_connection = True
            return
        if self.path == "/oversize":
            body = b"x" * 1400
            status = 200
        elif self.path == "/malformed-json":
            body = b'{"current":'
            status = 200
        elif self.path == "/status/429":
            body = b'{"error":true,"reason":"fixture rate limit"}'
            status = 429
        elif self.path == "/status/503":
            body = b'{"error":true,"reason":"fixture unavailable"}'
            status = 503
        else:
            body = b'{"fixture":true}'
            status = 200
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", required=True)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    args = parser.parse_args()
    http = ThreadingHTTPServer((args.bind, 18822), Handler)
    https = ThreadingHTTPServer((args.bind, 18823), Handler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.cert, args.key)
    https.socket = context.wrap_socket(https.socket, server_side=True)
    for server in (http, https):
        threading.Thread(target=server.serve_forever, daemon=True).start()
    print(f"fault fixture ready on {args.bind}:18822 and :18823", flush=True)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        http.shutdown()
        https.shutdown()


if __name__ == "__main__":
    main()
