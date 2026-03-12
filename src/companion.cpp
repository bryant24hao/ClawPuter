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
    drawSimStatusBar(canvas);
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

void Companion::triggerSleep() {
    setState(CompanionState::SLEEP);
}

// ── Weather Simulation Mode ──

static const WeatherType SIM_WEATHER_TYPES[] = {
    WeatherType::CLEAR, WeatherType::PARTLY_CLOUDY, WeatherType::OVERCAST,
    WeatherType::FOG, WeatherType::DRIZZLE, WeatherType::RAIN,
    WeatherType::SNOW, WeatherType::THUNDER
};

static const char* SIM_WEATHER_NAMES[] = {
    "Clear", "Cloudy", "Overcast", "Fog",
    "Drizzle", "Rain", "Snow", "Thunder"
};

static_assert(sizeof(SIM_WEATHER_TYPES)/sizeof(SIM_WEATHER_TYPES[0]) ==
              sizeof(SIM_WEATHER_NAMES)/sizeof(SIM_WEATHER_NAMES[0]),
              "SIM_WEATHER_TYPES and SIM_WEATHER_NAMES must have same count");

void Companion::toggleWeatherSim() {
    weatherSimMode = !weatherSimMode;
    if (weatherSimMode) {
        simWeatherIndex = 0;
        simWeatherData.temperature = 25.0f;
        simWeatherData.type = WeatherType::CLEAR;
        simWeatherData.isDay = true;
        simWeatherData.valid = true;
        setWeather(simWeatherData);
    }
    weatherParticlesInit = false; // re-init particles on both enter and exit
}

void Companion::setSimWeatherType(int index) {
    if (index < 1 || index > 8) return;
    simWeatherIndex = index - 1;
    simWeatherData.type = SIM_WEATHER_TYPES[simWeatherIndex];
    simWeatherData.valid = true;
    setWeather(simWeatherData);
    weatherParticlesInit = false; // re-init particles for new weather
}

AccessoryType Companion::getAccessoryForWeather(WeatherType type) {
    switch (type) {
        case WeatherType::CLEAR:
        case WeatherType::PARTLY_CLOUDY:
            return AccessoryType::SUNGLASSES;
        case WeatherType::RAIN:
        case WeatherType::DRIZZLE:
        case WeatherType::THUNDER:
            return AccessoryType::UMBRELLA;
        case WeatherType::SNOW:
            return AccessoryType::SNOW_HAT;
        case WeatherType::FOG:
        case WeatherType::OVERCAST:
            return AccessoryType::MASK;
        default:
            return AccessoryType::NONE;
    }
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

// Blend two RGB565 colors: result = a * (1-t) + b * t, t in [0..255]
static uint16_t blendRGB565(uint16_t a, uint16_t b, uint8_t t) {
    uint8_t r1 = (a >> 11) & 0x1F, g1 = (a >> 5) & 0x3F, b1 = a & 0x1F;
    uint8_t r2 = (b >> 11) & 0x1F, g2 = (b >> 5) & 0x3F, b2 = b & 0x1F;
    uint8_t r = r1 + ((int)(r2 - r1) * t / 255);
    uint8_t g = g1 + ((int)(g2 - g1) * t / 255);
    uint8_t bl = b1 + ((int)(b2 - b1) * t / 255);
    return (r << 11) | (g << 5) | bl;
}

void Companion::drawBackground(M5Canvas& canvas) {
    int h = displayHour();
    uint16_t skyColor, groundColor, groundTopColor;

    if (h >= 6 && h < 17) {
        skyColor = SKY_DAY;
        groundColor = GROUND_DAY;
        groundTopColor = GROUND_DAY_TOP;
    } else if (h >= 17 && h < 19) {
        skyColor = SKY_SUNSET;
        groundColor = Color::GROUND;
        groundTopColor = Color::GROUND_TOP;
    } else {
        skyColor = SKY_NIGHT;
        groundColor = Color::GROUND;
        groundTopColor = Color::GROUND_TOP;
    }

    // Weather sky tinting
    bool hideSun = false;
    if (weather.valid) {
        switch (weather.type) {
            case WeatherType::OVERCAST:
                skyColor = blendRGB565(skyColor, rgb565(100, 100, 110), 160);
                hideSun = true;
                break;
            case WeatherType::RAIN:
            case WeatherType::THUNDER:
                skyColor = blendRGB565(skyColor, rgb565(60, 60, 75), 180);
                hideSun = true;
                break;
            case WeatherType::DRIZZLE:
                skyColor = blendRGB565(skyColor, rgb565(90, 90, 105), 140);
                hideSun = true;
                break;
            case WeatherType::SNOW:
                skyColor = blendRGB565(skyColor, rgb565(120, 120, 135), 150);
                hideSun = true;
                break;
            case WeatherType::FOG:
                skyColor = blendRGB565(skyColor, rgb565(140, 140, 145), 170);
                hideSun = true;
                break;
            default:
                break;
        }
    }

    canvas.fillScreen(skyColor);

    // Day elements (hide sun during heavy weather)
    if (h >= 6 && h < 17) {
        if (!hideSun) {
            drawDayElements(canvas);
        } else {
            // Still draw clouds (darker) but no sun
            uint16_t darkCloud = rgb565(150, 150, 160);
            canvas.fillRoundRect(30, 10, 30, 10, 5, darkCloud);
            canvas.fillRoundRect(40, 5, 20, 10, 5, darkCloud);
            canvas.fillRoundRect(120, 15, 28, 9, 4, darkCloud);
            canvas.fillRoundRect(128, 9, 18, 9, 4, darkCloud);
            canvas.fillRoundRect(180, 12, 26, 8, 4, darkCloud);
        }
    } else if (h >= 17 && h < 19) {
        if (!hideSun) {
            canvas.fillCircle(200, GROUND_Y - 10, 12, SUN_COLOR);
            canvas.fillCircle(200, GROUND_Y - 10, 10, rgb565(255, 160, 40));
        }
    }

    // Night: stars + moon (hide in heavy weather)
    if ((h >= 19 || h < 6) && !hideSun) {
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
        canvas.fillCircle(30, 20, 10, MOON_COLOR);
        canvas.fillCircle(34, 17, 9, skyColor);
    }

    // Weather particle effects (rain, snow, fog, thunder flash)
    drawWeatherEffects(canvas);

    // Ground
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, groundColor);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, groundTopColor);

    for (int i = 0; i < 8; i++) {
        int gx = (i * 31 + 10) % SCREEN_W;
        canvas.drawPixel(gx, GROUND_Y + 4, groundTopColor);
        canvas.drawPixel(gx + 15, GROUND_Y + 8, groundTopColor);
    }
}

