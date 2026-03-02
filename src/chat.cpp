#include "chat.h"

void Chat::begin(M5Canvas& canvas) {
    if (!initialized) {
        messageCount = 0;
        inputBuffer = "";
        pendingMessage = "";
        waitingForAI = false;
        initialized = true;
    }
    // Recalculate and scroll to bottom
    totalContentH = calcTotalHeight(canvas);
    scrollToBottom();
}

void Chat::update(M5Canvas& canvas) {
    canvas.fillScreen(Color::BG_DAY);

    // Header
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(1);
    canvas.drawString("[TAB]back [;/]scroll [Fn]voice", 4, 2);
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
    userScrolled = false;

    // Add placeholder for AI response
    addMessage("thinking...", false);
}

void Chat::handleBackspace() {
    if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
    }
}

void Chat::scrollUp() {
    scrollY -= MSG_AREA_H / 2;  // half page
    if (scrollY < 0) scrollY = 0;
    userScrolled = true;
}

void Chat::scrollDown() {
    scrollY += MSG_AREA_H / 2;
    int maxScroll = totalContentH - MSG_AREA_H;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;
    // If we're at bottom, clear manual scroll flag
    if (scrollY >= maxScroll) userScrolled = false;
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
    if (!userScrolled) scrollToBottom();
}

void Chat::onAIResponseComplete() {
    waitingForAI = false;
    userScrolled = false;
    scrollToBottom();
}

String Chat::takePendingMessage() {
    String msg = pendingMessage;
    pendingMessage = "";
    return msg;
}

void Chat::setInput(const String& text) {
    inputBuffer = text;
}

void Chat::addMessage(const String& text, bool isUser) {
    int idx = messageCount % MAX_MESSAGES;
    messages[idx].text = text;
    messages[idx].isUser = isUser;
    messageCount++;
    if (!userScrolled) scrollToBottom();
}

int Chat::calcMessageHeight(M5Canvas& canvas, const Message& msg) {
    if (msg.isUser) {
        return LINE_H;
    }
    // AI messages may wrap
    String text = msg.text;
    int lines = 1;
    while (text.length() > 0) {
        int fitLen = text.length();
        while (fitLen > 0 && canvas.textWidth(text.substring(0, fitLen).c_str()) > MAX_W) {
            fitLen--;
        }
        if (fitLen == 0) fitLen = 1;
        text = text.substring(fitLen);
        if (text.length() > 0) lines++;
    }
    return lines * LINE_H;
}

int Chat::calcTotalHeight(M5Canvas& canvas) {
    int total = min(messageCount, MAX_MESSAGES);
    int startIdx = (messageCount > MAX_MESSAGES) ? (messageCount - MAX_MESSAGES) : 0;
    int h = 0;
    canvas.setTextSize(1);
    for (int i = 0; i < total; i++) {
        int idx = (startIdx + i) % MAX_MESSAGES;
        h += calcMessageHeight(canvas, messages[idx]);
    }
    return h;
}

void Chat::scrollToBottom() {
    // Will be called with canvas context from update/begin
    int maxScroll = totalContentH - MSG_AREA_H;
    if (maxScroll < 0) maxScroll = 0;
    scrollY = maxScroll;
}

void Chat::drawMessages(M5Canvas& canvas) {
    int total = min(messageCount, MAX_MESSAGES);
    int startIdx = (messageCount > MAX_MESSAGES) ? (messageCount - MAX_MESSAGES) : 0;

    // Recalculate total height each frame (AI messages grow during streaming)
    canvas.setTextSize(1);
    totalContentH = calcTotalHeight(canvas);

    // Clamp scrollY
    int maxScroll = totalContentH - MSG_AREA_H;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;
    if (scrollY < 0) scrollY = 0;
    // Auto-scroll if not manually scrolled
    if (!userScrolled) scrollY = maxScroll;

    // Draw with virtual Y offset
    int y = MSG_AREA_Y + 2 - scrollY;

    for (int i = 0; i < total; i++) {
        int idx = (startIdx + i) % MAX_MESSAGES;
        Message& msg = messages[idx];

        if (msg.isUser) {
            // User message - right aligned, blue
            if (y >= MSG_AREA_Y - LINE_H && y < SCREEN_H - INPUT_BAR_H) {
                canvas.setTextColor(Color::CHAT_USER);
                int tw = canvas.textWidth(msg.text.c_str());
                int tx = SCREEN_W - tw - 6;
                if (tx < 4) tx = 4;
                canvas.fillRoundRect(tx - 2, y - 1, min(tw + 4, SCREEN_W - 4), LINE_H, 2, Color::INPUT_BG);
                canvas.drawString(msg.text.c_str(), tx, y);
            }
            y += LINE_H;
        } else {
            // AI message - left aligned, green, word wrap
            canvas.setTextColor(Color::CHAT_AI);
            String text = msg.text;
            while (text.length() > 0) {
                int fitLen = text.length();
                while (fitLen > 0 && canvas.textWidth(text.substring(0, fitLen).c_str()) > MAX_W) {
                    fitLen--;
                }
                if (fitLen == 0) fitLen = 1;

                if (y >= MSG_AREA_Y - LINE_H && y < SCREEN_H - INPUT_BAR_H) {
                    String line = text.substring(0, fitLen);
                    canvas.drawString(line.c_str(), 6, y);
                }
                text = text.substring(fitLen);
                y += LINE_H;
            }
        }
    }

    // Scroll indicator
    if (totalContentH > MSG_AREA_H) {
        int barH = max(8, MSG_AREA_H * MSG_AREA_H / totalContentH);
        int barY = MSG_AREA_Y + (scrollY * (MSG_AREA_H - barH)) / maxScroll;
        canvas.fillRect(SCREEN_W - 2, barY, 2, barH, Color::STATUS_DIM);
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
