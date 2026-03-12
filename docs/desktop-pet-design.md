# macOS 桌面伴侣 App — 设计文档

## 目标

把 Cardputer 上的像素小龙虾同步到 Mac 桌面上，作为一个跟随光标、不遮挡用户操作的桌面宠物（类似 Shimeji）。独立的 macOS 原生 App，和 Cardputer 固件共享同一套像素素材。

## 技术方案

**Swift + AppKit**（非 SwiftUI），原因：NSWindow 的透明/置顶/点击穿透控制更直接。

### 核心机制

1. **透明置顶窗口** — `NSWindow` 设置 `isOpaque=false`, `level=.floating`, `ignoresMouseEvents=true`, `backgroundColor=.clear`
2. **光标跟随** — `NSEvent.addGlobalMonitorForEvents(matching: .mouseMoved)` 获取全局鼠标位置，窗口平滑跟随（带缓动，不严格贴合）
3. **智能定位** — 停在光标附近（偏移 80-120px），优先停在屏幕边缘或窗口边框上，不遮挡内容
4. **像素渲染** — RGB565 sprite 数据转 RGBA → `CGImage` → `NSImageView`，保持像素锐利（`magnificationFilter = .nearest`）
5. **动画状态机** — 复用 Cardputer 的状态：IDLE / HAPPY / SLEEP / WALK，用 Timer 驱动帧切换

### 文件结构

```
desktop/CardputerDesktopPet/
├── Package.swift                 # SPM 项目配置
└── Sources/
    ├── main.swift                # App 入口
    ├── AppDelegate.swift         # NSWindow 配置、全局事件监听
    ├── PetWindow.swift           # 透明窗口 + 点击穿透
    ├── PetView.swift             # 像素渲染 + 动画
    ├── SpriteRenderer.swift      # RGB565→CGImage 转换
    ├── SpriteData.swift          # 从 sprites.h 移植的像素数据
    └── PetBehavior.swift         # 状态机 + 光标跟随逻辑
```

放在 `ClawPuter/desktop/` 目录下，与固件同仓库。

## 实现步骤

### Step 1: 项目骨架 + 透明窗口

- 创建 SPM executable target（`macOS .v13`）
- `AppDelegate` 配置无标题栏透明窗口
- 关键 NSWindow 配置：
  ```swift
  window.isOpaque = false
  window.backgroundColor = .clear
  window.level = .floating
  window.ignoresMouseEvents = true
  window.collectionBehavior = [.canJoinAllSpaces, .stationary]
  window.styleMask = [.borderless]
  window.hasShadow = false
  ```
- 验证窗口置顶 + 点击穿透

### Step 2: 像素渲染

- `SpriteData.swift` — 把 `sprites.h` 的 RGB565 数据转为 Swift `[UInt16]` 数组
  - 8 帧：idle1-3, happy1-2, sleep1, talk1-2
  - 颜色定义保持一致

- `SpriteRenderer.swift` — RGB565 → RGBA8888 → CGImage
  ```swift
  func rgb565toRGBA(_ color: UInt16) -> (UInt8, UInt8, UInt8, UInt8) {
      let r = UInt8((color >> 11) & 0x1F)
      let g = UInt8((color >> 5) & 0x3F)
      let b = UInt8(color & 0x1F)
      let alpha: UInt8 = (color == 0xF81F) ? 0 : 255  // magenta = transparent
      return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), alpha)
  }
  ```
  - 16×16 → 128×128 放大显示，nearest neighbor 插值保持像素感

- `PetView.swift` — 自定义 `NSView`，`draw()` 方法渲染 CGImage
  - `CGContext.interpolationQuality = .none` 确保像素锐利

### Step 3: 动画系统

- `PetBehavior.swift` — 状态机
  | 状态 | 帧 | 触发条件 |
  |------|------|---------|
  | IDLE | idle1→idle2→idle3→idle2 循环 | 默认/光标停止 3 秒 |
  | WALK | idle1↔idle2 快速交替 | 光标移动中 |
  | HAPPY | happy1→happy2 | 光标点击（可选）|
  | SLEEP | sleep1 | 无操作 30 秒 |

- Timer 驱动帧切换，500ms/帧（WALK 状态加速到 200ms）
- 任何鼠标移动打断 SLEEP

