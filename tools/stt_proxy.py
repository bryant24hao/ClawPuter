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
import subprocess
import tempfile
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
SOCKS_PROXY = os.environ.get("STT_SOCKS_PROXY", "socks5h://127.0.0.1:1080")

if not GROQ_API_KEY:
    print("ERROR: GROQ_API_KEY not set. Export it or add to .env")
    sys.exit(1)


class ProxyHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        print(f"  Received {content_length} bytes from ESP32")

        # Write body to temp file for curl (avoids pipe size limits)
        try:
            with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as tmp:
                tmp.write(body)
                tmp_path = tmp.name

            # Forward to Groq via curl through SOCKS proxy
            result = subprocess.run(
                [
                    "curl", "-s",
                    "--proxy", SOCKS_PROXY,
                    "--max-time", "30",
                    "-X", "POST",
                    "https://api.groq.com/openai/v1/audio/transcriptions",
                    "-H", f"Authorization: Bearer {GROQ_API_KEY}",
                    "-H", f"Content-Type: {self.headers['Content-Type']}",
                    "--data-binary", f"@{tmp_path}",
                    "-w", "\n%{http_code}",
                ],
                capture_output=True,
                timeout=35,
            )

            os.unlink(tmp_path)

            output = result.stdout
            # Last line is the HTTP status code
            lines = output.rsplit(b"\n", 1)
            resp_body = lines[0] if len(lines) > 1 else output
            status = int(lines[-1]) if len(lines) > 1 and lines[-1].strip().isdigit() else 502

            # Ensure body ends with \n so ESP32's line-based reader can process it
            if resp_body and not resp_body.endswith(b"\n"):
                resp_body += b"\n"

            print(f"  Groq response: {status} {resp_body[:200].decode(errors='replace')}")

            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(resp_body)))
            self.end_headers()
            self.wfile.write(resp_body)
        except Exception as e:
            try:
                os.unlink(tmp_path)
            except Exception:
                pass
            msg = f'{{"error":{{"message":"{e}"}}}}'.encode()
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(msg)
            print(f"  Proxy error: {e}")

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
