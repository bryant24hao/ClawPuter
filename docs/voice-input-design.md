# 语音输入功能设计文档

## 概述

为 Cardputer 的 CHAT 模式添加 Push-to-Talk 语音输入功能。用户长按 `Fn` 键录音，松开后自动发送到 STT（语音转文字）服务，识别结果填入聊天输入框。

## 硬件约束

| 项目 | 详情 |
|------|------|
| 麦克风 | SPM1423HM4H-B (Knowles), PDM 数字 MEMS |
| 数据引脚 | GPIO 46 (PDM data in) |
| 时钟引脚 | GPIO 43 (PDM clock) — **与扬声器共享** |
| 扬声器 | NS4168, I2S, GPIO 41(BCK)/42(data)/43(WS) |
| 关键约束 | **麦克风和扬声器共享 GPIO 43，不能同时工作** |

必须在录音前 `Speaker.end()`，录音后 `Speaker.begin()` 恢复。

## 技术方案

### STT 服务

复用 OpenClaw Gateway 现有的 LAN 基础设施，调用 OpenAI Whisper 兼容的 `/v1/audio/transcriptions` 端点。

- 音频格式：PCM 16-bit signed, 16kHz, mono
- 传输格式：HTTP multipart/form-data，附带 WAV header
- 端点：`http://{OPENCLAW_HOST}:{OPENCLAW_PORT}/v1/audio/transcriptions`
- 认证：同 chat 的 Bearer token

### 录音参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 采样率 | 16,000 Hz | Whisper 原生采样率 |
| 位深 | 16-bit signed | PCM |
| 声道 | mono | |
| 最大录音时长 | 10 秒 | 16000 × 2 × 10 = 320KB |
| 增益 | magnification = 64 | PDM 麦克风增益补偿 |
| 降噪 | noise_filter_level = 64 | IIR 降噪 |

内存分配：使用 `heap_caps_malloc(MALLOC_CAP_8BIT)` 分配录音 buffer，内部 SRAM 约 320KB 可用。

### UX 流程

```
CHAT 模式下:

1. 用户按住 Fn 键
   → 停止扬声器 (Speaker.end)
   → 启动麦克风 (Mic.begin)
   → 屏幕底部显示 "🎤 Recording..." + 计时
   → 持续录音到 buffer

2. 用户松开 Fn 键（或录满 10 秒自动停止）
   → 停止麦克风 (Mic.end)
   → 恢复扬声器 (Speaker.begin)
   → 屏幕显示 "Transcribing..."
   → 将 PCM 数据加 WAV header，HTTP POST 到 STT 端点
   → 收到文本 → 填入 inputBuffer
   → 用户可以编辑后按 Enter 发送，或直接按 Enter 发送

3. 异常处理
   → 录音时间 < 0.3 秒 → 忽略（太短）
   → HTTP 失败 → 显示错误提示 1 秒，恢复输入
   → 空文本返回 → 显示 "No speech detected" 1 秒
```

### 视觉反馈

录音状态下，输入栏改为红色背景 + 麦克风图标 + 录音时长：
```
┌──────────────────────────────┐
│ [TAB] back  [;] up  [/] down │
│                              │
│  (chat messages area)        │
│                              │
│ ▓▓ Recording... 2.3s     ▓▓ │ ← 红色高亮的输入栏
└──────────────────────────────┘
```

## 改动文件

### 新建：`src/voice_input.h`

```cpp
#pragma once
#include <M5Cardputer.h>
#include "utils.h"

class VoiceInput {
public:
    void begin();

    // 按住 Fn 开始录音
    void startRecording();

    // 松开 Fn 停止录音并发起 STT
    // 返回 true 表示 STT 请求已发起
    bool stopRecording();

    // 轮询 STT 结果（非阻塞版本无法做到，用阻塞 HTTP）
    // 实际在 stopRecording 中同步完成
    bool isRecording() const;
    bool isTranscribing() const;

    // 获取转写结果（调用后清空）
    String takeResult();

    // 获取录音时长（秒）
    float getRecordingDuration() const;

    // 绘制录音状态
    void drawRecordingBar(M5Canvas& canvas);

private:
    int16_t* recordBuffer = nullptr;
    size_t bufferSize = 0;      // max samples
    size_t samplesRecorded = 0;
    bool recording = false;
    bool transcribing = false;
    unsigned long recordStartTime = 0;
    String result;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr float MAX_RECORD_SECONDS = 10.0f;
    static constexpr float MIN_RECORD_SECONDS = 0.3f;

    void initMic();
    void deinitMic();
    String sendToSTT(const int16_t* data, size_t sampleCount);
    void writeWavHeader(uint8_t* header, uint32_t dataSize);
};
```

