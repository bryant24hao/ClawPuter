# Moisture × AI Chat Integration — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 让 AI 聊天角色感知宠物湿润度状态，产生个性化回复风格，并在脱水时限制聊天能力，形成「先照顾宠物才能聊天」的养成闭环。

**Architecture:** 将宠物状态（moistureLevel、weather、mood）注入 AI 的 system prompt，使 AI 回复风格随湿润度动态变化。moisture=0 时在 Chat 层拦截输入，不发送请求。所有改动在 ESP32 固件端，不涉及桌面端。

**Tech Stack:** C++ (ESP32/Arduino)，OpenClaw Gateway (OpenAI-compatible API)，SSE streaming

---

## 核心设计

### 三个联动层级

| 湿润度 | AI 感知（system prompt） | 回复风格 | 聊天能力 |
|--------|------------------------|---------|---------|
| 4 | "你现在很开心，精力充沛" | 活泼、话多、会主动聊天 | 正常 |
| 3 | "你状态还不错" | 正常友好 | 正常 |
| 2 | "你有点渴了" | 偶尔提到渴，但还能聊 | 正常 |
| 1 | "你很渴，快没力气了" | 回复变短、语气懒洋洋 | 正常，但 AI 会求水 |
| 0 | — | — | **锁定**：显示 "too thirsty to chat... press H to spray" |

### 设计决策

1. **prompt 用英文**：AI 模型对英文指令遵循度更好，且 system prompt 走网络传输，英文更省字节
2. **moisture=0 在 Chat 层拦截**：不发请求给 OpenClaw，零网络开销，用户必须先按 H 喷水
3. **不新增 API 参数**：只改 system prompt 内容，OpenClaw Gateway 不需要任何修改
4. **内存安全**：system prompt 用栈上 `char[512]`，当前静态 prompt 约 230 字节，加上状态信息后约 400 字节，不超限

---

## Task 1: 传递宠物状态到 AIClient

**Files:**
- Modify: `src/ai_client.h`
- Modify: `src/ai_client.cpp:270-297`（buildRequestDoc 中的 system prompt）
- Modify: `src/main.cpp`（sendMessage 调用处）

### Step 1: 扩展 AIClient 接口，接受宠物状态

**`src/ai_client.h`** — 添加状态结构体和 setter：

```cpp
// 在 class AIClient 的 public 区域添加：
struct CompanionContext {
    int moisture = 4;       // 0-4
    int weatherType = -1;   // WeatherType enum as int
    float temperature = -999;
    int humidity = -1;      // real humidity %
};

void setCompanionContext(const CompanionContext& ctx);
```

```cpp
// 在 private 区域添加：
CompanionContext companionCtx;
```

### Step 2: 实现 setter

**`src/ai_client.cpp`** — 添加实现（位置：buildRequestDoc 之前）：

```cpp
void AIClient::setCompanionContext(const CompanionContext& ctx) {
    companionCtx = ctx;
}
```

### Step 3: 改造 buildRequestDoc 的 system prompt

**`src/ai_client.cpp`** — 替换 `else` 分支中的静态 prompt（约 293-297 行）：

```cpp
} else {
    // Dynamic system prompt based on companion state
    char prompt[512];
    const char* moodDesc;
    const char* styleHint;

    switch (companionCtx.moisture) {
        case 4:
            moodDesc = "You feel great, full of energy and very happy.";
            styleHint = "Be extra cheerful and talkative. Respond with enthusiasm.";
            break;
        case 3:
            moodDesc = "You feel good, nicely hydrated.";
            styleHint = "Be friendly and playful as usual.";
            break;
        case 2:
            moodDesc = "You are getting a bit thirsty.";
            styleHint = "Occasionally mention being thirsty, but still chat normally.";
            break;
        case 1:
            moodDesc = "You are very thirsty and low on energy.";
            styleHint = "Keep responses very short (under 10 words). "
                        "Sound tired and sluggish. Ask the user to spray water on you (press H).";
            break;
        default:
            moodDesc = "You feel okay.";
            styleHint = "Be friendly and playful.";
            break;
    }

    snprintf(prompt, sizeof(prompt),
        "You are a tiny pixel lobster companion living inside a Cardputer device. "
        "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
        "Use simple words. NEVER use emoji, markdown, or special Unicode. Plain text only. "
        "%s %s",
        moodDesc, styleHint);

    sysMsg["content"] = prompt;
}
```

### Step 4: main.cpp 中每次发消息前更新 context

**`src/main.cpp`** — 在 `aiClient.sendMessage(...)` 调用之前，添加：

```cpp
// Update AI with companion state before sending
AIClient::CompanionContext ctx;
ctx.moisture = companion.getMoistureLevel();
ctx.weatherType = static_cast<int>(companion.getWeatherType());
ctx.temperature = companion.getTemperature();
ctx.humidity = companion.getHumidityPercent();
aiClient.setCompanionContext(ctx);
```

### Step 5: 编译验证

```bash
cd ~/ClawPuter && source .env && pio run
```

Expected: SUCCESS，内存占用变化 < 100 bytes RAM

### Step 6: Commit

```bash
git add src/ai_client.h src/ai_client.cpp src/main.cpp
git commit -m "feat: inject companion moisture state into AI system prompt"
```

---

## Task 2: 脱水锁聊天

**Files:**
- Modify: `src/chat.cpp`（handleEnter, update）
- Modify: `src/companion.h`（添加 getter）
- Modify: `src/main.cpp`（传递 moisture 给 chat）

### Step 1: 让 Chat 知道当前湿润度

**`src/chat.h`** — public 区域添加：

