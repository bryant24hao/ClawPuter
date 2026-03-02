#include "ai_client.h"
#include "config.h"
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// OpenClaw Gateway on LAN — configure via environment variables
#ifndef OPENCLAW_HOST
#define OPENCLAW_HOST "192.168.1.100"  // default, override with -DOPENCLAW_HOST
#endif
#ifndef OPENCLAW_PORT
#define OPENCLAW_PORT "18789"
#endif
#ifndef OPENCLAW_TOKEN
#define OPENCLAW_TOKEN ""
#endif

void AIClient::begin(const String& key) {
    apiKey = key;
    historyCount = 0;
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

    Serial.println("[AI] Starting request...");
    Serial.println("[AI] Message: " + userMessage);

    WiFiClient client;
    client.setTimeout(60);

    HTTPClient http;
    String url = String("http://") + OPENCLAW_HOST + ":" + OPENCLAW_PORT + "/v1/chat/completions";
    Serial.println("[AI] Connecting to OpenClaw Gateway...");
    if (!http.begin(client, url)) {
        Serial.println("[AI] ERROR: http.begin() failed");
        busy = false;
        if (onError) onError("Connection failed");
        return;
    }

    http.setTimeout(60000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + OPENCLAW_TOKEN);

    String body = buildRequestBody(userMessage);
    Serial.println("[AI] Request body: " + body);
    Serial.println("[AI] Sending POST...");

    unsigned long t0 = millis();
    int httpCode = http.POST(body);
    unsigned long elapsed = millis() - t0;
    Serial.printf("[AI] POST done in %lums, HTTP code: %d\n", elapsed, httpCode);

    if (httpCode != 200) {
        String errMsg = "HTTP " + String(httpCode);
        // Try to read error body for more info
        String errBody = http.getString();
        Serial.println("[AI] Error body: " + errBody);
        http.end();
        busy = false;
        if (onError) onError(errMsg);
        return;
    }

    String payload = http.getString();
    http.end();
    Serial.println("[AI] Response: " + payload.substring(0, 500));

    String fullResponse;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("[AI] JSON parse error: %s\n", err.c_str());
        busy = false;
        if (onError) onError("JSON parse error");
        return;
    }

    const char* text = doc["choices"][0]["message"]["content"];
    if (text && strlen(text) > 0) {
        fullResponse = text;
        // Truncate long responses for tiny screen
        if (fullResponse.length() > 200) {
            fullResponse = fullResponse.substring(0, 200) + "...";
        }
        Serial.println("[AI] Reply: " + fullResponse);
        if (onToken) onToken(fullResponse);
    } else {
        Serial.println("[AI] No content in response");
        if (onToken) onToken("[No response]");
    }

    addToHistory(userMessage, fullResponse);

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

String AIClient::buildRequestBody(const String& userMessage) {
    JsonDocument doc;
    doc["model"] = "openclaw";
    doc["user"] = "cardputer";  // stable session key for conversation memory

    JsonArray messages = doc["messages"].to<JsonArray>();

    JsonObject sysMsg = messages.add<JsonObject>();
    sysMsg["role"] = "system";
    sysMsg["content"] = "You are a tiny pixel companion living inside a Cardputer device. "
                        "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
                        "Be friendly, cute, and playful. Use simple words.";

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

    String body;
    serializeJson(doc, body);
    return body;
}
