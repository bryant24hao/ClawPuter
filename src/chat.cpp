#include "chat.h"
#include <cstring>

// ── UTF-8 safe line-break helper ──
// Given text starting at `start`, find how many bytes fit within maxW pixels.
// Returns byte count, ensuring we don't split a multi-byte UTF-8 character.
static int fitBytes(M5Canvas& canvas, const char* start, int len, int maxW, char* buf, int bufSize) {
    if (len == 0) return 0;

    // Try full length first (common case: line fits)
    int tryLen = (len < bufSize - 1) ? len : bufSize - 1;
    memcpy(buf, start, tryLen);
    buf[tryLen] = '\0';
    if (canvas.textWidth(buf) <= maxW) return tryLen;

    // Binary search for the max fitting length
    int lo = 1, hi = tryLen;
    int best = 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        memcpy(buf, start, mid);
        buf[mid] = '\0';
        if (canvas.textWidth(buf) <= maxW) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    // Don't split a UTF-8 multi-byte character: back up if we landed on a continuation byte
    while (best > 0 && (start[best] & 0xC0) == 0x80) {
        best--;
    }
    if (best == 0) best = 1; // always advance at least 1 byte

    return best;
}

// Count wrapped lines for an AI message — zero heap allocation
static int countWrappedLines(M5Canvas& canvas, const char* text, int maxW, char* buf, int bufSize) {
    int len = strlen(text);
    if (len == 0) return 1;
    int pos = 0;
    int lines = 0;
    while (pos < len) {
        int fit = fitBytes(canvas, text + pos, len - pos, maxW, buf, bufSize);
        pos += fit;
        lines++;
    }
    return lines;
}

void Chat::begin(M5Canvas& canvas) {
    if (!initialized) {
        messageCount = 0;
        inputBuffer = "";
        pendingMessage = "";
        waitingForAI = false;
        // Pre-reserve all message slots so later addMessage() never allocates.
        // This keeps the large contiguous heap block intact for voice buffer.
        for (int i = 0; i < MAX_MESSAGES; i++) {
            messages[i].text.reserve(320);
        }
        initialized = true;
    }
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextSize(1);
    totalContentH = calcTotalHeight(canvas);
    scrollToBottom();
}

void Chat::update(M5Canvas& canvas) {
    canvas.fillScreen(Color::BG_DAY);
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextSize(1);

    // Header
    canvas.setTextColor(Color::CLOCK_TEXT);
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

    // Detect /draw commands before consuming input
    drawMode = false;
    drawSize = 8;
    if (inputBuffer.startsWith("/draw16")) {
        drawMode = true;
        drawSize = 16;
    } else if (inputBuffer.startsWith("/draw")) {
        drawMode = true;
        drawSize = 8;
    }

    addMessage(inputBuffer, true);
    pendingMessage = inputBuffer;
    inputBuffer = "";
    waitingForAI = true;
    userScrolled = false;

    // Add placeholder for AI response
    addMessage(drawMode ? "drawing..." : "thinking...", false);
}

void Chat::handleBackspace() {
    if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
    }
}

void Chat::scrollUp() {
    scrollY -= MSG_AREA_H / 2;
    if (scrollY < 0) scrollY = 0;
    userScrolled = true;
}

void Chat::scrollDown() {
    scrollY += MSG_AREA_H / 2;
    int maxScroll = totalContentH - MSG_AREA_H;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;
    if (scrollY >= maxScroll) userScrolled = false;
}

void Chat::appendAIToken(const char* token) {
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
    // Try to parse pixel art from the last AI message —
    // always check for [PIXELART:] tags, not just /draw commands.
    // AI may return pixel art spontaneously in conversation.
    if (messageCount > 0) {
        int idx = (messageCount - 1) % MAX_MESSAGES;
        Message& msg = messages[idx];
        if (!msg.isUser && msg.text.indexOf("[PIXELART:") >= 0) {
            // Auto-detect size from tag if not set by /draw command
            if (!drawMode) {
                int tagPos = msg.text.indexOf("[PIXELART:");
                int sizeVal = msg.text.substring(tagPos + 10).toInt();
                if (sizeVal == 16) drawSize = 16;
                else drawSize = 8;
            }
            parsePixelArtResponse(idx);
        }
    }
    drawMode = false;

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
    messages[idx].isUser = isUser;
    messages[idx].isPixelArt = false;
    messages[idx].pixelSize = 0;
    messages[idx].pixelSlot = -1;
    if (!isUser) {
        // AI messages grow via streaming — pre-reserve to avoid realloc
        messages[idx].text = "";
        messages[idx].text.reserve(320);
        messages[idx].text = text;
    } else {
        messages[idx].text = text;
    }
    messageCount++;
    if (!userScrolled) scrollToBottom();
}

