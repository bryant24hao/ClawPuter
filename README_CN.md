# Cardputer Companion 🦞

[English](README.md)

M5Stack Cardputer (ESP32-S3) 上的像素风桌面伴侣。OpenClaw 小龙虾角色 + 动画 + 时钟 + AI 聊天，通过 [OpenClaw](https://github.com/openclaw/openclaw) 接入 AI。

## 功能

- **伴侣模式** — 像素小龙虾，支持待机、开心、睡觉、说话、伸懒腰、东张西望等动画。根据时间自动切换日/夜背景（星星、月亮）。NTP 时钟显示。
- **聊天模式** — 键盘输入，通过 OpenClaw Gateway（OpenAI 兼容 API）进行 AI 对话，消息支持自动换行和翻页滚动。
- **OpenClaw 集成** — 局域网连接本地 OpenClaw Gateway。多模型自动切换（Kimi/Claude/GPT/Gemini），持久记忆，5400+ 社区技能。
- **开机动画** — 小龙虾像素画逐行渐入，模式切换像素擦除过渡。
- **音效** — 按键咔嗒、开心音阶、AI 回复提示音。

## 快速开始

### 1. 设置环境变量

```bash
export WIFI_SSID="你的WiFi名称"
export WIFI_PASS="你的WiFi密码"
export OPENCLAW_HOST="192.168.1.100"   # Mac/VPS 的局域网 IP
export OPENCLAW_PORT="18789"
export OPENCLAW_TOKEN="你的Gateway Token"
```

### 2. 编译烧录

```bash
pio run -t upload
```

首次烧录需要手动进入下载模式：按住 **G0** + 按 **Reset**，然后松开 G0。详见[烧录指南](docs/setup-and-flash.md)。

### 3. 串口调试

```bash
pio device monitor
```

## 操作方式

| 按键 | 伴侣模式 | 聊天模式 |
|------|---------|---------|
| TAB | 切换到聊天 | 切换到伴侣 |
| 空格 / Enter | 角色开心跳跃 | 发送消息 |
| Backspace | — | 删除字符 |
| Fn + ; | — | 向上翻页 |
| Fn + / | — | 向下翻页 |
| Fn + R | 重置配置 | — |
| 任意键 | 唤醒角色 | 输入字符 |

## 项目结构

```
src/
├── main.cpp          # 入口，模式调度，WiFi/NTP
├── companion.h/cpp   # 伴侣模式：动画、状态机、时钟
├── chat.h/cpp        # 聊天模式：消息气泡、输入栏、滚动
├── ai_client.h/cpp   # OpenClaw Gateway 客户端（OpenAI 兼容）
├── sprites.h         # 像素小龙虾素材（RGB565）
├── config.h/cpp      # WiFi/API 配置，NVS 持久化
└── utils.h           # 颜色定义、屏幕常量、定时器
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
- [问题排查](docs/troubleshooting.md)
- [路线图](docs/roadmap.md)

## 路线图

- [ ] 电量显示 + 低电量角色变虚弱
- [ ] 聊天历史持久化（NVS/SD 卡）
- [ ] 流式响应（SSE 逐字显示）
- [ ] 养成系统（饥饿值/心情值）
- [ ] 番茄钟
- [ ] 天气显示
- [ ] BLE 手机通知推送

完整路线图见 [roadmap.md](docs/roadmap.md)。

## 许可证

MIT
