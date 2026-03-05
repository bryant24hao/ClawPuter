#include "companion.h"
#include "sprites.h"
#include <time.h>

// Character draw dimensions
constexpr int CHAR_SCALE = 3;  // 16×3 = 48px on screen
constexpr int CHAR_DRAW_W = CHAR_W * CHAR_SCALE;
constexpr int CHAR_DRAW_H = CHAR_H * CHAR_SCALE;
constexpr int GROUND_Y = SCREEN_H - 28;

// Movement
constexpr int MOVE_STEP = 2;  // 2px per step (~120px/s at 60fps)
constexpr int MOVE_MIN_X = 0;
constexpr int MOVE_MAX_X = SCREEN_W - CHAR_DRAW_W;
constexpr int MOVE_MIN_Y = 16;  // below clock area
constexpr int MOVE_MAX_Y = GROUND_Y - CHAR_DRAW_H - 2;

// Day/night colors
constexpr uint16_t SKY_DAY    = rgb565(60, 120, 200);   // Blue sky
constexpr uint16_t SKY_SUNSET = rgb565(180, 80, 60);    // Orange sunset
constexpr uint16_t SKY_NIGHT  = rgb565(10, 10, 30);     // Dark night
constexpr uint16_t GROUND_DAY = rgb565(80, 140, 60);    // Green grass
constexpr uint16_t GROUND_DAY_TOP = rgb565(100, 170, 70);
constexpr uint16_t SUN_COLOR  = rgb565(255, 220, 60);
constexpr uint16_t MOON_COLOR = rgb565(220, 220, 200);
constexpr uint16_t CLOUD_COLOR = rgb565(220, 230, 240);

void Companion::begin(M5Canvas& canvas) {
    // Center position on first call; preserve across mode switches
    if (charX == 0 && charY == 0) {
        charX = (SCREEN_W - CHAR_DRAW_W) / 2;
        charY = MOVE_MAX_Y;
    }
    initStars();
    setState(CompanionState::IDLE);
    spontaneousTimer.setInterval(8000 + random(7000)); // 8-15s
}

void Companion::initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(10, SCREEN_W - 10);
        stars[i].y = random(5, GROUND_Y - 60);
        stars[i].visible = random(2) == 0;
    }
}

int Companion::currentHour() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) return 12; // default to noon
    return timeinfo.tm_hour;
}

int Companion::displayHour() {
    int sysHour = currentHour();
    float offset = (getNormX() - 0.5f) * 24.0f;
    int h = sysHour + (int)roundf(offset);
    return ((h % 24) + 24) % 24;  // wrap to [0, 23]
}

bool Companion::isNightTime() {
    int h = displayHour();
    return h >= 19 || h < 6;
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
        case CompanionState::STRETCH:
            animTimer.setInterval(400);
            break;
        case CompanionState::LOOK:
            animTimer.setInterval(300);
            break;
    }
}

void Companion::trySpontaneousAction() {
    if (state != CompanionState::IDLE) return;
    if (!spontaneousTimer.tick()) return;

    // Random action
    int action = random(100);
    if (action < 30) {
        setState(CompanionState::STRETCH);
    } else if (action < 60) {
        setState(CompanionState::LOOK);
    }
    // 40% chance: do nothing (stay idle)

    // Randomize next spontaneous timer
    spontaneousTimer.setInterval(8000 + random(7000));
}

