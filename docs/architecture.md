# 代码架构

## 文件职责

```
src/
├── main.cpp          # 入口 + 模式调度 + WiFi/NTP + Setup 向导
├── companion.h/cpp   # 伴侣模式
├── chat.h/cpp        # 聊天模式
├── ai_client.h/cpp   # API 客户端
├── sprites.h         # 像素素材
├── config.h/cpp      # 配置管理
└── utils.h           # 公共工具
```

## 应用模式

```
AppMode::SETUP  →  AppMode::COMPANION  ⇄  AppMode::CHAT
                        (TAB)                 (TAB)
```

- **SETUP**：首次启动无配置时进入，键盘输入 WiFi SSID/密码/API Key
- **COMPANION**：默认模式，像素角色 + 时钟
- **CHAT**：聊天模式，键盘输入 + API 对话

切换方式：`TAB` 键在 COMPANION 和 CHAT 之间切换。

## 主循环

```
setup():
  M5Cardputer.begin() → Serial.begin() → 加载配置 → 连 WiFi → NTP 同步 → 进入伴侣模式

loop() (每 16ms):
  M5Cardputer.update() → 读键盘 → 按当前模式调度 → pushSprite 到屏幕
```

帧率约 60fps（`delay(16)`），但 API 调用时会阻塞整个 loop。

## 伴侣模式状态机

```
        任意键
SLEEP ─────────→ IDLE
  ↑                │
  │ 30s无操作      │ Space/Enter
  │                ↓
  └────────────── HAPPY ──→ (3轮后自动回 IDLE)

外部触发：
  aiClient 发送时 → TALK
  aiClient 完成时 → IDLE
```

### 状态行为

| 状态 | 动画帧率 | 行为 |
|------|---------|------|
| IDLE | 500ms | 呼吸/眨眼（4帧循环），星星闪烁 |
| HAPPY | 200ms | 钳子举起 + 弹跳（2帧×3轮） |
| SLEEP | 1000ms | 闭眼 + 飘出 Z 字符 |
| TALK | 250ms | 张嘴/闭嘴交替 |

## 像素角色渲染

### 素材格式

16×16 像素，RGB565 格式，存在 Flash（`PROGMEM`）中：

```cpp
const uint16_t PROGMEM sprite_idle1[16 * 16] = {
    _, _, K, _, ...  // _ = 透明, K = 黑色轮廓, R = 红色身体
};
```

### 渲染流程

```
drawBackground()  → 星空 + 地面
drawCharacter()   → 3倍放大绘制当前帧（跳过透明色）
drawSleepZ()      → 睡眠状态的 Z 字符
drawClock()       → NTP 时间
drawStatusText()  → 状态文字 + TAB 提示
```

使用 `M5Canvas` 双缓冲：所有绘制在内存中的 canvas 上完成，最后 `pushSprite(0, 0)` 一次性推送到屏幕，避免闪烁。

### 透明色处理

使用 magenta (255, 0, 255) 作为透明色，绘制时逐像素跳过：

```cpp
void drawSprite16(canvas, x, y, data) {
    for each pixel:
        if (color != TRANSPARENT)
            canvas.fillRect(x + px*3, y + py*3, 3, 3, color);
}
```

## 聊天模式

### 屏幕布局

```
┌──────────────────────────┐
│ [TAB] back         (12px)│  ← 顶栏
├──────────────────────────┤
│                          │
│  AI 消息靠左（绿色）       │  ← 消息区（可滚动）
│         用户消息靠右（蓝色）│
│                          │
├──────────────────────────┤
│ > 输入文字_        (16px)│  ← 输入栏
└──────────────────────────┘
```

### 消息流

```
用户输入 → Enter → chat.handleEnter()
  → 添加用户消息到历史
  → 设置 pendingMessage
  → 主循环检测到 pending → 调用 aiClient.sendMessage()
  → onToken 回调更新 AI 消息
  → onDone 回调解除等待状态
```

## 配置管理

### 环境变量注入

```
platformio.ini:
  -DWIFI_SSID=\"${sysenv.WIFI_SSID}\"

main.cpp:
  #if defined(WIFI_SSID) && defined(WIFI_PASS)
  if (strlen(WIFI_SSID) > 0) {
      Config::setSSID(WIFI_SSID);  // 覆盖 NVS
      Config::save();
  }
  #endif
```

**始终覆盖**：编译时有环境变量就覆盖 NVS（早期版本有 `!Config::isValid()` 判断，导致旧 NVS 值不被更新）。

### NVS 存储

使用 ESP32 的 Preferences 库，namespace `"companion"`：

| Key | 内容 |
|-----|------|
| `ssid` | WiFi SSID |
| `pass` | WiFi 密码 |
| `apikey` | API Key |

重置：`Fn + R` 清空 NVS 并进入 Setup。

## API 客户端

### 当前配置（OpenClaw Gateway）

- Endpoint: `http://{OPENCLAW_HOST}:{OPENCLAW_PORT}/v1/chat/completions`
- SSE 流式请求（chunked transfer encoding），逐 token 回调显示
- 会话历史：内存中保留最近 2 轮对话
- 认证：`Authorization: Bearer {OPENCLAW_TOKEN}`

### 响应解析（零堆分配 SSE 解析）

```cpp
// 栈上 char[] 缓冲，直接字符串搜索 "content":"..."，无 JsonDocument
char contentBuf[128];
const char* key = strstr(json, "\"content\":\"");
// ... 手动提取内容，处理 JSON 转义
if (onToken) onToken(filteredBuf);  // const char* 回调，无临时 String
```
