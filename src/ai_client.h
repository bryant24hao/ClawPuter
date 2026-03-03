#pragma once
#include <Arduino.h>
#include <functional>
#include <ArduinoJson.h>

class AIClient {
public:
    using TokenCallback = std::function<void(const char* token)>;
    using DoneCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const String& error)>;

    void begin(const String& apiKey);

    // Send message to OpenClaw gateway with SSE streaming.
    // Calls onToken for each text token, onDone when complete, onError on failure.
    void sendMessage(const String& userMessage,
                     TokenCallback onToken,
                     DoneCallback onDone,
                     ErrorCallback onError);

    // Call in loop() to process streaming response
    void update();

    bool isBusy() const { return busy; }

private:
    String apiKey;
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
};
