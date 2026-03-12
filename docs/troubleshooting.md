# 问题排查手册

## WiFi 相关

### WiFi failed / 连不上

**最常见原因：用了 5GHz WiFi**

ESP32-S3 只支持 2.4GHz。检查 SSID 是否带 `_5G` 后缀，换成不带的版本（通常密码相同）。

查看 Mac 当前连接的 WiFi：
```bash
networksetup -getairportnetwork en0
```

扫描附近 WiFi：
```bash
/System/Library/PrivateFrameworks/Apple80211.framework/Resources/airport -s
```

### 环境变量没生效 / NVS 里是旧值

编译时环境变量会覆盖 NVS。确认：
1. 每次新终端都要重新 `export`
2. 在同一行或用 `&&` 连接 export 和 pio 命令

```bash
export WIFI_SSID="xxx" && export WIFI_PASS="yyy" && pio run -t upload
```

## 烧录相关

### 端口被占用 (Resource temporarily unavailable)

```
Could not open /dev/cu.usbmodem101, the port is busy
```

串口 monitor 正在占用端口。先 `Ctrl+C` 关闭 monitor 再烧录。

### Failed to connect to ESP32-S3: No serial data received

**原因**：
1. 端口不对（不是 Cardputer 的端口）
2. 需要手动进入下载模式

**解决**：
1. 确认端口：拔插 USB 前后对比 `ls /dev/cu.usb*`
2. 进入下载模式：按住 G0 → 按 Reset → 松开 Reset → 松开 G0

### 烧录成功但屏幕不亮

烧录时如果手动进入了下载模式，完成后设备还在下载模式。**按一下 Reset 按钮**重启。

如果看到 `Hard resetting via RTS pin...` 则会自动重启，无需手动操作。

## API 相关

### HTTP -1

TLS 连接失败。可能原因：
- CA 证书不匹配 → 用 `client.setInsecure()` 跳过验证
- DNS 解析失败 → 检查 WiFi 是否真的连上了
- 服务器不可达 → 检查 API URL 是否正确

### HTTP 401

API Key 无效。检查：
- Key 格式是否正确
- Key 是否属于当前 API 端点（不同平台的 key 不通用）
- 环境变量是否正确传入（串口日志里会打印 request body）

### HTTP 403

权限被拒。可能原因：
- API Key 不被当前端点接受
- 使用了第三方代理 key 直连官方 API

### HTTP 404

模型名不对。常见错误：
```
kimi-k2-0711     ← 不存在
moonshot-v1-8k   ← 可能已下线
kimi-k2.5        ← 正确
```

### HTTP 500

服务端错误。可能原因：
- 请求格式不兼容（如用 Anthropic 格式请求 OpenAI 兼容端点）
- 流式模式不被支持

### 回复为空 / 只显示 thinking...

Kimi K2.5 默认开启 thinking mode，`content` 字段可能为空。检查串口日志中的 `[AI] Response`，看 `reasoning_content` 是否有内容。

当前代码已有 fallback：content 为空时自动读取 reasoning_content。

### 请求超时 / 半天没回复

ESP32 上 HTTPS 比较慢：
- TLS 握手：2-5 秒
- API 响应（含 thinking）：5-20 秒
- 总计：10-30 秒属正常

当前超时设置：60 秒。超过 60 秒才会报错。

## 串口调试

### 串口没输出

1. 确认 `Serial.begin(115200)` 在 `setup()` 中
2. 确认 monitor 波特率匹配：`monitor_speed = 115200`
3. 连上 monitor 后按 Reset 按钮重启设备
4. 确认连的是正确的端口

### 看到 `auto detect board:14` 但没有自定义日志

说明 Serial 在 M5 库初始化时可用，但可能 `Serial.begin()` 被 M5 覆盖了。在 `M5Cardputer.begin()` **之后**调用 `Serial.begin(115200)` + `delay(500)` 等待 USB CDC 就绪。

### 有用的日志标签

```
[BOOT]  — 启动流程
[CHAT]  — 聊天模式键盘事件
[AI]    — API 请求全过程（连接、发送、响应、解析）
```

## 桌面宠物相关

### Desktop Pet 显示 "No ESP32 Connected" / "ESP32: Waiting..."

桌面端收不到 Cardputer 的 UDP 广播。按优先级排查：

1. **不在同一个网络** — 最常见原因。Mac 和 Cardputer 必须连接同一个 WiFi（同一个路由器/热点）。检查 Mac WiFi 名称和 Cardputer 串口日志中的 SSID 是否一致。
2. **macOS 本地网络权限未授权** — 系统设置 → 隐私与安全性 → 本地网络，确认 ClawPuter 开关已打开。如果列表中没有，退出 app 重新打开，等待权限弹窗。
3. **macOS 防火墙** — 系统设置 → 网络 → 防火墙，如果开启了，确保允许传入连接。
4. **Cardputer 未联网** — 检查 Cardputer 屏幕是否显示已连接 WiFi。如果卡在 "Connecting..."，检查 WiFi 配置（Fn+R 重新设置）。

### Desktop Pet 编译报 PCH / module cache 错误

```
PCH was compiled with module cache path '...' but the path is currently '...'
```

项目目录改名或移动后，Swift 编译缓存失效。清除 `.build` 目录重新编译：

```bash
rm -rf ~/ClawPuter/desktop/CardputerDesktopPet/.build
cd ~/ClawPuter/desktop/CardputerDesktopPet && ./run.sh
```

## 屏幕相关

### 屏幕闪烁

确认使用了双缓冲（`M5Canvas` + `pushSprite`），不要直接在 `Display` 上绘制。

### 文字显示不全

240×135 屏幕很小，`textSize(1)` 一行最多约 40 个英文字符。长文本需要手动换行或截断。
