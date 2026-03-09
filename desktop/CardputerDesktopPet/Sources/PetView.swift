import AppKit

/// NSView subclass that renders the pixel pet — supports both follow mode (transparent sprite)
/// and scene mode (full weather scene with sky, ground, celestials, particles)
class PetView: NSView {
    private var currentImage: NSImage?
    var facingLeft: Bool = false
    var petState: PetState = .idle
    var spriteSize: CGFloat = 128
    var weatherType: Int = -1  // -1=none, 0=CLEAR..7=THUNDER
    var sceneMode: Bool = false
    var spriteNormX: CGFloat = 0.5  // 0=left, 1=right; used in scene mode for position + time-travel
    var temperature: Float?  // from ESP32, nil when not connected
    private var sleepStartTime: TimeInterval = 0

    override var isFlipped: Bool { false }

    // ── Scene constants ──
    private let groundHeight: CGFloat = 28
    private let sceneCornerRadius: CGFloat = 12

    // ── Stars (night) ──
    private struct Star { var x: CGFloat; var y: CGFloat; var visible: Bool }
    private var stars: [Star] = []
    private var lastStarToggle: TimeInterval = 0

    // ── Weather particles ──
    private struct WeatherParticle {
        var x: CGFloat; var y: CGFloat; var speed: CGFloat
        var drift: CGFloat; var length: CGFloat
    }
    private var particles: [WeatherParticle] = []
    private var lastWeatherType: Int = -1

    // ── Thunder flash ──
    private var lastFlashTime: TimeInterval = 0
    private var nextFlashInterval: TimeInterval = 4.0
    private var flashActive: Bool = false

    // ── Fog offset ──
    private var fogOffset: Int = 0
    private var lastFogUpdate: TimeInterval = 0

    // ── Cached formatter ──
    private let clockFormatter: DateFormatter = {
        let f = DateFormatter()
        f.dateFormat = "HH:mm"
        return f
    }()

    func updateSprite(_ sprite: [UInt16], facingLeft: Bool, state: PetState, weatherType: Int = -1) {
        self.facingLeft = facingLeft
        if state == .sleep && self.petState != .sleep {
            sleepStartTime = ProcessInfo.processInfo.systemUptime
        }
        self.petState = state
        self.weatherType = weatherType
        self.currentImage = SpriteRenderer.render(sprite: sprite)
        needsDisplay = true
    }

    /// Sprite rect depends on mode:
    /// - Follow: bottom-left at (0,0)
    /// - Scene: X from spriteNormX, Y sitting on ground
    private var spriteRect: NSRect {
        if sceneMode {
            let x = spriteNormX * (bounds.width - spriteSize)
            return NSRect(x: x, y: groundHeight + 2, width: spriteSize, height: spriteSize)
        } else {
            return NSRect(x: 0, y: 0, width: spriteSize, height: spriteSize)
        }
    }

    // MARK: - Main draw

    override func draw(_ dirtyRect: NSRect) {
        guard let image = currentImage else { return }
        NSGraphicsContext.current?.imageInterpolation = .none

        if sceneMode {
            drawScene(image: image)
        } else {
            drawFollow(image: image)
        }
    }

    // MARK: - Follow mode (original transparent sprite behavior)

    private func drawFollow(image: NSImage) {
        var drawRect = spriteRect
        if facingLeft {
            let transform = NSAffineTransform()
            transform.translateX(by: bounds.width, yBy: 0)
            transform.scaleX(by: -1, yBy: 1)
            transform.concat()
            drawRect.origin.x = bounds.width - spriteSize
        }

        image.draw(in: drawRect,
                   from: NSRect(origin: .zero, size: image.size),
                   operation: .sourceOver, fraction: 1.0)
        drawAccessory(origin: drawRect.origin)

        if facingLeft {
            let transform = NSAffineTransform()
            transform.scaleX(by: -1, yBy: 1)
            transform.translateX(by: -bounds.width, yBy: 0)
            transform.concat()
        }

        if petState == .sleep {
            drawSleepZ()
        }
    }

    // MARK: - Scene mode (full weather scene)

