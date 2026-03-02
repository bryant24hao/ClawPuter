import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {

    private var petWindow: PetWindow!
    private var petView: PetView!
    private var behavior: PetBehavior!
    private var displayLink: CVDisplayLink?
    private var updateTimer: Timer?

    private let petSize: CGFloat = 128 // 16px * 8 scale

    func applicationDidFinishLaunching(_ notification: Notification) {
        // Hide dock icon — this is a background companion, not a regular app
        NSApp.setActivationPolicy(.accessory)

        // Create pet components
        behavior = PetBehavior()
        petView = PetView(frame: NSRect(x: 0, y: 0, width: petSize, height: petSize))
        petWindow = PetWindow(size: NSSize(width: petSize, height: petSize))
        petWindow.contentView = petView

        // Initial render
        petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft)
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
            petView.updateSprite(behavior.currentSprite(), facingLeft: behavior.facingLeft)
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        false // Keep running even if window is "closed"
    }
}
