import AppKit

/// Converts RGB565 sprite data to CGImage for display
enum SpriteRenderer {

    /// Convert a 16x16 RGB565 sprite to a CGImage scaled up by `scale` factor
    /// with nearest-neighbor interpolation for crisp pixel art.
    static func render(sprite: [UInt16], scale: Int = 8) -> NSImage? {
        let w = SPRITE_W
        let h = SPRITE_H
        guard sprite.count == w * h else { return nil }

        // Create CGContext that manages its own pixel buffer
        // (Passing &localArray as data: is UB — the pointer is only valid for the init call,
        //  but makeImage() reads from it later. Release-mode optimizations can invalidate it.)
        let colorSpace = CGColorSpaceCreateDeviceRGB()
        guard let context = CGContext(
            data: nil,
            width: w,
            height: h,
            bitsPerComponent: 8,
            bytesPerRow: w * 4,
            space: colorSpace,
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ), let buffer = context.data else {
            return nil
        }

        // Convert RGB565 → RGBA8888 directly into the context's buffer
        let rgba = buffer.assumingMemoryBound(to: UInt8.self)
        for i in 0..<(w * h) {
            let pixel = sprite[i]
            if pixel == TRANSPARENT_COLOR {
                rgba[i * 4 + 0] = 0
                rgba[i * 4 + 1] = 0
                rgba[i * 4 + 2] = 0
                rgba[i * 4 + 3] = 0
            } else {
                // RGB565: RRRRR GGGGGG BBBBB
                let r5 = (pixel >> 11) & 0x1F
                let g6 = (pixel >> 5) & 0x3F
                let b5 = pixel & 0x1F
                // Expand to 8-bit
                rgba[i * 4 + 0] = UInt8((r5 << 3) | (r5 >> 2))
                rgba[i * 4 + 1] = UInt8((g6 << 2) | (g6 >> 4))
                rgba[i * 4 + 2] = UInt8((b5 << 3) | (b5 >> 2))
                rgba[i * 4 + 3] = 255
            }
        }

        guard let smallImage = context.makeImage() else { return nil }

        // Scale up with nearest-neighbor for pixel-perfect rendering
        let scaledW = w * scale
        let scaledH = h * scale
        guard let scaledContext = CGContext(
            data: nil,
            width: scaledW,
            height: scaledH,
            bitsPerComponent: 8,
            bytesPerRow: scaledW * 4,
            space: colorSpace,
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue
        ) else {
            return nil
        }
        scaledContext.interpolationQuality = .none // Nearest neighbor
        scaledContext.draw(smallImage, in: CGRect(x: 0, y: 0, width: scaledW, height: scaledH))

        guard let scaledCGImage = scaledContext.makeImage() else { return nil }
        return NSImage(cgImage: scaledCGImage, size: NSSize(width: scaledW, height: scaledH))
    }
}
