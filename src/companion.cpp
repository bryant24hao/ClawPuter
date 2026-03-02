#include "companion.h"
#include "sprites.h"
#include <time.h>

// Character draw position (centered, above ground)
constexpr int CHAR_SCALE = 3;  // 16×3 = 48px on screen
constexpr int CHAR_DRAW_W = CHAR_W * CHAR_SCALE;
constexpr int CHAR_DRAW_H = CHAR_H * CHAR_SCALE;
constexpr int GROUND_Y = SCREEN_H - 28;
constexpr int CHAR_X = (SCREEN_W - CHAR_DRAW_W) / 2;
constexpr int CHAR_Y = GROUND_Y - CHAR_DRAW_H - 2;

void Companion::begin(M5Canvas& canvas) {
    initStars();
    setState(CompanionState::IDLE);
}

void Companion::initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(10, SCREEN_W - 10);
        stars[i].y = random(5, GROUND_Y - 60);
        stars[i].visible = random(2) == 0;
    }
}

void Companion::setState(CompanionState newState) {
    state = newState;
    frameIndex = 0;
    stateStartTime = millis();
    animTimer.reset();

    switch (state) {
        case CompanionState::IDLE:
            animTimer.setInterval(500);
            break;
        case CompanionState::HAPPY:
            animTimer.setInterval(200);
            break;
        case CompanionState::SLEEP:
            animTimer.setInterval(1000);
            break;
        case CompanionState::TALK:
            animTimer.setInterval(250);
            break;
    }
}

void Companion::update(M5Canvas& canvas) {
    // Advance animation frame
    if (animTimer.tick()) {
        frameIndex++;

        // State-specific logic
        switch (state) {
            case CompanionState::IDLE:
                frameIndex %= IDLE_FRAME_COUNT;
                break;
            case CompanionState::HAPPY:
                if (frameIndex >= HAPPY_FRAME_COUNT * 3) {
                    setState(CompanionState::IDLE);
                } else {
                    frameIndex %= HAPPY_FRAME_COUNT;
                }
                break;
            case CompanionState::SLEEP:
                frameIndex %= SLEEP_FRAME_COUNT;
                break;
            case CompanionState::TALK:
                frameIndex %= TALK_FRAME_COUNT;
                break;
        }
    }

    // Auto-sleep after idle timeout
    if (state == CompanionState::IDLE && idleTimeout.tick()) {
        setState(CompanionState::SLEEP);
    }

    // Twinkle stars
    if (starTimer.tick()) {
        int idx = random(MAX_STARS);
        stars[idx].visible = !stars[idx].visible;
    }

    // Draw everything
    drawBackground(canvas);
    drawCharacter(canvas);
    drawSleepZ(canvas);
    drawClock(canvas);
    drawStatusText(canvas);
}

void Companion::handleKey(char key) {
    // Any keypress wakes from sleep and resets idle timer
    idleTimeout.reset();

    if (state == CompanionState::SLEEP) {
        setState(CompanionState::IDLE);
        return;
    }

    // Space or Enter triggers happy
    if (key == ' ' || key == '\n') {
        triggerHappy();
    }
}

void Companion::triggerHappy() {
    setState(CompanionState::HAPPY);
    idleTimeout.reset();
}

void Companion::triggerTalk() {
    setState(CompanionState::TALK);
    idleTimeout.reset();
}

void Companion::triggerIdle() {
    setState(CompanionState::IDLE);
    idleTimeout.reset();
}

