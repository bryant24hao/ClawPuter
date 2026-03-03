#!/usr/bin/env python3
"""
Lightweight HTTP→HTTPS proxy for Groq Whisper STT API.
ESP32 connects here over HTTP; this script forwards to Groq over HTTPS.

Usage:
    python3 tools/stt_proxy.py

Requires GROQ_API_KEY environment variable (reads from .env if present).
Listens on port 8090 by default (change with STT_PROXY_PORT env var).
"""

import os
import sys
import ssl
import http.client
from http.server import HTTPServer, BaseHTTPRequestHandler

# Load .env file if present
env_file = os.path.join(os.path.dirname(os.path.dirname(__file__)), ".env")
if os.path.exists(env_file):
    with open(env_file) as f:
        for line in f:
            line = line.strip()
            if line.startswith("export ") and "=" in line:
                key, val = line[7:].split("=", 1)
                val = val.strip('"').strip("'")
                os.environ.setdefault(key, val)

GROQ_API_KEY = os.environ.get("GROQ_API_KEY", "")
PORT = int(os.environ.get("STT_PROXY_PORT", "8090"))

if not GROQ_API_KEY:
    print("ERROR: GROQ_API_KEY not set. Export it or add to .env")
    sys.exit(1)


class ProxyHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        print(f"  Received {content_length} bytes from ESP32")

        # Forward to Groq using http.client for reliable binary handling
        ctx = ssl.create_default_context()
        conn = http.client.HTTPSConnection("api.groq.com", context=ctx)

        try:
            conn.request(
                "POST",
                "/openai/v1/audio/transcriptions",
                body=body,
                headers={
                    "Authorization": f"Bearer {GROQ_API_KEY}",
                    "Content-Type": self.headers["Content-Type"],
                    "Content-Length": str(len(body)),
                    "User-Agent": "curl/8.7.1",
                },
            )

            resp = conn.getresponse()
            resp_body = resp.read()

            print(f"  Groq response: {resp.status} {resp_body[:200].decode(errors='replace')}")

            self.send_response(resp.status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(resp_body)))
            self.end_headers()
            self.wfile.write(resp_body)
        except Exception as e:
            msg = f'{{"error":{{"message":"{e}"}}}}'.encode()
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(msg)
            print(f"  Proxy error: {e}")
        finally:
            conn.close()

    def log_message(self, fmt, *args):
        print(f"[STT Proxy] {args[0]}")


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", PORT), ProxyHandler)
    print(f"STT Proxy listening on 0.0.0.0:{PORT}")
    print(f"Groq key: {GROQ_API_KEY[:10]}...")
    print(f"Forwarding to: api.groq.com/openai/v1/audio/transcriptions")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