void Companion::update(M5Canvas& canvas) {
    // Advance animation frame
    if (animTimer.tick()) {
        frameIndex++;

        switch (state) {
            case CompanionState::IDLE:
                frameIndex %= IDLE_FRAME_COUNT;
                break;
            case CompanionState::HAPPY:
                frameIndex %= HAPPY_FRAME_COUNT;
                if (millis() - stateStartTime > 1200) { // 3 cycles × 2 frames × 200ms
                    setState(CompanionState::IDLE);
                }
                break;
            case CompanionState::SLEEP:
                frameIndex %= SLEEP_FRAME_COUNT;
                break;
            case CompanionState::TALK:
                frameIndex %= TALK_FRAME_COUNT;
                break;
            case CompanionState::STRETCH:
                frameIndex %= HAPPY_FRAME_COUNT;
                if (millis() - stateStartTime > 1600) { // 2 cycles × 2 frames × 400ms
                    setState(CompanionState::IDLE);
                }
                break;
            case CompanionState::LOOK:
                frameIndex %= IDLE_FRAME_COUNT;
                if (millis() - stateStartTime > 2400) { // 2 cycles × 4 frames × 300ms
                    setState(CompanionState::IDLE);
                }
                break;
        }
    }

    // Auto-sleep after idle timeout
    if (state == CompanionState::IDLE && idleTimeout.tick()) {
        setState(CompanionState::SLEEP);
    }

    // Spontaneous actions
    trySpontaneousAction();

    // Twinkle stars (only at night)
    if (isNightTime() && starTimer.tick()) {
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
    idleTimeout.reset();

    if (state == CompanionState::SLEEP) {
        setState(CompanionState::IDLE);
        playKeyClick();
        return;
    }

    if (key == ' ' || key == '\n') {
        triggerHappy();
    } else {
        playKeyClick();
    }
}

void Companion::triggerHappy() {
    setState(CompanionState::HAPPY);
    idleTimeout.reset();
    playHappy();
}

void Companion::triggerTalk() {
    setState(CompanionState::TALK);
    idleTimeout.reset();
}

void Companion::triggerIdle() {
    setState(CompanionState::IDLE);
    idleTimeout.reset();
    playNotification();
}

void Companion::move(int dx, int dy) {
    charX += dx * MOVE_STEP;
    charY += dy * MOVE_STEP;

    // Clamp to bounds
    if (charX < MOVE_MIN_X) charX = MOVE_MIN_X;
    if (charX > MOVE_MAX_X) charX = MOVE_MAX_X;
    if (charY < MOVE_MIN_Y) charY = MOVE_MIN_Y;
    if (charY > MOVE_MAX_Y) charY = MOVE_MAX_Y;

    // Update facing direction
    if (dx < 0) facingLeft = true;
    if (dx > 0) facingLeft = false;

    // Reset idle timeout on movement
    idleTimeout.reset();

    // Wake from sleep
    if (state == CompanionState::SLEEP) {
        setState(CompanionState::IDLE);
    }
}

float Companion::getNormX() const {
    if (MOVE_MAX_X <= MOVE_MIN_X) return 0.5f;
    return (float)(charX - MOVE_MIN_X) / (MOVE_MAX_X - MOVE_MIN_X);
}

float Companion::getNormY() const {
    if (MOVE_MAX_Y <= MOVE_MIN_Y) return 0.5f;
    return (float)(charY - MOVE_MIN_Y) / (MOVE_MAX_Y - MOVE_MIN_Y);
}

// ── Sound Effects ──

void Companion::playKeyClick() {
    M5Cardputer.Speaker.tone(800, 30);
}

void Companion::playNotification() {
    M5Cardputer.Speaker.tone(1200, 80);
    delay(100);
    M5Cardputer.Speaker.tone(1600, 80);
}

void Companion::playHappy() {
    M5Cardputer.Speaker.tone(1000, 50);
    delay(60);
    M5Cardputer.Speaker.tone(1400, 50);
    delay(60);
    M5Cardputer.Speaker.tone(1800, 80);
}

// ── Drawing ──

void Companion::drawBackground(M5Canvas& canvas) {
    int h = displayHour();
    uint16_t skyColor, groundColor, groundTopColor;

    if (h >= 6 && h < 17) {
        // Daytime
        skyColor = SKY_DAY;
        groundColor = GROUND_DAY;
        groundTopColor = GROUND_DAY_TOP;
    } else if (h >= 17 && h < 19) {
        // Sunset
        skyColor = SKY_SUNSET;
        groundColor = Color::GROUND;
        groundTopColor = Color::GROUND_TOP;
    } else {
        // Night
        skyColor = SKY_NIGHT;
        groundColor = Color::GROUND;
        groundTopColor = Color::GROUND_TOP;
    }

    canvas.fillScreen(skyColor);

    // Day elements
    if (h >= 6 && h < 17) {
        drawDayElements(canvas);
    } else if (h >= 17 && h < 19) {
        // Sunset sun (low position)
        canvas.fillCircle(200, GROUND_Y - 10, 12, SUN_COLOR);
        canvas.fillCircle(200, GROUND_Y - 10, 10, rgb565(255, 160, 40));
    }

    // Night: stars + moon
    if (h >= 19 || h < 6) {
        for (int i = 0; i < MAX_STARS; i++) {
            if (stars[i].visible) {
                canvas.drawPixel(stars[i].x, stars[i].y, Color::STAR);
                if (i % 3 == 0) {
                    canvas.drawPixel(stars[i].x - 1, stars[i].y, Color::STAR);
                    canvas.drawPixel(stars[i].x + 1, stars[i].y, Color::STAR);
                    canvas.drawPixel(stars[i].x, stars[i].y - 1, Color::STAR);
                    canvas.drawPixel(stars[i].x, stars[i].y + 1, Color::STAR);
                }
            }
        }
        // Crescent moon
        canvas.fillCircle(30, 20, 10, MOON_COLOR);
        canvas.fillCircle(34, 17, 9, skyColor);  // cut out crescent
    }

    // Ground
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, groundColor);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, groundTopColor);

    for (int i = 0; i < 8; i++) {
        int gx = (i * 31 + 10) % SCREEN_W;
        canvas.drawPixel(gx, GROUND_Y + 4, groundTopColor);
        canvas.drawPixel(gx + 15, GROUND_Y + 8, groundTopColor);
    }
}