    private func drawScene(image: NSImage) {
        let now = ProcessInfo.processInfo.systemUptime
        let hour = displayHour

        // 1. Sky
        drawSky(hour: hour)

        // 2. Celestials — hidden in heavy weather
        if weatherType < 2 {
            drawCelestials(hour: hour)
        }

        // 3. Weather particles
        if weatherType >= 3 {
            updateParticles(now: now)
            drawWeatherParticles()
        }

        // 4. Ground
        drawGround(hour: hour)

        // 5. Clock
        drawClock()

        // 6-8. Sprite + accessories (with flip)
        var drawRect = spriteRect
        if facingLeft {
            let transform = NSAffineTransform()
            transform.translateX(by: bounds.width, yBy: 0)
            transform.scaleX(by: -1, yBy: 1)
            transform.concat()
            drawRect.origin.x = bounds.width - spriteSize - spriteRect.origin.x
        }

        image.draw(in: drawRect,
                   from: NSRect(origin: .zero, size: image.size),
                   operation: .sourceOver, fraction: 1.0)
        drawAccessory(origin: drawRect.origin)

        // Reset flip
        if facingLeft {
            let transform = NSAffineTransform()
            transform.scaleX(by: -1, yBy: 1)
            transform.translateX(by: -bounds.width, yBy: 0)
            transform.concat()
        }

        // 9. Sleep Z (always right-side up)
        if petState == .sleep {
            drawSleepZ()
        }

        // 10. Thunder flash (topmost)
        if weatherType == 7 {
            drawThunderFlash(now: now)
        }
    }

    // MARK: - Sky

    private func drawSky(hour: Int) {
        let baseColor = skyBaseColor(for: hour)
        let finalColor: NSColor
        if weatherType >= 2 {
            finalColor = blendColor(baseColor, weatherTint(), factor: weatherBlendFactor())
        } else {
            finalColor = baseColor
        }
        finalColor.setFill()
        NSBezierPath(roundedRect: bounds, xRadius: sceneCornerRadius, yRadius: sceneCornerRadius).fill()
    }

    private func skyBaseColor(for hour: Int) -> NSColor {
        if hour >= 6 && hour < 17 {
            return NSColor(red: 60/255, green: 120/255, blue: 200/255, alpha: 1)
        } else if hour >= 17 && hour < 19 {
            return NSColor(red: 180/255, green: 80/255, blue: 60/255, alpha: 1)
        } else {
            return NSColor(red: 10/255, green: 10/255, blue: 30/255, alpha: 1)
        }
    }

    private func weatherTint() -> NSColor {
        switch weatherType {
        case 2: return NSColor(red: 100/255, green: 100/255, blue: 110/255, alpha: 1)
        case 3: return NSColor(red: 140/255, green: 140/255, blue: 145/255, alpha: 1)
        case 4: return NSColor(red: 90/255, green: 90/255, blue: 105/255, alpha: 1)
        case 5: return NSColor(red: 60/255, green: 60/255, blue: 75/255, alpha: 1)
        case 6: return NSColor(red: 120/255, green: 120/255, blue: 135/255, alpha: 1)
        case 7: return NSColor(red: 60/255, green: 60/255, blue: 75/255, alpha: 1)
        default: return .black
        }
    }

    private func weatherBlendFactor() -> CGFloat {
        switch weatherType {
        case 2: return 0.63
        case 3: return 0.67
        case 4: return 0.55
        case 5: return 0.71
        case 6: return 0.59
        case 7: return 0.71
        default: return 0
        }
    }

    // MARK: - Celestials

    private func drawCelestials(hour: Int) {
        if hour >= 6 && hour < 17 {
            drawSun()
            drawClouds()
        } else if hour >= 17 && hour < 19 {
            drawSunset()
        } else {
            drawMoon()
            drawStars()
        }
    }

    private func drawSun() {
        let cx: CGFloat = 300, cy: CGFloat = 185
        let sunColor = NSColor(red: 1, green: 220/255, blue: 60/255, alpha: 1)
        sunColor.setFill()
        NSBezierPath(ovalIn: NSRect(x: cx - 12, y: cy - 12, width: 24, height: 24)).fill()

        sunColor.setStroke()
        for i in 0..<8 {
            let angle = CGFloat(i) * .pi / 4
            let path = NSBezierPath()
            path.move(to: NSPoint(x: cx + cos(angle) * 14, y: cy + sin(angle) * 14))
            path.line(to: NSPoint(x: cx + cos(angle) * 20, y: cy + sin(angle) * 20))
            path.lineWidth = 2
            path.stroke()
        }
    }

