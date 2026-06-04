#!/usr/bin/env python3
import io
import json
import os
import struct
import sys
from types import SimpleNamespace

import pytest

os.environ["STT_BACKEND"] = "local"
os.environ["WHISPER_LANGUAGE"] = "en"

sys.path.insert(0, os.path.dirname(__file__))
import stt_proxy  # noqa: E402


BOUNDARY = "----VoiceBoundary1234"


def make_wav(samples):
    pcm = b"".join(struct.pack("<h", sample) for sample in samples)
    header = bytearray(44)
    data_size = len(pcm)
    file_size = 36 + data_size
    byte_rate = 16000 * 2

    header[0:4] = b"RIFF"
    header[4:8] = struct.pack("<I", file_size)
    header[8:12] = b"WAVE"
    header[12:16] = b"fmt "
    header[16:20] = struct.pack("<I", 16)
    header[20:22] = struct.pack("<H", 1)
    header[22:24] = struct.pack("<H", 1)
    header[24:28] = struct.pack("<I", 16000)
    header[28:32] = struct.pack("<I", byte_rate)
    header[32:34] = struct.pack("<H", 2)
    header[34:36] = struct.pack("<H", 16)
    header[36:40] = b"data"
    header[40:44] = struct.pack("<I", data_size)
    return bytes(header) + pcm


def make_cardputer_multipart(wav):
    part_header = (
        f"--{BOUNDARY}\r\n"
        'Content-Disposition: form-data; name="file"; filename="recording.wav"\r\n'
        "Content-Type: audio/wav\r\n\r\n"
    ).encode()
    part_footer = (
        f"\r\n--{BOUNDARY}\r\n"
        'Content-Disposition: form-data; name="model"\r\n\r\n'
        "whisper-large-v3-turbo\r\n"
        f"--{BOUNDARY}--\r\n"
    ).encode()
    return part_header + wav + part_footer


def run_stt_handler(body):
    handler = stt_proxy.ProxyHandler.__new__(stt_proxy.ProxyHandler)
    handler.headers = {"Content-Length": str(len(body))}
    handler.rfile = io.BytesIO(body)
    handler.wfile = io.BytesIO()
    handler.status = None
    handler.response_headers = []

    def send_response(status):
        handler.status = status

    def send_header(name, value):
        handler.response_headers.append((name, value))

    handler.send_response = send_response
    handler.send_header = send_header
    handler.end_headers = lambda: None
    handler.handle_stt()
    return handler.status, dict(handler.response_headers), handler.wfile.getvalue()


class FakeWhisperModel:
    def __init__(self):
        self.wav_data = None
        self.language = None
        self.vad_filter = None

    def transcribe(self, wav_path, language=None, vad_filter=False):
        with open(wav_path, "rb") as wav_file:
            self.wav_data = wav_file.read()
        self.language = language
        self.vad_filter = vad_filter
        return [SimpleNamespace(text=" hello"), SimpleNamespace(text=" world ")], object()


@pytest.fixture
def fake_whisper_model():
    model = FakeWhisperModel()
    stt_proxy.STT_BACKEND = "local"
    stt_proxy.WHISPER_LANGUAGE = "en"
    stt_proxy._whisper_model = model
    try:
        yield model
    finally:
        stt_proxy._whisper_model = None


def test_cardputer_multipart_wav_reaches_local_whisper_backend(fake_whisper_model):
    wav = make_wav([0, 1200, -1200, 300, -300])
    body = make_cardputer_multipart(wav)

    status, headers, response_body = run_stt_handler(body)

    assert status == 200
    assert headers["Content-Type"] == "application/json"
    assert response_body[-1:] == b"\n"
    assert json.loads(response_body) == {"text": "hello world"}
    assert fake_whisper_model.wav_data == wav
    assert fake_whisper_model.language == "en"
    assert fake_whisper_model.vad_filter is True


@pytest.mark.integration
def test_cardputer_multipart_wav_with_real_faster_whisper(tmp_path, monkeypatch):
    if os.environ.get("RUN_REAL_FASTER_WHISPER_TEST") != "1":
        pytest.skip("set RUN_REAL_FASTER_WHISPER_TEST=1 to run real faster-whisper")

    hf_cache = tmp_path / "hf-cache"
    monkeypatch.setenv("HF_HOME", str(tmp_path / "hf-home"))
    monkeypatch.setenv("HF_HUB_CACHE", str(hf_cache))
    monkeypatch.setenv("HUGGINGFACE_HUB_CACHE", str(hf_cache))

    pytest.importorskip("faster_whisper")

    stt_proxy.STT_BACKEND = "local"
    stt_proxy.WHISPER_MODEL = os.environ.get("WHISPER_TEST_MODEL", "tiny")
    stt_proxy.WHISPER_DEVICE = os.environ.get("WHISPER_TEST_DEVICE", "cpu")
    stt_proxy.WHISPER_COMPUTE_TYPE = os.environ.get("WHISPER_TEST_COMPUTE_TYPE", "int8")
    stt_proxy.WHISPER_LANGUAGE = os.environ.get("WHISPER_TEST_LANGUAGE", "en")
    stt_proxy._whisper_model = None

    # Silence is deterministic and should produce an empty transcript while still
    # exercising the real faster-whisper model loading and transcribe path.
    wav = make_wav([0] * 16000)
    body = make_cardputer_multipart(wav)

    try:
        status, headers, response_body = run_stt_handler(body)
    finally:
        stt_proxy._whisper_model = None

    assert status == 200
    assert headers["Content-Type"] == "application/json"
    assert response_body[-1:] == b"\n"
    payload = json.loads(response_body)
    assert set(payload) == {"text"}
    assert isinstance(payload["text"], str)
