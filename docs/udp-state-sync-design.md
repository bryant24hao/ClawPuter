# WiFi UDP 状态同步：ESP32 → Mac 桌面宠物

## Context

桌面宠物当前有独立的状态机，和 Cardputer 上的小龙虾各跑各的。用户希望两边是同一只宠物 —— Cardputer 上在睡觉，Mac 桌面上也在睡觉；Cardputer 上 happy，桌面上也 happy。

方案：Cardputer 固件通过 WiFi UDP 广播当前状态，Mac 桌面端监听并同步。

## 协议设计

### UDP 广播包

- **端口**: 19820（固定）
- **目标**: 255.255.255.255（局域网广播）
- **频率**: 每 200ms 一次（5Hz，够用且不占带宽）
- **格式**: 紧凑 JSON

```json
{"s":0,"f":1,"m":"COMPANION","x":0.50,"y":0.80,"d":0}
```

| 字段 | 含义 | 值 |
|------|------|-----|
| `s` | CompanionState 枚举值 | 0=IDLE, 1=HAPPY, 2=SLEEP, 3=TALK, 4=STRETCH, 5=LOOK |
| `f` | frameIndex | 当前动画帧索引 |
| `m` | appMode | "COMPANION" / "CHAT" / "SETUP" |
| `x` | 宠物归一化 X 坐标 | 0.00（最左）~ 1.00（最右） |
| `y` | 宠物归一化 Y 坐标 | 0.00（最上）~ 1.00（最下） |
| `d` | 朝向 | 0=右, 1=左 |

用 JSON：方便调试（`nc -u -l 19820` 直接能看），ArduinoJson 固件里已有。

### 桌面端行为

- **收到 UDP 包** → 切到"同步模式"，用 ESP32 的状态和帧号渲染
- **3 秒没收到包** → 回退"独立模式"（光标跟随逻辑）
- 同步模式下光标跟随仍生效（位置跟光标，动画状态跟 ESP32）
- **位置同步** → 当 `x`/`y` 值变化时（阈值 > 0.005），桌面端将归一化坐标映射到屏幕坐标并设为目标位置。Y 轴翻转（ESP32 y=0 在上，macOS y=0 在下）

## 改动

### 1. 固件：新增 `state_broadcast.h/cpp`

**`src/state_broadcast.h`** — 新建
```cpp
#pragma once
void stateBroadcastBegin(const char* unicastTarget = nullptr);
void stateBroadcastTick(int state, int frame, const char* mode,
                        float normX = 0.5f, float normY = 0.5f, int direction = 0);
```

**`src/state_broadcast.cpp`** — 新建
- `WiFiUDP` + `ArduinoJson` 组装 JSON
- `Timer broadcastTimer{200}` 控制频率
- `stateBroadcastTick()`: timer 到了才发，非阻塞

**`src/companion.h`** — 加 1 行
- `int getFrameIndex() const { return frameIndex; }`

**`src/main.cpp`** — 3 处小改
1. `#include "state_broadcast.h"`
2. `connectWiFi()` 成功后调 `stateBroadcastBegin()`
3. `loop()` 末尾调 `stateBroadcastTick()`

### 2. 桌面端

**`Sources/UDPListener.swift`** — 新建
- `NWListener` (Network.framework) 监听 UDP 19820
- 收包 → 解析 JSON → 回调
- 后台 DispatchQueue

**`Sources/PetBehavior.swift`** — 改
- 新增 `syncMode` / `lastSyncTime` 属性
- 新增 `applySync(state:frame:)` 方法
- 补充 `.talk` / `.stretch` / `.look` 状态（固件有 6 个状态，桌面端当前只有 4 个）
- `update()` 中 syncMode 下跳过内部状态机，保留位置 lerp

**`Sources/SpriteData.swift`** — 改
- 补 talk 帧（`sprite_talk1`, `sprite_talk2`）—— 当前缺失
- `SpriteFrames` 补 `.talk` / `.stretch` / `.look` 映射（stretch 复用 happy，look 复用 idle，和固件一致）

**`Sources/AppDelegate.swift`** — 改
- 初始化 `UDPListener`，回调中调 `behavior.applySync()`

## 文件清单

| 文件 | 操作 |
|------|------|
| `src/state_broadcast.h` | 新建 |
| `src/state_broadcast.cpp` | 新建 |
| `src/main.cpp` | 改（3 处） |
| `src/companion.h` | 改（1 行 getter） |
| `desktop/.../Sources/UDPListener.swift` | 新建 |
| `desktop/.../Sources/PetBehavior.swift` | 改 |
| `desktop/.../Sources/SpriteData.swift` | 改 |
| `desktop/.../Sources/AppDelegate.swift` | 改 |

## 验证

1. **固件单独验证**：烧录后 Mac 运行 `nc -u -l 19820`，能看到 JSON 包
2. **端到端验证**：同一局域网，Cardputer IDLE → 桌面也 IDLE；按空格 HAPPY → 桌面同步举钳子；30 秒 → 两边同时睡觉
3. **断连回退**：拔 Cardputer → 3 秒后桌面回退独立模式
4. **隐私检查**：提交前扫描 diff 无真实 IP / 端口
