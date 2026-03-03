#pragma once
#include <M5Cardputer.h>
#include "utils.h"

class Chat {
public:
    void begin(M5Canvas& canvas);
    void update(M5Canvas& canvas);
    void handleKey(char key);
    void handleEnter();
    void handleBackspace();
    void scrollUp();
    void scrollDown();

    // Called when AI response token arrives
    void appendAIToken(const String& token);
    void onAIResponseComplete();

    bool hasPendingMessage() const { return pendingMessage.length() > 0; }
    String takePendingMessage();

    // Set input buffer (for voice input results)
    void setInput(const String& text);
    const String& getInput() const { return inputBuffer; }

private:
    static constexpr int MAX_MESSAGES = 20;
    static constexpr int INPUT_BAR_H = 16;
    static constexpr int MSG_AREA_Y = 16;
    static constexpr int MSG_AREA_H = SCREEN_H - INPUT_BAR_H - MSG_AREA_Y;
    static constexpr int LINE_H = 14;
    static constexpr int MAX_W = SCREEN_W - 12;

    struct Message {
        String text;
        bool isUser;
    };

    Message messages[MAX_MESSAGES];
    int messageCount = 0;
    String inputBuffer;
    String pendingMessage;
    int scrollY = 0;         // pixel offset from top of content
    int totalContentH = 0;   // total rendered height of all messages
    bool waitingForAI = false;
    bool initialized = false;
    bool userScrolled = false; // user manually scrolled up

    void drawMessages(M5Canvas& canvas);
    void drawInputBar(M5Canvas& canvas);
    void addMessage(const String& text, bool isUser);
    void scrollToBottom();
    int calcMessageHeight(M5Canvas& canvas, const Message& msg);
    int calcTotalHeight(M5Canvas& canvas);
};
