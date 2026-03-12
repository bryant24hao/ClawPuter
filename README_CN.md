# ClawPuter 🦞

[English](README.md)

M5Stack Cardputer (ESP32-S3) 上的像素风桌面伴侣。小龙虾角色 + 动画 + 实时天气 + AI 聊天 + 语音输入/TTS + macOS 桌面宠物同步。

## 功能

- **伴侣模式** — 像素小龙虾，支持待机、开心、睡觉、说话、伸懒腰、东张西望等动画。键盘方向键移动（按住持续走）。时光倒流天空（向左走=过去，向右走=未来）。NTP 时钟显示。
- **实时天气** — 每 15 分钟从 Open-Meteo 获取天气数据（免费，无需 API Key）。根据天气类型显示雨滴、雪花、雾气、雷电闪光等背景特效。宠物自动佩戴天气配饰（墨镜、雨伞、雪帽、口罩）。时钟旁显示实时温度。
- **天气模拟** — Fn+W 切换模拟模式，数字键 1-8 预览全部 8 种天气类型（晴天、多云、阴天、雾、小雨、大雨、雪、雷暴）。
- **聊天模式** — 键盘输入，AI 对话支持 SSE 流式响应（逐字显示），消息自动换行和翻页滚动。
- **像素画生成** — `/draw a cat` 生成 8x8 AI 像素画，渲染为 96x96 彩色网格嵌入聊天。`/draw16` 生成 16x16。16 色固定调色板，自动同步到 Mac 桌面。
- **语音输入** — 按住 Fn 键说话（最长 3 秒），松开后通过 Groq Whisper API 语音转文字，识别结果自动填入输入栏。
- **TTS 语音回复** — AI 回复通过扬声器朗读。按任意键可中断播放。麦克风和扬声器共享 GPIO 43，系统自动切换。
- **桌面宠物同步** — macOS 端桌面宠物应用，通过 UDP 接收小龙虾状态、位置和天气信息，实时同步动画。支持跟随模式（跟踪光标）和场景模式（天气面板）。
- **桌面双向控制** — Mac ↔ ESP32 双向通信。远程触发动画、发送文字/消息到 Cardputer、转发通知为 Toast 叠加层、查看同步聊天历史、弹窗展示高清像素画。宠物在 Chat Viewer 打开时自动停靠在窗口顶部。
- **OpenClaw 集成** — 局域网连接本地 OpenClaw Gateway。多模型自动切换（Kimi/Claude/GPT/Gemini），持久记忆，5400+ 社区技能。
- **双 WiFi + 离线模式** — 主 WiFi 连不上自动尝试备用（手机热点），Gateway IP 自动切换。所有 WiFi 都失败可进入离线模式（伴侣模式正常可用，聊天显示离线提示）。
- **运行时配置** — Setup 向导支持运行时修改 WiFi、Gateway、STT Host，编译时值作为默认，Fn+R 重置。
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

# 天气（可选）
export DEFAULT_CITY="Beijing"                # 天气查询城市

# 备用 WiFi（可选，手机热点降级）
export WIFI_SSID2="<your-hotspot-ssid>"
export WIFI_PASS2="<your-hotspot-password>"
export OPENCLAW_HOST2="<your-hotspot-host-ip>"  # 热点网络上 Mac 的 IP
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
| Fn + W | 切换天气模拟 | — |
| 1-8（天气模拟中） | 切换天气类型 | — |
| Fn + R | 重置配置 + Setup 向导 | — |
| 任意键（睡眠中） | 唤醒角色 | — |
| 任意键（TTS 播放中） | — | 中断语音播放 |
| TAB（Setup 中） | 退出向导，进入伴侣模式 | — |

## 玩法详解

### 伴侣模式 — 你的像素宠物

小龙虾生活在 240×135 像素的屏幕上，背景会随时间和天气动态变化：

- **日夜循环** — 天空颜色根据 NTP 实时时间变化。白天蓝天白云，17-19 点橙色夕阳，19 点后深色夜空配闪烁星星和月亮。
- **时光倒流天空** — 向左/右移动宠物会让天空偏移 ±12 小时。走到最左边看昨晚的星空，走到最右边看明天的日出。底部时钟始终显示真实时间。
- **自发动作** — 宠物每 8-15 秒随机伸懒腰或东张西望。30 秒无互动后自动入睡，显示 "Zzz" 动画。按任意键唤醒。
- **键盘移动** — 按住 `,` `/` `;` `.` 持续走动，精灵自动翻转朝向。
- **互动** — 按空格或 Enter 让宠物开心跳跃，配欢快音效。