### 新建：`src/voice_input.cpp`

实现：
- `begin()`: 分配录音 buffer（PSRAM 优先，fallback 内部 SRAM）
- `startRecording()`: Speaker.end → Mic.begin → 开始录音
- `stopRecording()`: Mic.end → Speaker.begin → 如果时长够 → 调 STT
- `sendToSTT()`: 构造 multipart/form-data 请求，POST 到 gateway
- `writeWavHeader()`: 44 字节标准 WAV header
- `drawRecordingBar()`: 在录音状态下绘制带计时的红色提示栏

### 修改：`src/chat.h`

```diff
+ // 设置输入框内容（供语音识别结果填入）
+ void setInput(const String& text);
+
+ // 获取当前输入框内容（语音模式下判断是否为空）
+ const String& getInput() const { return inputBuffer; }
```

### 修改：`src/chat.cpp`

```diff
+ void Chat::setInput(const String& text) {
+     inputBuffer = text;
+ }
```

### 修改：`src/main.cpp`

在 CHAT 模式的按键处理中增加 Fn 键检测：

```diff
+ #include "voice_input.h"
+
+ VoiceInput voiceInput;

  // setup() 中 WiFi 连接成功后:
+ voiceInput.begin();

  // CHAT 模式 loop 中:
+ // 检测 Fn 键按下/松开
+ static bool fnWasPressed = false;
+ bool fnPressed = M5Cardputer.Keyboard.isKeyPressed(KEY_FN);  // 需要检查 API
+
+ if (fnPressed && !fnWasPressed && !aiClient.isBusy()) {
+     voiceInput.startRecording();
+ }
+ if (!fnPressed && fnWasPressed && voiceInput.isRecording()) {
+     if (voiceInput.stopRecording()) {
+         String text = voiceInput.takeResult();
+         if (text.length() > 0) {
+             chat.setInput(text);
+         }
+     }
+ }
+ fnWasPressed = fnPressed;
+
+ // 录音状态下，用 voiceInput 替代正常的输入栏绘制
+ if (voiceInput.isRecording()) {
+     voiceInput.drawRecordingBar(canvas);
+ }
```

## WAV Header 格式

```
Offset  Size  Field           Value
0       4     ChunkID         "RIFF"
4       4     ChunkSize       36 + dataSize
8       4     Format          "WAVE"
12      4     Subchunk1ID     "fmt "
16      4     Subchunk1Size   16
20      2     AudioFormat     1 (PCM)
22      2     NumChannels     1
24      4     SampleRate      16000
28      4     ByteRate        32000
32      2     BlockAlign      2
34      2     BitsPerSample   16
36      4     Subchunk2ID     "data"
40      4     Subchunk2Size   dataSize
44      ...   Data            PCM samples
```

## HTTP STT 请求

```
POST /v1/audio/transcriptions HTTP/1.1
Host: {OPENCLAW_HOST}:{OPENCLAW_PORT}
Authorization: Bearer {OPENCLAW_TOKEN}
Content-Type: multipart/form-data; boundary=----AudioBoundary

------AudioBoundary
Content-Disposition: form-data; name="file"; filename="recording.wav"
Content-Type: audio/wav

{WAV binary data}
------AudioBoundary
Content-Disposition: form-data; name="model"

whisper-1
------AudioBoundary--
```

响应：
```json
{"text": "transcribed text here"}
```

## 按键检测

M5Cardputer 的 Keyboard 类通过 `keysState()` 获取按键状态。`keys.fn` 可以检测 Fn 键是否按下。

需要在每帧检测 Fn 状态变化（按下/松开）来实现 Push-to-Talk：
- `keys.fn` 从 false → true: 开始录音
- `keys.fn` 从 true → false: 停止录音 + STT

注意：因为 `keysState()` 只在 `isChange() && isPressed()` 时被调用，我们需要改为每帧都获取键盘状态，或者改用 `M5Cardputer.Keyboard.isKeyPressed(KEY_FN)`。

**方案**：在 CHAT 模式中，每帧调用 `M5Cardputer.update()` 后读取 Fn 键状态。当前的 `keyPressed` 逻辑只在按键变化时触发，不适合持续检测长按。需要增加一个持续检测路径。

## 验证

1. **麦克风测试**: 录音 → Serial 打印 buffer 中的采样值，确认非零
2. **WAV 生成测试**: 将 WAV 数据 POST 到 Mac 上的测试服务器，播放验证
3. **端到端**: Fn 长按说话 → 松开 → 文字出现在输入框 → Enter 发送
4. **边界测试**: 录音 < 0.3 秒忽略、录满 10 秒自动停止、网络失败恢复
5. **Speaker/Mic 切换**: 录音后扬声器恢复正常（AI 回复后的提示音能响）
