#pragma once
#include <M5Cardputer.h>
#include "utils.h"

// ── Pixel Art 16-color palette (RGB565, PROGMEM) ──
// 0=transparent 1=black 2=white 3=red 4=darkred 5=orange 6=yellow 7=green
// 8=darkgreen 9=blue a=lightblue b=purple c=pink d=brown e=gray f=lightgray
static const uint16_t PROGMEM PIXEL_ART_PALETTE[16] = {
    Color::TRANSPARENT,             // 0 transparent
    rgb565(0, 0, 0),               // 1 black
    rgb565(255, 255, 255),         // 2 white
    rgb565(220, 50, 47),           // 3 red
    rgb565(160, 30, 25),           // 4 dark red
    rgb565(230, 140, 60),          // 5 orange
    rgb565(250, 220, 50),          // 6 yellow
    rgb565(50, 180, 50),           // 7 green
    rgb565(30, 100, 30),           // 8 dark green
    rgb565(40, 60, 200),           // 9 blue
    rgb565(100, 160, 240),         // a light blue
    rgb565(130, 50, 180),          // b purple
    rgb565(230, 120, 180),         // c pink
    rgb565(120, 70, 30),           // d brown
    rgb565(128, 128, 128),         // e gray
    rgb565(192, 192, 192),         // f light gray
};

class Chat {
public:
    void begin(M5Canvas& canvas);
    void update(M5Canvas& canvas);
    void handleKey(char key);
    void handleEnter();
    void handleBackspace();
    void scrollUp();
    void scrollDown();

    // Called when AI response token arrives (const char* to avoid heap allocation)
    void appendAIToken(const char* token);
    void onAIResponseComplete();

    bool hasPendingMessage() const { return pendingMessage.length() > 0; }
    String takePendingMessage();

    // Set input buffer (for voice input results)
    void setInput(const String& text);
    const String& getInput() const { return inputBuffer; }

    // Moisture level for dehydration lock
    void setMoistureLevel(int level) { moistureLevel = level; }

    // Thinking model indicator
    void setAIThinking(bool thinking) { aiThinking = thinking; }

    // Check if last user message was a /draw command (for AI prompt routing)
    bool isDrawCommand() const { return drawMode; }
    int getDrawSize() const { return drawSize; }

    // Get the last message (for desktop sync)
    int getMessageCount() const { return messageCount; }

    // Check if pixel art was just parsed (cleared after reading)
    bool hasNewPixelArt() const { return newPixelArt; }
    void clearNewPixelArt() { newPixelArt = false; }
    // Get the last parsed pixel art raw rows for broadcasting
    // Returns the size (8 or 16), fills rows array with hex strings
    int getLastPixelArtRows(char rows[][17], int maxRows) const;

private:
    static constexpr int MAX_MESSAGES = 20;
    static constexpr int INPUT_BAR_H = 16;
    static constexpr int MSG_AREA_Y = 16;
    static constexpr int MSG_AREA_H = SCREEN_H - INPUT_BAR_H - MSG_AREA_Y;
    static constexpr int LINE_H = 14;
    static constexpr int MAX_W = SCREEN_W - 12;
    static constexpr int PIXEL_ART_RENDER_SIZE = 96;  // 96×96px on screen

    struct Message {
        String text;
        bool isUser;
        bool isPixelArt = false;       // pixel art message
        uint8_t pixelSize = 0;         // 8 or 16
        int8_t pixelSlot = -1;         // index into shared pixel buffer (-1 = none)
    };

    // Shared pixel buffers — only 2 slots needed (saves ~9KB vs per-message)
    static constexpr int PIXEL_SLOTS = 2;
    uint16_t pixelBuffers[PIXEL_SLOTS][256];  // RGB565 bitmap (max 16×16)
    int nextPixelSlot = 0;

    Message messages[MAX_MESSAGES];
    int messageCount = 0;
    String inputBuffer;
    String pendingMessage;
    int scrollY = 0;         // pixel offset from top of content
    int totalContentH = 0;   // total rendered height of all messages
    bool waitingForAI = false;
    bool initialized = false;
    bool userScrolled = false; // user manually scrolled up

    int moistureLevel = 2;  // synced from companion every frame
    bool aiThinking = false;

    // /draw command state
    bool drawMode = false;
    int drawSize = 8;  // 8 or 16
    bool newPixelArt = false;  // set when pixel art is parsed, cleared by caller

    void drawMessages(M5Canvas& canvas);
    void drawInputBar(M5Canvas& canvas);
    void addMessage(const String& text, bool isUser);
    void scrollToBottom();
    int calcMessageHeight(M5Canvas& canvas, const Message& msg);
    int calcTotalHeight(M5Canvas& canvas);

    // Pixel art support
    void parsePixelArtResponse(int msgIdx);
    void drawPixelArt(M5Canvas& canvas, const Message& msg, int x, int y);
};
