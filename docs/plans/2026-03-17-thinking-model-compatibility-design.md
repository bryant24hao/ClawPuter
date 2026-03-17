# Thinking Model Compatibility — 设计方案

## 给产品经理的摘要

### 问题是什么？

ClawPuter 通过 OpenClaw Gateway 和 AI 模型聊天。当用户配置了"推理型模型"（如 GPT-5.4）时，会出现三个问题：

1. **屏幕上显示模型的"内心独白"** — 推理型模型在回答前会先"思考"，这些思考内容本应隐藏，但 Gateway 把它和正式回答混在一起发过来了，用户看到一堆莫名其妙的英文推理过程
2. **长时间卡在"等待中"** — 模型思考太久（30-60 秒），超过了设备的等待时限，用户什么都收不到
3. **回答文不对题** — 模型超时后，Gateway 自动重试，但重试时把用户的原始问题替换成了一句固定的英文（"Continue where you left off"），导致模型回答的根本不是用户问的问题

### 影响范围

- 使用非推理型模型（如 Kimi K2.5、GPT-5.3-codex）的用户 **不受影响**
- 使用推理型模型（GPT-5.4、Claude 等）的用户 **必现这三个问题**
- 项目已开源，需要一个对所有模型通用的修复

### 修复后的体验

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 快速模型（Kimi 等） | 正常 | 正常（无变化） |
| 推理模型思考 20 秒 | 屏幕显示思考内容 | 显示"thinking..."，20 秒后显示正式回答 |
| 推理模型思考 50 秒 | 30 秒超时，空白 | 显示"thinking..."，50 秒后显示正式回答 |
| 推理模型超时 2 分钟 | 空白或乱码 | 显示"taking too long, try again" |
| Gateway 重试注入 | 显示"我不记得之前在做什么" | 显示"try again"友好提示 |

### 改动范围

仅改 ESP32 固件端（2 个文件），不改 OpenClaw Gateway，不改桌面端。用户升级固件即可，无需修改任何配置。

---

## 技术设计

### 根因分析

通过 curl 抓取 OpenClaw Gateway 的原始 SSE 响应，确认了数据格式：

```
data: {"choices":[{"delta":{"role":"assistant"}}]}
data: {"choices":[{"delta":{"content":"think\nThe user is asking..."}}]}   ← thinking chunk
data: {"choices":[{"delta":{"content":"Hi! I'm your pixel companion"}}]}   ← 正式回答
data: [DONE]
```

**发现 1**：Gateway 把 thinking 内容放在 `"content"` 字段里，以 `think\n` 为前缀。和正式回答用同一个字段，只是前缀不同。我们的 SSE parser 无法区分，全部当成回答显示。

**发现 2**：thinking chunk 是完整的一个 SSE event（一行 `data:`）。我们的 parser 按行解析（`lineBuf` 积累到 `\n` 才处理），所以 `think\n` 前缀检查在每个 event 级别是可靠的。

**发现 3**：Gateway 的 fallback 重试机制（`resolveFallbackRetryPrompt` 函数）在模型超时后，把用户消息替换为 `"Continue where you left off. The previous model attempt failed or timed out."`。这是 Gateway 的 agent 设计，不是 bug，但对简单聊天场景有害。

**发现 4**：`strstr(json, "\"content\":\"")` 不会误匹配 `"reasoning_content":"`，因为搜索模式前面有 `"` 字符，而 `reasoning_content` 中 `content` 前面是 `_`。现有 parser 在这一点上是安全的。

### 改动清单

#### 改动 1：滚动空闲超时（替代固定 30 秒超时）

**文件**：`src/ai_client.cpp`

**现状**：
```cpp
unsigned long deadline = millis() + 30000;
// loop: while (... && millis() < deadline)
```
固定 30 秒，推理模型 thinking 阶段经常超过这个时间。

**改为**：
```cpp
unsigned long lastActivity = millis();
unsigned long startTime = millis();

// loop 中每次收到数据时：
lastActivity = millis();

// 超时判断（overflow-safe 减法）：
if (millis() - lastActivity > 30000 || millis() - startTime > 120000) break;
```

- 30 秒空闲超时：只要有数据流入（包括 thinking chunk），就继续等
- 120 秒安全上限：防止无限等待
- 使用减法比较，避免 `millis()` 溢出问题（ESP32 约 49.7 天溢出一次）

**两条解析路径都要改**：chunked 路径（约 line 145-200）和 non-chunked 路径（约 line 201-225）。

#### 改动 2：过滤 thinking chunk

**文件**：`src/ai_client.cpp`

在 `extractContent()` 返回后、`onToken()` 调用前，加入过滤：

```cpp
int clen = extractContent(data, contentBuf, sizeof(contentBuf));
if (clen > 0) {
    // Filter thinking content: skip chunks starting with "think\n"
    if (clen >= 6 && memcmp(contentBuf, "think\n", 6) == 0) {
        thinkingDetected = true;
        lastActivity = millis();  // model is working, reset idle timer
        continue;  // skip this chunk entirely
    }

    int flen = filterForDisplayBuf(contentBuf, filteredBuf, sizeof(filteredBuf));
    // ... existing onToken/fullResponse logic
}
```