void Companion::drawDayElements(M5Canvas& canvas) {
    // Sun
    canvas.fillCircle(200, 18, 10, SUN_COLOR);
    // Sun rays
    for (int i = 0; i < 8; i++) {
        float angle = i * 0.785f; // 45 degree increments
        int x1 = 200 + cos(angle) * 13;
        int y1 = 18 + sin(angle) * 13;
        int x2 = 200 + cos(angle) * 16;
        int y2 = 18 + sin(angle) * 16;
        canvas.drawLine(x1, y1, x2, y2, SUN_COLOR);
    }

    // Clouds
    canvas.fillRoundRect(40, 12, 24, 8, 4, CLOUD_COLOR);
    canvas.fillRoundRect(48, 8, 16, 8, 4, CLOUD_COLOR);

    canvas.fillRoundRect(130, 18, 20, 6, 3, CLOUD_COLOR);
    canvas.fillRoundRect(136, 14, 14, 6, 3, CLOUD_COLOR);
}

void Companion::drawCharacter(M5Canvas& canvas) {
    const uint16_t* frame = nullptr;

    switch (state) {
        case CompanionState::IDLE:
        case CompanionState::LOOK:
            frame = idle_frames[frameIndex % IDLE_FRAME_COUNT];
            break;
        case CompanionState::HAPPY:
        case CompanionState::STRETCH:
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
        int xOffset = 0;

        // Bounce for happy
        if (state == CompanionState::HAPPY && frameIndex % 2 == 0) {
            yOffset = -6;
        }
        // Slight sway for look
        if (state == CompanionState::LOOK) {
            xOffset = (frameIndex % 2 == 0) ? -3 : 3;
        }
        // Slight stretch up
        if (state == CompanionState::STRETCH && frameIndex % 2 == 0) {
            yOffset = -3;
        }

        drawSprite16(canvas, charX + xOffset, charY + yOffset, frame, facingLeft);
    }
}

void Companion::drawSprite16(M5Canvas& canvas, int x, int y, const uint16_t* data, bool flip) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            int srcX = flip ? (CHAR_W - 1 - px) : px;
            uint16_t color = pgm_read_word(&data[py * CHAR_W + srcX]);
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

    if (phase >= 1) {
        canvas.setTextSize(1);
        canvas.drawString("z", charX + CHAR_DRAW_W + 4, charY + 10);
    }
    if (phase >= 2) {
        canvas.setTextSize(1);
        canvas.drawString("Z", charX + CHAR_DRAW_W + 10, charY);
    }
    if (phase >= 3) {
        canvas.setTextSize(2);
        canvas.drawString("Z", charX + CHAR_DRAW_W + 16, charY - 12);
    }
}