### 天气系统

- **自动刷新**：每 15 分钟从 Open-Meteo 获取天气数据（免费，不需要 API Key），根据 `DEFAULT_CITY` 配置自动定位。
- **背景特效**：雨滴下落、雪花飘飞（带横向漂移）、雾气点阵闪烁、雷暴白色闪光。天空色调随天气变暗。
- **宠物配饰**：晴天/多云戴墨镜 🕶️、雨天/雷暴撑雨伞 ☂️、下雪戴红色雪帽 🎅、雾天/阴天戴口罩 😷。配饰随精灵左右翻转。
- **温度显示**：时钟旁显示当前温度（°），用竖线分隔。
- **模拟模式**：Fn+W 进入天气模拟，用数字键 1-8 切换 8 种天气：1=晴天 2=多云 3=阴天 4=雾 5=小雨 6=大雨 7=雪 8=雷暴。底部显示 `[SIM] 天气名称` 状态栏。再按 Fn+W 退出。

### 聊天模式 — AI 对话

- 键盘输入消息，Enter 发送。AI 回复逐字流式显示，配打字音效。
- **像素画**：输入 `/draw a cat` 生成 8x8 像素画，或 `/draw16 a heart` 生成 16x16。AI 返回十六进制编码的像素数据，渲染为 96x96 彩色网格嵌入聊天消息。16 色固定调色板，解析失败自动降级为普通文字。
- **语音输入**：按住 Fn 录音（最长 3 秒），松开后发送到 Groq Whisper 转文字。转写过程中显示 "Transcribing..." 进度条。
- **TTS 语音回复**：AI 回复完成后，通过扬声器朗读回复内容。按任意键可中断播放。
- **离线模式**：未连接 WiFi 时，发送消息显示 `[Offline] No network connection`。

### 桌面宠物同步与双向控制

macOS Swift 应用（`desktop/CardputerDesktopPet/`）通过局域网与 Cardputer 双向通信。

**ESP32 → Mac（UDP 19820）：**
- 宠物状态、位置、天气 5Hz 广播同步
- 像素画生成后自动弹出 256x256 高清窗口，支持历史浏览
- 聊天消息实时同步到 Chat Viewer 窗口

**Mac → ESP32（UDP 19822）：**
- **触发动画**：开心、待机、睡觉、说话 — 从菜单栏 Control 子菜单触发
- **发送文字**：在 Mac 输入，出现在 Cardputer 聊天输入框
- **发送消息**：输入并自动发送（相当于按 Enter）
- **转发通知**：发送 Toast 叠加层（应用名、标题、内容），Cardputer 屏幕顶部显示 3 秒
- **请求聊天历史**：拉取完整对话记录

**显示模式：**
- **跟随模式**（默认）：透明精灵跟随光标，天气配饰同步。Chat Viewer 打开时宠物自动停靠在窗口顶部。
- **场景模式**：菜单栏下方 360x200 像素天气场景面板。
- 从菜单栏下拉菜单切换模式。

**构建桌面应用：**
```bash
cd desktop/CardputerDesktopPet && ./run.sh
```
自动编译 Swift 应用、打包为 `.app` bundle（含 `Info.plist`，macOS 本地网络权限所需）、Ad-hoc 签名、通过 `open` 启动。

### 联网与配置

- **双 WiFi**：主 WiFi 连不上 → 自动尝试备用 WiFi（如手机热点），Gateway IP 自动切换。
- **离线模式**：所有 WiFi 失败后，按 Tab 进入离线伴侣模式（动画、时钟、音效照常工作）。
- **运行时配置**：Fn+R 打开 Setup 向导，可修改 WiFi SSID/密码、Gateway 地址/端口/Token、STT Host，无需重新烧录。
- **WiFi 失败菜单**：连接失败后提供三个选项——重试 / Setup 向导 / 离线模式。

## 项目结构