    private func drawSunset() {
        let cx: CGFloat = 300, cy: CGFloat = groundHeight + 15
        NSColor(red: 1, green: 220/255, blue: 60/255, alpha: 1).setFill()
        NSBezierPath(ovalIn: NSRect(x: cx - 15, y: cy - 15, width: 30, height: 30)).fill()
        NSColor(red: 230/255, green: 120/255, blue: 50/255, alpha: 1).setFill()
        NSBezierPath(ovalIn: NSRect(x: cx - 12, y: cy - 12, width: 24, height: 24)).fill()
    }

    private func drawMoon() {
        let cx: CGFloat = 45, cy: CGFloat = 185
        NSColor(red: 220/255, green: 220/255, blue: 200/255, alpha: 1).setFill()
        NSBezierPath(ovalIn: NSRect(x: cx - 12, y: cy - 12, width: 24, height: 24)).fill()
        // Crescent overlay
        skyBaseColor(for: 22).setFill()
        NSBezierPath(ovalIn: NSRect(x: cx - 5, y: cy - 10, width: 20, height: 20)).fill()
    }

    private func drawStars() {
        let now = ProcessInfo.processInfo.systemUptime

        if stars.isEmpty {
            let positions: [(CGFloat, CGFloat)] = [
                (80, 180), (150, 190), (220, 175), (290, 188),
                (40, 160), (120, 170), (200, 165), (330, 170),
                (70, 145), (180, 150), (260, 155), (310, 148)
            ]
            stars = positions.map { Star(x: $0.0, y: $0.1, visible: true) }
            lastStarToggle = now
        }

        if now - lastStarToggle > 0.8 {
            lastStarToggle = now
            for i in stars.indices {
                stars[i].visible = Bool.random()
            }
        }

        let starColor = NSColor(red: 200/255, green: 200/255, blue: 140/255, alpha: 1)
        starColor.setFill()

        for (i, star) in stars.enumerated() where star.visible {
            if i % 3 == 0 {
                NSRect(x: star.x - 1, y: star.y - 3, width: 2, height: 6).fill()
                NSRect(x: star.x - 3, y: star.y - 1, width: 6, height: 2).fill()
            } else {
                NSRect(x: star.x - 1, y: star.y - 1, width: 2, height: 2).fill()
            }
        }
    }

    private func drawClouds() {
        let c = NSColor(red: 220/255, green: 230/255, blue: 240/255, alpha: 1)
        c.setFill()
        NSBezierPath(roundedRect: NSRect(x: 40, y: 170, width: 50, height: 14), xRadius: 7, yRadius: 7).fill()
        NSBezierPath(roundedRect: NSRect(x: 55, y: 178, width: 30, height: 10), xRadius: 5, yRadius: 5).fill()
        NSBezierPath(roundedRect: NSRect(x: 210, y: 180, width: 45, height: 12), xRadius: 6, yRadius: 6).fill()
        NSBezierPath(roundedRect: NSRect(x: 220, y: 186, width: 28, height: 10), xRadius: 5, yRadius: 5).fill()
    }

    // MARK: - Weather Particles

    private func updateParticles(now: TimeInterval) {
        if weatherType != lastWeatherType {
            lastWeatherType = weatherType
            particles.removeAll()
            initParticles()
        }

        if weatherType == 3 {
            if now - lastFogUpdate > 0.15 {
                lastFogUpdate = now
                fogOffset = (fogOffset + 1) % 6
            }
            return
        }

        for i in particles.indices {
            particles[i].y -= particles[i].speed
            particles[i].x += particles[i].drift
            if particles[i].y < groundHeight {
                particles[i].y = bounds.height + CGFloat.random(in: 0...20)
                particles[i].x = CGFloat.random(in: 0...bounds.width)
                if weatherType == 6 {
                    particles[i].drift = CGFloat([-1, 0, 1].randomElement()!)
                }
            }
        }
    }