### Step 4: 光标跟随

- `NSEvent.addGlobalMonitorForEvents(matching: [.mouseMoved, .leftMouseDragged])` 监听全局鼠标
- **平滑跟随**：lerp 缓动
  ```swift
  // 每帧（60fps Timer）
  currentX += (targetX - currentX) * 0.08
  currentY += (targetY - currentY) * 0.08
  ```
- **目标位置**：光标右下方 80px 偏移
- 光标静止 → IDLE；快速移动 → WALK
- 速度阈值：移动 > 5px/帧 视为"快速移动"

### Step 5: 智能避让

- 不超出屏幕可见区域（`NSScreen.main?.visibleFrame`）
- 光标在屏幕右半边 → 角色跟在左侧
- 光标在屏幕左半边 → 角色跟在右侧
- 角色"重力"：目标 Y 坐标偏向屏幕底部

## v1 范围

### 做
- ✅ 透明置顶窗口 + 点击穿透
- ✅ 像素小龙虾渲染（128×128 放大）
- ✅ 4 种状态动画
- ✅ 光标平滑跟随
- ✅ 屏幕边界避让
- ✅ 菜单栏图标（可退出 App）

### 不做
- ❌ Accessibility API 获取窗口边界（需要额外权限）
- ❌ 右键菜单互动
- ❌ 拖拽（窗口设为点击穿透）

## v2 新增：Cardputer 位置同步

### Step 6: 硬件位置同步

- `UDPListener.swift` 解析 UDP 包中的 `x`/`y`/`d` 字段
- `PetBehavior.applySync()` 扩展：接收归一化坐标和朝向
- **位置去重**：只有当硬件坐标变化（阈值 > 0.005）时才覆盖桌面端目标位置，避免持续广播抢占鼠标跟随
- **坐标映射**：`targetX = screenMinX + normX * (screenWidth - petSize)`，Y 轴翻转（`1.0 - normY`）
- **朝向同步**：`d=0` 朝右，`d=1` 朝左
- 鼠标跟随和硬件位置共存：鼠标始终可以移动宠物位置，硬件位置变化时覆盖目标

## v3 新增：双模式 + 天气场景

详细设计文档见 [`desktop-weather-scene-prd.md`](desktop-weather-scene-prd.md)。

### 核心变化

- **双模式架构**：Follow Mode（原始透明跟随）和 Scene Mode（完整天气场景面板）
- **Scene Mode**：360×200 NSPanel 锚定在 menu bar 🦞 图标下方，渲染天空/天体/天气粒子/地面/时钟+温度
- **Time-travel**：仅 Scene Mode 生效，sprite X 位置偏移 displayHour ±12h
- **温度同步**：ESP32 广播包新增 `"t"` 字段，Scene Mode 地面区域显示温度
- **切换方式**：menu bar 🦞 下拉菜单选择 Follow Mode / Scene Mode

### 新增文件结构

```
Sources/
├── PetView.swift          # 新增 sceneMode 分支：drawFollow() / drawScene()
│                          # 场景渲染：sky, celestials, particles, ground, clock
├── AppDelegate.swift      # 双窗口管理：petWindow(follow) + scenePanel(scene)
│                          # menu bar 模式切换
└── UDPListener.swift      # 新增 temperature 解析
```

## 参考项目

- [Shijima-Qt](https://github.com/pixelomer/Shijima-Qt) — Qt 跨平台 Shimeji，macOS Accessibility API 参考
- [WindowPet](https://github.com/SeakMengs/WindowPet) — Tauri 桌面宠物，点击穿透实现参考
- [Desktop Goose](https://samperson.itch.io/desktop-goose) — 桌面鹅，行为逻辑参考

## 验证方式

```bash
cd desktop/CardputerDesktopPet && swift build && swift run
```

1. 桌面出现 128×128 像素小龙虾
2. 移动鼠标，角色平滑跟随（有缓动感）
3. 停止移动 3 秒 → IDLE 眨眼动画
4. 停止移动 30 秒 → SLEEP 状态
5. 角色不拦截点击，鼠标事件穿透到下方窗口
6. 菜单栏有小龙虾图标，可退出
