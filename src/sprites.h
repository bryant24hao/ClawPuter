#pragma once
#include <cstdint>
#include "utils.h"

// Sprite dimensions
constexpr int CHAR_W = 16;
constexpr int CHAR_H = 16;

// Shorthand color aliases for sprite data
constexpr uint16_t _ = Color::TRANSPARENT;  // Transparent
constexpr uint16_t K = Color::BLACK;        // Black (outline)
constexpr uint16_t W = Color::WHITE;        // White (eyes)
constexpr uint16_t S = rgb565(140, 100, 180); // Skin/body purple
constexpr uint16_t D = rgb565(100, 70, 140);  // Dark body
constexpr uint16_t H = rgb565(180, 130, 220); // Highlight
constexpr uint16_t E = rgb565(60, 40, 80);    // Eye pupil
constexpr uint16_t R = rgb565(220, 100, 100); // Red (cheeks/blush)
constexpr uint16_t B = rgb565(80, 60, 120);   // Boot/feet

// ── Idle frame 1: eyes open ──
const uint16_t PROGMEM sprite_idle1[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, W, W, S, S, W, W, S, S, K, _, _,
    _, _, K, S, S, W, E, S, S, W, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, R, S, S, R, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Idle frame 2: eyes half-closed (blink) ──
const uint16_t PROGMEM sprite_idle2[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, K, S, S, K, K, S, S, K, K, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, R, S, S, R, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, K, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Idle frame 3: slight body bob ──
const uint16_t PROGMEM sprite_idle3[CHAR_W * CHAR_H] = {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, W, W, S, S, W, W, S, S, K, _, _,
    _, _, K, S, S, W, E, S, S, W, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, R, S, S, R, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Happy frame 1: jump up ──
const uint16_t PROGMEM sprite_happy1[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, K, S, S, W, W, S, S, W, W, S, S, K, _, _,
    _, _, K, S, S, W, E, S, S, W, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, R, S, K, K, S, R, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, K, K, K, D, D, D, D, D, D, K, K, K, _, _,
    _, _, _, _, _, _, K, K, K, K, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
};

// ── Happy frame 2: arms up ──
const uint16_t PROGMEM sprite_happy2[CHAR_W * CHAR_H] = {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, H, H, S, S, H, H, S, S, K, _, _,
    _, _, K, S, S, H, E, S, S, H, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, R, S, K, K, S, R, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, K, K, K, D, D, D, D, D, D, K, K, K, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Sleep frame 1 ──
const uint16_t PROGMEM sprite_sleep1[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, K, S, S, K, K, S, S, K, K, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Talk frame 1: mouth open ──
const uint16_t PROGMEM sprite_talk1[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, W, W, S, S, W, W, S, S, K, _, _,
    _, _, K, S, S, W, E, S, S, W, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, S, K, K, S, S, S, K, _, _, _,
    _, _, _, K, S, S, K, K, K, K, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// ── Talk frame 2: mouth closed ──
const uint16_t PROGMEM sprite_talk2[CHAR_W * CHAR_H] = {
    _, _, _, _, _, K, K, K, K, K, K, _, _, _, _, _,
    _, _, _, _, K, S, S, S, S, S, S, K, _, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, K, S, S, W, W, S, S, W, W, S, S, K, _, _,
    _, _, K, S, S, W, E, S, S, W, E, S, S, K, _, _,
    _, _, K, S, S, S, S, S, S, S, S, S, S, K, _, _,
    _, _, _, K, S, S, K, K, K, K, S, S, K, _, _, _,
    _, _, _, K, S, S, S, S, S, S, S, S, K, _, _, _,
    _, _, _, _, K, K, S, S, S, S, K, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, D, D, D, D, K, _, _, _, _,
    _, _, _, _, K, D, D, K, K, D, D, K, _, _, _, _,
    _, _, _, _, K, B, B, K, K, B, B, K, _, _, _, _,
    _, _, _, _, _, K, K, _, _, K, K, _, _, _, _, _,
};

// Sprite lookup arrays
const uint16_t* const idle_frames[] = { sprite_idle1, sprite_idle2, sprite_idle1, sprite_idle3 };
constexpr int IDLE_FRAME_COUNT = 4;

const uint16_t* const happy_frames[] = { sprite_happy1, sprite_happy2 };
constexpr int HAPPY_FRAME_COUNT = 2;

const uint16_t* const sleep_frames[] = { sprite_sleep1 };
constexpr int SLEEP_FRAME_COUNT = 1;

const uint16_t* const talk_frames[] = { sprite_talk1, sprite_talk2 };
constexpr int TALK_FRAME_COUNT = 2;
