#pragma once
#include <M5Cardputer.h>
#include "utils.h"
#include "weather_client.h"

enum class CompanionState {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,   // spontaneous stretch
    LOOK       // spontaneous look around
};

enum class AccessoryType : uint8_t {
    NONE, SUNGLASSES, UMBRELLA, SNOW_HAT, MASK
};

class Companion {
public:
    void begin(M5Canvas& canvas);
    void update(M5Canvas& canvas);
    void handleKey(char key);

    void move(int dx, int dy);  // dx/dy: -1, 0, +1

    // External triggers
    void triggerHappy();
    void triggerTalk();
    void triggerIdle();
    void triggerSleep();

    void setWeather(const WeatherData& wd) { weather = wd; }

    // Moisture system
    void spray();
    int getMoistureLevel() const { return moistureLevel; }
    void debugSetMoisture(int level) { moistureLevel = max(0, min(4, level)); Serial.printf("[MOISTURE] Debug set -> %d\n", moistureLevel); }
    int getHumidityPercent() const { return weather.humidity; }

    // Weather simulation mode
    void toggleWeatherSim();
    void setSimWeatherType(int index); // 1-8
    bool isWeatherSimMode() const { return weatherSimMode; }

    CompanionState getState() const { return state; }
    WeatherType getWeatherType() const { return weather.type; }
    float getTemperature() const { return weather.temperature; }
    bool hasValidWeather() const { return weather.valid; }
    int getFrameIndex() const { return frameIndex; }
    float getNormX() const;
    float getNormY() const;
    bool isFacingLeft() const { return facingLeft; }

    // Notification toast overlay
    void showNotification(const char* app, const char* title, const char* body);
    void drawNotificationOverlay(M5Canvas& canvas);
    bool hasActiveNotification() const { return notificationActive; }

    // Sound effects
    static void playKeyClick();
    static void playNotification();
    static void playHappy();

private:
    CompanionState state = CompanionState::IDLE;
    int frameIndex = 0;
    int charX = 0, charY = 0;  // pixel position (initialized in begin())
    bool facingLeft = false;
    Timer animTimer{500};
    Timer idleTimeout{30000};  // 30s → sleep
    Timer clockTimer{1000};
    Timer spontaneousTimer{8000};  // random actions every 8-15s
    unsigned long stateStartTime = 0;

    // Star twinkling
    struct Star { int x, y; bool visible; };
    static constexpr int MAX_STARS = 12;
    Star stars[MAX_STARS];
    Timer starTimer{800};

    // Day/night
    bool isNightTime();
    int currentHour();
    int displayHour();  // time-travel: hour adjusted by pet X position

    void drawBackground(M5Canvas& canvas);
    void drawWeatherEffects(M5Canvas& canvas);
    void drawCharacter(M5Canvas& canvas);
    void drawClock(M5Canvas& canvas);
    void drawSleepZ(M5Canvas& canvas);
    void drawStatusText(M5Canvas& canvas);
    void drawDayElements(M5Canvas& canvas);
    void drawAccessory(M5Canvas& canvas, int charDrawX, int charDrawY);
    void drawSimStatusBar(M5Canvas& canvas);
    static AccessoryType getAccessoryForWeather(WeatherType type);

    void setState(CompanionState newState);
    void initStars();
    void trySpontaneousAction();

    // Weather simulation
    bool weatherSimMode = false;
    int simWeatherIndex = 0;  // 0-7 for 8 weather types
    WeatherData simWeatherData;

    // Weather state
    WeatherData weather;
    struct RainDrop { int16_t x, y; };
    static constexpr int MAX_RAIN = 15;
    RainDrop rainDrops[MAX_RAIN];
    struct Snowflake { int16_t x, y; int8_t drift; };
    static constexpr int MAX_SNOW = 15;
    Snowflake snowflakes[MAX_SNOW];
    bool weatherParticlesInit = false;
    unsigned long lastThunderFlash = 0;
    bool thunderFlashing = false;

    void initWeatherParticles();

    // Moisture system
    int moistureLevel = 2;              // 0-4, start mid-low for engagement
    bool moistureInitialized = false;   // set initial based on first weather
    Timer moistureDecayTimer{1200000};  // 20min default
    Timer moistureRecoverTimer{900000}; // 15min rain auto-recover
    unsigned long lastSprayTime = 0;
    static constexpr unsigned long SPRAY_COOLDOWN = 3000;
    int moveStepCount = 0;              // movement drains moisture
    static constexpr int STEPS_PER_DRAIN = 20;  // every 20 steps = -1 moisture

    struct SprayParticle { int16_t x, y; int8_t vx, vy; uint8_t life; };
    static constexpr int MAX_SPRAY = 5;
    SprayParticle sprayParticles[MAX_SPRAY];
    bool sprayActive = false;

    void updateMoisture();
    void playSpraySound();
    void drawMoistureDrops(M5Canvas& canvas, int startX, int y);
    void drawSprayParticles(M5Canvas& canvas);

    // Draw a sprite with transparency (flip=true for horizontal mirror)
    void drawSprite16(M5Canvas& canvas, int x, int y, const uint16_t* data, bool flip = false);

    // Notification overlay state
    bool notificationActive = false;
    unsigned long notificationStartTime = 0;
    static constexpr unsigned long NOTIFICATION_DURATION = 3000;  // 3 seconds
    char notifyApp[32];
    char notifyTitle[48];
    char notifyBody[64];
};

// Boot animation (called from main.cpp)
void playBootAnimation(M5Canvas& canvas);

// Mode transition animation
void playTransition(M5Canvas& canvas, bool toChat);
