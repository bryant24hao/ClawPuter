# ESP32 语音聊天开发踩坑记录与原则

> CardputerCompanion 项目在 M5Cardputer (ESP32-S3, 无 PSRAM, 328KB SRAM) 上实现语音输入 + AI 流式聊天的完整踩坑记录。
> 初版: 2026-03-03 | 更新: 2026-03-03（坑 7: 按需分配的碎片化陷阱）

---

## 目录

1. [硬件约束总览](#1-硬件约束总览)
2. [坑 1: HTTP Chunked Transfer Encoding 断裂 SSE 流](#2-坑-1-http-chunked-transfer-encoding-断裂-sse-流)
3. [坑 2: M5Cardputer 键盘在阻塞调用后失灵](#3-坑-2-m5cardputer-键盘在阻塞调用后失灵)
4. [坑 3: Arduino String 导致堆碎片化](#4-坑-3-arduino-string-导致堆碎片化)
5. [坑 4: 160KB 语音缓冲区挤占聊天堆内存](#5-坑-4-160kb-语音缓冲区挤占聊天堆内存)
6. [坑 5: STT 等待响应期间只有 13KB 可用堆](#6-坑-5-stt-等待响应期间只有-13kb-可用堆)
7. [坑 6: 渲染循环中的 substring() 每帧碎片化](#7-坑-6-渲染循环中的-substring-每帧碎片化)
8. [坑 7: "按需分配、尽早释放"的碎片化陷阱](#8-坑-7-按需分配尽早释放的碎片化陷阱)
9. [沉淀：ESP32 内存受限设备开发原则](#9-沉淀esp32-内存受限设备开发原则)
10. [沉淀：M5Cardputer 特有注意事项](#10-沉淀m5cardputer-特有注意事项)
11. [诊断工具箱](#11-诊断工具箱)

---

## 1. 硬件约束总览

```
ESP32-S3 (M5StampS3)
  总 SRAM:         ~328 KB
  WiFi 栈:         ~90 KB（连接后常驻）
  Canvas 精灵图:    ~65 KB（240x135 RGB565）
  可用堆 (无语音):  ~173 KB
  语音录音缓冲:     ~160 KB（16kHz x 5s x 16bit）
  可用堆 (录音中):  ~13 KB  ← 极度危险
```

**核心矛盾**: 录音需要 160KB 连续内存，聊天需要足够堆做 HTTP + JSON + 流式解析。两者不能同时满足。

---

## 2. 坑 1: HTTP Chunked Transfer Encoding 断裂 SSE 流

### 现象
AI 回复前 1-2 轮正常，第 3 轮开始返回 0 字节。

### 根因
OpenClaw 网关返回 `Transfer-Encoding: chunked`。原代码用 `client.readStringUntil('\n')` 逐行读取 SSE，但 chunked 编码下一个 `data: {...}` 行可能跨越两个 chunk 边界：

```
[chunk 1]
data: {"choices":[{"delta":{"conte

[chunk 2]
nt":"你好"}}]}
```

`readStringUntil('\n')` 在 chunk 1 结尾处读到 chunk 边界的 `\r\n`（chunk trailer），误以为是一行结束，把 `data: {"choices":[{"delta":{"conte` 当作完整行去解析 JSON — 失败。

### 解决方案
实现字节级状态机，手动解码 chunked transfer：

```
状态: CS_SIZE → CS_DATA → CS_TRAILER → CS_SIZE → ...
```

- `CS_SIZE`: 读 chunk 大小（十六进制）
- `CS_DATA`: 逐字节读内容，放入 `lineBuf[]`。遇到 `\n` 才算一行结束
- `CS_TRAILER`: 跳过 chunk 尾部的 `\r\n`

**关键**: `lineBuf` 跨 chunk 边界保持不清空，直到遇到 `\n` 才处理。这样被 chunk 边界切断的 `data:` 行会正确拼接。

### 原则
> **P1: 不要假设 HTTP 响应的行边界和 chunk 边界对齐。** 任何用 `readStringUntil('\n')` 读 chunked 流的代码都有 bug。必须先解码 chunk，再在数据中找行边界。

---

## 3. 坑 2: M5Cardputer 键盘在阻塞调用后失灵

### 现象
语音输入 + STT 完成后，文本正确出现在输入框，但 **按 Enter 没反应**。前 2 轮正常，第 3 轮起失效。

### 根因
`M5Cardputer.Keyboard.isChange()` 内部维护一个"上次按键状态"的 baseline。阻塞的 STT HTTP 调用耗时 2-5 秒，期间没有调用 `M5Cardputer.update()`，baseline 变得陈旧。STT 返回后，`isChange()` 比较当前状态和陈旧 baseline，发现"没有变化"（因为阻塞期间用户已经松开了所有键，baseline 记录的也是"无键按下"）。

**但** `keysState()` 始终返回当前硬件状态，不依赖 baseline。这解释了为什么 Fn 键（用 `keysState()` 检测）一直正常，而 Enter（用 `isChange()` 检测）失败。

### 解决方案
**完全放弃 `isChange()`**，改用 `keysState()` + 手动边沿检测：

```cpp
auto ks = M5Cardputer.Keyboard.keysState();
static bool pEnter = false;
bool enterDown = ks.enter && !pEnter;  // 上升沿
pEnter = ks.enter;                      // 保存 baseline
```

阻塞调用后，重新同步 baseline：

```cpp
if (didBlock) {
    M5Cardputer.update();
    ks = M5Cardputer.Keyboard.keysState();
}
// 用 didBlock 标志抑制阻塞帧的虚假边沿
bool enterDown = !didBlock && ks.enter && !pEnter;
```

### 原则
> **P2: 有阻塞调用的 loop() 中，不要依赖框架的 "状态变化检测" API。** 自己用 `static` 变量做边沿检测更可靠。阻塞调用后必须重新同步硬件状态。

---

## 4. 坑 3: Arduino String 导致堆碎片化

### 现象
每轮对话堆减少 500-800 字节，3-4 轮后堆不够用。

### 根因
Arduino `String` 的 `+=` 每次可能触发 `realloc()`。如果新块无法原地扩展，分配器会分配新块、拷贝、释放旧块 — 留下一个无法利用的空洞。`substring()` 也是每次 `malloc` + `free`。

在 60fps 渲染循环中：
```cpp
// 每帧执行 7-8 次（每条可见 AI 消息一次），每次内部循环 20-40 次
while (text.length() > 0) {
    while (fitLen > 0 && canvas.textWidth(text.substring(0, fitLen).c_str()) > MAX_W) {
        fitLen--;  // 每次 substring() = 一次 malloc + free
    }
    text = text.substring(fitLen);  // 又一次 malloc + free
}
```

这意味着 **每帧产生 100+ 次 malloc/free**，持续碎片化堆。

### 解决方案
用 `const char*` 指针 + 栈上 `char buf[]` 完全替代：

```cpp
const char* text = msg.text.c_str();
int pos = 0;
while (pos < len) {
    // 二分查找 + memcpy 到栈 buffer，零堆分配
    int fit = fitBytes(canvas, text + pos, len - pos, MAX_W, buf, sizeof(buf));
    memcpy(buf, text + pos, fit);
    buf[fit] = '\0';
    canvas.drawString(buf, 6, y);
    pos += fit;
}
```

同时用二分查找替代线性递减（O(log n) vs O(n) 次 `textWidth` 调用）。

### 原则
> **P3: 60fps 渲染循环中绝对不能有堆分配。** 所有文本处理用 `const char*` + 栈 `char[]` + `memcpy`。`substring()` 在循环中是禁用词。

> **P4: 所有长生命周期的 String 必须 `reserve(N)`。** 包括 AI 消息 (`reserve(320)`)、聊天历史 (`reserve(120/320)`)、fullResponse (`reserve(320)`)。预分配避免 `+=` 时 realloc。

---

## 5. 坑 4: 160KB 语音缓冲区挤占聊天堆内存

### 现象
录音缓冲在 `begin()` 时一次性分配 160KB，之后永远不释放。聊天时只剩 ~33KB 可用堆，3-4 轮就耗尽。

### 解决方案
**按需分配、用完即释放**:

```
begin()           → 只计算 maxSamples，不分配
startRecording()  → heap_caps_malloc(160KB)
stopRecording()   → sendToSTT() 内部上传完数据后立即 heap_caps_free()
```

效果: 聊天时有 ~190KB 可用堆（vs 之前 ~33KB）。

### 原则
> **P5: 大块内存（>10KB）必须按需分配、尽早释放。** 不要在 `begin()` 里分配然后永远持有。用 `heap_caps_malloc` / `heap_caps_free` 精确控制生命周期。

---

## 6. 坑 5: STT 等待响应期间只有 13KB 可用堆

### 现象
语音数据上传完成后，等待 Whisper 服务器处理（2-5 秒），此时 160KB 缓冲区仍被占用，但数据已经完全发送 — 白白浪费。这期间只有 ~13KB 可用堆，`String response += line` 的任何堆分配都可能失败。

### 解决方案
**上传完就释放，不等响应**:

```cpp
// 发送 PCM 数据...
client.flush();
freeBuffer();  // 160KB 立刻回收！
// 现在有 ~190KB 等 Whisper 响应
```

同时，STT 响应解析也全部改为零堆分配:
- `String response` → `char responseBuf[512]`（栈）
- `readStringUntil('\n')` → 字节级读取到 `char lineBuf[256]`（栈）
- `ArduinoJson` → `strstr()` 手动提取 `"text":"..."` 字段

### 原则
> **P6: 大块资源在"逻辑用完"时就释放，不要等函数返回。** 如果数据已发送到网络，本地副本立即可释放。不要让"代码整洁"（函数末尾统一释放）压过"内存安全"。

---

## 7. 坑 6: 渲染循环中的 substring() 每帧碎片化

（详见 [坑 3](#4-坑-3-arduino-string-导致堆碎片化)，此处补充其他热路径中发现的同类问题）

### 发现的所有热路径堆分配

| 位置 | 原代码 | 修复后 |
|------|--------|--------|
| `chat.cpp` drawMessages | `text.substring(0, fitLen)` | `memcpy` + 栈 `char buf[64]` |
| `chat.cpp` drawInputBar | `"> " + inputBuffer + "_"` | `snprintf(buf, ...)` |
| `ai_client.cpp` SSE 循环 | `filterForDisplay(String(contentBuf))` | `filterForDisplayBuf()` 栈内原地过滤 |
| `ai_client.cpp` 发送请求体 | `String body = buildRequestBody()` | `serializeJson(doc, client)` 直接序列化到 socket |
| `voice_input.cpp` multipart | `String partHeader = "..." + boundary + "..."` | `snprintf(partHeader, ...)` 栈数组 |
| `voice_input.cpp` 转写动画 | `String msg += "."` | `static const char* msgs[]` 预定义数组 |
| `main.cpp` 连接动画 | `"Connecting" + String("....").substring(...)` | `static const char* dots[]` |
| `main.cpp` 密码遮罩 | `masked += '*'` 循环 | `memset(masked, '*', len)` |
| `main.cpp` 键盘 baseline | `std::vector<char> pWord` | `char pWordChar` |
| `state_broadcast.cpp` | `JsonDocument` 每 200ms | `snprintf()` |

### 原则
> **P7: 审计每一个 60fps 循环中的表达式，确保零堆分配。** Arduino 的 `String` 拼接、`substring()`、`std::vector` 赋值、`JsonDocument` 构造 — 全部是堆分配。用 `snprintf` + 栈 `char[]` + `const char*` 指针替代。

---

## 8. 坑 7: "按需分配、尽早释放"的碎片化陷阱

> **本节记录 2026-03-03 晚间发现的 bug，直接推翻了之前的 P5 原则。**

### 背景

坑 4 的解决方案是"语音缓冲区按需分配、用完释放"（P5），让聊天时有 ~190KB 可用堆。这在单轮对话中完美运行。

### 现象

**第二轮语音对话必定失败**。Serial 日志：

```
[VOICE] Buffer freed, heap=183672          ← 第一轮录音后释放，堆回到 183KB
[AI] Done, 47 chars, heap=186652, largest=94196   ← AI 回复后，最大连续块只有 94KB!
[VOICE] Buffer allocation failed! need=96000 heap=187192 largest=94196  ← 差 1804 字节
```

总空闲堆 187KB，但最大连续块只有 94KB，装不下 96KB 的语音缓冲区。

### 根因：释放-重分配循环是碎片化陷阱

完整的内存布局变化过程：

```
启动后（WiFi 已连接，Chat 消息已预分配）:
┌─────────────┬──────────────────────────────────────────────────┐
│ WiFi+系统    │              大连续空闲块 ~160KB+                │
│ ~130KB 散布  │                                                  │
└─────────────┴──────────────────────────────────────────────────┘
                ↑ 语音缓冲在这里分配 96KB，成功 ✓

第一轮录音 + STT 完成，缓冲释放：
┌─────────────┬──────────────────────────────────────────────────┐
│ WiFi+系统    │              大连续空闲块 ~160KB+                │
└─────────────┴──────────────────────────────────────────────────┘
                ↑ 96KB 被释放，空间回来了

第一轮 AI 对话（sendMessage 内部的临时分配/释放）：
┌─────────────┬────┬───┬────┬───┬──────────────────────────────┐
│ WiFi+系统    │WiFi│   │WiFi│   │      空闲 ~94KB              │
│              │内部│空洞│内部│空洞│                              │
└─────────────┴────┴───┴────┴───┴──────────────────────────────┘
                                  ↑ 最大连续块只有 94KB，不够 96KB ✗
```

**关键洞察**：`sendMessage()` 内部创建 `WiFiClient`（~4-8KB 内部缓冲）、`JsonDocument`、`String fullResponse` 等临时对象。虽然这些对象在函数返回后都被释放，但 **WiFi 协议栈的内部分配**（SSL session cache、DNS cache、socket buffer 等）是持久的，它们被插入到之前语音缓冲所在的大连续块中间，把它切碎了。

即使我们的代码做到了零泄漏，**ESP-IDF WiFi 栈的内部分配行为不受我们控制**。每次网络请求都可能在堆中间留下新的持久碎片。

### 为什么之前没发现

坑 4 的测试是"录音 → 聊天 → 录音"单轮循环。第一轮 STT 请求的 WiFi 内部分配恰好没有切碎大块。但加入 AI 聊天后，两次网络请求（STT + AI）的 WiFi 内部分配叠加，把大块切得更碎。

### 解决方案

**推翻 P5，改为永久持有**：

```cpp
// voice_input.cpp — begin() 时一次分配，永不释放
void VoiceInput::begin() {
    maxSamples = (size_t)(SAMPLE_RATE * MAX_RECORD_SEC);
    allocBuffer();  // 启动时堆最完整，分配必定成功
    // 之后永不调用 freeBuffer()
}

// startRecording() 不再分配，只检查
void VoiceInput::startRecording() {
    if (recording || maxSamples == 0 || !recordBuffer) return;
    // 直接使用已有缓冲...
}

// stopRecording() 和 sendToSTT() 中移除所有 freeBuffer() 调用
```

**配合的其他修复**：

| 修复 | 原因 |
|------|------|
| 语音缓冲 5s→3s（160KB→96KB） | 减小缓冲区，给其他操作留更多堆空间 |
| HTTP header 解析改用栈缓冲 | 消除 `readStringUntil()` 的堆碎片化 |
| Token 回调 `const String&` → `const char*` | 消除每个 SSE token 的临时 String 分配 |
| 错误回调用 `snprintf` 栈缓冲 | 消除 `"[Error: " + error + "]"` 的 String 拼接 |
| Chat 消息启动时预分配 20 个槽位 | 防止后续 `addMessage` 在堆中间分配新 String |

### 永久持有的代价 vs 收益

```
代价: 96KB 常驻内存（约占可用堆的 50%）
剩余: ~91KB 给 WiFiClient + JsonDocument + 流式解析

AI sendMessage() 峰值需求:
  WiFiClient:    ~8KB
  JsonDocument:  ~3KB (含 2 轮历史)
  fullResponse:  ~320B
  栈缓冲:        ~1KB
  合计:          ~12KB ← 91KB 绰绰有余
```

### 原则更新

> **P5（修订）: 大块内存的分配策略取决于"最大连续块"与"分配大小"的比值。**
>
> - 如果分配大小 **远小于** `largest_free_block`（< 50%）→ 按需分配、尽早释放
> - 如果分配大小 **接近** `largest_free_block`（> 70%）→ **一次分配、永久持有**
>
> 原因：释放后的大块空间会被 WiFi 栈等不受控的内部分配切碎，再次分配时可能找不到等大的连续块。这不是内存泄漏，而是**碎片化陷阱（fragmentation trap）**。

> **P15: WiFi 协议栈的内部分配行为不受应用层控制。** 每次 `WiFiClient::connect()` 都可能在堆中间留下持久碎片。不要假设"释放后就能重新分配同样大的块"。

> **P16: 启动时是堆最完整的时刻。** 所有大块分配应在 `begin()` / `setup()` 中完成。`loop()` 中只做小块操作。

---

## 9. 沉淀：ESP32 内存受限设备开发原则

### 内存管理

| # | 原则 | 说明 |
|---|------|------|
| P3 | **渲染循环零堆分配** | 60fps 循环中不允许 `String`、`substring()`、`new`、`JsonDocument` |
| P4 | **长生命周期 String 必须 reserve()** | AI 消息、历史、fullResponse 等，在创建时 `reserve(N)` |
| P5 | **大块内存按需分配、尽早释放** | 录音缓冲 160KB 只在录音时持有，用完立即 `heap_caps_free` |
| P6 | **逻辑用完就释放** | 数据发送完毕后立刻释放本地副本，不等函数返回 |
| P7 | **审计所有热路径** | 用 `snprintf` + `char[]` + `const char*` 替代所有 String 操作 |

### 网络通信

| # | 原则 | 说明 |
|---|------|------|
| P1 | **不假设行边界和 chunk 边界对齐** | chunked transfer 必须先解码 chunk 再找行边界 |
| P8 | **HTTP 响应头用字节级解析** | `readStringUntil` 在低内存时不可靠，用字节级状态机 |
| P9 | **JSON 解析用 strstr() 替代 ArduinoJson** | 对已知结构的简单 JSON，`strstr` + 手动转义解码 = 零堆分配 |
| P10 | **serializeJson 直接写 WiFiClient** | `measureJson()` 算 Content-Length，`serializeJson(doc, client)` 直接发送，省掉中间 String |
| P11 | **永远不要在请求间断开 WiFi** | ESP32 WiFi off→on 会永久丢失 ~170KB 堆（已知 bug） |

### 硬件交互

| # | 原则 | 说明 |
|---|------|------|
| P2 | **阻塞调用后重新同步硬件状态** | 不依赖 `isChange()`，自己做边沿检测 |
| P12 | **Mic 和 Speaker 共享 GPIO** | M5Cardputer 的 Mic 和 Speaker 共用 GPIO 43，必须 `Speaker.end()` 后才能 `Mic.begin()`，反之亦然 |

### 诊断

| # | 原则 | 说明 |
|---|------|------|
| P13 | **关注 largest_free_block 而非 getFreeHeap** | 碎片化时总堆很大但最大连续块很小，后者才是真正的分配上限 |
| P14 | **每轮操作后输出堆诊断** | `heap=%u, largest=%u, min_ever=%u`，用于追踪碎片化趋势 |

---

## 9. 沉淀：M5Cardputer 特有注意事项

1. **键盘 `isChange()` 不可靠**: 任何超过 ~100ms 的阻塞调用后，`isChange()` 的内部 baseline 就会失效。必须用 `keysState()` + 手动边沿检测。

2. **Mic/Speaker 互斥**: GPIO 43 共享。切换顺序: `Speaker.end()` → `Mic.begin()` → 录音 → `Mic.end()` → `Speaker.begin()`。

3. **无 PSRAM**: M5StampS3 没有 PSRAM。所有内存管理必须在 ~328KB SRAM 内完成。不能用依赖 PSRAM 的库或示例代码。

4. **屏幕 240x135**: 所有文本渲染需要自己做换行。`efontCN_12` 字体下中文字约 12px 宽，英文约 6px。每行约 20 个中文字或 40 个英文字符。

5. **`keys.word` 是 `std::vector<char>`**: 不是 `std::string`。比较和赋值行为不同。

---

## 10. 诊断工具箱

### 串口监控堆状态
```
[AI] Done, 96 chars, heap=192000, largest=180000, min_ever=28000
[VOICE] Buffer allocated: 160000 bytes, heap=32000
[VOICE] Buffer freed, heap=192000
```

- `heap`: 当前空闲堆
- `largest`: 最大连续空闲块（碎片化指标）
- `min_ever`: 运行以来最低水位（接近 0 说明曾经差点 OOM）

### Mac 端调试 UDP 广播
```bash
nc -u -l 19820
```

### 堆碎片化判断
```
如果 heap=80KB 但 largest=8KB → 严重碎片化
如果 largest 每轮下降 → 有泄漏或碎片化源
如果 largest 稳定 → 健康
```

### 关键日志行含义
```
[AI] Connecting to ...       → 开始 AI 请求
[AI] Sent, heap=xxx          → 请求体发送完毕，doc 已释放
[AI] Stream: chunked=1       → 确认使用 chunked 解码
[AI] Done, 96 chars, ...     → AI 响应完成，堆诊断
[VOICE] Buffer allocated     → 160KB 缓冲已分配
[VOICE] Audio sent           → 音频数据上传完毕
[VOICE] Buffer freed         → 160KB 已释放（应在 "Audio sent" 之后立即出现）
[VOICE] Response: {...}      → STT 响应原始 JSON
[VOICE] STT result: xxx      → 最终转写结果
```

---

## 附录: 修复前后对比

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| Flash | 1161 KB | 1153 KB (-8KB) |
| 静态 RAM | 49872 B | 49824 B |
| 聊天时可用堆 | ~33 KB | ~190 KB |
| STT 等待时可用堆 | ~13 KB | ~190 KB |
| 每帧堆分配次数 | ~100+ | 0 |
| 每 SSE token 堆分配 | 4 次 | 1 次 |
| 可持续对话轮数 | 3-4 轮 | 理论无限 |
| 线性搜索 fitLen | O(n) | O(log n) 二分 |