```
src/
├── main.cpp              # 入口，模式调度，WiFi/NTP
├── companion.h/cpp       # 伴侣模式：动画、状态机、时钟、天气特效
├── chat.h/cpp            # 聊天模式：消息气泡、输入栏、滚动、像素画渲染
├── ai_client.h/cpp       # AI 客户端（OpenClaw/Claude），SSE 流式响应，/draw prompt 路由
├── voice_input.h/cpp     # 按键说话录音、WAV 编码、STT 代理客户端
├── tts_playback.h/cpp    # TTS 语音回复，PCM 下载 + DMA 播放
├── weather_client.h/cpp  # Open-Meteo 天气 API、地理编码、15 分钟自动刷新
├── state_broadcast.h/cpp # UDP 状态广播 + 一次性像素画/聊天消息同步
├── cmd_server.h/cpp      # 命令服务器（TCP 19821 + UDP 19822），Mac→ESP32 控制
├── sprites.h             # 像素小龙虾素材（RGB565）
├── config.h/cpp          # WiFi/API 配置，NVS 持久化
└── utils.h               # 颜色定义、屏幕常量、定时器

desktop/
└── CardputerDesktopPet/
    ├── Sources/
    │   ├── main.swift            # 入口
    │   ├── AppDelegate.swift     # 菜单栏、模式切换、控制命令、停靠逻辑
    │   ├── UDPListener.swift     # UDP 接收器，提取源 IP
    │   ├── TCPSender.swift       # UDP 命令发送（Mac→ESP32）
    │   ├── PetBehavior.swift     # 移动逻辑、跟随模式、停靠目标
    │   ├── PetWindow.swift       # 透明宠物精灵窗口
    │   ├── SceneWindow.swift     # 天气场景面板
    │   ├── PixelArtPopover.swift # 浮动 256x256 像素画展示
    │   └── ChatViewerWindow.swift # 聊天历史查看器 + 远程发送
    ├── Info.plist                # 应用 bundle 元数据 + 网络权限
    └── run.sh                    # 编译、打包、签名、启动脚本

tools/
└── stt_proxy.py          # 本地 HTTP 代理：ESP32 音频 → Groq Whisper API + TTS
```

## iPhone 热点小贴士

使用 iPhone 热点作为备用 WiFi 时：

1. **开启"最大化兼容性"** — 进入设置 > 个人热点，打开此选项。iPhone 默认 5GHz，ESP32 只支持 2.4GHz。
2. **保持热点设置页面打开** — iPhone 热点无设备连接时进入休眠，ESP32 扫描不到。需在 iPhone 上打开"个人热点"设置页面保持唤醒。
3. **UDP 广播被隔离** — iPhone 热点有客户端隔离，设备间 UDP 广播被过滤。固件已做绕过：同时发 broadcast + unicast 到 Gateway IP。
4. **查看 Mac 的热点 IP** — Mac 连上 iPhone 热点后，用 `ifconfig en0` 查看 IP（通常是 `172.20.10.x`），设为 `OPENCLAW_HOST2`。

## 硬件

- **M5Stack Cardputer** — ESP32-S3，240×135 IPS 屏幕，56 键键盘，PDM 麦克风（SPM1423），扬声器（与麦克风共享 GPIO 43）
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
- [像素画设计](docs/pixel-art-design.md)
- [桌面双向通信设计](docs/desktop-bidirectional-design.md)
- [ESP32 内存踩坑记录](docs/esp32-voice-chat-lessons.md)
- [问题排查](docs/troubleshooting.md)
- [路线图](docs/roadmap.md)

## 路线图

- [x] 流式响应（SSE 逐字显示）
- [x] 语音输入（按住说话 + Groq Whisper 语音转文字）
- [x] 桌面宠物同步（macOS 端通过 UDP 同步）
- [x] 双 WiFi + 离线模式 + 运行时配置
- [x] TTS 语音回复（AI 通过扬声器播放回复）
- [x] 宠物移动 + 时光倒流天空 + 桌面位置同步
- [x] 实时天气（Open-Meteo API + 背景特效 + 宠物配饰）
- [x] 天气模拟模式（Fn+W + 1-8 预览全部天气类型）
- [x] 像素画生成（/draw 命令，支持 8x8 和 16x16）
- [x] 桌面双向控制（Mac ↔ ESP32 命令、聊天同步、像素画同步、通知转发）
- [ ] 电量显示 + 低电量角色变虚弱
- [ ] 聊天历史持久化（NVS/SD 卡）
- [ ] 养成系统（饥饿值/心情值 + 洗澡/喂食等互动）
- [ ] 番茄钟
- [ ] BLE 手机通知推送
- [ ] 像素美术升级（星露谷风格，专业画师优化角色/背景/道具）
- [ ] 长语音 TTS 支持（服务端切片缓存或 SD 卡流式播放）
- [ ] M5Burner 固件发布（社区用户一键烧录）

完整路线图见 [roadmap.md](docs/roadmap.md)。

## 许可证

MIT