// ── Zero-heap-allocation message height/drawing ──
// Shared stack buffer for textWidth measurement (avoids per-call allocation)

int Chat::calcMessageHeight(M5Canvas& canvas, const Message& msg) {
    if (msg.isUser) return LINE_H;
    if (msg.isPixelArt) {
        // 96px grid + 2px border + LINE_H for label = 96 + 4 + LINE_H
        return PIXEL_ART_RENDER_SIZE + 4 + LINE_H;
    }

    char buf[64]; // stack buffer for textWidth measurement
    int lines = countWrappedLines(canvas, msg.text.c_str(), MAX_W, buf, sizeof(buf));
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
    if (!userScrolled) scrollY = maxScroll;

    // Stack buffer shared across all line measurements (zero heap allocation)
    char buf[64];
    int y = MSG_AREA_Y + 2 - scrollY;

    for (int i = 0; i < total; i++) {
        int idx = (startIdx + i) % MAX_MESSAGES;
        Message& msg = messages[idx];

        if (msg.isUser) {
            if (y >= MSG_AREA_Y - LINE_H && y < SCREEN_H - INPUT_BAR_H) {
                canvas.setTextColor(Color::CHAT_USER);
                int tw = canvas.textWidth(msg.text.c_str());
                int tx = SCREEN_W - tw - 6;
                if (tx < 4) tx = 4;
                canvas.fillRoundRect(tx - 2, y - 1, min(tw + 4, SCREEN_W - 4), LINE_H, 2, Color::INPUT_BG);
                canvas.drawString(msg.text.c_str(), tx, y);
            }
            y += LINE_H;
        } else if (msg.isPixelArt) {
            // Pixel art message — render grid with border and label
            int msgH = calcMessageHeight(canvas, msg);
            if (y >= MSG_AREA_Y - msgH && y < SCREEN_H - INPUT_BAR_H) {
                // Size label
                canvas.setTextColor(Color::STATUS_DIM);
                char label[8];
                snprintf(label, sizeof(label), "%dx%d", msg.pixelSize, msg.pixelSize);
                canvas.drawString(label, 6, y);

                // Border + pixel grid
                int gridY = y + LINE_H;
                int gridX = 6;
                canvas.drawRect(gridX - 1, gridY - 1,
                                PIXEL_ART_RENDER_SIZE + 2, PIXEL_ART_RENDER_SIZE + 2,
                                Color::STATUS_DIM);
                drawPixelArt(canvas, msg, gridX, gridY);
            }
            y += msgH;
        } else {
            // AI message — word wrap using pointer arithmetic, zero heap allocation
            canvas.setTextColor(Color::CHAT_AI);
            const char* text = msg.text.c_str();
            int len = msg.text.length();
            int pos = 0;

            while (pos < len) {
                int fit = fitBytes(canvas, text + pos, len - pos, MAX_W, buf, sizeof(buf));

                if (y >= MSG_AREA_Y - LINE_H && y < SCREEN_H - INPUT_BAR_H) {
                    memcpy(buf, text + pos, fit);
                    buf[fit] = '\0';
                    canvas.drawString(buf, 6, y);
                }

                pos += fit;
                y += LINE_H;
            }
            // Empty message still takes one line
            if (len == 0) y += LINE_H;
        }
    }

    // Scroll indicator
    if (totalContentH > MSG_AREA_H && maxScroll > 0) {
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
        canvas.drawString(drawMode ? "drawing..." : "waiting...", 4, barY + 4);
    } else {
        // Show input with cursor — use snprintf to avoid String concatenation
        char display[128];
        snprintf(display, sizeof(display), "> %s_", inputBuffer.c_str());
        // Truncate from left if too long
        const char* p = display;
        while (canvas.textWidth(p) > SCREEN_W - 8 && strlen(p) > 4) {
            p += 1; // skip one byte forward
            // Skip UTF-8 continuation bytes
            while ((*p & 0xC0) == 0x80) p++;
        }
        if (p != display) {
            // We truncated — show with ">" prefix
            char truncated[128];
            snprintf(truncated, sizeof(truncated), "> %s", p);
            canvas.drawString(truncated, 4, barY + 4);
        } else {
            canvas.drawString(display, 4, barY + 4);
        }
    }
}

// ── Pixel Art Support ──

// Convert hex char to palette index (0-15), returns -1 on invalid
static int hexCharToIndex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

