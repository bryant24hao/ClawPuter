#pragma once
#include <M5Cardputer.h>
#include "utils.h"

enum class CompanionState {
    IDLE,
    HAPPY,
    SLEEP,
    TALK
};

class Companion {
public:
    void begin(M5Canvas& canvas);
    void update(M5Canvas& canvas);
    void handleKey(char key);

    // External triggers
    void triggerHappy();
    void triggerTalk();
    void triggerIdle();

    CompanionState getState() const { return state; }

private:
    CompanionState state = CompanionState::IDLE;
    int frameIndex = 0;
    Timer animTimer{500};
    Timer idleTimeout{30000};  // 30s → sleep
    Timer clockTimer{1000};
    unsigned long stateStartTime = 0;

    // Star twinkling
    struct Star { int x, y; bool visible; };
    static constexpr int MAX_STARS = 12;
    Star stars[MAX_STARS];
    Timer starTimer{800};

    void drawBackground(M5Canvas& canvas);
    void drawCharacter(M5Canvas& canvas);
    void drawClock(M5Canvas& canvas);
    void drawSleepZ(M5Canvas& canvas);
    void drawStatusText(M5Canvas& canvas);

    void setState(CompanionState newState);
    void initStars();

    // Draw a sprite with transparency
    void drawSprite16(M5Canvas& canvas, int x, int y, const uint16_t* data);
};
