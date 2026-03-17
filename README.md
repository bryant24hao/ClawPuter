# ClawPuter 🦞

[中文](README_CN.md)

A pixel-art desktop companion running on M5Stack Cardputer (ESP32-S3). Features an OpenClaw-themed lobster character with animations, real-time weather, AI chat with voice input/TTS, and a synced macOS desktop pet.

<p align="center">
  <img src="assets/hardware.png" alt="ClawPuter on M5Stack Cardputer" width="500">
  <img src="assets/desktop-pet.png" alt="ClawPuter Desktop Pet" width="500">
</p>

## Why "ClawPuter"?

The idea started one afternoon at a friend's office. We were chatting about [OpenClaw](https://github.com/openclaw) and I mentioned I'd been getting into hardware lately. He happened to have an M5Stack **Cardputer** lying around and lent it to me. That same afternoon, the concept clicked: what if we gave OpenClaw — a digital life — a more embodied, physical form? The Cardputer was the perfect vessel. So I mashed the names together: **Claw** (from OpenClaw) + Card**puter** = **ClawPuter**.

## Features

- **Companion Mode** — Pixel lobster with idle, happy, sleep, talk, stretch, and look-around animations. Movable with keyboard (hold to walk). Day/night background with time-travel sky (walk left = past, right = future). NTP clock display.
- **Moisture System** — Virtual hydration mechanic driven by real weather. Humidity affects decay speed, rain pauses decay, walking drains water faster. Press H to spray water and restore moisture. When dehydrated: movement slows, chat is locked, pet shows "so thirsty..." status. Water drop indicators in the top bar and on the desktop scene.
- **Real-Time Weather** — Fetches weather from Open-Meteo API every 15 minutes. Background effects for rain, drizzle, snow, fog, and thunderstorms. Pet wears weather-appropriate accessories (sunglasses, umbrella, snow hat, mask). Temperature displayed next to the clock.
- **Weather Simulation** — Fn+W toggles simulation mode, number keys 1-8 to preview all 8 weather types (Clear, Cloudy, Overcast, Fog, Drizzle, Rain, Snow, Thunder).
- **Chat Mode** — Full keyboard input, AI conversation with SSE streaming (token-by-token display), scrollable message history with word-wrap.
- **Pixel Art Generation** — `/draw a cat` generates 8x8 AI pixel art rendered as a 96x96 color grid in chat. `/draw16` for 16x16. 16-color palette, mixed with text messages, auto-syncs to Mac desktop.
- **Voice Input** — Push-to-talk via Fn key, speech-to-text via Groq Whisper API (through a local proxy server).
- **TTS Voice Replies** — AI responses are spoken through the built-in speaker. Press any key to interrupt playback. Mic and speaker share GPIO 43 — the system handles switching automatically.
- **Desktop Pet Sync** — macOS companion app receives lobster state, position, and weather over UDP, rendering a synced desktop pet on your Mac. Supports Follow Mode (cursor tracking) and Scene Mode (weather panel).
- **Desktop Bidirectional Control** — Mac ↔ ESP32 two-way communication. Remote trigger animations, send text/messages to Cardputer, forward notifications as toast overlays, view synced chat history, and display pixel art in a high-res popover. Pet perches on the Chat Viewer window when open.
- **Thinking Model Compatibility** — Works with reasoning models (GPT-5.4, Claude, etc.) that output extended thinking. Thinking content is automatically filtered, with a "thinking..." indicator shown during the thinking phase. Gateway fallback injection is detected and handled gracefully.
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

# Weather (optional)
export DEFAULT_CITY="Beijing"                # City name for weather lookup

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
| H | Spray water (when moisture ≤ 1) | Spray water (when moisture ≤ 1), otherwise type 'h' |
| Fn + W | Toggle weather simulation | — |
| 1-8 (in weather sim) | Select weather type | — |
| Fn + R | Reset config + setup wizard | — |
| Fn + 0 | Debug: set moisture to 0 | — |
| Any key (while sleeping) | Wake up lobster | — |
| Any key (during TTS) | — | Stop voice playback |
| TAB (in setup) | Exit setup, return to companion | — |

## What Can It Do?

### Companion Mode — Your Pixel Pet

The lobster lives on a 240x135 pixel screen with a dynamic background:

- **Day/Night cycle** — Sky color changes based on real time (NTP). Blue sky with sun and clouds during daytime, orange sunset at 17-19h, dark night with twinkling stars and crescent moon after 19h.
- **Time-travel sky** — Moving the pet left/right shifts the displayed sky ±12 hours. Walk to the left edge to see last night's starry sky, or to the right to see tomorrow's sunrise. The clock at the bottom always shows the real time.
- **Spontaneous actions** — The pet randomly stretches or looks around every 8-15 seconds when idle. After 30 seconds of no interaction, it falls asleep with animated "Zzz". Press any key to wake it up.
- **Keyboard movement** — Hold `,` `/` `;` `.` for continuous walking. The sprite flips to face the direction of movement.
- **Interaction** — Press Space or Enter to make the pet jump happily with a cheerful melody.

