#pragma once
#include <Arduino.h>
#include <functional>
#include <ArduinoJson.h>

class AIClient {
public:
    using TokenCallback = std::function<void(const char* token)>;
    using DoneCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const String& error)>;

    void begin(const String& apiKey, const String& gwHost,
               const String& gwPort, const String& gwToken);

    // Send message to OpenClaw gateway with SSE streaming.
    // Calls onToken for each text token, onDone when complete, onError on failure.
    void sendMessage(const String& userMessage,
                     TokenCallback onToken,
                     DoneCallback onDone,
                     ErrorCallback onError);

    // Set pixel art mode for next request (uses specialized prompt, higher limit)
    void setPixelArtMode(bool enabled, int size = 8);

    // Call in loop() to process streaming response
    void update();

    bool isBusy() const { return busy; }

    // Last AI response text (for TTS playback)
    const String& getLastResponse() const { return lastResponse; }

    // Companion state for dynamic system prompt
    struct CompanionContext {
        int moisture = 4;
        int weatherType = -1;
        float temperature = -999;
        int humidity = -1;
    };
    void setCompanionContext(const CompanionContext& ctx) { companionCtx = ctx; }
    void clearHistory() { historyCount = 0; }

    // Thinking model support: set when model streams thinking content
    bool thinkingDetected = false;

private:
    CompanionContext companionCtx;
    String lastResponse;
    String apiKey;
    String gwHost;
    String gwPort;
    String gwToken;
    bool busy = false;

    // Conversation history (keep last N exchanges for context)
    static constexpr int MAX_HISTORY = 2;
    struct Exchange {
        String user;
        String assistant;
    };
    Exchange history[MAX_HISTORY];
    int historyCount = 0;

    void addToHistory(const String& user, const String& assistant);
    // Build JSON request body into doc. Caller uses measureJson/serializeJson.
    void buildRequestDoc(const String& userMessage, JsonDocument& doc);

    // Pixel art mode
    bool pixelArtMode = false;
    int pixelArtSize = 8;
};
