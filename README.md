# Cardputer Companion 🦞

[中文](README_CN.md)

A pixel-art desktop companion running on M5Stack Cardputer (ESP32-S3). Features an OpenClaw-themed lobster character with animations, day/night clock, AI chat with voice input, and a synced macOS desktop pet.

## Features

- **Companion Mode** — Pixel lobster with idle, happy, sleep, talk, stretch, and look-around animations. Movable with keyboard (hold to walk). Day/night background with time-travel sky (walk left = past, right = future). NTP clock display.
- **Chat Mode** — Full keyboard input, AI conversation with SSE streaming (token-by-token display), scrollable message history with word-wrap.
- **Voice Input** — Push-to-talk via Fn key, speech-to-text via Groq Whisper API (through a local proxy server).
- **Desktop Pet Sync** — macOS companion app receives lobster state and position over UDP, rendering a synced desktop pet on your Mac. Move the pet on Cardputer and it moves on your desktop too.
- **OpenClaw Integration** — Connects to your local OpenClaw Gateway over LAN. Multi-model fallback (Kimi/Claude/GPT/Gemini), persistent memory, 5400+ community skills.
- **Dual WiFi + Offline Mode** — Auto-fallback to secondary WiFi (e.g. phone hotspot). Gateway host switches automatically. Offline mode if all WiFi fails (companion works, chat shows offline).
- **Runtime Config** — Setup wizard to configure WiFi, Gateway, STT host at runtime. Build-time values as defaults, Fn+R to reset.
- **Boot Animation** — Lobster pixel-art line-by-line reveal with pixel wipe transitions between modes.
- **Sound Effects** — Key clicks, happy melody, typing chirps during AI streaming, notification tones.

## Quick Start

### 1. Set Environment Variables

```bash
# WiFi
export WIFI_SSID="<your-wifi-ssid>"
export WIFI_PASS="<your-wifi-password>"

# AI Backend (OpenClaw Gateway)
export OPENCLAW_HOST="<your-host-ip>"       # OpenClaw Gateway LAN IP
export OPENCLAW_PORT="<your-port>"           # Gateway port
export OPENCLAW_TOKEN="<your-gateway-token>"

# Voice Input (optional, for stt_proxy.py)
export GROQ_API_KEY="<your-groq-api-key>"    # Used by tools/stt_proxy.py, not the firmware
export STT_PROXY_HOST="<your-host-ip>"       # Machine running stt_proxy.py
export STT_PROXY_PORT="8090"                 # STT proxy port (default 8090)

# Secondary WiFi (optional, for phone hotspot fallback)
export WIFI_SSID2="<your-hotspot-ssid>"
export WIFI_PASS2="<your-hotspot-password>"
export OPENCLAW_HOST2="<your-hotspot-host-ip>"  # Mac's IP on the hotspot network
```

### 2. Build & Flash

```bash
pio run -t upload
```

First-time flashing may require download mode: hold **G0** + press **Reset**, then release G0. See [Setup Guide](docs/setup-and-flash.md) for details.

### 3. Start STT Proxy (for voice input)

```bash
python3 tools/stt_proxy.py
```

The proxy runs on your Mac/PC, relays audio from the Cardputer to Groq Whisper API, and returns transcriptions. Requires `GROQ_API_KEY` in your `.env` or environment.

### 4. Serial Debug

```bash
pio device monitor
```

## Controls

| Key | Companion Mode | Chat Mode |
|-----|---------------|-----------|
| TAB | Switch to chat | Switch to companion |
| `,` (hold) | Move left | — |
| `/` (hold) | Move right | — |
| `;` (hold) | Move up | — |
| `.` (hold) | Move down | — |
| Space / Enter | Happy animation | Send message |
| Backspace | — | Delete character |
| Fn (hold) | — | Push-to-talk voice input |
| Fn + ; | — | Scroll up |
| Fn + / | — | Scroll down |
| Fn + R | Reset config + setup wizard | — |
| TAB (in setup) | Exit setup, return to companion | — |
| Any key | Wake up lobster | Type character |

## Project Structure

```
src/
├── main.cpp              # Entry point, mode dispatch, WiFi/NTP
├── companion.h/cpp       # Companion mode: animations, state machine, clock
├── chat.h/cpp            # Chat mode: messages, input bar, scrolling
├── ai_client.h/cpp       # AI client (OpenClaw/Claude), SSE streaming
├── voice_input.h/cpp     # Push-to-talk recording, WAV encoding, STT proxy client
├── state_broadcast.h/cpp # UDP state broadcast for desktop pet sync
├── sprites.h             # Pixel lobster sprites (RGB565)
├── config.h/cpp          # WiFi/API config, NVS persistence
└── utils.h               # Colors, screen constants, Timer

desktop/
└── CardputerDesktopPet/  # macOS desktop pet (Swift, receives UDP state)

tools/
└── stt_proxy.py          # Local HTTP proxy: ESP32 audio → Groq Whisper API
```

## iPhone Hotspot Tips

If using an iPhone hotspot as secondary WiFi:

1. **Enable "Maximize Compatibility"** — Go to Settings > Personal Hotspot and turn it on. iPhone defaults to 5GHz; ESP32 only supports 2.4GHz.
2. **Keep hotspot settings page open** — iPhone hotspot goes dormant when no devices are connected. The Cardputer won't find it unless the Personal Hotspot settings screen is open on the iPhone.
3. **UDP broadcast is blocked** — iPhone hotspot isolates clients, blocking UDP broadcast between devices. The firmware works around this by sending both broadcast and unicast (directly to the gateway host IP).
4. **Find your Mac's hotspot IP** — When your Mac connects to the iPhone hotspot, check its IP with `ifconfig en0` (usually `172.20.10.x`). Set this as `OPENCLAW_HOST2`.

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
- [Voice Input Design](docs/voice-input-design.md)
- [Desktop Pet Design](docs/desktop-pet-design.md)
- [UDP State Sync Design](docs/udp-state-sync-design.md)
- [ESP32 Memory Lessons](docs/esp32-voice-chat-lessons.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Roadmap](docs/roadmap.md)

## Roadmap

- [x] Streaming responses (SSE token-by-token display)
- [x] Voice input (push-to-talk + Groq Whisper STT)
- [x] Desktop pet sync (macOS companion via UDP)
- [x] Dual WiFi + offline mode + runtime config (Phase 4.5)
- [x] TTS voice replies (AI speaks through speaker)
- [x] Pet movement + time-travel sky + desktop position sync
- [ ] Battery display with low-power character animation
- [ ] Chat history persistence (NVS/SD card)
- [ ] Pet system (hunger/mood mechanics)
- [ ] Pomodoro timer
- [ ] Weather display
- [ ] BLE phone notifications

See [full roadmap](docs/roadmap.md).

## License

MIT