```cpp
void setMoistureLevel(int level) { moistureLevel = level; }
```

**`src/chat.h`** — private 区域添加：

```cpp
int moistureLevel = 4;
```

### Step 2: main.cpp 每帧更新 chat 的 moisture

**`src/main.cpp`** — 在 CHAT 模式的 update 循环中（`chat.update(canvas)` 之前）：

```cpp
chat.setMoistureLevel(companion.getMoistureLevel());
```

### Step 3: handleEnter 拦截脱水状态

**`src/chat.cpp`** — `handleEnter()` 开头添加：

```cpp
void Chat::handleEnter() {
    // Block chat when dehydrated
    if (moistureLevel == 0) return;

    // ... existing code ...
```

### Step 4: drawInputBar 显示脱水提示

**`src/chat.cpp`** — `drawInputBar()` 中，当 `waitingForAI` 显示等待文本的条件之前，添加脱水提示：

```cpp
void Chat::drawInputBar(M5Canvas& canvas) {
    // ... existing background drawing ...

    if (moistureLevel == 0) {
        // Dehydrated: show spray hint instead of input
        canvas.setTextColor(rgb565(255, 100, 80));  // warm red
        canvas.setTextSize(1);
        canvas.drawString("thirsty... press H to spray!", 6, SCREEN_H - INPUT_BAR_H + 4);
        return;
    }

    // ... existing waitingForAI / input rendering ...
```

### Step 5: 编译验证

```bash
cd ~/ClawPuter && source .env && pio run
```

### Step 6: Commit

```bash
git add src/chat.h src/chat.cpp src/main.cpp
git commit -m "feat: lock chat when moisture=0, show spray hint"
```

---

## Task 3: 聊天模式中支持喷水

**Files:**
- Modify: `src/main.cpp`（CHAT 模式按键处理）

### 设计

当前 H 键喷水只在 COMPANION 模式生效。需要在 CHAT 模式也能按 H 喷水，这样用户可以在聊天界面直接解锁。

### Step 1: CHAT 模式按键处理中加入 H 键

**`src/main.cpp`** — 在 CHAT 模式的按键处理中（handleKey 之前），添加：

```cpp
// Allow spraying in chat mode too
if (ks.word.size() > 0 && ks.word[0] == 'h') {
    companion.spray();
}
```

注意：spray() 自带 cooldown，和 chat 的 handleKey('h') 不冲突——chat 收到 'h' 只是往输入框添加字符，如果 moisture=0 输入框被锁定也不会有问题。

但这里有个小问题：如果 moisture > 0，按 h 会同时喷水 + 输入字符 'h'。解决方案：moisture ≤ 1 时才触发 spray，避免正常打字时误触发：

```cpp
if (ks.word.size() > 0 && ks.word[0] == 'h' && companion.getMoistureLevel() <= 1) {
    companion.spray();
    // Don't pass 'h' to chat when spraying
    continue;  // or skip the handleKey call for this key
}
```

### Step 2: 编译验证

```bash
cd ~/ClawPuter && source .env && pio run
```

### Step 3: 手动测试流程

1. 等 moisture 衰减到 0
2. 按 TAB 进入聊天 → 看到 "thirsty... press H to spray!"
3. 按 H → 水花动画 + moisture 变 1 → 输入框恢复
4. 打字发消息 → AI 回复带"still thirsty"风格

### Step 4: Commit

```bash
git add src/main.cpp
git commit -m "feat: allow H to spray in chat mode when dehydrated"
```

---

## Task 4: AI 回复风格验证

这个 Task 不需要写代码，是功能集成测试。

### 测试矩阵

| 测试场景 | 操作 | 期望 AI 行为 |
|---------|------|-------------|
| moisture=4 + 聊天 | 说 "hello" | 热情活泼回复，可能多说几句 |
| moisture=1 + 聊天 | 说 "hello" | 很短的回复（< 10 词），提到渴/要水 |
| moisture=0 + 按 TAB | 进聊天 | 输入框显示 "thirsty... press H to spray!" |
| moisture=0 + 按 ENTER | 尝试发消息 | 无反应（被拦截） |
| moisture=0 + 按 H | 喷水 | 水花 + moisture→1 + 输入框恢复 |
| moisture=1 + 继续聊 | 说 "how are you" | AI 说自己渴/没力气 |

### 验证 system prompt 内容

串口监控，搜索 `POST /v1/chat/completions`，确认 JSON body 中 system prompt 包含状态描述。

---

## 关键文件清单

| 文件 | 改动 |
|------|------|
| `src/ai_client.h` | +CompanionContext 结构体、+setCompanionContext() |
| `src/ai_client.cpp` | buildRequestDoc 中动态 system prompt |
| `src/chat.h` | +moistureLevel 字段、+setMoistureLevel() |
| `src/chat.cpp` | handleEnter 拦截、drawInputBar 脱水提示 |
| `src/main.cpp` | 更新 context、chat mode 按 H 喷水 |

## 内存预算

| 项 | 字节 |
|----|------|
| CompanionContext 结构体 | 16 bytes RAM |
| prompt char[512]（栈） | 0（函数返回即释放） |
| chat.moistureLevel | 4 bytes RAM |
| **合计** | **~20 bytes RAM** |

## 不做的事

- **不改 OpenClaw Gateway**：只改 system prompt，对 API 完全透明
- **不新增网络请求**：moisture 状态内嵌在已有的 chat 请求中
- **不做 AI 主动推送**：AI 只在用户发消息时回复，不主动弹消息
- **不存聊天中的 moisture 变化到 NVS**：和其他 moisture 数据一样，开机重置
- **不改桌面端**：这是纯固件端功能
