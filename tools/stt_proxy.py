#!/usr/bin/env python3
"""
Lightweight HTTP proxy for Whisper STT and Edge TTS.
ESP32 connects here over HTTP; STT can run locally with faster-whisper or forward to Groq,
TTS uses edge-tts locally and returns raw PCM audio.

Usage:
    python3 tools/stt_proxy.py

Set STT_BACKEND=local for local faster-whisper, or STT_BACKEND=groq for Groq.
Listens on port 8090 by default (change with STT_PROXY_PORT env var).
"""

import json
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
SOCKS_PROXY = os.environ.get("STT_SOCKS_PROXY", "")
STT_BACKEND = os.environ.get("STT_BACKEND", "groq").lower()
WHISPER_MODEL = os.environ.get("WHISPER_MODEL", "base")
WHISPER_DEVICE = os.environ.get("WHISPER_DEVICE", "cpu")
WHISPER_COMPUTE_TYPE = os.environ.get("WHISPER_COMPUTE_TYPE", "int8")
WHISPER_LANGUAGE = os.environ.get("WHISPER_LANGUAGE", "") or None
_whisper_model = None

if STT_BACKEND not in ("groq", "local", "whisper", "faster-whisper"):
    print(f"ERROR: unsupported STT_BACKEND={STT_BACKEND}")
    sys.exit(1)

if STT_BACKEND == "groq" and not GROQ_API_KEY:
    print("ERROR: GROQ_API_KEY not set. Export it or add to .env")
    sys.exit(1)


def extract_wav(body):
    start = body.find(b"RIFF")
    if start < 0 or start + 12 > len(body) or body[start + 8:start + 12] != b"WAVE":
        raise ValueError("WAV payload not found in multipart body")
    total_size = int.from_bytes(body[start + 4:start + 8], "little") + 8
    end = start + total_size
    if end > len(body):
        raise ValueError("Incomplete WAV payload")
    return body[start:end]


def transcribe_local(body):
    global _whisper_model

    if _whisper_model is None:
        try:
            from faster_whisper import WhisperModel
        except ImportError as e:
            raise RuntimeError("Install local STT dependency: pip install faster-whisper") from e
        print(f"  [STT] Loading faster-whisper model={WHISPER_MODEL} device={WHISPER_DEVICE} compute={WHISPER_COMPUTE_TYPE}")
        _whisper_model = WhisperModel(WHISPER_MODEL, device=WHISPER_DEVICE, compute_type=WHISPER_COMPUTE_TYPE)

    wav_data = extract_wav(body)
    with tempfile.NamedTemporaryFile(delete=False, suffix=".wav") as tmp:
        tmp.write(wav_data)
        tmp_path = tmp.name

    try:
        segments, _ = _whisper_model.transcribe(tmp_path, language=WHISPER_LANGUAGE, vad_filter=True)
        text = "".join(segment.text for segment in segments).strip()
        return json.dumps({"text": text}, ensure_ascii=False).encode() + b"\n"
    finally:
        os.unlink(tmp_path)


class ProxyHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == "/v1/audio/speech":
            return self.handle_tts()
        return self.handle_stt()

    def handle_tts(self):
        """TTS endpoint: POST {"text":"...", "voice":"..."} → raw PCM (s16le, 16kHz, mono)"""
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        try:
            req = json.loads(body)
            text = req.get("text", "").strip()
            voice = req.get("voice", "zh-CN-XiaoxiaoNeural")

            if not text:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(b'{"error":"empty text"}')
                return

            print(f"  [TTS] text={text[:60]}... voice={voice}")

            # Generate MP3 via edge-tts
            with tempfile.NamedTemporaryFile(delete=False, suffix=".mp3") as mp3_f:
                mp3_path = mp3_f.name
            with tempfile.NamedTemporaryFile(delete=False, suffix=".pcm") as pcm_f:
                pcm_path = pcm_f.name

            try:
                # Run edge-tts to produce MP3
                result = subprocess.run(
                    ["edge-tts", "--voice", voice, "--text", text, "--write-media", mp3_path],
                    capture_output=True, timeout=30,
                )
                if result.returncode != 0:
                    err = result.stderr.decode(errors="replace")[:200]
                    print(f"  [TTS] edge-tts failed: {err}")
                    self.send_response(500)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(f'{{"error":"edge-tts failed: {err}"}}'.encode())
                    return

                # Convert MP3 → raw PCM (signed 16-bit LE, 8kHz, mono)
                # 8kHz = telephone quality, fine for tiny Cardputer speaker.
                # Doubles duration capacity: 96KB buffer = 6s @ 8kHz vs 3s @ 16kHz.
                result = subprocess.run(
                    ["ffmpeg", "-y", "-i", mp3_path,
                     "-f", "s16le", "-acodec", "pcm_s16le",
                     "-ar", "8000", "-ac", "1", pcm_path],
                    capture_output=True, timeout=30,
                )
                if result.returncode != 0:
                    err = result.stderr.decode(errors="replace")[:200]
                    print(f"  [TTS] ffmpeg failed: {err}")
                    self.send_response(500)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(f'{{"error":"ffmpeg failed: {err}"}}'.encode())
                    return

                with open(pcm_path, "rb") as f:
                    pcm_data = f.read()

                # Truncate to ESP32 buffer size (96KB) with fade-out if needed
                MAX_PCM_BYTES = 160000  # 80000 samples × 2 bytes = 10s @ 8kHz
                if len(pcm_data) > MAX_PCM_BYTES:
                    import array
                    pcm_data = pcm_data[:MAX_PCM_BYTES]
                    # Fade out last 0.2s (1600 samples at 8kHz = 3200 bytes)
                    samples = array.array('h')
                    samples.frombytes(pcm_data)
                    fade_len = 1600
                    fade_start = len(samples) - fade_len
                    for i in range(fade_len):
                        samples[fade_start + i] = int(samples[fade_start + i] * (1.0 - i / fade_len))
                    pcm_data = samples.tobytes()
                    print(f"  [TTS] Truncated to {MAX_PCM_BYTES} bytes with fade-out")

                print(f"  [TTS] PCM: {len(pcm_data)} bytes ({len(pcm_data)/16000:.1f}s @ 8kHz)")

                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(len(pcm_data)))
                self.end_headers()
                self.wfile.write(pcm_data)

            finally:
                for p in (mp3_path, pcm_path):
                    try:
                        os.unlink(p)
                    except Exception:
                        pass

        except json.JSONDecodeError:
            self.send_response(400)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"error":"invalid JSON"}')
        except Exception as e:
            print(f"  [TTS] Error: {e}")
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(f'{{"error":"{e}"}}'.encode())

    def handle_stt(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        print(f"  Received {content_length} bytes from ESP32")

        if STT_BACKEND in ("local", "whisper", "faster-whisper"):
            try:
                resp_body = transcribe_local(body)
                print(f"  Local Whisper response: {resp_body[:200].decode(errors='replace')}")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(resp_body)))
                self.end_headers()
                self.wfile.write(resp_body)
            except Exception as e:
                msg = json.dumps({"error": {"message": str(e)}}).encode()
                self.send_response(502)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(msg)
                print(f"  Local Whisper error: {e}")
            return

        tmp_path = None
        try:
            with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as tmp:
                tmp.write(body)
                tmp_path = tmp.name

            cmd = ["curl", "-s"]
            if SOCKS_PROXY:
                cmd += ["--proxy", SOCKS_PROXY]
            cmd += [
                "--max-time", "30",
                "-X", "POST",
                "https://api.groq.com/openai/v1/audio/transcriptions",
                "-H", f"Authorization: Bearer {GROQ_API_KEY}",
                "-H", f"Content-Type: {self.headers['Content-Type']}",
                "--data-binary", f"@{tmp_path}",
                "-w", "\n%{http_code}",
            ]
            result = subprocess.run(cmd, capture_output=True, timeout=35)

            os.unlink(tmp_path)
            tmp_path = None

            output = result.stdout
            lines = output.rsplit(b"\n", 1)
            resp_body = lines[0] if len(lines) > 1 else output
            status = int(lines[-1]) if len(lines) > 1 and lines[-1].strip().isdigit() else 502

            if resp_body and not resp_body.endswith(b"\n"):
                resp_body += b"\n"

            print(f"  Groq response: {status} {resp_body[:200].decode(errors='replace')}")

            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(resp_body)))
            self.end_headers()
            self.wfile.write(resp_body)
        except Exception as e:
            if tmp_path:
                try:
                    os.unlink(tmp_path)
                except Exception:
                    pass
            msg = json.dumps({"error": {"message": str(e)}}).encode()
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(msg)
            print(f"  Proxy error: {e}")

    def log_message(self, fmt, *args):
        print(f"[STT Proxy] {args[0]}")


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", PORT), ProxyHandler)
    print(f"STT/TTS Proxy listening on 0.0.0.0:{PORT}")
    print(f"STT backend: {STT_BACKEND}")
    if STT_BACKEND == "groq":
        print(f"Groq key: {'set' if GROQ_API_KEY else 'NOT SET'} ({len(GROQ_API_KEY)} chars)")
        print(f"  STT: POST /v1/audio/transcriptions → Groq")
    else:
        print(f"  STT: POST /v1/audio/transcriptions → faster-whisper {WHISPER_MODEL}")
    print(f"  TTS: POST /v1/audio/speech → edge-tts + ffmpeg")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
