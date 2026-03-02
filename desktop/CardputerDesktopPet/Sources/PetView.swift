import AppKit

/// NSView subclass that renders the pixel pet sprite
class PetView: NSView {
    private var currentImage: NSImage?
    var facingLeft: Bool = false

    override var isFlipped: Bool { false }

    func updateSprite(_ sprite: [UInt16], facingLeft: Bool) {
        self.facingLeft = facingLeft
        self.currentImage = SpriteRenderer.render(sprite: sprite)
        needsDisplay = true
    }

    override func draw(_ dirtyRect: NSRect) {
        guard let image = currentImage else { return }

        NSGraphicsContext.current?.imageInterpolation = .none

        if facingLeft {
            // Flip horizontally
            let transform = NSAffineTransform()
            transform.translateX(by: bounds.width, yBy: 0)
            transform.scaleX(by: -1, yBy: 1)
            transform.concat()
        }

        image.draw(in: bounds,
                   from: NSRect(origin: .zero, size: image.size),
                   operation: .sourceOver,
                   fraction: 1.0)
    }
}
