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

    // Called when AI response token arrives
    void appendAIToken(const String& token);
    void onAIResponseComplete();

    bool hasPendingMessage() const { return pendingMessage.length() > 0; }
    String takePendingMessage();

private:
    static constexpr int MAX_MESSAGES = 20;
    static constexpr int INPUT_BAR_H = 16;
    static constexpr int MSG_AREA_Y = 12;
    static constexpr int MSG_AREA_H = SCREEN_H - INPUT_BAR_H - MSG_AREA_Y;

    struct Message {
        String text;
        bool isUser;
    };

    Message messages[MAX_MESSAGES];
    int messageCount = 0;
    String inputBuffer;
    String pendingMessage;
    int scrollOffset = 0;
    bool waitingForAI = false;

    void drawMessages(M5Canvas& canvas);
    void drawInputBar(M5Canvas& canvas);
    void addMessage(const String& text, bool isUser);
    void scrollToBottom();
};