    private func initParticles() {
        let w = bounds.width, h = bounds.height
        switch weatherType {
        case 4:
            particles = (0..<12).map { _ in
                WeatherParticle(x: .random(in: 0...w), y: .random(in: groundHeight...h),
                                speed: 4, drift: 0, length: 8)
            }
        case 5, 7:
            particles = (0..<20).map { _ in
                WeatherParticle(x: .random(in: 0...w), y: .random(in: groundHeight...h),
                                speed: 7, drift: 0, length: 14)
            }
        case 6:
            particles = (0..<20).map { _ in
                WeatherParticle(x: .random(in: 0...w), y: .random(in: groundHeight...h),
                                speed: 1.5, drift: CGFloat([-1, 0, 1].randomElement()!), length: 3)
            }
        default: break
        }
    }

    private func drawWeatherParticles() {
        if weatherType == 3 {
            drawFog()
            return
        }
        if weatherType == 6 {
            NSColor(red: 220/255, green: 220/255, blue: 230/255, alpha: 1).setFill()
            for p in particles {
                NSRect(x: p.x, y: p.y, width: 2, height: 2).fill()
            }
        } else {
            NSColor(red: 140/255, green: 160/255, blue: 200/255, alpha: 1).setStroke()
            for p in particles {
                let path = NSBezierPath()
                path.move(to: NSPoint(x: p.x, y: p.y))
                path.line(to: NSPoint(x: p.x, y: p.y + p.length))
                path.lineWidth = 1
                path.stroke()
            }
        }
    }

    private func drawFog() {
        NSColor(red: 180/255, green: 180/255, blue: 190/255, alpha: 0.6).setFill()
        let spacing: CGFloat = 6
        var y = groundHeight
        var row = 0
        while y < bounds.height {
            var x: CGFloat = CGFloat((row + fogOffset) % Int(spacing))
            while x < bounds.width {
                NSRect(x: x, y: y, width: 2, height: 2).fill()
                x += spacing
            }
            y += spacing
            row += 1
        }
    }

    // MARK: - Thunder Flash

    private func drawThunderFlash(now: TimeInterval) {
        if flashActive {
            if now - lastFlashTime > 0.05 {
                flashActive = false
            } else {
                NSColor(red: 0.78, green: 0.78, blue: 0.86, alpha: 1).setFill()
                bounds.fill()
            }
        } else if now - lastFlashTime > nextFlashInterval {
            lastFlashTime = now
            nextFlashInterval = Double.random(in: 3...5)
            flashActive = true
            NSColor(red: 0.78, green: 0.78, blue: 0.86, alpha: 1).setFill()
            bounds.fill()
        }
    }

    // MARK: - Ground

    private func drawGround(hour: Int) {
        groundColor(for: hour).setFill()
        NSRect(x: 0, y: 0, width: bounds.width, height: groundHeight).fill()

        // Top highlight (2px)
        NSColor(red: 100/255, green: 170/255, blue: 70/255, alpha: 1).setFill()
        NSRect(x: 0, y: groundHeight - 2, width: bounds.width, height: 2).fill()

        // Grass tufts
        NSColor(red: 60/255, green: 130/255, blue: 50/255, alpha: 1).setFill()
        for gx in [20, 65, 110, 155, 205, 250, 295, 335] as [CGFloat] {
            NSRect(x: gx, y: groundHeight, width: 2, height: 3).fill()
            NSRect(x: gx - 2, y: groundHeight, width: 2, height: 2).fill()
            NSRect(x: gx + 2, y: groundHeight, width: 2, height: 2).fill()
        }
    }

    private func groundColor(for hour: Int) -> NSColor {
        if hour >= 6 && hour < 19 {
            return NSColor(red: 80/255, green: 140/255, blue: 60/255, alpha: 1)
        } else {
            return NSColor(red: 50/255, green: 100/255, blue: 40/255, alpha: 1)
        }
    }

    // MARK: - Clock