void Companion::drawBackground(M5Canvas& canvas) {
    // Sky gradient
    canvas.fillScreen(Color::BG_DAY);

    // Stars
    for (int i = 0; i < MAX_STARS; i++) {
        if (stars[i].visible) {
            canvas.drawPixel(stars[i].x, stars[i].y, Color::STAR);
            // Some stars are bigger (cross shape)
            if (i % 3 == 0) {
                canvas.drawPixel(stars[i].x - 1, stars[i].y, Color::STAR);
                canvas.drawPixel(stars[i].x + 1, stars[i].y, Color::STAR);
                canvas.drawPixel(stars[i].x, stars[i].y - 1, Color::STAR);
                canvas.drawPixel(stars[i].x, stars[i].y + 1, Color::STAR);
            }
        }
    }

    // Ground
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, Color::GROUND);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, Color::GROUND_TOP);

    // Ground detail dots
    for (int i = 0; i < 8; i++) {
        int gx = (i * 31 + 10) % SCREEN_W;
        canvas.drawPixel(gx, GROUND_Y + 4, Color::GROUND_TOP);
        canvas.drawPixel(gx + 15, GROUND_Y + 8, Color::GROUND_TOP);
    }
}

void Companion::drawCharacter(M5Canvas& canvas) {
    const uint16_t* frame = nullptr;

    switch (state) {
        case CompanionState::IDLE:
            frame = idle_frames[frameIndex % IDLE_FRAME_COUNT];
            break;
        case CompanionState::HAPPY:
            frame = happy_frames[frameIndex % HAPPY_FRAME_COUNT];
            break;
        case CompanionState::SLEEP:
            frame = sleep_frames[frameIndex % SLEEP_FRAME_COUNT];
            break;
        case CompanionState::TALK:
            frame = talk_frames[frameIndex % TALK_FRAME_COUNT];
            break;
    }

    if (frame) {
        int yOffset = 0;
        // Bounce effect for happy state
        if (state == CompanionState::HAPPY && frameIndex % 2 == 0) {
            yOffset = -4;
        }
        drawSprite16(canvas, CHAR_X, CHAR_Y + yOffset, frame);
    }
}

void Companion::drawSprite16(M5Canvas& canvas, int x, int y, const uint16_t* data) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            uint16_t color = pgm_read_word(&data[py * CHAR_W + px]);
            if (color != Color::TRANSPARENT) {
                canvas.fillRect(x + px * CHAR_SCALE, y + py * CHAR_SCALE,
                               CHAR_SCALE, CHAR_SCALE, color);
            }
        }
    }
}

void Companion::drawClock(M5Canvas& canvas) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) {
        canvas.setTextColor(Color::CLOCK_TEXT);
        canvas.setTextSize(1);
        canvas.drawString("--:--", SCREEN_W / 2 - 15, GROUND_Y + 8);
        return;
    }

    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // Draw time centered at bottom
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(2);
    int tw = canvas.textWidth(timeStr);
    canvas.drawString(timeStr, (SCREEN_W - tw) / 2, GROUND_Y + 6);
}

void Companion::drawSleepZ(M5Canvas& canvas) {
    if (state != CompanionState::SLEEP) return;

    unsigned long elapsed = millis() - stateStartTime;
    int phase = (elapsed / 600) % 4;

    canvas.setTextColor(Color::CLOCK_TEXT);

    // Floating Z's at different positions
    if (phase >= 1) {
        canvas.setTextSize(1);
        canvas.drawString("z", CHAR_X + CHAR_DRAW_W + 4, CHAR_Y + 10);
    }
    if (phase >= 2) {
        canvas.setTextSize(1);
        canvas.drawString("Z", CHAR_X + CHAR_DRAW_W + 10, CHAR_Y);
    }
    if (phase >= 3) {
        canvas.setTextSize(2);
        canvas.drawString("Z", CHAR_X + CHAR_DRAW_W + 16, CHAR_Y - 12);
    }
}

void Companion::drawStatusText(M5Canvas& canvas) {
    const char* statusStr = "";
    switch (state) {
        case CompanionState::IDLE:   statusStr = "chillin'"; break;
        case CompanionState::HAPPY:  statusStr = "yay!"; break;
        case CompanionState::SLEEP:  statusStr = "zzz..."; break;
        case CompanionState::TALK:   statusStr = "talking..."; break;
    }

    canvas.setTextColor(Color::STATUS_DIM);
    canvas.setTextSize(1);
    canvas.drawString(statusStr, 4, 4);

    // TAB hint
    canvas.drawString("[TAB] chat", SCREEN_W - 60, 4);
}
