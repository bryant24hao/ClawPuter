import AppKit

/// Transparent, topmost, click-through window for the desktop pet
class PetWindow: NSWindow {

    init(size: NSSize) {
        super.init(
            contentRect: NSRect(origin: NSPoint(x: 400, y: 400), size: size),
            styleMask: .borderless,
            backing: .buffered,
            defer: false
        )

        // Transparent + floating + click-through
        isOpaque = false
        backgroundColor = .clear
        level = .floating
        ignoresMouseEvents = true
        hasShadow = false

        // Don't show in Mission Control / Exposé
        collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]

        // No title bar
        titlebarAppearsTransparent = true
        titleVisibility = .hidden
    }
}
