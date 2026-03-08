# OpenClaw 集成调研

## 什么是 OpenClaw？

[OpenClaw](https://github.com/openclaw/openclaw) 是一个开源的个人 AI 助手框架（前身叫 Clawdbot / Moltbot），由 Peter Steinberger 开发。2026 年 1 月底发布后一周内 GitHub 星标突破 10 万，是 GitHub 历史上增长最快的开源项目之一。

**核心架构：**

```
消息平台 (WhatsApp/Telegram/Slack/...)
        ↓
  Gateway Daemon（常驻进程，路由消息、管理会话）
        ↓
  Agent Runtime（推理循环、记忆、插件、Skills）
        ↓
  LLM Provider (Claude / GPT / DeepSeek / ...)
```

- 本地优先：运行在你的机器或 VPS 上
- 多渠道：WhatsApp、Telegram、Slack、Discord、Signal、iMessage 等
- 多模型：Claude、GPT、DeepSeek 均可
- 可执行动作：发邮件、创建文件、爬网页、设提醒、调 API、定时任务

## ESP32 上的 OpenClaw 生态

已有多个项目把 OpenClaw 搬到了 ESP32 硬件上：

### 1. MimiClaw — 最成熟的方案

- **仓库**: [memovai/mimiclaw](https://github.com/memovai/mimiclaw)
- **硬件**: ESP32-S3（和我们的 Cardputer 同芯片）
- **特点**:
  - 99.2% 纯 C 裸金属实现，无需 Linux/Node.js
  - 双核运行（网络 I/O + AI 处理分开）
  - WebSocket gateway 端口 18789
  - 支持 Anthropic Claude 和 OpenAI GPT
  - ReAct agent loop + tool calling
  - 通过 Telegram 交互
  - 功耗约 0.5W，USB 供电即可
  - OTA 空中升级
- **技术栈**: ESP-IDF（非 Arduino）
- **Flash 配置**: 16MB Flash QIO, 8MB Octal PSRAM, 240MHz

### 2. ZClaw — 最小方案

- **特点**: 888KB 极小体积，专为资源受限设备优化
- **报道**: [Geeky Gadgets](https://www.geeky-gadgets.com/zclaw-esp32-openclaw-agent/)
- GPIO 直接控制，可读传感器、控制继电器

### 3. openclaw-esp32 — Arduino 方案

- **仓库**: [hrwtech/openclaw-esp32](https://github.com/hrwtech/openclaw-esp32)
- **特点**: 基于 Arduino 框架（和我们的项目技术栈一致）

### 4. HeyClawy — 语音伴侣

- **仓库**: [omeriko9/HeyClawy](https://github.com/omeriko9/HeyClawy)
- **特点**: 语音优先的 ESP32 OpenClaw 伴侣
  - 按键说话 或 唤醒词（默认 "Hey Jarvis"）
  - TTS 语音回复
  - 内置 Web 设置页面

## OpenClaw 扩展机制

### Skills（技能）

Skills 是 OpenClaw 的核心扩展方式，有两种类型：

1. **SKILL.md（自然语言技能）**: Markdown 文件 + YAML 前置数据，描述任务流程
2. **TypeScript 技能**: 完整编程能力，可调 API、执行复杂逻辑

社区已有 5400+ 技能，通过 [ClawHub](https://github.com/VoltAgent/awesome-openclaw-skills) 管理。

创建自定义技能：
```bash
openclaw skills create my-skill
cd my-skill && npm install && npm run dev
```

### MCP Server（模型上下文协议）

OpenClaw 原生支持 [MCP](https://safeclaw.io/blog/openclaw-mcp)，在 `openclaw.json` 中配置 MCP server：

```json
{
  "mcpServers": {
    "my-server": {
      "command": "node",
      "args": ["server.js"]
    }
  }
}
```

MCP server 作为独立进程运行，通过 JSON-RPC 2.0 与 OpenClaw 通信，暴露的工具可被任何 agent 调用。

## CardputerCompanion × OpenClaw 集成方案

### 方案 A：Cardputer 作为 OpenClaw 的物理终端（推荐起步）✅ 已验证可行

```
Cardputer (ESP32-S3)  ←── HTTP (OpenAI 兼容) ──→  OpenClaw Gateway (Mac)
   键盘输入                                          Agent Runtime
   屏幕显示                                          Skills / MCP / 记忆
   像素角色反应                                       LLM 调用 (Kimi/Claude/GPT)
```

**关键发现**：OpenClaw Gateway 自带 [OpenAI 兼容 HTTP API](https://docs.openclaw.ai/gateway/openai-http-api)，端点为 `POST /v1/chat/completions`。这意味着我们现有调 Kimi K2.5 的代码 **几乎不用改** 就能对接 OpenClaw。

**请求格式**（和 Kimi K2.5 完全一样）：
```bash
curl http://<YOUR_MAC_IP>:<your-port>/v1/chat/completions \
  -H 'Authorization: Bearer <gateway-token>' \
  -H 'Content-Type: application/json' \
  -d '{
    "model": "openclaw",
    "messages": [{"role":"user","content":"你好"}],
    "stream": false
  }'
```

**会话保持**：在请求中加 `"user": "cardputer"` 字段，Gateway 会自动生成稳定的 session key，实现跨请求对话记忆。

**做法**：
1. Mac 端：`openclaw.json` 中启用 HTTP API + 改绑定为 LAN
2. Cardputer 端：`ai_client.cpp` 改 URL 指向 Mac 的局域网 IP
3. 键盘输入 → OpenClaw Agent → Skills/Tools/记忆 → 回复到 Cardputer
4. 像素小龙虾根据回复类型做出不同反应

**优点**：改动极小（只改 URL 和 token），复用 OpenClaw 5400+ skills + 持久记忆 + 多模型切换
**缺点**：依赖同局域网的 Mac 运行 OpenClaw

### 方案 B：Cardputer 运行精简 OpenClaw Agent

```
Cardputer (ESP32-S3)
   ├── 精简 Agent Loop (ReAct)
   ├── 基础 Tool Calling
   ├── 键盘 + 屏幕 + 像素角色
   └── WiFi → LLM API
```

**做法**：
- 参考 MimiClaw / ZClaw 的架构，在 Cardputer 上跑精简版 agent loop
- 支持基础 tool calling（获取天气、设提醒、简单计算）
- 像素角色 + 键盘交互作为独特的物理界面

**优点**：完全独立，无需外部服务器
**缺点**：开发量大，受限于 ESP32 资源（8MB PSRAM）

### 方案 C：混合模式（最终目标）

```
Cardputer
   ├── 离线模式：本地精简 agent（基础功能）
   └── 在线模式：连接 OpenClaw Gateway（完整能力）
```

**做法**：
- 默认本地运行基础 agent（时钟、提醒、简单聊天）
- 检测到 OpenClaw Gateway 时自动升级为完整终端
- Cardputer 特有的 skill：物理按键快捷操作、屏幕通知、像素角色状态

## 当前环境（已验证）

你的 Mac 上已有 OpenClaw 运行：

| 项 | 值 |
|---|---|
| 版本 | 2026.2.26 |
| Gateway | `ws://127.0.0.1:<your-port>`（当前仅 loopback） |
| 认证 | token 模式 |
| 主模型 | Kimi K2.5（回退 Gemini 3 Pro / GPT 5.2） |
| 渠道 | Telegram + 飞书 |
| Mac 局域网 IP | <YOUR_MAC_IP> |
| Agent | 1 个 (main)，32 个活跃 session |

## 实施步骤

### 第一步：启用 HTTP API + LAN 绑定（Mac 侧，5 分钟）

修改 `~/.openclaw/openclaw.json`：

```json
{
  "gateway": {
    "bind": "lan",           // ← 从 "loopback" 改为 "lan"
    "http": {
      "endpoints": {
        "chatCompletions": {
          "enabled": true    // ← 新增：启用 OpenAI 兼容 API
        }
      }
    }
  }
}
```

然后重启 Gateway：`openclaw gateway restart`

验证：
```bash
curl http://<YOUR_MAC_IP>:<your-port>/v1/chat/completions \
  -H 'Authorization: Bearer <gateway-token>' \
  -H 'Content-Type: application/json' \
  -d '{"model":"openclaw","messages":[{"role":"user","content":"hi"}]}'
```

### 第二步：修改 Cardputer 固件（10 分钟）

`ai_client.cpp` 改动极小，因为 OpenClaw HTTP API 和 Kimi K2.5 格式完全兼容：

| 当前 (Kimi) | 改为 (OpenClaw) |
|---|---|
| `https://api.moonshot.cn/v1/chat/completions` | `http://<YOUR_MAC_IP>:<your-port>/v1/chat/completions` |
| `Authorization: Bearer <kimi-key>` | `Authorization: Bearer <gateway-token>` |
| `"model": "kimi-k2.5"` | `"model": "openclaw"` |
| HTTPS + `setInsecure()` | HTTP（局域网，无需 TLS） |

额外可加 `"user": "cardputer"` 字段实现会话保持。

### 第三步：写 Cardputer Skill（未来）

为 OpenClaw 写一个 `cardputer` skill，让 OpenClaw 能：
- 推送通知到 Cardputer 屏幕（角色举牌）
- 触发像素角色特定动画
- 读取 Cardputer 的按键输入作为快捷指令

### 第四步：探索本地 Agent（未来）

参考 MimiClaw 的 ReAct loop 实现，在 Cardputer 上跑基础 tool calling。

## 方案 A 的优势（对比直接调 Kimi API）

| 能力 | 直接调 Kimi | 通过 OpenClaw |
|---|---|---|
| 模型 | 仅 Kimi K2.5 | Kimi + Claude + GPT + Gemini 自动切换 |
| 对话记忆 | 无持久化 | OpenClaw 自带 session 管理 + 持久记忆 |
| 工具调用 | 无 | 5400+ skills（发邮件、查天气、控设备...）|
| System Prompt | 硬编码 | OpenClaw agent 配置，可随时调整 |
| 安全 | API key 存在设备上 | token 只访问本地 Gateway |
| 延迟 | 公网 RTT | 局域网 <10ms |

## 参考资源

- [OpenClaw 官方文档](https://docs.openclaw.ai/)
- [OpenClaw GitHub](https://github.com/openclaw/openclaw)
- [MimiClaw — ESP32-S3 上的 OpenClaw](https://github.com/memovai/mimiclaw)
- [ZClaw — 888KB 极小 Agent](https://news.ycombinator.com/item?id=47100232)
- [openclaw-esp32 — Arduino 方案](https://github.com/hrwtech/openclaw-esp32)
- [HeyClawy — 语音 ESP32 伴侣](https://github.com/omeriko9/HeyClawy)
- [OpenClaw Skills 开发文档](https://docs.openclaw.ai/tools/skills)
- [OpenClaw MCP 集成指南](https://safeclaw.io/blog/openclaw-mcp)
- [5400+ OpenClaw Skills 合集](https://github.com/VoltAgent/awesome-openclaw-skills)
- [CNX Software: MimiClaw 报道](https://www.cnx-software.com/2026/02/13/mimiclaw-is-an-openclaw-like-ai-assistant-for-esp32-s3-boards/)
- [Hackster: $5 MimiClaw](https://www.hackster.io/news/openclaw-for-the-rest-of-us-the-5-mimiclaw-assistant-297b325507fd)