void Chat::parsePixelArtResponse(int msgIdx) {
    Message& msg = messages[msgIdx];
    const char* text = msg.text.c_str();

    // Find [PIXELART:N] tag
    const char* start = strstr(text, "[PIXELART:");
    if (!start) return;

    int size = atoi(start + 10);  // parse N after ":"
    if (size != 8 && size != 16) return;

    // Find closing tag
    const char* end = strstr(start, "[/PIXELART]");
    if (!end) return;

    // Find the start of pixel data (after the first ']' in [PIXELART:N])
    const char* dataStart = strchr(start + 10, ']');
    if (!dataStart || dataStart >= end) return;
    dataStart++;  // skip ']'

    // Skip whitespace/newlines
    while (dataStart < end && (*dataStart == '\n' || *dataStart == '\r' || *dataStart == ' '))
        dataStart++;

    // Allocate a pixel slot (round-robin)
    int slot = nextPixelSlot;
    nextPixelSlot = (nextPixelSlot + 1) % PIXEL_SLOTS;
    uint16_t* pixels = pixelBuffers[slot];

    // Invalidate any old message that used this slot
    int total = min(messageCount, MAX_MESSAGES);
    int startOld = (messageCount > MAX_MESSAGES) ? (messageCount - MAX_MESSAGES) : 0;
    for (int i = 0; i < total; i++) {
        int oi = (startOld + i) % MAX_MESSAGES;
        if (messages[oi].pixelSlot == slot) {
            messages[oi].pixelSlot = -1;
            messages[oi].isPixelArt = false;
        }
    }

    // Parse row by row
    int row = 0;
    const char* p = dataStart;
    while (p < end && row < size) {
        // Skip whitespace between rows
        while (p < end && (*p == '\n' || *p == '\r' || *p == ' ')) p++;
        if (p >= end) break;

        // Parse one row of hex chars
        int col = 0;
        while (p < end && col < size && *p != '\n' && *p != '\r') {
            int idx = hexCharToIndex(*p);
            if (idx < 0) { p++; continue; }  // skip non-hex chars

            uint16_t color = pgm_read_word(&PIXEL_ART_PALETTE[idx]);
            pixels[row * size + col] = color;
            col++;
            p++;
        }

        // Fill remaining cols with transparent if row was short
        while (col < size) {
            pixels[row * size + col] = Color::TRANSPARENT;
            col++;
        }
        row++;
    }

    // Fill remaining rows with transparent
    while (row < size) {
        for (int col = 0; col < size; col++)
            pixels[row * size + col] = Color::TRANSPARENT;
        row++;
    }

    // Mark as pixel art
    msg.isPixelArt = true;
    msg.pixelSize = size;
    msg.pixelSlot = slot;
    msg.text = "[pixel art]";  // Release large text buffer
    newPixelArt = true;

    Serial.printf("[CHAT] Parsed %dx%d pixel art (slot %d)\n", size, size, slot);
}

int Chat::getLastPixelArtRows(char rows[][17], int maxRows) const {
    // Find the most recent pixel art message with a valid slot
    int total = min(messageCount, MAX_MESSAGES);
    int startIdx = (messageCount > MAX_MESSAGES) ? (messageCount - MAX_MESSAGES) : 0;

    for (int i = total - 1; i >= 0; i--) {
        int idx = (startIdx + i) % MAX_MESSAGES;
        const Message& msg = messages[idx];
        if (!msg.isPixelArt || msg.pixelSlot < 0) continue;

        const uint16_t* pixels = pixelBuffers[msg.pixelSlot];
        int size = msg.pixelSize;
        int rowCount = min(size, maxRows);

        // Reverse-convert pixels back to hex chars
        for (int r = 0; r < rowCount; r++) {
            for (int c = 0; c < size && c < 16; c++) {
                uint16_t color = pixels[r * size + c];
                // Find matching palette index
                char hex = '0';  // default transparent
                for (int p = 0; p < 16; p++) {
                    if (pgm_read_word(&PIXEL_ART_PALETTE[p]) == color) {
                        hex = (p < 10) ? ('0' + p) : ('a' + p - 10);
                        break;
                    }
                }
                rows[r][c] = hex;
            }
            rows[r][size] = '\0';
        }
        return size;
    }
    return 0;
}

void Chat::drawPixelArt(M5Canvas& canvas, const Message& msg, int x, int y) {
    if (msg.pixelSlot < 0) return;  // No valid pixel data
    const uint16_t* pixels = pixelBuffers[msg.pixelSlot];
    int scale = (msg.pixelSize == 8) ? 12 : 6;  // Both render to 96×96
    for (int py = 0; py < msg.pixelSize; py++) {
        for (int px = 0; px < msg.pixelSize; px++) {
            uint16_t color = pixels[py * msg.pixelSize + px];
            if (color != Color::TRANSPARENT) {
                canvas.fillRect(x + px * scale, y + py * scale, scale, scale, color);
            }
        }
    }
}
