# API 接入踩坑记录

## 演变历程

```
Anthropic 直连 → skyapi 代理 → Kimi K2.5 (moonshot.cn)
```

每次切换都踩了不同的坑，记录如下。

## 1. Anthropic 直连

**配置**：
- Endpoint: `https://api.anthropic.com/v1/messages`
- Auth: `x-api-key` header
- Format: Anthropic Messages API

**问题**：HTTP -1（TLS 连接失败）

**原因**：硬编码的 ISRG Root X1 证书可能与 api.anthropic.com 的证书链不匹配。

**解决**：`client.setInsecure()` 跳过证书验证。对于 hobby 项目可接受，生产环境需要正确的 CA 证书。

## 2. skyapi.org 代理

**配置**：
- Endpoint: `https://api.skyapi.org/v1/messages`
- Auth: 同 Anthropic 格式
- Key: 第三方代理 key（非 `sk-ant-` 格式）

**问题**：
1. 域名拼写坑：`skypeapi.org`（错）→ `skyapi.org`（对）
2. HTTP 403：API Key 被拒（代理 key 格式不同）
3. HTTP 500：服务端错误，可能不支持流式或模型名不对

**教训**：第三方代理的兼容性不确定，排查成本高。

## 3. Kimi K2.5 (最终方案)

**配置**：
- Endpoint: `https://api.moonshot.cn/v1/chat/completions`
- Auth: `Authorization: Bearer sk-xxx`
- Format: OpenAI 兼容格式
- Model: `kimi-k2.5`

**踩坑过程**：

### 坑 1：域名

```
api.moonshot.ai  ← 错（401）
api.moonshot.cn  ← 对
```

Moonshot 的国内域名是 `.cn`，不是 `.ai`。

### 坑 2：模型名

```
kimi-k2-0711    ← 404
moonshot-v1-8k  ← 404（旧模型可能已下线）
kimi-k2.5       ← 正确
```

### 坑 3：Thinking Mode

Kimi K2.5 **默认开启 thinking mode**。响应结构：

```json
{
  "choices": [{
    "message": {
      "content": "",                    // 可能为空！
      "reasoning_content": "思考过程..."  // 实际内容在这里
    }
  }]
}
```

当 `max_tokens` 太小（如 80），思考过程用完所有 token，`content` 为空字符串。

**解决方案**：
1. `max_tokens` 提高到 1024
2. 解析时 fallback：`content` 为空则读 `reasoning_content`
3. 超过 200 字符截断（屏幕小）

### 坑 4：响应慢

ESP32 上 HTTPS 请求本身就慢（TLS 握手），加上 Kimi thinking mode，一次请求可能要 10-20 秒。

**解决**：
- `client.setTimeout(60)` — WiFiClientSecure 60 秒超时
- `http.setTimeout(60000)` — HTTPClient 60 秒超时

## API 格式对比

### Anthropic Messages API

```json
POST /v1/messages
Headers: x-api-key, anthropic-version
{
  "model": "claude-3-5-haiku-20241022",
  "system": "...",
  "messages": [{"role": "user", "content": "..."}]
}
Response: { "content": [{"text": "..."}] }
```

### OpenAI 兼容格式（Kimi/通用）

```json
POST /v1/chat/completions
Headers: Authorization: Bearer xxx
{
  "model": "kimi-k2.5",
  "messages": [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "..."}
  ]
}
Response: { "choices": [{"message": {"content": "..."}}] }
```

**关键区别**：
- Auth header 不同（`x-api-key` vs `Authorization: Bearer`）
- system message 位置不同（顶层字段 vs messages 数组第一条）
- 响应结构不同（`content[0].text` vs `choices[0].message.content`）

## 流式 vs 非流式

最初设计了 SSE 流式解析（Anthropic 格式），但在代理和 Kimi 上不稳定。当前使用**非流式**请求：简单可靠，代价是等待时间稍长，回复一次性显示。

未来优化：可以重新启用流式（`stream: true`），逐 token 显示回复，体验更好。
