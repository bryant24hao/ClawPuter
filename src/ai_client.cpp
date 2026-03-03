#include "ai_client.h"
#include "config.h"
#include "utils.h"
#include <WiFiClient.h>
#include <ArduinoJson.h>

// OpenClaw Gateway on LAN — configure via environment variables
#ifndef OPENCLAW_HOST
#define OPENCLAW_HOST ""
#endif
#ifndef OPENCLAW_PORT
#define OPENCLAW_PORT ""
#endif
#ifndef OPENCLAW_TOKEN
#define OPENCLAW_TOKEN ""
#endif

void AIClient::begin(const String& key) {
    apiKey = key;
    historyCount = 0;
    // Pre-reserve history Strings to avoid per-round realloc fragmentation
    for (int i = 0; i < MAX_HISTORY; i++) {
        history[i].user.reserve(120);
        history[i].assistant.reserve(320);
    }
}

void AIClient::sendMessage(const String& userMessage,
                           TokenCallback onToken,
                           DoneCallback onDone,
                           ErrorCallback onError) {
    if (busy) {
        if (onError) onError("Already processing");
        return;
    }
    busy = true;

    WiFiClient client;
    client.setTimeout(5);  // 5s per read operation

    int port = atoi(OPENCLAW_PORT);
    Serial.printf("[AI] Connecting to %s:%d...\n", OPENCLAW_HOST, port);

    if (!client.connect(OPENCLAW_HOST, port)) {
        Serial.println("[AI] Connection failed");
        busy = false;
        if (onError) onError("Connection failed");
        return;
    }

    // Build JSON doc, measure length, serialize directly to socket.
    // No intermediate String body — saves ~800 bytes of heap.
    {
        JsonDocument doc;
        buildRequestDoc(userMessage, doc);
        size_t bodyLen = measureJson(doc);
        client.printf("POST /v1/chat/completions HTTP/1.1\r\n"
                      "Host: %s:%s\r\n"
                      "Authorization: Bearer %s\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %d\r\n"
                      "Connection: close\r\n\r\n",
                      OPENCLAW_HOST, OPENCLAW_PORT, OPENCLAW_TOKEN, bodyLen);
        serializeJson(doc, client);
    } // doc freed here

    Serial.printf("[AI] Sent, heap=%u\n", ESP.getFreeHeap());

    // Read HTTP response headers
    unsigned long deadline = millis() + 30000;
    bool httpOk = false;
    bool chunked = false;
    while (client.connected() && millis() < deadline) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();
            if (line.startsWith("HTTP/") && line.indexOf("200") > 0) httpOk = true;
            if (line.startsWith("Transfer-Encoding") && line.indexOf("chunked") > 0) chunked = true;
            if (line.length() == 0) break;
        } else {
            delay(10);
        }
    }

    if (!httpOk) {
        client.stop();
        busy = false;
        if (onError) onError("HTTP error");
        return;
    }

    // Parse SSE stream using zero-heap-allocation approach:
    // - Stack char arrays for chunk/line parsing (no String in hot loop)
    // - Direct string search for "content" field (no JsonDocument allocation)
    String fullResponse;
    fullResponse.reserve(320);

    Serial.printf("[AI] Stream: chunked=%d, heap=%u\n", chunked, ESP.getFreeHeap());

    // Extract "content":"..." from SSE JSON without JsonDocument.
    // Returns length written to outBuf, 0 if no content found.
    auto extractContent = [](const char* json, char* outBuf, int outSize) -> int {
        const char* key = strstr(json, "\"content\":\"");
        if (!key) return 0;
        const char* p = key + 11; // skip "content":"
        int i = 0;
        while (*p && *p != '"' && i < outSize - 1) {
            if (*p == '\\' && p[1]) {
                // Handle JSON escape sequences
                switch (p[1]) {
                    case '"':  outBuf[i++] = '"';  p += 2; break;
                    case '\\': outBuf[i++] = '\\'; p += 2; break;
                    case 'n':  outBuf[i++] = '\n'; p += 2; break;
                    case '/':  outBuf[i++] = '/';  p += 2; break;
                    default:   p += 2; break; // skip unknown escapes
                }
            } else {
                outBuf[i++] = *p++;
            }
        }
        outBuf[i] = '\0';
        return i;
    };

    // Stack-allocated buffers — no heap allocation in streaming loop
    char sizeBuf[16];
    int sizeLen = 0;
    char lineBuf[512];
    int lineLen = 0;
    char contentBuf[128];
    char filteredBuf[128];

    if (chunked) {
        // Chunked transfer decoding via byte-level state machine.
        // lineBuf carries across chunk boundaries so split data: lines reassemble.
        enum { CS_SIZE, CS_DATA, CS_TRAILER } cs = CS_SIZE;
        long chunkRemain = 0;
        bool streamDone = false;

        while (!streamDone && (client.connected() || client.available()) && millis() < deadline) {
            if (!client.available()) { delay(1); continue; }
            char c = client.read();

            switch (cs) {
            case CS_SIZE:
                if (c == '\n') {
                    sizeBuf[sizeLen] = '\0';
                    if (sizeLen > 0 && sizeBuf[sizeLen-1] == '\r') sizeBuf[--sizeLen] = '\0';
                    chunkRemain = strtol(sizeBuf, NULL, 16);
                    sizeLen = 0;
                    if (chunkRemain <= 0) { streamDone = true; break; }
                    cs = CS_DATA;
                } else if (sizeLen < (int)sizeof(sizeBuf) - 1) {
                    sizeBuf[sizeLen++] = c;
                }
                break;

            case CS_DATA:
                chunkRemain--;
                if (c == '\n') {
                    lineBuf[lineLen] = '\0';
                    if (lineLen > 0 && lineBuf[lineLen-1] == '\r') lineBuf[--lineLen] = '\0';

                    // Process SSE line
                    if (lineLen > 6 && memcmp(lineBuf, "data: ", 6) == 0) {
                        const char* data = lineBuf + 6;
                        if (strcmp(data, "[DONE]") == 0) {
                            streamDone = true;
                        } else {
                            int clen = extractContent(data, contentBuf, sizeof(contentBuf));
                            if (clen > 0) {
                                int flen = filterForDisplayBuf(contentBuf, filteredBuf, sizeof(filteredBuf));
                                if (flen > 0) {
                                    fullResponse += filteredBuf;
                                    if (onToken) onToken(String(filteredBuf));
                                }
                            }
                        }
                    }
                    lineLen = 0;
                } else if (lineLen < (int)sizeof(lineBuf) - 1) {
                    lineBuf[lineLen++] = c;
                }
                if (chunkRemain <= 0) cs = CS_TRAILER;
                break;

            case CS_TRAILER:
                if (c == '\n') cs = CS_SIZE;
                break;
            }

            if (fullResponse.length() > 300) break;
        }
    } else {
        // Non-chunked: read byte-by-byte into lineBuf (same zero-alloc approach)
        while ((client.connected() || client.available()) && millis() < deadline) {
            if (!client.available()) { delay(5); continue; }
            char c = client.read();
            if (c == '\n') {
                lineBuf[lineLen] = '\0';
                if (lineLen > 0 && lineBuf[lineLen-1] == '\r') lineBuf[--lineLen] = '\0';
                if (lineLen > 6 && memcmp(lineBuf, "data: ", 6) == 0) {
                    const char* data = lineBuf + 6;
                    if (strcmp(data, "[DONE]") == 0) break;
                    int clen = extractContent(data, contentBuf, sizeof(contentBuf));
                    if (clen > 0) {
                        int flen = filterForDisplayBuf(contentBuf, filteredBuf, sizeof(filteredBuf));
                        if (flen > 0) {
                            fullResponse += filteredBuf;
                            if (onToken) onToken(String(filteredBuf));
                        }
                    }
                }
                lineLen = 0;
            } else if (lineLen < (int)sizeof(lineBuf) - 1) {
                lineBuf[lineLen++] = c;
            }
            if (fullResponse.length() > 300) break;
        }
    }

    client.stop();
    Serial.printf("[AI] Done, %d chars, heap=%u, largest=%u, min_ever=%u\n",
        fullResponse.length(), ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        ESP.getMinFreeHeap());

    if (fullResponse.length() > 0) {
        addToHistory(userMessage, fullResponse);
    }

    busy = false;
    if (onDone) onDone();
}

void AIClient::update() {
}

void AIClient::addToHistory(const String& user, const String& assistant) {
    if (historyCount < MAX_HISTORY) {
        history[historyCount].user = user;
        history[historyCount].assistant = assistant;
        historyCount++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            history[i] = history[i + 1];
        }
        history[MAX_HISTORY - 1].user = user;
        history[MAX_HISTORY - 1].assistant = assistant;
    }
}

void AIClient::buildRequestDoc(const String& userMessage, JsonDocument& doc) {
    doc["model"] = "openclaw";
    doc["user"] = "cardputer";
    doc["stream"] = true;

    JsonArray messages = doc["messages"].to<JsonArray>();

    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are a tiny pixel companion living inside a Cardputer device. "
                        "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
                        "Be friendly and playful. Use simple words. "
                        "NEVER use emoji, markdown formatting, or special Unicode characters. Plain text only.";

    for (int i = 0; i < historyCount; i++) {
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = history[i].user;

        JsonObject assistMsg = messages.add<JsonObject>();
        assistMsg["role"] = "assistant";
        assistMsg["content"] = history[i].assistant;
    }

    JsonObject currentMsg = messages.add<JsonObject>();
    currentMsg["role"] = "user";
    currentMsg["content"] = userMessage;
}
