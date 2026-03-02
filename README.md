# Cardputer Pixel Companion

A pixel-art desktop companion on M5Stack Cardputer (ESP32-S3). OpenClaw lobster character + clock + AI chat.

M5Stack Cardputer (ESP32-S3) 上的像素风桌面伴侣。OpenClaw 小龙虾角色 + 时钟 + AI 聊天。

## Features / 功能

- **Companion Mode / 伴侣模式**: Pixel lobster with idle/blink/happy/sleep/talk animations, starry background, NTP clock
- **Chat Mode / 聊天模式**: Keyboard input, Kimi K2.5 API conversation, message bubbles
- **Config Management / 配置管理**: WiFi/API Key via environment variables or keyboard input, NVS persistent storage

## Quick Start / 快速开始

### 1. Set Environment Variables / 设置环境变量

```bash
export WIFI_SSID="your_wifi_name"
export WIFI_PASS="your_wifi_password"
export CLAUDE_API_KEY="your_api_key"
```

### 2. Build & Flash / 编译烧录

```bash
cd CardputerCompanion
pio run -t upload
```

First-time flashing may require entering download mode (hold G0 + press Reset). See [Flash Guide](docs/setup-and-flash.md).

首次烧录可能需要手动进入下载模式（按住 G0 + 按 Reset），详见 [烧录指南](docs/setup-and-flash.md)。

### 3. Serial Debug / 串口调试

```bash
pio device monitor
```

## Controls / 操作方式

| Key / 按键 | Companion Mode / 伴侣模式 | Chat Mode / 聊天模式 |
|------------|--------------------------|---------------------|
| TAB | Switch to chat / 切换到聊天 | Switch to companion / 切换到伴侣 |
| Space/Enter | Happy animation / 角色开心跳跃 | Send message / 发送消息 |
| Backspace | - | Delete char / 删除字符 |
| Fn+R | Reset config / 重置配置 | - |
| Any key / 任意键 | Wake up / 唤醒角色 | Type char / 输入字符 |

## Project Structure / 项目结构

```
src/
├── main.cpp          # Entry point, mode dispatch, WiFi/NTP init
├── companion.h/cpp   # Companion mode: character rendering, state machine, clock
├── chat.h/cpp        # Chat mode: message bubbles, input bar, scrolling
├── ai_client.h/cpp   # API client (OpenAI-compatible format)
├── sprites.h         # Pixel lobster sprites (RGB565 const arrays)
├── config.h/cpp      # WiFi/API Key config, NVS read/write
└── utils.h           # Color definitions, screen constants, Timer utility
```

## Hardware / 硬件

- M5Stack Cardputer (ESP32-S3, 240x135 display, 56-key keyboard)
- ESP32-S3 supports **2.4GHz WiFi only**

## Docs / 文档

- [Open Source Research / 开源项目调研](docs/research.md)
- [Setup & Flash Guide / 环境搭建与烧录](docs/setup-and-flash.md)
- [Hardware Notes / 硬件要点](docs/hardware-notes.md)
- [API Integration / API 接入记录](docs/api-integration.md)
- [Architecture / 代码架构](docs/architecture.md)
- [Troubleshooting / 问题排查](docs/troubleshooting.md)
