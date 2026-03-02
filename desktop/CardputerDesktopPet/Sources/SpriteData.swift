import Foundation

// RGB565 color conversion matching Cardputer's utils.h
func rgb565(_ r: UInt8, _ g: UInt8, _ b: UInt8) -> UInt16 {
    (UInt16(r & 0xF8) << 8) | (UInt16(g & 0xFC) << 3) | UInt16(b >> 3)
}

// Color aliases — OpenClaw lobster palette (matching sprites.h)
// Swift reserves `_`, so we use `T_` for transparent
private let T_: UInt16 = rgb565(255, 0, 255) // Transparent (magenta)
private let K: UInt16 = 0x0000               // Black (outline)
private let W: UInt16 = 0xFFFF               // White (eyes)
private let R: UInt16 = rgb565(210, 50, 40)  // Red (main body)
private let D: UInt16 = rgb565(160, 30, 25)  // Dark red (shadow/belly)
private let H: UInt16 = rgb565(240, 100, 80) // Highlight red (light)
private let O: UInt16 = rgb565(230, 140, 60) // Orange (belly/claws inner)
private let E: UInt16 = rgb565(20, 20, 20)   // Eye pupil
private let C: UInt16 = rgb565(190, 40, 35)  // Claw red
private let T: UInt16 = rgb565(180, 60, 50)  // Tail/legs

let SPRITE_W = 16
let SPRITE_H = 16
let TRANSPARENT_COLOR: UInt16 = rgb565(255, 0, 255) // Magenta = transparent

// Using T_ for transparent since _ is a keyword in Swift
// Each sprite is 16x16 = 256 pixels

// ── Idle frame 1: eyes open, claws down ──
let sprite_idle1: [UInt16] = [
    T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,
    T_,T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,T_,
    T_,T_,T_,T_,K, K, K, K, K, K, K, K, T_,T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,K, R, R, W, W, R, R, W, W, R, R, K, T_,T_,
    T_,T_,K, R, R, W, E, R, R, W, E, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, R, O, O, R, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,K, K, T_,K, R, R, R, R, R, R, K, T_,K, K, T_,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    T_,K, K, T_,T_,K, D, D, D, D, K, T_,T_,K, K, T_,
    T_,T_,T_,T_,K, T, K, D, D, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,K, T, K, T, T, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, T_,K, K, T_,K, T_,T_,T_,T_,T_,
]

// ── Idle frame 2: blink ──
let sprite_idle2: [UInt16] = [
    T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,
    T_,T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,T_,
    T_,T_,T_,T_,K, K, K, K, K, K, K, K, T_,T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,K, R, R, K, K, R, R, K, K, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, R, O, O, R, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,K, K, T_,K, R, R, R, R, R, R, K, T_,K, K, T_,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    T_,K, K, T_,T_,K, D, D, D, D, K, T_,T_,K, K, T_,
    T_,T_,T_,T_,K, T, K, D, D, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,K, T, K, T, T, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, T_,K, K, T_,K, T_,T_,T_,T_,T_,
]

// ── Idle frame 3: body bob (1px down) ──
let sprite_idle3: [UInt16] = [
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
    T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,
    T_,T_,T_,K, T_,T_,T_,T_,T_,T_,T_,T_,K, T_,T_,T_,
    T_,T_,T_,T_,K, K, K, K, K, K, K, K, T_,T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,K, R, R, W, W, R, R, W, W, R, R, K, T_,T_,
    T_,T_,K, R, R, W, E, R, R, W, E, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, R, O, O, R, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,K, K, T_,K, R, R, R, R, R, R, K, T_,K, K, T_,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    T_,K, K, T_,T_,K, D, D, D, D, K, T_,T_,K, K, T_,
    T_,T_,T_,T_,K, T, K, D, D, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, T_,K, K, T_,K, T_,T_,T_,T_,T_,
]

// ── Happy frame 1: claws up! ──
let sprite_happy1: [UInt16] = [
    K, C, C, K, T_,T_,T_,T_,T_,T_,T_,T_,K, C, C, K,
    K, C, C, K, T_,T_,T_,T_,T_,T_,T_,T_,K, C, C, K,
    T_,K, K, T_,K, K, K, K, K, K, K, K, T_,K, K, T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,K, R, R, H, H, R, R, H, H, R, R, K, T_,T_,
    T_,T_,K, R, R, H, E, R, R, H, E, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, O, K, K, O, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,T_,T_,K, R, R, R, R, R, R, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, D, D, D, D, K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, D, D, D, D, K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, D, D, D, D, K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,T_,K, K, K, K, T_,T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
]

// ── Happy frame 2: claws spread wide ──
let sprite_happy2: [UInt16] = [
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
    K, C, K, T_,K, K, K, K, K, K, K, K, T_,K, C, K,
    K, C, C, K, R, R, R, R, R, R, R, R, K, C, C, K,
    T_,K, K, K, R, R, R, R, R, R, R, R, K, K, K, T_,
    T_,T_,K, R, R, H, H, R, R, H, H, R, R, K, T_,T_,
    T_,T_,K, R, R, H, E, R, R, H, E, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, O, K, K, O, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,T_,T_,K, R, R, R, R, R, R, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, D, D, D, D, K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, D, D, D, D, K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,K, T, K, D, D, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,K, T, K, T, T, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, T_,K, K, T_,K, T_,T_,T_,T_,T_,
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
]

// ── Sleep frame 1: eyes closed, claws tucked ──
let sprite_sleep1: [UInt16] = [
    T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,T_,
    T_,T_,T_,T_,K, K, K, K, K, K, K, K, T_,T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,K, R, R, K, K, R, R, K, K, R, R, K, T_,T_,
    T_,T_,K, R, R, R, R, R, R, R, R, R, R, K, T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,T_,T_,K, R, R, R, R, R, R, R, R, K, T_,T_,T_,
    T_,K, K, T_,K, R, R, R, R, R, R, K, T_,K, K, T_,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    K, C, C, K, T_,K, D, D, D, D, K, T_,K, C, C, K,
    T_,K, K, T_,T_,K, D, D, D, D, K, T_,T_,K, K, T_,
    T_,T_,T_,T_,K, T, K, D, D, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,K, T, K, T, T, K, T, K, T_,T_,T_,T_,
    T_,T_,T_,T_,T_,K, T_,K, K, T_,K, T_,T_,T_,T_,T_,
]

// Frame lookup arrays (matching Cardputer firmware)
struct SpriteFrames {
    static let idle: [[UInt16]] = [sprite_idle1, sprite_idle2, sprite_idle1, sprite_idle3]
    static let happy: [[UInt16]] = [sprite_happy1, sprite_happy2]
    static let sleep: [[UInt16]] = [sprite_sleep1]
    // Walk reuses idle frames
    static let walk: [[UInt16]] = [sprite_idle1, sprite_idle3]
}
