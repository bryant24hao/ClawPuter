import AppKit

enum PetState {
    case idle
    case walk
    case happy
    case sleep
    case talk
    case stretch
    case look
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

    // ── Sync mode ──
    private(set) var syncMode: Bool = false
    private var lastSyncTime: TimeInterval = 0
    private let syncTimeout: TimeInterval = 3.0 // fall back to independent mode after 3s
    private var syncDirty: Bool = false // set by applySync, consumed by update
    private var lastSyncNormX: Float = -1
    private var lastSyncNormY: Float = -1

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

            // In sync mode, don't override state from cursor movement
            if !syncMode {
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
        }

        lastCursorX = point.x
        lastCursorY = point.y
    }

    /// Apply state from ESP32 UDP broadcast
    func applySync(state espState: Int, frame: Int,
                   normX: Float? = nil, normY: Float? = nil, direction: Int? = nil) {
        lastSyncTime = ProcessInfo.processInfo.systemUptime
        syncMode = true

        // Map ESP32 CompanionState enum to PetState
        // 0=IDLE, 1=HAPPY, 2=SLEEP, 3=TALK, 4=STRETCH, 5=LOOK
        switch espState {
        case 0: state = .idle
        case 1: state = .happy
        case 2: state = .sleep
        case 3: state = .talk
        case 4: state = .stretch
        case 5: state = .look
        default: state = .idle
        }

        // Use the firmware's frame index directly
        let frames = currentFrames()
        currentFrame = frame % frames.count

        // Sync position when hardware position changes
        if let nx = normX, let ny = normY {
            let posChanged = abs(nx - lastSyncNormX) > 0.005 || abs(ny - lastSyncNormY) > 0.005
            if posChanged {
                lastSyncNormX = nx
                lastSyncNormY = ny
                if let screen = NSScreen.main?.frame {
                    let petSize: CGFloat = 128
                    targetX = screen.minX + CGFloat(max(0, min(1, nx))) * (screen.width - petSize)
                    targetY = screen.minY + CGFloat(1.0 - max(0, min(1, ny))) * (screen.height - petSize)
                }
            }
        }

        // Sync facing direction
        if let d = direction {
            facingLeft = (d != 0)
        }

        syncDirty = true
    }

    /// Update loop — call at ~60fps
    func update() -> Bool {
        let now = ProcessInfo.processInfo.systemUptime
        let dt = now - lastUpdateTime
        lastUpdateTime = now
        var needsRedraw = syncDirty
        syncDirty = false

        cursorIdleTime += dt
        cursorSpeedAccumulator *= 0.95 // Decay speed accumulator

        // Check sync timeout
        if syncMode && (now - lastSyncTime > syncTimeout) {
            syncMode = false
            state = .idle
            currentFrame = 0
            needsRedraw = true
        }

        // State transitions (only in independent mode)
        if !syncMode {
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
            case .sleep, .talk, .stretch, .look:
                break
            }
        }

        // Smooth position lerp (always active, even in sync mode)
        let dx = targetX - posX
        let dy = targetY - posY
        let distance = sqrt(dx * dx + dy * dy)

        if distance > moveThreshold {
            posX += dx * lerpFactor
            posY += dy * lerpFactor
            needsRedraw = true
        }

        // Frame animation (only in independent mode — sync mode sets frame directly)
        if !syncMode {
            frameTimer += dt
            if frameTimer >= frameInterval {
                frameTimer -= frameInterval
                advanceFrame()
                needsRedraw = true
            }
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
        case .talk: return SpriteFrames.talk
        case .stretch: return SpriteFrames.stretch
        case .look: return SpriteFrames.look
        }
    }

    func currentSprite() -> [UInt16] {
        let frames = currentFrames()
        return frames[currentFrame % frames.count]
    }
}
