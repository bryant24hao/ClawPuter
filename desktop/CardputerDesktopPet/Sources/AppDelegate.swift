import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {

    enum DisplayMode { case follow, scene }

    private var displayMode: DisplayMode = .follow

    // Follow mode
    private var petWindow: PetWindow!
    private var petView: PetView!

    // Scene mode
    private var scenePanel: NSPanel!
    private var sceneView: PetView!

    private var behavior: PetBehavior!
    private var updateTimer: Timer?
    private var udpListener: UDPListener!
    private var statusItem: NSStatusItem!
    private var followMenuItem: NSMenuItem!
    private var sceneMenuItem: NSMenuItem!

    private let petSize: CGFloat = 128
    private let followPadRight: CGFloat = 40
    private let followPadTop: CGFloat = 48
    private let sceneW: CGFloat = 360
    private let sceneH: CGFloat = 200

    // Track raw normX from UDP for scene mode sprite positioning
    private var lastNormX: CGFloat = 0.5
    private var lastTemperature: Float?

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)

        behavior = PetBehavior()

        // ── Follow mode window (original behavior) ──
        let followSize = NSSize(width: petSize + followPadRight, height: petSize + followPadTop)
        petView = PetView(frame: NSRect(origin: .zero, size: followSize))
        petView.spriteSize = petSize
        petView.sceneMode = false
        petWindow = PetWindow(size: followSize)
        petWindow.contentView = petView

        // ── Scene mode panel (anchored below menu bar) ──
        let sceneSize = NSSize(width: sceneW, height: sceneH)
        sceneView = PetView(frame: NSRect(origin: .zero, size: sceneSize))
        sceneView.spriteSize = petSize
        sceneView.sceneMode = true
        sceneView.wantsLayer = true
        sceneView.layer?.cornerRadius = 12
        sceneView.layer?.masksToBounds = true

        scenePanel = NSPanel(
            contentRect: NSRect(origin: .zero, size: sceneSize),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        scenePanel.isOpaque = false
        scenePanel.backgroundColor = .clear
        scenePanel.level = .floating
        scenePanel.ignoresMouseEvents = true
        scenePanel.hasShadow = true
        scenePanel.collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]
        scenePanel.contentView = sceneView

        // Initial render — follow mode
        petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft, state: behavior.state)
        petWindow.orderFront(nil)

        if let screen = NSScreen.main {
            behavior.posX = screen.frame.midX
            behavior.posY = screen.frame.midY
            petWindow.setFrameOrigin(NSPoint(x: behavior.posX, y: behavior.posY))
        }

        // Mouse monitor — only active in follow mode
        NSEvent.addGlobalMonitorForEvents(matching: [.mouseMoved, .leftMouseDragged, .rightMouseDragged]) { [weak self] event in
            guard self?.displayMode == .follow else { return }
            self?.behavior.onMouseMoved(to: NSEvent.mouseLocation)
        }

        // Reposition scene panel when display configuration changes
        NotificationCenter.default.addObserver(
            forName: NSApplication.didChangeScreenParametersNotification,
            object: nil, queue: .main
        ) { [weak self] _ in
            guard self?.displayMode == .scene else { return }
            self?.positionScenePanel()
        }

        // 60fps update loop
        updateTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            self?.tick()
        }
        RunLoop.current.add(updateTimer!, forMode: .common)

        // UDP listener
        udpListener = UDPListener()
        udpListener.onStateReceived = { [weak self] (state: Int, frame: Int, mode: String, normX: Float?, normY: Float?, direction: Int?, weatherType: Int?, temperature: Float?) in
            self?.behavior.applySync(state: state, frame: frame,
                                     normX: normX, normY: normY, direction: direction,
                                     weatherType: weatherType)
            if let nx = normX { self?.lastNormX = CGFloat(nx) }
            if let t = temperature { self?.lastTemperature = t }
        }
        udpListener.start()

        // ── Menu bar ──
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.title = "🦞"
        }
        buildMenu()
    }

    private func buildMenu() {
        let menu = NSMenu()

        followMenuItem = NSMenuItem(title: "Follow Mode", action: #selector(switchToFollow), keyEquivalent: "")
        followMenuItem.target = self
        followMenuItem.state = .on

        sceneMenuItem = NSMenuItem(title: "Scene Mode", action: #selector(switchToScene), keyEquivalent: "")
        sceneMenuItem.target = self
        sceneMenuItem.state = .off

        menu.addItem(followMenuItem)
        menu.addItem(sceneMenuItem)
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit Pixel Lobster", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))

        statusItem.menu = menu
    }

    @objc private func switchToFollow() {
        guard displayMode != .follow else { return }
        displayMode = .follow
        followMenuItem.state = .on
        sceneMenuItem.state = .off

        scenePanel.orderOut(nil)
        petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft,
                             state: behavior.state, weatherType: behavior.weatherType)
        petWindow.orderFront(nil)
    }

    @objc private func switchToScene() {
        guard displayMode != .scene else { return }
        displayMode = .scene
        followMenuItem.state = .off
        sceneMenuItem.state = .on

        petWindow.orderOut(nil)
        positionScenePanel()
        sceneView.spriteNormX = behavior.syncMode ? lastNormX : 0.5
        sceneView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft,
                               state: behavior.state, weatherType: behavior.weatherType)
        scenePanel.orderFront(nil)
    }

    private func positionScenePanel() {
        // Anchor below the status item button
        if let button = statusItem.button, let buttonWindow = button.window {
            let buttonBounds = button.convert(button.bounds, to: nil)
            let screenFrame = buttonWindow.convertToScreen(buttonBounds)
            let x = screenFrame.midX - sceneW / 2
            let y = screenFrame.minY - sceneH - 4
            // Clamp to screen edges
            if let screen = NSScreen.main {
                let clampedX = max(screen.frame.minX + 8, min(x, screen.frame.maxX - sceneW - 8))
                scenePanel.setFrameOrigin(NSPoint(x: clampedX, y: y))
            } else {
                scenePanel.setFrameOrigin(NSPoint(x: x, y: y))
            }
        }
    }

    private func tick() {
        let needsRedraw = behavior.update()

        switch displayMode {
        case .follow:
            petWindow.setFrameOrigin(NSPoint(x: behavior.posX, y: behavior.posY))
            if needsRedraw {
                petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft,
                                     state: behavior.state, weatherType: behavior.weatherType)
            }
            // Continuous redraw for sleep Z animation
            if behavior.state == .sleep {
                petView.needsDisplay = true
            }

        case .scene:
            sceneView.spriteNormX = behavior.syncMode ? lastNormX : 0.5
            sceneView.temperature = lastTemperature
            if needsRedraw {
                sceneView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft,
                                       state: behavior.state, weatherType: behavior.weatherType)
            }
            // Scene always redraws for clock, weather particles, sleep Z
            sceneView.needsDisplay = true
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false
    }
}
