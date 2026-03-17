#include "ai_client.h"
#include "config.h"
#include "utils.h"
#include <WiFiClient.h>
#include <ArduinoJson.h>

void AIClient::begin(const String& key, const String& host,
                     const String& port, const String& token) {
    apiKey = key;
    gwHost = host;
    gwPort = port;
    gwToken = token;
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

    int port = atoi(gwPort.c_str());
    if (port <= 0 || port > 65535) {
        Serial.println("[AI] Invalid port");
        busy = false;
        if (onError) onError("Invalid port");
        return;
    }
    Serial.printf("[AI] Connecting to %s:%d...\n", gwHost.c_str(), port);

    if (!client.connect(gwHost.c_str(), port)) {
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
                      "Content-Length: %u\r\n"
                      "Connection: close\r\n\r\n",
                      gwHost.c_str(), gwPort.c_str(), gwToken.c_str(), bodyLen);
        serializeJson(doc, client);
    } // doc freed here

    Serial.printf("[AI] Sent, heap=%u\n", ESP.getFreeHeap());

    // Read HTTP response headers — zero heap allocation (stack buffer only)
    unsigned long deadline = millis() + 30000;
    bool httpOk = false;
    bool chunked = false;
    char hdrBuf[256];
    int hdrLen = 0;
    while (client.connected() && millis() < deadline) {
        if (!client.available()) { delay(10); continue; }
        char c = client.read();
        if (c == '\n') {
            // Strip trailing \r
            if (hdrLen > 0 && hdrBuf[hdrLen - 1] == '\r') hdrLen--;
            hdrBuf[hdrLen] = '\0';
            if (hdrLen == 0) break; // empty line = end of headers
            if (strstr(hdrBuf, "HTTP/") == hdrBuf && strstr(hdrBuf, "200")) httpOk = true;
            if (strstr(hdrBuf, "chunked")) chunked = true;
            hdrLen = 0;
        } else if (hdrLen < (int)sizeof(hdrBuf) - 1) {
            hdrBuf[hdrLen++] = c;
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

    // Rolling idle timeout (overflow-safe) + thinking model support
    unsigned long lastActivity = millis();
    unsigned long startTime = millis();
    thinkingDetected = false;
    bool firstContentSeen = false;
    bool gatewayFallbackDetected = false;

    // Process a single SSE content extraction — shared by chunked & non-chunked paths.
    // Returns true if stream should stop (e.g. [DONE]).
    auto processSSELine = [&](const char* data) -> bool {
        lastActivity = millis();  // data received, reset idle timer

        if (strcmp(data, "[DONE]") == 0) return true;

        int clen = extractContent(data, contentBuf, sizeof(contentBuf));
        if (clen > 0) {
            // Filter thinking content: chunks starting with "think\n"
            if (clen >= 6 && memcmp(contentBuf, "think\n", 6) == 0) {
                thinkingDetected = true;
                Serial.printf("[AI] Thinking detected, filtering %d chars\n", clen);
                return false;  // skip, but keep waiting
            }

            // Detect gateway fallback injection on first real content
            if (!firstContentSeen) {
                firstContentSeen = true;
                if (strstr(contentBuf, "Continue where you left off") ||
                    strstr(contentBuf, "previous model attempt")) {
                    gatewayFallbackDetected = true;
                    Serial.println("[AI] Gateway fallback detected, will discard");
                    return false;
                }
            }

            // Normal content — display it
            if (!gatewayFallbackDetected) {
                int flen = filterForDisplayBuf(contentBuf, filteredBuf, sizeof(filteredBuf));
                if (flen > 0) {
                    fullResponse += filteredBuf;
                    if (onToken) onToken(filteredBuf);
                }
            }
        }
        return false;
    };

    // Timeout check helper (overflow-safe subtraction)
    auto isTimedOut = [&]() -> bool {
        return (millis() - lastActivity > 30000) ||   // 30s idle
               (millis() - startTime > 120000);        // 120s safety cap
    };

    if (chunked) {
        // Chunked transfer decoding via byte-level state machine.
        // lineBuf carries across chunk boundaries so split data: lines reassemble.
        enum { CS_SIZE, CS_DATA, CS_TRAILER } cs = CS_SIZE;
        long chunkRemain = 0;
        bool streamDone = false;

        while (!streamDone && (client.connected() || client.available()) && !isTimedOut()) {
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

                    if (lineLen > 6 && memcmp(lineBuf, "data: ", 6) == 0) {
                        if (processSSELine(lineBuf + 6)) {
                            streamDone = true;
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

            if (fullResponse.length() > (unsigned)(pixelArtMode ? 400 : 300)) break;
        }
    } else {
        // Non-chunked: read byte-by-byte into lineBuf (same zero-alloc approach)
        while ((client.connected() || client.available()) && !isTimedOut()) {
            if (!client.available()) { delay(5); continue; }
            char c = client.read();
            if (c == '\n') {
                lineBuf[lineLen] = '\0';
                if (lineLen > 0 && lineBuf[lineLen-1] == '\r') lineBuf[--lineLen] = '\0';
                if (lineLen > 6 && memcmp(lineBuf, "data: ", 6) == 0) {
                    if (processSSELine(lineBuf + 6)) break;
                }
                lineLen = 0;
            } else if (lineLen < (int)sizeof(lineBuf) - 1) {
                lineBuf[lineLen++] = c;
            }
            if (fullResponse.length() > (unsigned)(pixelArtMode ? 400 : 300)) break;
        }
    }

    client.stop();
    Serial.printf("[AI] Done, %d chars, thinking=%d, fallback=%d, heap=%u, largest=%u, min_ever=%u\n",
        fullResponse.length(), thinkingDetected, gatewayFallbackDetected,
        ESP.getFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        ESP.getMinFreeHeap());

    // Gateway fallback detected: discard response, report as error
    if (gatewayFallbackDetected) {
        fullResponse = "";
        lastResponse = "";
        busy = false;
        pixelArtMode = false;
        if (onError) onError("Model timeout, try again");
        return;
    }

    if (fullResponse.length() > 0) {
        // Sanitize pixel art before adding to history:
        // Replace raw [PIXELART:...]...[/PIXELART] with a summary so the model
        // remembers it drew something, but won't be primed to output pixel format.
        if (fullResponse.indexOf("[PIXELART:") >= 0) {
            addToHistory(userMessage, "(drew a pixel art)");
        } else if (!pixelArtMode) {
            addToHistory(userMessage, fullResponse);
        }
        lastResponse = fullResponse;
    } else {
        lastResponse = "";
    }

    busy = false;
    pixelArtMode = false;  // Reset after request
    if (onDone) onDone();
}

void AIClient::update() {
}

void AIClient::setPixelArtMode(bool enabled, int size) {
    pixelArtMode = enabled;
    pixelArtSize = size;
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

    if (pixelArtMode) {
        // Pixel art specialized prompt — no history needed
        char prompt[512];
        snprintf(prompt, sizeof(prompt),
            "You are a pixel art generator. Output ONLY a %dx%d pixel art grid. "
            "Palette: 0=transparent 1=black 2=white 3=red 4=darkred 5=orange "
            "6=yellow 7=green 8=darkgreen 9=blue a=lightblue b=purple "
            "c=pink d=brown e=gray f=lightgray. "
            "Format: [PIXELART:%d] then %d rows of %d hex chars, then [/PIXELART]. "
            "No other text. No spaces in rows.",
            pixelArtSize, pixelArtSize, pixelArtSize, pixelArtSize, pixelArtSize);
        sysMsg["content"] = prompt;
    } else {
        sysMsg["content"] = "You are a tiny pixel companion living inside a Cardputer device. "
                            "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
                            "Be friendly and playful. Use simple words. "
                            "NEVER use emoji, markdown formatting, or special Unicode characters. Plain text only.";
    }

    // Only include history for non-pixel-art requests
    if (!pixelArtMode) {
        for (int i = 0; i < historyCount; i++) {
            JsonObject userMsg = messages.add<JsonObject>();
            userMsg["role"] = "user";
            userMsg["content"] = history[i].user;

            JsonObject assistMsg = messages.add<JsonObject>();
            assistMsg["role"] = "assistant";
            assistMsg["content"] = history[i].assistant;
        }
    }

    JsonObject currentMsg = messages.add<JsonObject>();
    currentMsg["role"] = "user";

    if (pixelArtMode) {
        // Strip the /draw or /draw16 prefix, send just the subject
        const char* subject = userMessage.c_str();
        if (strncmp(subject, "/draw16 ", 8) == 0) subject += 8;
        else if (strncmp(subject, "/draw16", 7) == 0) subject += 7;
        else if (strncmp(subject, "/draw ", 6) == 0) subject += 6;
        else if (strncmp(subject, "/draw", 5) == 0) subject += 5;
        // Skip leading whitespace
        while (*subject == ' ') subject++;
        if (*subject == '\0') subject = "a cute lobster";  // default subject
        currentMsg["content"] = subject;
    } else {
        // Prepend hydration context as a tag in user message (not system prompt).
        // This nudges the model to reflect moisture state without overriding its persona.
        const char* prefix = nullptr;
        if (companionCtx.moisture == 0)      prefix = "[you are extremely thirsty, beg for water] ";
        else if (companionCtx.moisture == 1) prefix = "[you are very thirsty, mention needing water] ";
        else if (companionCtx.moisture == 2) prefix = "[you are a bit thirsty] ";
        else if (companionCtx.moisture == 3) prefix = "[you feel good] ";

        if (prefix) {
            char combined[256];
            snprintf(combined, sizeof(combined), "%s%s", prefix, userMessage.c_str());
            currentMsg["content"] = combined;
        } else {
            currentMsg["content"] = userMessage;
        }
    }
}
