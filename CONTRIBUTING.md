# Contributing to ClawPuter

Thanks for your interest! Here's how to contribute.

## Development Setup

1. **Hardware**: M5Stack Cardputer (ESP32-S3)
2. **Toolchain**: [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
3. **Build**:
   ```bash
   cp .env.example .env   # fill in your credentials
   source .env && pio run -t upload
   ```

## Project Structure

```
src/
├── main.cpp           # Entry point, mode management, WiFi/NTP setup
├── companion.h/cpp    # Pet character, animations, weather rendering
├── chat.h/cpp         # Chat UI, pixel art, SSE streaming
├── sprites.h          # 16x16 pixel art sprite data (RGB565)
├── ai_client.h/cpp    # OpenClaw Gateway API client
├── voice_input.h/cpp  # Push-to-talk Groq Whisper STT
├── tts_playback.h/cpp # Edge TTS speaker output
├── weather_client.h/cpp # Open-Meteo weather API
├── config.h/cpp       # Runtime config & setup wizard
├── cmd_server.h/cpp   # Network command server (desktop sync)
├── state_broadcast.h/cpp # UDP state broadcast
└── utils.h            # Color helpers, Timer, text filter
desktop/               # macOS companion app (Swift)
tools/                 # STT proxy, utilities
docs/                  # Design docs, architecture, lessons
```

## Guidelines

- **ESP32 memory rules**: No heap allocation in the render loop (60fps). Use `char[]` + `snprintf`, never `String` or `JsonDocument` in hot paths. See [docs/esp32-voice-chat-lessons.md](docs/esp32-voice-chat-lessons.md).
- **Keep it small**: The Cardputer has a 240x135 screen and limited RAM. Prefer simple, focused changes.
- **Test on hardware**: If you have a Cardputer, test your changes on real hardware. If not, `pio run` (compile-only) still catches most issues.
- **No secrets**: Never hardcode IPs, passwords, API keys, or WiFi SSIDs. Use `${sysenv.VAR}` in `platformio.ini` and environment variables.

## Submitting Changes

1. Fork the repo and create a feature branch
2. Make your changes
3. Run `pio run` to verify it compiles
4. Open a Pull Request with a clear description of what and why

## Reporting Bugs

Open an [issue](../../issues) with:
- What you expected vs. what happened
- Steps to reproduce
- Serial monitor output if relevant (`pio device monitor`)

## Ideas & Feature Requests

Check the [roadmap](docs/roadmap.md) first — your idea might already be planned. If not, open an issue tagged as a feature request.
