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

    void setWeather(const WeatherData& wd) { weather = wd; }

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

    // Draw a sprite with transparency (flip=true for horizontal mirror)
    void drawSprite16(M5Canvas& canvas, int x, int y, const uint16_t* data, bool flip = false);
};

// Boot animation (called from main.cpp)
void playBootAnimation(M5Canvas& canvas);

// Mode transition animation
void playTransition(M5Canvas& canvas, bool toChat);