void Companion::initWeatherParticles() {
    for (int i = 0; i < MAX_RAIN; i++) {
        rainDrops[i].x = random(SCREEN_W);
        rainDrops[i].y = random(GROUND_Y);
    }
    for (int i = 0; i < MAX_SNOW; i++) {
        snowflakes[i].x = random(SCREEN_W);
        snowflakes[i].y = random(GROUND_Y);
        snowflakes[i].drift = random(3) - 1; // -1, 0, or 1
    }
    weatherParticlesInit = true;
}

void Companion::drawWeatherEffects(M5Canvas& canvas) {
    if (!weather.valid) return;
    if (!weatherParticlesInit) initWeatherParticles();

    switch (weather.type) {
        case WeatherType::RAIN:
        case WeatherType::DRIZZLE:
        case WeatherType::THUNDER: {
            // Rain drops — vertical lines falling down
            int count = (weather.type == WeatherType::DRIZZLE) ? 8 : MAX_RAIN;
            int speed = (weather.type == WeatherType::DRIZZLE) ? 3 : 5;
            int len = (weather.type == WeatherType::DRIZZLE) ? 3 : 5;
            uint16_t rainColor = rgb565(140, 160, 200);

            for (int i = 0; i < count; i++) {
                rainDrops[i].y += speed;
                if (rainDrops[i].y >= GROUND_Y) {
                    rainDrops[i].y = random(-10, 0);
                    rainDrops[i].x = random(SCREEN_W);
                }
                if (rainDrops[i].y >= 0) {
                    int endY = rainDrops[i].y + len;
                    if (endY > GROUND_Y) endY = GROUND_Y;
                    canvas.drawFastVLine(rainDrops[i].x, rainDrops[i].y, endY - rainDrops[i].y, rainColor);
                }
            }

            // Thunder: flash every 3-5 seconds
            if (weather.type == WeatherType::THUNDER) {
                unsigned long now = millis();
                if (!thunderFlashing && now - lastThunderFlash > 3000 + random(2000)) {
                    thunderFlashing = true;
                    lastThunderFlash = now;
                }
                if (thunderFlashing) {
                    // Flash white for ~50ms (3 frames at 60fps)
                    if (now - lastThunderFlash < 50) {
                        canvas.fillScreen(rgb565(200, 200, 220));
                        // Redraw rain on top of flash
                        for (int i = 0; i < count; i++) {
                            if (rainDrops[i].y >= 0) {
                                int endY = rainDrops[i].y + len;
                                if (endY > GROUND_Y) endY = GROUND_Y;
                                canvas.drawFastVLine(rainDrops[i].x, rainDrops[i].y, endY - rainDrops[i].y, rainColor);
                            }
                        }
                    } else {
                        thunderFlashing = false;
                    }
                }
            }
            break;
        }

        case WeatherType::SNOW: {
            uint16_t snowColor = rgb565(220, 220, 230);
            for (int i = 0; i < MAX_SNOW; i++) {
                snowflakes[i].y += 1;  // Slow fall
                snowflakes[i].x += snowflakes[i].drift;
                // Re-randomize drift occasionally
                if (random(20) == 0) snowflakes[i].drift = random(3) - 1;

                if (snowflakes[i].y >= GROUND_Y) {
                    snowflakes[i].y = random(-5, 0);
                    snowflakes[i].x = random(SCREEN_W);
                }
                // Wrap X
                if (snowflakes[i].x < 0) snowflakes[i].x = SCREEN_W - 1;
                if (snowflakes[i].x >= SCREEN_W) snowflakes[i].x = 0;

                if (snowflakes[i].y >= 0) {
                    canvas.drawPixel(snowflakes[i].x, snowflakes[i].y, snowColor);
                    // Larger flakes for every 3rd
                    if (i % 3 == 0) {
                        canvas.drawPixel(snowflakes[i].x + 1, snowflakes[i].y, snowColor);
                        canvas.drawPixel(snowflakes[i].x, snowflakes[i].y + 1, snowColor);
                    }
                }
            }
            break;
        }

        case WeatherType::FOG: {
            // Semi-transparent fog: scatter gray dots across sky
            uint16_t fogColor = rgb565(160, 160, 165);
            // Deterministic pattern based on frame for slight shimmer
            int offset = (millis() / 200) % 3;
            for (int y = 20 + offset; y < GROUND_Y; y += 4) {
                for (int x = (y % 6); x < SCREEN_W; x += 6) {
                    canvas.drawPixel(x, y, fogColor);
                }
            }
            break;
        }

        default:
            break;
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
        drawAccessory(canvas, charX + xOffset, charY + yOffset);
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

    // Layout: center "HH:MM  0°" as a whole if weather valid
    char tempStr[8] = {};
    int tempW = 0;
    if (weather.valid) {
        int tempInt = (int)roundf(weather.temperature);
        if (tempInt < -99) tempInt = -99;
        if (tempInt > 99) tempInt = 99;
        snprintf(tempStr, sizeof(tempStr), "%d~", tempInt); // ~ as degree placeholder
        tempW = canvas.textWidth(tempStr);
    }

    int tw = canvas.textWidth(timeStr);
    int sep = weather.valid ? 10 : 0; // space before separator
    int sepW = weather.valid ? 1 : 0; // separator line width
    int sep2 = weather.valid ? 10 : 0; // space after separator
    int totalW = tw + sep + sepW + sep2 + tempW;
    int startX = (SCREEN_W - totalW) / 2;

    canvas.drawString(timeStr, startX, GROUND_Y + 6);

    if (weather.valid) {
        // Draw separator line
        int sepX = startX + tw + sep;
        canvas.drawFastVLine(sepX, GROUND_Y + 8, 12, Color::STATUS_DIM);

        // Draw temperature number
        char numStr[8];
        int tempInt = (int)roundf(weather.temperature);
        if (tempInt < -99) tempInt = -99;
        if (tempInt > 99) tempInt = 99;
        snprintf(numStr, sizeof(numStr), "%d", tempInt);
        int tempX = sepX + sepW + sep2;
        canvas.drawString(numStr, tempX, GROUND_Y + 6);
        // Draw small ° circle instead of font glyph
        int numW = canvas.textWidth(numStr);
        canvas.drawCircle(tempX + numW + 3, GROUND_Y + 8, 2, Color::CLOCK_TEXT);
    }
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

// ── Accessories ──

void Companion::drawAccessory(M5Canvas& canvas, int x, int y) {
    if (!weather.valid) return;
    AccessoryType acc = getAccessoryForWeather(weather.type);
    if (acc == AccessoryType::NONE) return;

    bool flip = facingLeft;

    switch (acc) {
        case AccessoryType::SUNGLASSES: {
            uint16_t glassColor = rgb565(20, 20, 40);
            if (!flip) {
                canvas.fillRect(x + 12, y + 15, 9, 3, glassColor);  // left lens
                canvas.fillRect(x + 27, y + 15, 9, 3, glassColor);  // right lens
                canvas.drawFastHLine(x + 21, y + 16, 6, glassColor); // bridge
            } else {
                canvas.fillRect(x + CHAR_DRAW_W - 21, y + 15, 9, 3, glassColor);
                canvas.fillRect(x + CHAR_DRAW_W - 36, y + 15, 9, 3, glassColor);
                canvas.drawFastHLine(x + CHAR_DRAW_W - 27, y + 16, 6, glassColor);
            }
            break;
        }
        case AccessoryType::UMBRELLA: {
            uint16_t umbColor = rgb565(60, 60, 200);
            uint16_t handleColor = rgb565(120, 80, 40);
            // Umbrella canopy above head
            canvas.fillRoundRect(x + 6, y - 10, 36, 8, 4, umbColor);
            // Handle
            canvas.drawFastVLine(x + 24, y - 2, 8, handleColor);
            break;
        }
        case AccessoryType::SNOW_HAT: {
            uint16_t hatColor = rgb565(200, 60, 60);
            canvas.fillRoundRect(x + 9, y + 3, 30, 6, 3, hatColor);
            // Pompom
            canvas.fillCircle(x + 24, y + 2, 3, 0xFFFF); // white
            break;
        }
        case AccessoryType::MASK: {
            uint16_t maskColor = rgb565(180, 200, 180);
            uint16_t strapColor = rgb565(120, 120, 120);
            if (!flip) {
                canvas.fillRect(x + 12, y + 21, 24, 6, maskColor);
                canvas.drawFastHLine(x + 9, y + 23, 3, strapColor);
                canvas.drawFastHLine(x + 36, y + 23, 3, strapColor);
            } else {
                canvas.fillRect(x + CHAR_DRAW_W - 36, y + 21, 24, 6, maskColor);
                canvas.drawFastHLine(x + CHAR_DRAW_W - 12, y + 23, 3, strapColor);
                canvas.drawFastHLine(x + CHAR_DRAW_W - 39, y + 23, 3, strapColor);
            }
            break;
        }
        default:
            break;
    }
}

void Companion::drawSimStatusBar(M5Canvas& canvas) {
    if (!weatherSimMode) return;

    // Semi-transparent black bar at bottom
    int barY = SCREEN_H - 12;
    canvas.fillRect(0, barY, SCREEN_W, 12, rgb565(20, 20, 20));

    canvas.setTextColor(Color::WHITE);
    canvas.setTextSize(1);

    char label[32];
    snprintf(label, sizeof(label), "[SIM] %s (%d)", SIM_WEATHER_NAMES[simWeatherIndex], simWeatherIndex + 1);
    int tw = canvas.textWidth(label);
    canvas.drawString(label, (SCREEN_W - tw) / 2, barY + 2);
}

// ── Notification Toast ──

void Companion::showNotification(const char* app, const char* title, const char* body) {
    strncpy(notifyApp, app, sizeof(notifyApp) - 1);
    notifyApp[sizeof(notifyApp) - 1] = '\0';
    strncpy(notifyTitle, title, sizeof(notifyTitle) - 1);
    notifyTitle[sizeof(notifyTitle) - 1] = '\0';
    strncpy(notifyBody, body, sizeof(notifyBody) - 1);
    notifyBody[sizeof(notifyBody) - 1] = '\0';
    notificationActive = true;
    notificationStartTime = millis();
    playNotification();
}

void Companion::drawNotificationOverlay(M5Canvas& canvas) {
    if (!notificationActive) return;
    if (millis() - notificationStartTime > NOTIFICATION_DURATION) {
        notificationActive = false;
        return;
    }

    // Dark semi-transparent bar at top
    int barH = 28;
    canvas.fillRect(0, 0, SCREEN_W, barH, rgb565(30, 30, 40));
    canvas.drawFastHLine(0, barH, SCREEN_W, Color::STATUS_DIM);

    canvas.setTextSize(1);

    // App name + title on first line
    char line1[80];
    if (notifyApp[0]) {
        snprintf(line1, sizeof(line1), "[%s] %s", notifyApp, notifyTitle);
    } else {
        snprintf(line1, sizeof(line1), "%s", notifyTitle);
    }
    canvas.setTextColor(Color::WHITE);
    canvas.drawString(line1, 4, 2);

    // Body on second line
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.drawString(notifyBody, 4, 15);
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
    const char* title = "ClawPuter";
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