### Moisture System — Keep Your Lobster Hydrated

A virtual hydration mechanic tied to real-world weather:

- **4-level moisture indicator** shown in the top bar (water drops). Desktop scene mode mirrors the same indicator.
- **Weather-driven decay**: low humidity (< 40%) = fast drain, high humidity (> 70%) = slow drain, rain/snow pauses decay entirely.
- **Movement drain**: walking around costs moisture — every 20 steps drains 1 level. Stay still to conserve water.
- **Spray water**: press H to restore 1 moisture level (3-second cooldown). Triggers a water spray particle animation and happy jump.
- **Dehydration effects**: at moisture 1, movement slows to 50% and the pet shows "need water...". At moisture 0, the pet cannot move, cannot jump, chat mode is locked ("thirsty... press H to spray!"), and shows "so thirsty...".
- **Water bottle accessory**: when moisture ≤ 1, the pet holds a tiny water bottle instead of weather accessories.
- **Rain recovery**: during rain/thunderstorms, moisture auto-recovers +1 every 15 minutes.
- **Initial moisture**: set based on first weather data — dry climate starts thirsty, humid climate starts comfortable.

### Weather — Real-Time & Simulated

- **Auto-updates** every 15 minutes from Open-Meteo (free, no API key needed). City configured via `DEFAULT_CITY` env var.
- **Background effects**: rain drops fall, snow drifts with horizontal drift, fog dots shimmer, thunder flashes the screen white. Sky tints darker under heavy weather. Sun/moon/stars hidden during overcast conditions.
- **Accessories**: the pet automatically wears sunglasses (clear/cloudy), umbrella (rain/drizzle/thunder), snow hat (snow), or mask (fog/overcast). Accessories flip with the sprite direction.
- **Temperature** shown next to the clock with a ° symbol, separated by a vertical divider.
- **Simulation mode**: Fn+W enters weather simulation, keys 1-8 preview all 8 weather types (1=Clear, 2=Cloudy, 3=Overcast, 4=Fog, 5=Drizzle, 6=Rain, 7=Snow, 8=Thunder). A `[SIM]` status bar appears at the bottom. Press Fn+W again to return to real weather.

### Chat Mode — AI Conversation

- Type messages and press Enter to send. AI replies stream token-by-token with typing sound effects.
- **Pixel art**: Type `/draw a cat` to generate an 8x8 pixel art, or `/draw16 a heart` for 16x16. AI returns hex-encoded pixel data, rendered as a 96x96 color grid inline with chat messages. 16-color fixed palette. Parse failures gracefully fall back to plain text.
- **Voice input**: Hold Fn to record (up to 3 seconds), release to transcribe via Groq Whisper. A "Transcribing..." progress bar is shown during processing. Transcription fills the input bar.
- **TTS**: After AI replies, the response is spoken through the built-in speaker. A "Speaking..." indicator is shown during audio download. Press any key to stop playback.
- **Scrolling**: Use Fn+; to scroll up and Fn+/ to scroll down through message history.
- **Offline mode**: If WiFi is not connected, sending a message shows `[Offline] No network connection`.

### Desktop Pet Sync & Bidirectional Control

A macOS Swift app (`desktop/CardputerDesktopPet/`) communicates with the Cardputer over the local network.

**ESP32 → Mac (UDP 19820):**
- Pet state, position, weather, and temperature broadcast at 5Hz
- Pixel art data synced on generation — pops up a 256x256 high-res popover with history browsing
- Chat messages synced in real-time to the Chat Viewer window

**Mac → ESP32 (UDP 19822):**
- **Trigger animations**: happy, idle, sleep, talk — from the menu bar Control submenu
- **Send text**: type on Mac, appears in Cardputer's chat input
- **Send message**: type and auto-send (equivalent to pressing Enter)
- **Forward notifications**: send toast overlays (app name, title, body) displayed for 3 seconds on Cardputer
- **Request chat history**: pull full conversation log

**Display modes:**
- **Follow Mode** (default): transparent sprite follows your cursor, with weather accessories synced. Pet perches on top of the Chat Viewer window when it's open.
- **Scene Mode**: a 360x200 pixel weather scene panel anchored below the menu bar icon.
- Switch modes from the menu bar dropdown.

**Building the desktop app:**
```bash
cd desktop/CardputerDesktopPet && ./run.sh
```
This builds the Swift app, wraps it in a `.app` bundle with `Info.plist` (required for macOS local network permissions), ad-hoc signs it, and launches via `open`.

### Networking & Config

- **Dual WiFi**: Primary WiFi fails → automatically tries secondary (e.g. phone hotspot). Gateway host switches automatically.
- **Offline mode**: If all WiFi fails, press Tab to enter offline companion mode (animations, clock, sound effects all work).
- **Runtime config**: Fn+R opens a setup wizard to change WiFi SSID/password, Gateway host/port/token, STT host — no re-flashing needed. Press Tab in setup to cancel and return to companion mode.
- **WiFi failure menu**: Retry / Setup wizard (Fn+R) / Offline mode (Tab) — no more stuck on "Connecting...".

