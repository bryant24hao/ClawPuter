# Cardputer Companion 🦞

[English](README.md)

M5Stack Cardputer (ESP32-S3) 上的像素风桌面伴侣。小龙虾角色 + 动画 + 时钟 + AI 聊天 + 语音输入 + macOS 桌面宠物同步。

## 功能

- **伴侣模式** — 像素小龙虾，支持待机、开心、睡觉、说话、伸懒腰、东张西望等动画。键盘方向键移动（按住持续走）。时光倒流天空（向左走=过去，向右走=未来）。NTP 时钟显示。
- **聊天模式** — 键盘输入，AI 对话支持 SSE 流式响应（逐字显示），消息自动换行和翻页滚动。
- **语音输入** — 按住 Fn 键说话，通过 Groq Whisper API 进行语音转文字（经本地代理服务器中转）。
- **桌面宠物同步** — macOS 端桌面宠物应用，通过 UDP 接收小龙虾状态和位置，实时同步动画。在 Cardputer 上移动宠物，桌面上也跟着动。
- **OpenClaw 集成** — 局域网连接本地 OpenClaw Gateway。多模型自动切换（Kimi/Claude/GPT/Gemini），持久记忆，5400+ 社区技能。
- **开机动画** — 小龙虾像素画逐行渐入，模式切换像素擦除过渡。
- **音效** — 按键咔嗒、开心音阶、AI 流式回复打字音、通知提示音。

## 快速开始

### 1. 设置环境变量

```bash
# WiFi
export WIFI_SSID="<your-wifi-ssid>"
export WIFI_PASS="<your-wifi-password>"

# AI 后端（OpenClaw Gateway）
export OPENCLAW_HOST="<your-host-ip>"       # OpenClaw Gateway 局域网 IP
export OPENCLAW_PORT="<your-port>"           # Gateway 端口
export OPENCLAW_TOKEN="<your-gateway-token>"

# 语音输入（可选，供 stt_proxy.py 使用）
export GROQ_API_KEY="<your-groq-api-key>"    # 供 tools/stt_proxy.py 使用，非固件
export STT_PROXY_HOST="<your-host-ip>"       # 运行 stt_proxy.py 的机器 IP
export STT_PROXY_PORT="8090"                 # STT 代理端口（默认 8090）
```

### 2. 编译烧录

```bash
pio run -t upload
```

首次烧录需要手动进入下载模式：按住 **G0** + 按 **Reset**，然后松开 G0。详见[烧录指南](docs/setup-and-flash.md)。

### 3. 启动 STT 代理（语音输入）

```bash
python3 tools/stt_proxy.py
```

代理运行在 Mac/PC 上，将 Cardputer 录制的音频转发到 Groq Whisper API 进行语音识别。需要在 `.env` 或环境变量中配置 `GROQ_API_KEY`。

### 4. 串口调试

```bash
pio device monitor
```

## 操作方式

| 按键 | 伴侣模式 | 聊天模式 |
|------|---------|---------|
| TAB | 切换到聊天 | 切换到伴侣 |
| `,`（按住） | 向左移动 | — |
| `/`（按住） | 向右移动 | — |
| `;`（按住） | 向上移动 | — |
| `.`（按住） | 向下移动 | — |
| 空格 / Enter | 角色开心跳跃 | 发送消息 |
| Backspace | — | 删除字符 |
| Fn（长按） | — | 按住说话，松开转文字 |
| Fn + ; | — | 向上翻页 |
| Fn + / | — | 向下翻页 |
| Fn + R | 重置配置 | — |
| 任意键 | 唤醒角色 | 输入字符 |

## 项目结构

```
src/
├── main.cpp              # 入口，模式调度，WiFi/NTP
├── companion.h/cpp       # 伴侣模式：动画、状态机、时钟
├── chat.h/cpp            # 聊天模式：消息气泡、输入栏、滚动
├── ai_client.h/cpp       # AI 客户端（OpenClaw/Claude），SSE 流式响应
├── voice_input.h/cpp     # 按键说话录音、WAV 编码、STT 代理客户端
├── state_broadcast.h/cpp # UDP 状态广播，桌面宠物同步
├── sprites.h             # 像素小龙虾素材（RGB565）
├── config.h/cpp          # WiFi/API 配置，NVS 持久化
└── utils.h               # 颜色定义、屏幕常量、定时器

desktop/
└── CardputerDesktopPet/  # macOS 桌面宠物（Swift，接收 UDP 状态同步）

tools/
└── stt_proxy.py          # 本地 HTTP 代理：ESP32 音频 → Groq Whisper API
```

## 硬件

- **M5Stack Cardputer** — ESP32-S3，240×135 IPS 屏幕，56 键键盘
- ESP32-S3 **仅支持 2.4GHz WiFi**（不支持 5GHz）

## OpenClaw 配置

本项目连接运行在 Mac 或 VPS 上的 OpenClaw Gateway：

1. [安装 OpenClaw](https://openclaw.ai)
2. 在 `~/.openclaw/openclaw.json` 中启用局域网绑定和 HTTP API：
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
3. 重启 Gateway：`openclaw gateway restart`
4. 将 `OPENCLAW_HOST` 设为 Mac 的局域网 IP

完整集成方案见 [OpenClaw 调研文档](docs/openclaw-research.md)。

## 文档

- [环境搭建与烧录](docs/setup-and-flash.md)
- [硬件要点](docs/hardware-notes.md)
- [API 接入记录](docs/api-integration.md)
- [代码架构](docs/architecture.md)
- [OpenClaw 集成](docs/openclaw-research.md)
- [语音输入设计](docs/voice-input-design.md)
- [桌面宠物设计](docs/desktop-pet-design.md)
- [UDP 状态同步设计](docs/udp-state-sync-design.md)
- [ESP32 内存踩坑记录](docs/esp32-voice-chat-lessons.md)
- [问题排查](docs/troubleshooting.md)
- [路线图](docs/roadmap.md)

## 路线图

- [x] 流式响应（SSE 逐字显示）
- [x] 语音输入（按住说话 + Groq Whisper 语音转文字）
- [x] 桌面宠物同步（macOS 端通过 UDP 同步）
- [x] TTS 语音回复（AI 通过扬声器播放回复）
- [x] 宠物移动 + 时光倒流天空 + 桌面位置同步
- [ ] 电量显示 + 低电量角色变虚弱
- [ ] 聊天历史持久化（NVS/SD 卡）
- [ ] 养成系统（饥饿值/心情值）
- [ ] 番茄钟
- [ ] 天气显示
- [ ] BLE 手机通知推送

完整路线图见 [roadmap.md](docs/roadmap.md)。

## 许可证

MIT