**为什么 `think\n` 前缀可靠**：
- Gateway 将整个 thinking 内容放在一个 SSE event 的 `content` 字段中
- 我们的 parser 按行解析，每次处理一个完整的 `data:` 行
- `think\n` 是 Gateway 统一使用的 thinking 标记前缀
- 正常回答不会以 `think\n` 开头（用户的 system prompt 要求纯文本英文回答）

#### 改动 3：Thinking 状态标志

**文件**：`src/ai_client.h`、`src/ai_client.cpp`

在 `AIClient` 上加一个 public flag：
```cpp
bool thinkingDetected = false;
```

在 `sendMessage()` 开头重置为 `false`，检测到 thinking chunk 时设为 `true`。

不新增 callback（review 建议：flag 比 callback 更轻量，不增加 `std::function` 开销）。

#### 改动 4：Chat UI 显示 "thinking..."

**文件**：`src/chat.cpp`

`drawInputBar()` 中，现有逻辑：
```cpp
if (waitingForAI) {
    canvas.drawString("waiting...", 4, barY + 4);
}
```

改为：
```cpp
if (waitingForAI) {
    const char* hint = aiThinking ? "thinking..." : "waiting...";
    canvas.drawString(hint, 4, barY + 4);
}
```

`aiThinking` 从 `AIClient::thinkingDetected` 同步（main.cpp 中每帧更新）。

#### 改动 5：Fallback 注入检测

**文件**：`src/ai_client.cpp`

在第一个非 thinking 的 content chunk 到达时，检查是否被 Gateway 替换：

```cpp
if (clen > 0 && !firstContentSeen) {
    firstContentSeen = true;
    // Detect gateway fallback injection
    if (strstr(contentBuf, "Continue where you left off") ||
        strstr(contentBuf, "previous model attempt")) {
        // Gateway replaced user message, response is invalid
        gatewayFallbackDetected = true;
    }
}
```

streaming 结束后，如果 `gatewayFallbackDetected`：
```cpp
if (gatewayFallbackDetected) {
    fullResponse = "";
    lastResponse = "";
    busy = false;
    if (onError) onError("Model timeout, try again");
    return;
}
```

**为什么只检查第一个 content chunk**：Gateway 的 fallback 注入替换了整个用户消息，模型的回答从第一句话就会体现出来（"I don't have context..." 或 "Continue where..."）。检查第一个 chunk 足够，避免对完整 response 做字符串搜索。

### 内存预算

| 项 | 字节 |
|----|------|
| `thinkingDetected` (bool) | 1 |
| `lastActivity` (unsigned long, 栈) | 4 |
| `startTime` (unsigned long, 栈) | 4 |
| `firstContentSeen` (bool, 栈) | 1 |
| `gatewayFallbackDetected` (bool, 栈) | 1 |
| **合计** | **11 bytes**（其中 10 bytes 是栈变量，函数返回即释放） |

### 不做的事

- **不改 OpenClaw Gateway** — 这是 Gateway 的设计行为（agent 重试），不是 bug。ESP32 端自己处理。
- **不新增 callback** — 用 flag 代替，更轻量。
- **不改 `extractContent` 的 `strstr` 匹配逻辑** — 已验证不会误匹配 `reasoning_content`。
- **不增大 `contentBuf`** — 128 字节对 thinking chunk 会截断，但 `think\n` 前缀在前 6 字节就能判断，截断不影响过滤。
- **不重构双路径** — chunked 和 non-chunked 分别改，保持最小改动。

### 测试计划

```bash
# 编译
cd ~/ClawPuter && source .env && pio run

# Native 测试（已有）
pio test -e native
```

#### 手动测试矩阵

| 测试场景 | 操作 | 期望结果 |
|---------|------|---------|
| 非推理模型正常聊天 | Kimi K2.5 + "你好" | 正常回答，无变化 |
| 推理模型快速回答 | GPT-5.4 thinking < 30s | 显示"thinking..."，然后显示正式回答 |
| 推理模型慢速回答 | GPT-5.4 thinking 30-60s | 显示"thinking..."一直保持，然后显示正式回答 |
| 推理模型超时 | GPT-5.4 thinking > 120s | 显示"taking too long, try again" |
| Gateway fallback 触发 | 模型超时 → 回答含 "Continue" | 显示"try again"而非错误内容 |
| 30s 无数据 | 模型/网络卡死 | 30s 后超时，显示错误 |
| 像素画命令 | `/draw a cat` | 正常生成像素画（不受 thinking 过滤影响） |
| 脱水锁定 | moisture=0 时聊天 | 显示 "thirsty... press H to spray!" |

### 关键文件清单

| 文件 | 改动 |
|------|------|
| `src/ai_client.h` | +`thinkingDetected` flag |
| `src/ai_client.cpp` | 滚动超时、thinking 过滤、fallback 检测（双路径） |
| `src/chat.h` | +`aiThinking` flag |
| `src/chat.cpp` | drawInputBar 显示 "thinking..." |
| `src/main.cpp` | 同步 `aiClient.thinkingDetected` → `chat.aiThinking` |
