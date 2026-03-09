# PRD: 桌面端完整天气场景（双模式）

## 背景

桌面宠物当前是透明窗口上的一个 128px sprite，配饰（墨镜/雨伞/雪帽/口罩）已同步，但没有天气背景效果。之前尝试过两个方案：

1. **透明粒子方案**：在透明窗口上画半透明粒子 → macOS 窗口合成器 60fps 重绘透明窗口时白帧闪烁，否决。
2. **全场景跟随方案**：360×200 不透明场景窗口跟鼠标走 → 带背景的大窗口跟鼠标移动视觉突兀，否决。

## 最终方案：双模式

### 核心思路

将"跟随鼠标"和"天气场景"拆成两个独立模式，用户通过 menu bar 菜单切换。

### 两种模式

| | 跟随模式 (Follow) | 场景模式 (Scene) |
|---|---|---|
| **窗口类型** | 透明窗口，原始行为 | Menu bar popover 面板 |
| **窗口位置** | 跟鼠标走 | 固定在 menu bar 下方，不可拖动 |
| **窗口大小** | 168×176 (sprite + padding) | 360×200 |
| **背景** | 透明 | 完整天气场景 |
| **Sprite 移动** | 屏幕上跟鼠标 | 场景内左右走（UDP normX 映射） |
| **Time-travel** | 无 | 有（sprite X 偏移 displayHour） |
| **天气效果** | 仅配饰 | 配饰 + 天空色调 + 粒子 + 天体遮挡 |
| **默认** | ✓ 启动时默认 | 需手动切换 |

### 切换方式

Menu bar status item (🦞) 的菜单：
```
┌─────────────────────────┐
│ ● Follow Mode           │  ← 当前模式打勾
│   Scene Mode            │
│ ─────────────────────── │
│ Quit Pixel Lobster    Q │
└─────────────────────────┘
```

切换时：
- Follow → Scene：隐藏跟随窗口，显示 menu bar popover 场景面板
- Scene → Follow：关闭 popover，显示跟随窗口

## 跟随模式 (Follow Mode)

与改动前完全一致：
- 透明 borderless 窗口，`backgroundColor = .clear`, `isOpaque = false`
- Sprite 跟鼠标走，lerp 平滑移动
- 配饰根据 weatherType 显示
- Sleep Z 浮动动画
- 窗口大小 = sprite 128 + padding (40 right, 48 top) = 168×176

## 场景模式 (Scene Mode)

### 窗口

场景作为 NSPopover 或 NSPanel 从 menu bar status item 弹出，固定在 menu bar 下方。

具体实现方式：使用 NSPanel（borderless, floating），锚定到 status item button 的正下方。点击 status item 时 toggle 显示/隐藏。

### 布局 (360×200, NSView y=0 在底部)

```
y=200 ┌────────────────────────────┐
      │    ☁    ☀         ☁       │  sky (42px above sprite)
y=158 │                           │
      │       ┌──────────┐        │
      │       │  sprite  │  z Z   │  128×128, spriteY=30
      │       │  128×128 │        │
y=30  │       └──────────┘        │  2px gap above ground
y=28  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  ground top highlight
      │      12:34                │  ground area (28px)
y=0   └────────────────────────────┘
      x=0                     x=360
```

常量：`windowW=360, windowH=200, groundH=28, spriteY=30`

Sprite X 位置：
- 有 UDP 同步：`spriteX = normX * (360 - 128)` = 0~232
- 无 UDP 同步：居中 `spriteX = (360 - 128) / 2 = 116`

### 颜色方案（从 ESP32 移植）

```
天空：
  白天    rgb(60, 120, 200)    亮蓝
  黄昏    rgb(180, 80, 60)     橙色
  夜晚    rgb(10, 10, 30)      深蓝黑

地面：
  白天    rgb(80, 140, 60)     草绿
  高光    rgb(100, 170, 70)    亮绿（地面顶部 2px 线）
  夜晚    rgb(50, 100, 40)     暗绿

天体：
  太阳    rgb(255, 220, 60)    亮黄
  月亮    rgb(220, 220, 200)   灰白
  星星    rgb(200, 200, 140)   暖黄
  云      rgb(220, 230, 240)   浅灰

天气色调叠加（blendFactor 0~1）：
  阴天    rgb(100, 100, 110)  factor=0.63
  小雨    rgb(90, 90, 105)    factor=0.55
  雨/雷   rgb(60, 60, 75)     factor=0.71
  雪      rgb(120, 120, 135)  factor=0.59
  雾      rgb(140, 140, 145)  factor=0.67
```

### 天空渲染

`displayHour` = Mac 本地时间 hour + time-travel 偏移

Time-travel 偏移（与 ESP32 一致）：
- `offset = Int((spriteX / (windowW - spriteSize)) * 24) - 12`
- 即 sprite 在最左边 = -12h，中间 = 0，最右边 = +12h

天空基色：
- 6:00~17:00 → 白天蓝
- 17:00~19:00 → 黄昏橙
- 19:00~6:00 → 夜晚深蓝

天气叠加：`weatherType >= 2` 时混合天气色调。

### 天体

