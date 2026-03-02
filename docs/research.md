# 开源项目调研

## 调研背景

目标：在 M5Stack Cardputer (ESP32-S3) 上做一个像素风桌面伴侣，支持 AI 聊天。调研现有开源项目，寻找灵感和技术参考。

## 开发环境选型

评估了四种方案：

| 方案 | 优势 | 劣势 | 结论 |
|------|------|------|------|
| Arduino IDE | 入门简单，社区资源多 | GUI 操作，不适合 CLI 工作流 | ❌ |
| ESP-IDF | 底层控制力强 | 学习曲线陡，开发慢 | ❌ |
| **PlatformIO** | 依赖管理好，CLI 友好，适合 Claude Code | 首次配置稍慢 | ✅ 选用 |
| MicroPython | 快速原型 | 性能差，库支持弱 | ❌ |

## 开源项目全景（30 个项目，6 个分类）

### Tier 1：AI 助手 / LLM 项目（最相关）

| 项目 | Stars | 简介 | 参考价值 |
|------|-------|------|---------|
| [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) | ~24,400 | MCP AI 聊天机器人，离线语音唤醒 + 流式 ASR/LLM/TTS，支持 Qwen/DeepSeek，70+ 硬件 | ESP32 AI 项目的"黄金标准"，架构参考 |
| [LLMCardputer](https://github.com/GOROman/LLMCardputer) | 55 | Cardputer + ModuleLLM 本地离线推理（Qwen2.5-0.5B/1.5B），无需 WiFi | 本地 LLM 的可能性参考 |
| [AI-Cardputer-Adv](https://github.com/thebolined/AI-Cardputer-adv-A-Portable-AI-Assistant) | 6 | ChatGPT 风格终端，赛博朋克开机动画，WiFi + Qwen API | 小屏幕文字聊天 UI 参考 |
| [ESP-SparkBot](https://github.com/espressif2022/esp_sparkbot) | 13 | Espressif 官方桌面伴侣机器人，语音+摄像头+表情+运动 | 表情动画系统参考 |
| [Generative AI Virtual Agent](https://www.hackster.io/johnnietien/generative-ai-virtual-agent-with-m5stack-4c133a) | - | M5Stack + ChatGPT/PaLM2 语音助手教程 | 语音 AI 全流程参考 |

### Tier 2：综合固件 / OS

| 项目 | Stars | 简介 | 参考价值 |
|------|-------|------|---------|
| [Bruce Firmware](https://github.com/BruceDevices/firmware) | ~5,000 | 功能最丰富的多用途固件（WiFi/BLE/IR/RFID/BadUSB） | UI 架构和外设管理代码质量很高 |
| [MicroHydra](https://github.com/echo-lalia/MicroHydra) | 282 | MicroPython App Launcher，用 RTC 内存实现"重启切换应用" | 资源受限设备的架构思路 |
| [M5 Launcher](https://github.com/bmorcelli/Launcher) | 250+ | App Launcher，250+ 预编译固件 | 固件管理参考 |

### Tier 3：安全与网络工具

| 项目 | Stars | 简介 |
|------|-------|------|
| [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) | ~10,000 | ESP32 最高星安全项目，专业抓包分析 |
| [Evil-M5Project](https://github.com/7h30th3r0n3/Evil-M5Project) | ~2,000 | WiFi 网络探测 / 道德黑客 |
| [M5Stick NEMO](https://github.com/n0xa/m5stick-nemo) | ~1,200 | 数字安全防御固件 |
| [Meshtastic](https://github.com/m5stack/meshtastic-firmware) | - | 离网 LoRa Mesh 加密通信 |
| [MeshCore Cardputer ADV](https://github.com/Stachugit/MeshCore-Cardputer-ADV) | - | Mesh 网络增强 UI，聊天气泡 |

### Tier 4：复古游戏与模拟器

| 项目 | Stars | 简介 |
|------|-------|------|
| [Game Station Emulators](https://github.com/geo-tp/Cardputer-Game-Station-Emulators) | 132 | 10 款主机模拟器（NES/SNES/GB/GBC/MD 等），320KB RAM 跑起来 |
| [Gameboy Enhanced](https://github.com/Mr-PauI/Gameboy-Enhanced-Firmware-m5stack-cardputer-) | 73 | 增强版 GB/GBC 模拟器，支持音频和存档 |
| [Cardputer DOOM](https://github.com/Logimancer/Cardputer-doom) | - | DOOM 移植 |
| [**Raising Hell**](https://github.com/acpayers-alt/raising-hell-cardputer) | - | **原创虚拟宠物**，生命阶段/喂食/小游戏/睡眠/衰退 |
| [Tamaputer](https://github.com/mindovermiles262/tamaputer) | - | 拓麻歌子 P1 模拟器 |
| [C64 Emulator](https://github.com/iele/M5Cardputer-C64-Emulator) | - | Commodore 64 模拟 |

### Tier 5：创意工具

| 项目 | Stars | 简介 |
|------|-------|------|
| [Ultimate Remote](https://github.com/geo-tp/Ultimate-Remote) | 151 | 万能红外遥控器，3498 个设备配置 |
| [ESP32 Bus Pirate](https://github.com/geo-tp/ESP32-Bus-Pirate) | - | 逻辑分析仪 / 协议调试 |
| [WebRadio](https://github.com/cyberwisk/M5Cardputer_WebRadio) | - | 网络电台 |
| [BLE HID Keyboard](https://github.com/Gitshaoxiang/M5Cardputer-BLE-HID-Keyboard) | - | 蓝牙键盘 |
| [SSH Client](https://github.com/aat440hz/SSHClient-M5Cardputer) | - | SSH 终端 |
| [LoRa Chat](https://github.com/nonik0/CardputerLoRaChat) | - | LoRa 点对点聊天 |
| [MiniAcid](https://github.com/urtubia/miniacid) | - | 音乐合成器 |
| [Rust Cardputer HAL](https://github.com/Kezii/Rust-M5Stack-Cardputer) | - | Rust 硬件抽象层 + 3D 图形 |
| [PyDOS + PyBASIC](https://github.com/RetiredWizard/PyDOS) | - | 复古 DOS 环境 + BASIC 编程 |

### Tier 6：资源汇总

| 项目 | 简介 |
|------|------|
| [Awesome M5Stack Cardputer](https://github.com/terremoth/awesome-m5stack-cardputer) | 最全的 Cardputer 项目列表 |
| [Cardputer Firmware List](https://github.com/ru84r8/Cardputer-firmware-list) | 固件列表 + 安装说明 |

## 重点项目深入分析

### Raising-Hell-Cardputer（像素宠物架构）

最接近我们"像素伴侣"的项目。

- **资源管理**：SD 卡存储动画帧（`/graphics/`、`/pet/`、`/anim/`），适合 240×280 的大量帧资源
- **状态机**：IDLE → SLEEPING → PLAYING → FEEDING → DECAYING，基于时间+输入+快乐值触发转换
- **架构**：正在重构为模块化状态机，分离平台逻辑和游戏逻辑
- **教训**：大量动画帧必须用 SD 卡，不要嵌入固件

**我们的取舍**：v1 用 16×16 简单像素角色，帧数少（<10 帧），直接存 Flash（`const uint16_t[] PROGMEM`），不引入 SD 卡复杂度。

### ESP-SparkBot（表情显示）

Espressif 官方桌面伴侣。

- **渲染**：使用 LVGL（轻量级图形库），硬件加速
- **表情系统**：动态帧渲染，灵感来自 Anki Cozmo 机器人的眼睛设计
- **硬件**：1.3" 240×240 LCD + 摄像头 + 麦克风 + 加速度计

**我们的取舍**：LVGL 功能强大但引入复杂度高，v1 直接用 M5GFX 的 Canvas 双缓冲，够用且简单。

### AI-Cardputer-Adv（聊天 UI）

最接近我们"AI 聊天"功能的项目。

- **UI**：LVGL 渲染，聊天气泡 + 主题定制
- **网络**：WiFi Manager 配网，NVS 持久化凭证
- **API**：HTTPClient + ArduinoJson，接 Qwen API

**我们的取舍**：借鉴了 NVS 持久化和 HTTPClient 方案，但 UI 用 M5Canvas 手动绘制而非 LVGL。

## ESP32 上的 LLM 集成库

| 库 | 特点 |
|---|------|
| [ESPAI](https://github.com/enkei0x/espai) | 统一 AI 库，支持 Claude/OpenAI/Gemini 流式，FreeRTOS 异步，457+ 测试 |
| [ESP32_AI_Connect](https://github.com/AvantMaker/ESP32_AI_Connect) | 轻量集成，实时流式，支持 Claude/OpenAI/DeepSeek |
| HTTPClient + ArduinoJson | Arduino 内置，最简单直接 |

**我们的选择**：HTTPClient + ArduinoJson，最小依赖，v1 够用。未来可考虑 ESPAI 获得更好的流式支持。

## 最终技术决策

| 组件 | 选择 | 理由 | 对比放弃的方案 |
|------|------|------|--------------|
| 渲染 | M5Canvas 双缓冲 | 内置于 M5GFX，无额外依赖 | LVGL（功能强但重） |
| 像素素材 | Flash 存储（PROGMEM） | v1 素材量小，不需要 SD 卡 | SD 卡（Raising Hell 方案） |
| HTTP 客户端 | HTTPClient + WiFiClientSecure | Arduino 内置 | ESPAI（功能多但依赖重） |
| JSON 解析 | ArduinoJson | 轻量通用 | cJSON（更底层） |
| 凭证存储 | Preferences (NVS) | ESP32 原生，掉电不丢失 | SD 卡配置文件 |
| 状态管理 | 有限状态机 | 简单清晰，4 个状态够用 | 行为树（过度设计） |
| 图形库 | M5GFX (LovyanGFX) | Cardputer 自带 | LVGL/TFT_eSPI |

## 参考资料

- [Awesome M5Stack Cardputer](https://github.com/terremoth/awesome-m5stack-cardputer) — 项目列表
- [M5Stack Cardputer 文档](https://docs.m5stack.com/en/core/Cardputer-Adv)
- [Streaming LLM APIs 原理](https://til.simonwillison.net/llms/streaming-llm-apis)
- [ElatoAI: ESP32 上的实时语音 AI](https://developers.openai.com/cookbook/examples/voice_solutions/running_realtime_api_speech_on_esp32_arduino_edge_runtime_elatoai/)
- [Gemini AI + Cardputer + LVGL](https://www.hackster.io/nishad2m8/gemini-ai-cardputer-lvgl-9d7752)