void Companion::drawStatusText(M5Canvas& canvas) {
    const char* statusStr = "";
    switch (state) {
        case CompanionState::IDLE:    statusStr = "chillin'"; break;
        case CompanionState::HAPPY:   statusStr = "yay!"; break;
        case CompanionState::SLEEP:   statusStr = "zzz..."; break;
        case CompanionState::TALK:    statusStr = "talking..."; break;
        case CompanionState::STRETCH: statusStr = "*stretch*"; break;
        case CompanionState::LOOK:    statusStr = "hmm?"; break;
    }

    canvas.setTextColor(Color::STATUS_DIM);
    canvas.setTextSize(1);
    canvas.drawString(statusStr, 4, 4);
    canvas.drawString("[TAB] chat", SCREEN_W - 60, 4);
}

// ══════════════════════════════════════════════════════════════
// Boot Animation
// ══════════════════════════════════════════════════════════════

void playBootAnimation(M5Canvas& canvas) {
    // Phase 1: Black screen → pixel lobster fades in line by line
    for (int row = 0; row < CHAR_H; row++) {
        canvas.fillScreen(Color::BLACK);

        // Draw revealed rows of the lobster (centered, large)
        int scale = 5;
        int drawW = CHAR_W * scale;
        int drawH = CHAR_H * scale;
        int ox = (SCREEN_W - drawW) / 2;
        int oy = (SCREEN_H - drawH) / 2 - 10;

        for (int py = 0; py <= row; py++) {
            for (int px = 0; px < CHAR_W; px++) {
                uint16_t color = pgm_read_word(&sprite_idle1[py * CHAR_W + px]);
                if (color != Color::TRANSPARENT) {
                    canvas.fillRect(ox + px * scale, oy + py * scale, scale, scale, color);
                }
            }
        }

        canvas.pushSprite(0, 0);
        delay(60);
    }

    // Phase 2: Hold the full logo
    delay(400);

    // Phase 3: Title text appears below
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(1);
    int textY = (SCREEN_H + CHAR_H * 5) / 2 - 2;
    const char* title = "Pixel Companion";
    int tw = canvas.textWidth(title);
    canvas.drawString(title, (SCREEN_W - tw) / 2, textY);
    canvas.pushSprite(0, 0);
    delay(800);

    // Phase 4: Fade out (darken progressively)
    for (int i = 0; i < 8; i++) {
        canvas.fillRect(0, 0, SCREEN_W, SCREEN_H, canvas.color565(0, 0, 0));
        // Overlay semi-transparent black by drawing translucent pixels
        for (int y = 0; y < SCREEN_H; y += 2) {
            for (int x = (i % 2 == 0 ? 0 : 1); x < SCREEN_W; x += 2) {
                canvas.drawPixel(x, y, Color::BLACK);
            }
        }
        canvas.pushSprite(0, 0);
        delay(60);
    }
    canvas.fillScreen(Color::BLACK);
    canvas.pushSprite(0, 0);
    delay(200);
}

// ══════════════════════════════════════════════════════════════
// Mode Transition Animation
// ══════════════════════════════════════════════════════════════

void playTransition(M5Canvas& canvas, bool toChat) {
    // Slide transition: wipe left (to chat) or right (to companion)
    int dir = toChat ? -1 : 1;

    // Capture isn't possible, so just do a quick pixel wipe
    for (int step = 0; step < 8; step++) {
        int x = step * (SCREEN_W / 8);
        if (toChat) {
            canvas.fillRect(0, 0, x + SCREEN_W / 8, SCREEN_H, Color::BLACK);
        } else {
            canvas.fillRect(SCREEN_W - x - SCREEN_W / 8, 0, x + SCREEN_W / 8, SCREEN_H, Color::BLACK);
        }
        canvas.pushSprite(0, 0);
        delay(25);
    }
}
