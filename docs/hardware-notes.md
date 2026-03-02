# 硬件要点

## M5Stack Cardputer 规格

| 项 | 参数 |
|---|---|
| MCU | ESP32-S3 (Xtensa dual-core, 240MHz) |
| RAM | 320KB SRAM + 8MB PSRAM |
| Flash | 8MB |
| 屏幕 | 1.14" IPS, 240×135, ST7789V2 |
| 键盘 | 56 键矩阵键盘 |
| USB | USB-C (CDC on boot) |
| WiFi | 2.4GHz only (802.11 b/g/n) |
| 电池 | 120mAh LiPo |

## 关键限制

### WiFi 仅支持 2.4GHz

ESP32-S3 **不支持 5GHz WiFi**。如果路由器 SSID 带 `_5G` 后缀，必须用对应的 2.4G 版本（通常去掉 `_5G`）。

这是我们开发中遇到的第一个坑——连 `CU_pCjY_5G` 始终失败，换成 `CU_pCjY` 后立即连上。

### 屏幕尺寸

240×135 像素非常小：
- 默认字体 `textSize(1)` 约 6×8 像素，一行约 40 个英文字符
- `textSize(2)` 约 12×16 像素，适合时钟显示
- 像素角色用 16×16 原始尺寸，3 倍放大后 48×48，占屏幕约 1/3 宽度

### 内存

- RAM 15.4%（50KB / 320KB）：JSON 解析和字符串操作是主要消耗
- Flash 31.2%（1MB / 3.3MB）：大头是 M5 库和 TLS 库
- 聊天历史限制 20 条消息，API 上下文限制 6 轮对话

## 键盘 API

```cpp
M5Cardputer.update();
if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto keys = M5Cardputer.Keyboard.keysState();

    keys.tab     // TAB 键
    keys.enter   // Enter 键
    keys.del     // Backspace 键
    keys.fn      // Fn 键（组合用）
    keys.word    // std::vector<char>，本次按下的字符
}
```

**注意**：必须先 `M5Cardputer.update()` 再读键盘状态。

## 屏幕渲染（双缓冲）

```cpp
M5Canvas canvas(&M5Cardputer.Display);
canvas.createSprite(240, 135);

// 在 canvas 上绘制（不会闪烁）
canvas.fillScreen(BLACK);
canvas.drawString("Hello", 10, 10);

// 一次性推送到屏幕
canvas.pushSprite(0, 0);
```

横屏模式：`M5Cardputer.Display.setRotation(1)` → width=240, height=135

## USB 端口识别

macOS 上 Cardputer 通常出现为 `/dev/cu.usbmodemXXX`。如果有多个 USB 设备：

```bash
# 列出所有 USB 串口
ls /dev/cu.usb*

# 拔掉 Cardputer 前后对比，消失的就是它
```

ESP32-S3 使用 USB CDC（不是 CH340/CP2102），所以不需要额外驱动。
