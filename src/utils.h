#pragma once
#include <cstdint>
#include <Arduino.h>

// Screen dimensions (landscape)
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// RGB565 color helpers
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Color palette
namespace Color {
    constexpr uint16_t BLACK      = 0x0000;
    constexpr uint16_t WHITE      = 0xFFFF;
    constexpr uint16_t BG_DAY     = rgb565(24, 20, 37);    // Dark purple background
    constexpr uint16_t BG_NIGHT   = rgb565(10, 10, 20);    // Darker night background
    constexpr uint16_t GROUND     = rgb565(46, 34, 47);    // Ground color
    constexpr uint16_t GROUND_TOP = rgb565(62, 53, 70);    // Ground highlight
    constexpr uint16_t STAR       = rgb565(200, 200, 140); // Star/twinkle
    constexpr uint16_t CLOCK_TEXT = rgb565(180, 180, 200); // Clock text color
    constexpr uint16_t STATUS_DIM = rgb565(100, 100, 120); // Dim status text
    constexpr uint16_t CHAT_USER  = rgb565(80, 120, 200);  // User message bubble
    constexpr uint16_t CHAT_AI    = rgb565(60, 160, 80);   // AI message bubble
    constexpr uint16_t INPUT_BG   = rgb565(40, 36, 50);    // Input bar background
    constexpr uint16_t TRANSPARENT = rgb565(255, 0, 255);  // Magenta = transparent
}

// Strip characters unsupported by efontCN (emoji = 4-byte UTF-8, markdown */_)
// In-place version: filter src into dst buffer, returns length written.
// Zero heap allocation — use this in hot paths (SSE streaming).
inline int filterForDisplayBuf(const char* src, char* dst, int dstSize) {
    const char* p = src;
    int i = 0;
    while (*p && i < dstSize - 1) {
        uint8_t c = (uint8_t)*p;
        if (c < 0x80) {
            if (c == '*' || c == '_') { p++; continue; }
            if (c >= 0x20 || c == '\n') dst[i++] = (char)c;
            p++;
        } else if (c < 0xC0) {
            p++; // stray continuation byte
        } else if (c < 0xE0) {
            if (p[1] && i + 1 < dstSize - 1) { dst[i++] = p[0]; dst[i++] = p[1]; }
            p += 2;
        } else if (c < 0xF0) {
            if (p[1] && p[2] && i + 2 < dstSize - 1) { dst[i++] = p[0]; dst[i++] = p[1]; dst[i++] = p[2]; }
            p += 3;
        } else {
            p += 4; // 4-byte emoji — skip
        }
    }
    dst[i] = '\0';
    return i;
}

// String version — convenience for non-hot-path callers
inline String filterForDisplay(const String& input) {
    // Use stack buffer, return as String
    int len = input.length();
    if (len > 512) len = 512;
    char buf[512];
    int outLen = filterForDisplayBuf(input.c_str(), buf, sizeof(buf));
    return String(buf);
}

// Simple non-blocking timer
struct Timer {
    unsigned long interval;
    unsigned long lastTick;

    Timer(unsigned long ms = 1000) : interval(ms), lastTick(0) {}

    bool tick() {
        unsigned long now = millis();
        if (now - lastTick >= interval) {
            lastTick = now;
            return true;
        }
        return false;
    }

    void reset() { lastTick = millis(); }
    void setInterval(unsigned long ms) { interval = ms; }
};