**白天**（6~17h）：
- 太阳：实心圆 r=12 + 8 条射线，位于右上角 (300, 185)
- 云：2 组，每组 2 个圆角矩形叠加

**黄昏**（17~19h）：
- 落日：大圆 r=15 + 橙色内圆 r=12，位于地面线附近 (300, groundH+15)

**夜晚**（19~6h）：
- 月牙：实心圆 r=12 + 天空色偏移圆形成月牙，左上角 (45, 185)
- 星星：12 颗固定位置，每 800ms 随机闪烁，每第 3 颗画十字形

**天气遮挡**：`weatherType >= 2` 时不画天体。

### 天气粒子

| 天气 | type | 粒子数 | 速度 (px/frame) | 视觉 |
|------|------|--------|----------------|------|
| 雾 | 3 | 散点网格 | 水平漂移 | 灰色点阵，间隔 6px，缓慢偏移 |
| 小雨 | 4 | 12 | 4 | 蓝灰竖线 `rgb(140,160,200)`, len=8 |
| 雨 | 5 | 20 | 7 | 蓝灰竖线, len=14 |
| 雪 | 6 | 20 | 1.5 | 白点 `rgb(220,220,230)`, drift=random(-1,0,1) |
| 雷暴 | 7 | 20 | 7 | 雨 + 全屏白闪 `rgb(200,200,220)`, 3~5s 间隔, 50ms 持续 |

粒子在天空区域（groundH ~ windowH）运动，触底回顶。

### 地面

- `groundColor` 填充底部 28px
- 顶部 2px `rgb(100,170,70)` 高光线
- 8 个装饰草丛点

### 时钟

- Mac 系统时间 `HH:mm` 格式
- 居中在地面区域
- `NSFont.monospacedSystemFont(ofSize: 11)`
- 颜色 `rgb(180,180,200)`

### Sleep Z 位置（相对 spriteRect.origin）

```
"z" at origin + (106, 68)   — 12pt bold
"Z" at origin + (118, 96)   — 20pt bold
"Z" at origin + (112, 132)  — 34pt bold
```

### 配饰

坐标相对 sprite origin，不受窗口变化影响。与跟随模式共用同一套 drawAccessory() 代码。

### 动画驱动

- tick() 中：`weatherType >= 3` 或 sleep 状态 → `needsDisplay = true`
- 无天气且非 sleep 时，仅在 updateSprite() 时重绘

## 改动范围

| # | 文件 | 改动 |
|---|------|------|
| 1 | `PetView.swift` | 新增 `sceneMode` 属性。draw() 分两条路径：scene mode 画完整场景，follow mode 画原始透明 sprite。场景渲染方法、粒子系统。新增 `spriteNormX` 属性用于场景内定位。 |
| 2 | `AppDelegate.swift` | 新增模式切换逻辑。场景模式：创建 scene panel 锚定 menu bar。跟随模式：恢复原始 pet window 行为。menu bar 菜单加切换项。 |
| 3 | `PetWindow.swift` | 不改动——跟随模式仍使用原始 PetWindow。 |
| 4 | `PetBehavior.swift` | 不改动——跟随模式行为不变，场景模式下 normX 由 AppDelegate 传给 PetView。 |

**不需要改动**：UDPListener.swift、SpriteRenderer.swift、SpriteData.swift、ESP32 固件。

## 阶段拆分

### Phase 1：双模式骨架 + 菜单切换
- Menu bar 菜单加 Follow/Scene 切换项
- Follow mode = 原始行为（恢复之前的代码）
- Scene mode = 360×200 panel 从 menu bar 弹出，蓝天 + 绿地 + sprite 居中
- 切换时正确隐藏/显示窗口
- **验证**：两个模式可切换，follow mode 行为与改动前一致，scene mode 无白帧

### Phase 2：场景日夜 + 天体
- displayHour 计算 + time-travel
- drawSky() 日夜切换
- drawCelestials() 太阳/落日/月牙/星星/云
- **验证**：手动 time-travel（在场景中移动 sprite）天空正确变化

### Phase 3：天气效果
- 粒子系统 + drawWeatherParticles()
- 天气色调叠加
- 雷暴闪电
- 天气时隐藏天体
- **验证**：连 Cardputer 切换天气，效果正确

### Phase 4：时钟 + 润色
- drawClock() 时钟显示
- drawGround() 草丛装饰
- sprite normX 映射（场景内走动）
- **验证**：时钟正确，整体视觉和谐

## 设计决策记录

1. **双模式而非单一模式**：跟鼠标走适合轻量陪伴，场景适合沉浸天气展示。两者互补，用户按需切换。
2. **默认跟随模式**：保持原有体验，新功能作为增强而非替换。
3. **场景固定在 menu bar**：作为 menu bar panel 最自然，位置固定不可拖动，避免"拖着大块东西走"的突兀感。
4. **Time-travel 仅场景模式**：跟随模式下 sprite 位置是屏幕坐标，与天空无关；场景模式下 sprite 在小窗口内走，X 偏移影响 displayHour 与 ESP32 一致。
5. **360×200 窗口尺寸**：与 ESP32 屏幕比例接近，足够展示场景又不过大。
6. **温度后续再加**：MVP 先不传温度，避免改 ESP32 固件。
