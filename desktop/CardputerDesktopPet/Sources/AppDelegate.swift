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
    private var lastMoisture: Int?
    private var lastHumidityPct: Int?

    // Desktop integration
    private var pixelArtPopover = PixelArtPopover()
    private var chatViewer = ChatViewerWindow()
    private var connectionIndicator: NSMenuItem?

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
        udpListener.onStateReceived = { [weak self] (state: Int, frame: Int, mode: String, normX: Float?, normY: Float?, direction: Int?, weatherType: Int?, temperature: Float?, moisture: Int?, humidityPct: Int?) in
            self?.behavior.applySync(state: state, frame: frame,
                                     normX: normX, normY: normY, direction: direction,
                                     weatherType: weatherType, moisture: moisture)
            if let nx = normX { self?.lastNormX = CGFloat(nx) }
            if let t = temperature { self?.lastTemperature = t }
            if let m = moisture { self?.lastMoisture = m }
            if let rh = humidityPct { self?.lastHumidityPct = rh }

            // Update connection indicator
            if let m = self?.lastMoisture, let rh = self?.lastHumidityPct {
                self?.connectionIndicator?.title = "ESP32: Connected · 💧\(m)/4 · \(rh)%"
            } else {
                self?.connectionIndicator?.title = "ESP32: Connected"
            }
        }

        udpListener.onPixelArtReceived = { [weak self] (size: Int, rows: [String]) in
            print("[UDP] Pixel art received: \(size)x\(size)")
            self?.pixelArtPopover.show(size: size, rows: rows)
        }

        udpListener.onChatMessageReceived = { [weak self] (role: String, text: String) in
            print("[UDP] Chat: \(role): \(text)")
            self?.chatViewer.addMessage(role: role, text: text)
        }

        udpListener.start()

        // Chat viewer send callback
        chatViewer.onSendMessage = { [weak self] text in
            guard let address = self?.udpListener.esp32Address else {
                print("[TCP] No ESP32 address known")
                return
            }
            DispatchQueue.global(qos: .userInitiated).async {
                TCPSender.sendText(address: address, text: text, autoSend: true)
            }
        }

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

        // ── Control submenu ──
        let controlItem = NSMenuItem(title: "Control", action: nil, keyEquivalent: "")
        let controlMenu = NSMenu()

        let happyItem = NSMenuItem(title: "Trigger Happy", action: #selector(triggerHappy), keyEquivalent: "")
        happyItem.target = self
        controlMenu.addItem(happyItem)

        let sleepItem = NSMenuItem(title: "Trigger Sleep", action: #selector(triggerSleep), keyEquivalent: "")
        sleepItem.target = self
        controlMenu.addItem(sleepItem)

        controlMenu.addItem(.separator())

        let sendTextItem = NSMenuItem(title: "Send Text...", action: #selector(showSendTextDialog), keyEquivalent: "")
        sendTextItem.target = self
        controlMenu.addItem(sendTextItem)

        let notifyItem = NSMenuItem(title: "Send Notification...", action: #selector(showNotifyDialog), keyEquivalent: "")
        notifyItem.target = self
        controlMenu.addItem(notifyItem)

        controlItem.submenu = controlMenu
        menu.addItem(controlItem)

        menu.addItem(.separator())

        let chatViewerItem = NSMenuItem(title: "Chat Viewer", action: #selector(showChatViewer), keyEquivalent: "")
        chatViewerItem.target = self
        menu.addItem(chatViewerItem)

        menu.addItem(.separator())

        // Connection status
        connectionIndicator = NSMenuItem(title: "ESP32: Waiting...", action: nil, keyEquivalent: "")
        connectionIndicator?.isEnabled = false
        menu.addItem(connectionIndicator!)

        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit ClawPuter", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))

        statusItem.menu = menu
    }

    // ── Control actions ──

    @objc private func triggerHappy() {
        sendCommand { address in
            TCPSender.triggerAnimate(address: address, state: "happy")
        }
    }

    @objc private func triggerSleep() {
        sendCommand { address in
            TCPSender.triggerAnimate(address: address, state: "sleep")
        }
    }

    @objc private func showSendTextDialog() {
        let alert = NSAlert()
        alert.messageText = "Send Text to Cardputer"
        alert.informativeText = "Enter text to send to the chat input:"
        alert.addButton(withTitle: "Send")
        alert.addButton(withTitle: "Cancel")

        let input = NSTextField(frame: NSRect(x: 0, y: 0, width: 260, height: 24))
        input.placeholderString = "Hello from Mac!"
        alert.accessoryView = input

        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            let text = input.stringValue
            if !text.isEmpty {
                sendCommand { address in
                    TCPSender.sendText(address: address, text: text, autoSend: true)
                }
            }
        }
    }

    @objc private func showNotifyDialog() {
        let alert = NSAlert()
        alert.messageText = "Send Notification"
        alert.informativeText = "Enter notification details:"
        alert.addButton(withTitle: "Send")
        alert.addButton(withTitle: "Cancel")

        let container = NSView(frame: NSRect(x: 0, y: 0, width: 260, height: 80))

        let titleLabel = NSTextField(labelWithString: "Title:")
        titleLabel.frame = NSRect(x: 0, y: 56, width: 40, height: 20)
        container.addSubview(titleLabel)
        let titleField = NSTextField(frame: NSRect(x: 45, y: 54, width: 215, height: 22))
        titleField.placeholderString = "Notification title"
        container.addSubview(titleField)

        let bodyLabel = NSTextField(labelWithString: "Body:")
        bodyLabel.frame = NSRect(x: 0, y: 28, width: 40, height: 20)
        container.addSubview(bodyLabel)
        let bodyField = NSTextField(frame: NSRect(x: 45, y: 26, width: 215, height: 22))
        bodyField.placeholderString = "Notification body"
        container.addSubview(bodyField)

        let appLabel = NSTextField(labelWithString: "App:")
        appLabel.frame = NSRect(x: 0, y: 0, width: 40, height: 20)
        container.addSubview(appLabel)
        let appField = NSTextField(frame: NSRect(x: 45, y: -2, width: 215, height: 22))
        appField.placeholderString = "App name (optional)"
        container.addSubview(appField)

        alert.accessoryView = container

        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            let title = titleField.stringValue
            let body = bodyField.stringValue
            let app = appField.stringValue
            if !title.isEmpty || !body.isEmpty {
                sendCommand { address in
                    TCPSender.sendNotification(address: address, app: app, title: title, body: body)
                }
            }
        }
    }

    @objc private func showChatViewer() {
        chatViewer.show()
    }

    private func sendCommand(_ block: @escaping (String) -> Void) {
        guard let address = udpListener.esp32Address else {
            let alert = NSAlert()
            alert.messageText = "No ESP32 Connected"
            alert.informativeText = "Waiting for UDP broadcast from Cardputer..."
            alert.runModal()
            return
        }
        DispatchQueue.global(qos: .userInitiated).async {
            block(address)
        }
    }

    // ── Display mode switching ──

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
        // Perch on Chat Viewer when visible (follow mode only)
        if displayMode == .follow, chatViewer.isWindowVisible, let chatFrame = chatViewer.windowFrame {
            let windowW = petSize + followPadRight
            behavior.perchTarget = NSPoint(
                x: chatFrame.midX - windowW / 2,
                y: chatFrame.maxY - 5  // feet slightly overlap title bar
            )
        } else {
            behavior.perchTarget = nil
        }

        let needsRedraw = behavior.update()

        switch displayMode {
        case .follow:
            petWindow.setFrameOrigin(NSPoint(x: behavior.posX, y: behavior.posY))
            petView.moisture = lastMoisture
            if needsRedraw {
                petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft,
                                     state: behavior.state, weatherType: behavior.weatherType)
            }
            // Continuous redraw for sleep Z animation or low moisture bubble animation
            if behavior.state == .sleep || (lastMoisture ?? 4) <= 1 {
                petView.needsDisplay = true
            }

        case .scene:
            sceneView.spriteNormX = behavior.syncMode ? lastNormX : 0.5
            sceneView.temperature = lastTemperature
            sceneView.moisture = lastMoisture
            sceneView.humidityPercent = lastHumidityPct
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