## Project Structure

```
src/
├── main.cpp              # Entry point, mode dispatch, WiFi/NTP
├── companion.h/cpp       # Companion mode: animations, state machine, clock, weather effects
├── chat.h/cpp            # Chat mode: messages, input bar, scrolling, pixel art rendering
├── ai_client.h/cpp       # AI client (OpenClaw/Claude), SSE streaming, /draw prompt routing
├── voice_input.h/cpp     # Push-to-talk recording, WAV encoding, STT proxy client
├── tts_playback.h/cpp    # TTS voice replies, PCM download and DMA playback
├── weather_client.h/cpp  # Open-Meteo weather API, geocoding, 15-min auto refresh
├── state_broadcast.h/cpp # UDP state broadcast + one-shot pixel art / chat sync
├── cmd_server.h/cpp      # Command server (TCP 19821 + UDP 19822) for Mac→ESP32 control
├── sprites.h             # Pixel lobster sprites (RGB565)
├── config.h/cpp          # WiFi/API config, NVS persistence
└── utils.h               # Colors, screen constants, Timer

desktop/
└── CardputerDesktopPet/
    ├── Sources/
    │   ├── main.swift            # Entry point
    │   ├── AppDelegate.swift     # Menu bar, mode switching, control commands, perch logic
    │   ├── UDPListener.swift     # UDP receiver with source IP extraction
    │   ├── TCPSender.swift       # UDP command sender (Mac→ESP32)
    │   ├── PetBehavior.swift     # Movement, follow mode, perch target
    │   ├── PetWindow.swift       # Transparent pet sprite window
    │   ├── SceneWindow.swift     # Weather scene panel
    │   ├── PixelArtPopover.swift # Floating 256x256 pixel art display
    │   └── ChatViewerWindow.swift # Chat history viewer + remote send
    ├── Info.plist                # App bundle metadata + network permissions
    └── run.sh                    # Build, bundle, sign, and launch script

tools/
└── stt_proxy.py          # Local HTTP proxy: ESP32 audio → Groq Whisper API + TTS
```

## iPhone Hotspot Tips

If using an iPhone hotspot as secondary WiFi:

1. **Enable "Maximize Compatibility"** — Go to Settings > Personal Hotspot and turn it on. iPhone defaults to 5GHz; ESP32 only supports 2.4GHz.
2. **Keep hotspot settings page open** — iPhone hotspot goes dormant when no devices are connected. The Cardputer won't find it unless the Personal Hotspot settings screen is open on the iPhone.
3. **UDP broadcast is blocked** — iPhone hotspot isolates clients, blocking UDP broadcast between devices. The firmware works around this by sending both broadcast and unicast (directly to the gateway host IP).
4. **Find your Mac's hotspot IP** — When your Mac connects to the iPhone hotspot, check its IP with `ifconfig en0` (usually `172.20.10.x`). Set this as `OPENCLAW_HOST2`.

## Hardware

- **M5Stack Cardputer** — ESP32-S3, 240×135 IPS display, 56-key keyboard, PDM microphone (SPM1423), speaker (shared GPIO 43)
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
- [Pixel Art Design](docs/pixel-art-design.md)
- [Desktop Bidirectional Design](docs/desktop-bidirectional-design.md)
- [ESP32 Memory Lessons](docs/esp32-voice-chat-lessons.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Roadmap](docs/roadmap.md)

## Roadmap

- [x] Streaming responses (SSE token-by-token display)
- [x] Voice input (push-to-talk + Groq Whisper STT)
- [x] Desktop pet sync (macOS companion via UDP)
- [x] Dual WiFi + offline mode + runtime config
- [x] TTS voice replies (AI speaks through speaker)
- [x] Pet movement + time-travel sky + desktop position sync
- [x] Real-time weather (Open-Meteo API + background effects + accessories)
- [x] Weather simulation mode (Fn+W + 1-8 to preview all weather types)
- [x] Pixel art generation (/draw command with 8x8 and 16x16 support)
- [x] Desktop bidirectional control (Mac ↔ ESP32 commands, chat sync, pixel art sync, notifications)
- [x] Moisture system (hydration mechanic with weather linkage, spray interaction, movement drain)
- [x] Thinking model compatibility (auto-filter thinking content, rolling timeout, fallback detection)
- [ ] Battery display with low-power character animation
- [ ] Chat history persistence (NVS/SD card)
- [ ] Pet system expansion (hunger/mood mechanics, feeding interactions)
- [ ] Pomodoro timer
- [ ] BLE phone notifications
- [ ] Professional pixel art upgrade (Stardew Valley style)
- [ ] Long TTS support (server-side chunked streaming or SD card)
- [ ] M5Burner firmware publishing (one-click flash for community)

See [full roadmap](docs/roadmap.md).

## License

MIT
