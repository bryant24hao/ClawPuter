# 环境搭建与烧录

## 开发环境

- **PlatformIO CLI**（通过 Homebrew 或 pip 安装）
- macOS，USB-C 连接 Cardputer

验证安装：

```bash
pio --version
```

## 项目依赖

`platformio.ini` 中的依赖会在首次编译时自动下载：

```ini
lib_deps =
    m5stack/M5Cardputer@^1.0.2
    bblanchon/ArduinoJson@^7.0.0
```

## 环境变量配置

WiFi 和 API Key 通过环境变量在编译时注入，避免在 Cardputer 小键盘上手动输入：

```bash
export WIFI_SSID="你的2.4G WiFi名"
export WIFI_PASS="你的WiFi密码"
export CLAUDE_API_KEY="你的API Key"
```

`platformio.ini` 通过 `${sysenv.XXX}` 读取这些变量，编译时通过 `-D` 宏定义传给代码。

**注意**：每次新开终端都要重新 export，或者写入 `~/.zshrc`。

### 凭证优先级

1. 编译时环境变量（始终覆盖 NVS）
2. NVS 中已保存的配置
3. 首次启动的键盘输入 Setup 向导

## 编译

```bash
cd CardputerCompanion
pio run
```

首次编译约 30 秒（下载依赖 + 编译库），后续增量编译约 5 秒。

编译产物：`.pio/build/m5stack-cardputer/firmware.bin`

## 烧录

### 正常烧录

```bash
pio run -t upload
```

### 进入下载模式（首次或烧录失败时）

ESP32-S3 有时需要手动进入下载模式：

1. **按住 G0 按钮**（机身侧面小按钮）
2. **同时按一下 Reset 按钮**，然后松开 Reset
3. **松开 G0**
4. 运行 `pio run -t upload`

成功后会显示：

```
Hard resetting via RTS pin...
========================= [SUCCESS] Took XX.XX seconds =========================
```

如果看到 `Hard resetting via RTS pin`，设备会自动重启，无需手动按 Reset。否则需要手动按 Reset。

### USB 端口

Cardputer 的 USB 端口名可能变化。查看当前端口：

```bash
ls /dev/cu.usb*
```

如果有多个设备，需要在 `platformio.ini` 中指定正确的端口：

```ini
upload_port = /dev/cu.usbmodem101
monitor_port = /dev/cu.usbmodem101
```

**判断哪个是 Cardputer**：拔掉 Cardputer USB 前后各运行一次 `ls /dev/cu.usb*`，消失的那个就是。

## 串口调试

```bash
pio device monitor
```

或指定端口：

```bash
pio device monitor -p /dev/cu.usbmodem101
```

退出：`Ctrl+C`

### 编译烧录 + 自动打开串口

```bash
pio run -t upload && pio device monitor
```

**注意**：串口 monitor 会占用端口，烧录前必须先关闭 monitor（`Ctrl+C`）。

## 编译烧录一条龙命令

```bash
export WIFI_SSID="YOUR_WIFI_SSID" && \
export WIFI_PASS="YOUR_WIFI_PASSWORD" && \
export CLAUDE_API_KEY="YOUR_API_KEY" && \
export OPENCLAW_HOST="192.168.1.100" && \
export OPENCLAW_PORT="18789" && \
export OPENCLAW_TOKEN="YOUR_GATEWAY_TOKEN" && \
pio run -t upload && pio device monitor
```
