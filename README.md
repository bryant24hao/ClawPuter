# Cardputer Companion 🦞

[中文](README_CN.md)

A pixel-art desktop companion running on M5Stack Cardputer (ESP32-S3). Features an OpenClaw-themed lobster character with animations, day/night clock, and AI chat — powered by [OpenClaw](https://github.com/openclaw/openclaw).

## Features

- **Companion Mode** — Pixel lobster with idle, happy, sleep, talk, stretch, and look-around animations. Day/night background with stars and moon. NTP clock display.
- **Chat Mode** — Full keyboard input, AI conversation via OpenClaw Gateway (OpenAI-compatible API), scrollable message history with word-wrap.
- **OpenClaw Integration** — Connects to your local OpenClaw Gateway over LAN. Multi-model fallback (Kimi/Claude/GPT/Gemini), persistent memory, 5400+ community skills.
- **Boot Animation** — Lobster pixel-art line-by-line reveal with pixel wipe transitions between modes.
- **Sound Effects** — Key clicks, happy melody, notification tones via built-in speaker.

## Quick Start

### 1. Set Environment Variables

```bash
export WIFI_SSID="your_wifi_name"
export WIFI_PASS="your_wifi_password"
export OPENCLAW_HOST="192.168.1.100"   # your Mac/VPS LAN IP
export OPENCLAW_PORT="18789"
export OPENCLAW_TOKEN="your_gateway_token"
```

### 2. Build & Flash

```bash
pio run -t upload
```

First-time flashing may require download mode: hold **G0** + press **Reset**, then release G0. See [Setup Guide](docs/setup-and-flash.md) for details.

### 3. Serial Debug

```bash
pio device monitor
```

## Controls

| Key | Companion Mode | Chat Mode |
|-----|---------------|-----------|
| TAB | Switch to chat | Switch to companion |
| Space / Enter | Happy animation | Send message |
| Backspace | — | Delete character |
| Fn + ; | — | Scroll up |
| Fn + / | — | Scroll down |
| Fn + R | Reset config | — |
| Any key | Wake up lobster | Type character |

## Project Structure

```
src/
├── main.cpp          # Entry point, mode dispatch, WiFi/NTP
├── companion.h/cpp   # Companion mode: animations, state machine, clock
├── chat.h/cpp        # Chat mode: messages, input bar, scrolling
├── ai_client.h/cpp   # OpenClaw Gateway client (OpenAI-compatible)
├── sprites.h         # Pixel lobster sprites (RGB565)
├── config.h/cpp      # WiFi/API config, NVS persistence
└── utils.h           # Colors, screen constants, Timer
```

## Hardware

- **M5Stack Cardputer** — ESP32-S3, 240×135 IPS display, 56-key keyboard
- ESP32-S3 supports **2.4GHz WiFi only** (no 5GHz)

## OpenClaw Setup

This project connects to an OpenClaw Gateway running on your Mac or VPS. To set up:

1. [Install OpenClaw](https://openclaw.ai)
2. Enable LAN binding and HTTP API in `~/.openclaw/openclaw.json`:
   ```json
   {
     "gateway": {
       "bind": "lan",
       "http": {
         "endpoints": {
           "chatCompletions": { "enabled": true }
         }
       }
     }
   }
   ```
3. Restart gateway: `openclaw gateway restart`
4. Set `OPENCLAW_HOST` to your Mac's LAN IP

See [OpenClaw Research](docs/openclaw-research.md) for the full integration guide.

## Documentation

- [Setup & Flash Guide](docs/setup-and-flash.md)
- [Hardware Notes](docs/hardware-notes.md)
- [API Integration Story](docs/api-integration.md)
- [Architecture](docs/architecture.md)
- [OpenClaw Integration](docs/openclaw-research.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Roadmap](docs/roadmap.md)

## Roadmap

- [ ] Battery display with low-power character animation
- [ ] Chat history persistence (NVS/SD card)
- [ ] Streaming responses (SSE)
- [ ] Pet system (hunger/mood mechanics)
- [ ] Pomodoro timer
- [ ] Weather display
- [ ] BLE phone notifications

See [full roadmap](docs/roadmap.md).

## License

MIT
