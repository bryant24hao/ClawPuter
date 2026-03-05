import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {

    private var petWindow: PetWindow!
    private var petView: PetView!
    private var behavior: PetBehavior!
    private var displayLink: CVDisplayLink?
    private var updateTimer: Timer?
    private var udpListener: UDPListener!

    private let petSize: CGFloat = 128 // 16px * 8 scale
    private let windowPadRight: CGFloat = 40  // Extra space for sleep Z overlay
    private let windowPadTop: CGFloat = 48

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Hide dock icon — this is a background companion, not a regular app
        NSApp.setActivationPolicy(.accessory)

        // Create pet components — window is larger than sprite to fit Z overlay
        let windowSize = NSSize(width: petSize + windowPadRight, height: petSize + windowPadTop)
        behavior = PetBehavior()
        petView = PetView(frame: NSRect(origin: .zero, size: windowSize))
        petView.spriteSize = petSize
        petWindow = PetWindow(size: windowSize)
        petWindow.contentView = petView

        // Initial render
        petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft, state: behavior.state)
        petWindow.orderFront(nil)

        // Position near center of screen initially
        if let screen = NSScreen.main {
            let screenFrame = screen.frame
            behavior.posX = screenFrame.midX
            behavior.posY = screenFrame.midY
            petWindow.setFrameOrigin(NSPoint(x: behavior.posX, y: behavior.posY))
        }

        // Global mouse move monitor
        NSEvent.addGlobalMonitorForEvents(matching: [.mouseMoved, .leftMouseDragged, .rightMouseDragged]) { [weak self] event in
            // NSEvent.mouseLocation is in screen coordinates (origin bottom-left)
            let mousePos = NSEvent.mouseLocation
            self?.behavior.onMouseMoved(to: mousePos)
        }

        // Update loop at ~60fps
        updateTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            self?.tick()
        }
        RunLoop.current.add(updateTimer!, forMode: .common)

        // Start UDP listener for Cardputer state sync
        udpListener = UDPListener()
        udpListener.onStateReceived = { [weak self] state, frame, mode, normX, normY, direction in
            // Already on main queue (UDPListener dispatches to main)
            self?.behavior.applySync(state: state, frame: frame,
                                     normX: normX, normY: normY, direction: direction)
        }
        udpListener.start()

        // Menu bar status item for quit
        let statusBar = NSStatusBar.system
        let statusItem = statusBar.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.title = "🦞"
        }
        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Quit Pixel Lobster", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        statusItem.menu = menu
    }

    private func tick() {
        let needsRedraw = behavior.update()

        // Always update position (smooth movement)
        petWindow.setFrameOrigin(NSPoint(x: behavior.posX, y: behavior.posY))

        if needsRedraw {
            petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft, state: behavior.state)
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false // Keep running even if window is "closed"
    }
}
