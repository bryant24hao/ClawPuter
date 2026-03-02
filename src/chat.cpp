#include "chat.h"

void Chat::begin(M5Canvas& canvas) {
    messageCount = 0;
    inputBuffer = "";
    pendingMessage = "";
    scrollOffset = 0;
    waitingForAI = false;
}

void Chat::update(M5Canvas& canvas) {
    canvas.fillScreen(Color::BG_DAY);

    // Header
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(1);
    canvas.drawString("[TAB] back", 4, 2);
    canvas.drawFastHLine(0, MSG_AREA_Y - 1, SCREEN_W, Color::GROUND_TOP);

    drawMessages(canvas);
    drawInputBar(canvas);
}

void Chat::handleKey(char key) {
    if (waitingForAI) return;
    if (inputBuffer.length() < 100) {
        inputBuffer += key;
    }
}

void Chat::handleEnter() {
    if (inputBuffer.length() == 0 || waitingForAI) return;

    addMessage(inputBuffer, true);
    pendingMessage = inputBuffer;
    inputBuffer = "";
    waitingForAI = true;

    // Add placeholder for AI response
    addMessage("thinking...", false);
}

void Chat::handleBackspace() {
    if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
    }
}

void Chat::appendAIToken(const String& token) {
    if (messageCount > 0 && !messages[(messageCount - 1) % MAX_MESSAGES].isUser) {
        Message& lastMsg = messages[(messageCount - 1) % MAX_MESSAGES];
        if (lastMsg.text == "thinking...") {
            lastMsg.text = token;
        } else {
            lastMsg.text += token;
        }
    }
    scrollToBottom();
}

void Chat::onAIResponseComplete() {
    waitingForAI = false;
}

String Chat::takePendingMessage() {
    String msg = pendingMessage;
    pendingMessage = "";
    return msg;
}

void Chat::addMessage(const String& text, bool isUser) {
    int idx = messageCount % MAX_MESSAGES;
    messages[idx].text = text;
    messages[idx].isUser = isUser;
    messageCount++;
    scrollToBottom();
}

void Chat::scrollToBottom() {
    // Simple: always show latest messages
    int total = min(messageCount, MAX_MESSAGES);
    int visibleLines = MSG_AREA_H / 12; // ~12px per line
    if (total > visibleLines) {
        scrollOffset = total - visibleLines;
    } else {
        scrollOffset = 0;
    }
}

void Chat::drawMessages(M5Canvas& canvas) {
    int total = min(messageCount, MAX_MESSAGES);
    int startIdx = (messageCount > MAX_MESSAGES) ? (messageCount - MAX_MESSAGES) : 0;

    canvas.setTextSize(1);
    int y = MSG_AREA_Y + 2;
    int lineH = 12;

    for (int i = scrollOffset; i < total && y < SCREEN_H - INPUT_BAR_H - 2; i++) {
        int idx = (startIdx + i) % MAX_MESSAGES;
        Message& msg = messages[idx];

        if (msg.isUser) {
            // User message - right aligned, blue
            canvas.setTextColor(Color::CHAT_USER);
            int tw = canvas.textWidth(msg.text.c_str());
            int tx = SCREEN_W - tw - 6;
            if (tx < 4) tx = 4;
            canvas.fillRoundRect(tx - 2, y - 1, min(tw + 4, SCREEN_W - 4), lineH, 2, Color::INPUT_BG);
            canvas.drawString(msg.text.c_str(), tx, y);
        } else {
            // AI message - left aligned, green
            canvas.setTextColor(Color::CHAT_AI);
            // Word wrap for long messages
            String text = msg.text;
            int maxW = SCREEN_W - 12;
            while (text.length() > 0 && y < SCREEN_H - INPUT_BAR_H - 2) {
                // Find how many chars fit
                int fitLen = text.length();
                while (fitLen > 0 && canvas.textWidth(text.substring(0, fitLen).c_str()) > maxW) {
                    fitLen--;
                }
                if (fitLen == 0) fitLen = 1;

                String line = text.substring(0, fitLen);
                canvas.drawString(line.c_str(), 6, y);
                text = text.substring(fitLen);
                if (text.length() > 0) y += lineH;
            }
        }
        y += lineH;
    }
}

void Chat::drawInputBar(M5Canvas& canvas) {
    int barY = SCREEN_H - INPUT_BAR_H;
    canvas.fillRect(0, barY, SCREEN_W, INPUT_BAR_H, Color::INPUT_BG);
    canvas.drawFastHLine(0, barY, SCREEN_W, Color::GROUND_TOP);

    canvas.setTextColor(Color::WHITE);
    canvas.setTextSize(1);

    if (waitingForAI) {
        canvas.setTextColor(Color::STATUS_DIM);
        canvas.drawString("waiting...", 4, barY + 4);
    } else {
        // Show input with cursor
        String display = "> " + inputBuffer + "_";
        // Truncate from left if too long
        int maxW = SCREEN_W - 8;
        while (canvas.textWidth(display.c_str()) > maxW && display.length() > 2) {
            display = "> " + display.substring(3);
        }
        canvas.drawString(display.c_str(), 4, barY + 4);
    }
}
