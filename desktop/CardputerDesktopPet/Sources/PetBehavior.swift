import AppKit

enum PetState {
    case idle
    case walk
    case happy
    case sleep
}

/// Manages pet state machine, cursor following, and animation timing
class PetBehavior {
    var state: PetState = .idle
    var currentFrame: Int = 0
    var facingLeft: Bool = false

    // Position (top-left of the pet window in screen coordinates)
    var posX: CGFloat = 400
    var posY: CGFloat = 400

    // Target position (near cursor)
    private var targetX: CGFloat = 400
    private var targetY: CGFloat = 400

    // Cursor tracking
    private var lastCursorX: CGFloat = 0
    private var lastCursorY: CGFloat = 0
    private var cursorIdleTime: TimeInterval = 0
    private var lastUpdateTime: TimeInterval = 0

    // Animation
    private var frameTimer: TimeInterval = 0
    private let frameInterval: TimeInterval = 0.5 // 500ms per frame, matching Cardputer

    // Movement
    private let followOffset: CGFloat = 80
    private let lerpFactor: CGFloat = 0.08
    private let moveThreshold: CGFloat = 2.0
    private let cursorMoveThreshold: CGFloat = 5.0

    // State transition timers
    private let idleToSleepTime: TimeInterval = 30.0
    private let walkToIdleTime: TimeInterval = 1.5 // seconds of cursor stillness → idle

    // Happy trigger
    private var cursorSpeedAccumulator: CGFloat = 0
    private let happySpeedThreshold: CGFloat = 3000
    private var happyTimer: TimeInterval = 0
    private let happyDuration: TimeInterval = 2.0

    init() {
        lastUpdateTime = ProcessInfo.processInfo.systemUptime
    }

    /// Called when mouse moves globally
    func onMouseMoved(to point: NSPoint) {
        let dx = point.x - lastCursorX
        let dy = point.y - lastCursorY
        let distance = sqrt(dx * dx + dy * dy)

        if distance > cursorMoveThreshold {
            cursorIdleTime = 0
            cursorSpeedAccumulator += distance

            // Determine target position: follow the cursor with offset
            let screen = NSScreen.main?.frame ?? NSRect(x: 0, y: 0, width: 1920, height: 1080)

            // Prefer right-below, but flip if cursor is near right/bottom edge
            var offsetX = followOffset
            var offsetY = -followOffset // Screen coords: y increases upward

            if point.x > screen.maxX - 200 {
                offsetX = -followOffset - 128 // Go to left side
            }
            if point.y < screen.minY + 200 {
                offsetY = followOffset + 128
            }

            targetX = point.x + offsetX
            targetY = point.y + offsetY

            // Clamp to screen
            targetX = max(screen.minX, min(targetX, screen.maxX - 128))
            targetY = max(screen.minY, min(targetY, screen.maxY - 128))

            // Face toward cursor
            facingLeft = point.x < posX + 64

            // Transition to walk if idle/sleep
            if state == .idle || state == .sleep {
                state = .walk
                currentFrame = 0
            }

            // Check for happy trigger (fast cursor movement)
            if cursorSpeedAccumulator > happySpeedThreshold && state != .happy {
                state = .happy
                currentFrame = 0
                happyTimer = 0
                cursorSpeedAccumulator = 0
            }
        }

        lastCursorX = point.x
        lastCursorY = point.y
    }

    /// Update loop — call at ~60fps
    func update() -> Bool {
        let now = ProcessInfo.processInfo.systemUptime
        let dt = now - lastUpdateTime
        lastUpdateTime = now
        var needsRedraw = false

        cursorIdleTime += dt
        cursorSpeedAccumulator *= 0.95 // Decay speed accumulator

        // State transitions
        switch state {
        case .walk:
            if cursorIdleTime > walkToIdleTime {
                state = .idle
                currentFrame = 0
                needsRedraw = true
            }
        case .idle:
            if cursorIdleTime > idleToSleepTime {
                state = .sleep
                currentFrame = 0
                needsRedraw = true
            }
        case .happy:
            happyTimer += dt
            if happyTimer > happyDuration {
                state = .idle
                currentFrame = 0
                needsRedraw = true
            }
        case .sleep:
            break
        }

        // Smooth position lerp
        let dx = targetX - posX
        let dy = targetY - posY
        let distance = sqrt(dx * dx + dy * dy)

        if distance > moveThreshold {
            posX += dx * lerpFactor
            posY += dy * lerpFactor
            needsRedraw = true
        }

        // Frame animation
        frameTimer += dt
        if frameTimer >= frameInterval {
            frameTimer -= frameInterval
            advanceFrame()
            needsRedraw = true
        }

        return needsRedraw
    }

    private func advanceFrame() {
        let frames = currentFrames()
        currentFrame = (currentFrame + 1) % frames.count
    }

    func currentFrames() -> [[UInt16]] {
        switch state {
        case .idle: return SpriteFrames.idle
        case .walk: return SpriteFrames.walk
        case .happy: return SpriteFrames.happy
        case .sleep: return SpriteFrames.sleep
        }
    }

    func currentSprite() -> [UInt16] {
        let frames = currentFrames()
        return frames[currentFrame % frames.count]
    }
}