    private func drawClock() {
        var displayText = clockFormatter.string(from: Date())
        if let t = temperature {
            displayText += " | \(Int(roundf(t)))°"
        }
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: 11, weight: .medium),
            .foregroundColor: NSColor(red: 180/255, green: 180/255, blue: 200/255, alpha: 1)
        ]
        let size = displayText.size(withAttributes: attrs)
        let x = (bounds.width - size.width) / 2
        let y = (groundHeight - size.height) / 2
        displayText.draw(at: NSPoint(x: x, y: y), withAttributes: attrs)
    }

    // MARK: - Sleep Z

    private func drawSleepZ() {
        let elapsed = ProcessInfo.processInfo.systemUptime - sleepStartTime
        let phase = Int(elapsed / 0.6) % 4
        let color = NSColor.white.withAlphaComponent(0.9)
        let base = spriteRect.origin  // (0,0) in follow mode; scene coords in scene mode

        if phase >= 1 {
            let attrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.monospacedSystemFont(ofSize: 12, weight: .bold),
                .foregroundColor: color
            ]
            "z".draw(at: NSPoint(x: base.x + 106, y: base.y + 68), withAttributes: attrs)
        }
        if phase >= 2 {
            let attrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.monospacedSystemFont(ofSize: 20, weight: .bold),
                .foregroundColor: color
            ]
            "Z".draw(at: NSPoint(x: base.x + 118, y: base.y + 96), withAttributes: attrs)
        }
        if phase >= 3 {
            let attrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.monospacedSystemFont(ofSize: 34, weight: .bold),
                .foregroundColor: color
            ]
            "Z".draw(at: NSPoint(x: base.x + 112, y: base.y + 132), withAttributes: attrs)
        }
    }

    // MARK: - Accessories

    private func drawAccessory(origin: NSPoint) {
        let ox = origin.x
        let oy = origin.y

        switch weatherType {
        case 0, 1: // Sunglasses
            NSColor(red: 20/255, green: 20/255, blue: 40/255, alpha: 1).setFill()
            NSRect(x: ox + 32, y: oy + spriteSize - 48, width: 24, height: 8).fill()
            NSRect(x: ox + 72, y: oy + spriteSize - 48, width: 24, height: 8).fill()
            NSRect(x: ox + 56, y: oy + spriteSize - 46, width: 16, height: 3).fill()

        case 4, 5, 7: // Umbrella
            NSColor(red: 60/255, green: 60/255, blue: 200/255, alpha: 1).setFill()
            NSBezierPath(roundedRect: NSRect(x: ox + 16, y: oy + spriteSize + 6, width: 96, height: 21),
                         xRadius: 11, yRadius: 11).fill()
            NSColor(red: 120/255, green: 80/255, blue: 40/255, alpha: 1).setFill()
            NSRect(x: ox + 62, y: oy + spriteSize - 16, width: 4, height: 22).fill()

        case 6: // Snow hat
            NSColor(red: 200/255, green: 60/255, blue: 60/255, alpha: 1).setFill()
            NSBezierPath(roundedRect: NSRect(x: ox + 24, y: oy + spriteSize - 24, width: 80, height: 16),
                         xRadius: 8, yRadius: 8).fill()
            NSColor.white.setFill()
            NSBezierPath(ovalIn: NSRect(x: ox + 56, y: oy + spriteSize, width: 16, height: 16)).fill()

        case 2, 3: // Mask
            NSColor(red: 180/255, green: 200/255, blue: 180/255, alpha: 1).setFill()
            NSRect(x: ox + 32, y: oy + spriteSize - 72, width: 64, height: 16).fill()
            NSColor(red: 120/255, green: 120/255, blue: 120/255, alpha: 1).setFill()
            NSRect(x: ox + 24, y: oy + spriteSize - 64, width: 8, height: 3).fill()
            NSRect(x: ox + 96, y: oy + spriteSize - 64, width: 8, height: 3).fill()

        default: break
        }
    }

    // MARK: - Helpers

    private var displayHour: Int {
        let baseHour = Calendar.current.component(.hour, from: Date())
        if sceneMode {
            // Time-travel: sprite position offsets the hour (-12 to +12)
            let offset = Int(spriteNormX * 24) - 12
            return (baseHour + offset + 24) % 24
        }
        return baseHour
    }

    private func blendColor(_ base: NSColor, _ tint: NSColor, factor: CGFloat) -> NSColor {
        guard let b = base.usingColorSpace(.sRGB),
              let t = tint.usingColorSpace(.sRGB) else { return base }
        let f = min(max(factor, 0), 1)
        return NSColor(red: b.redComponent * (1 - f) + t.redComponent * f,
                       green: b.greenComponent * (1 - f) + t.greenComponent * f,
                       blue: b.blueComponent * (1 - f) + t.blueComponent * f,
                       alpha: 1)
    }
}
