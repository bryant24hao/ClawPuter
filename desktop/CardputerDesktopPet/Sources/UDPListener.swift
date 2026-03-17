import Foundation

/// Listens for UDP broadcast packets from the Cardputer firmware using BSD sockets
class UDPListener {
    private var socketFD: Int32 = -1
    private var listenThread: Thread?
    private var running = false

    /// The ESP32's IP address, extracted from UDP source — used by TCPSender
    private(set) var esp32Address: String?

    /// Callback: (state, frameIndex, appMode, normX, normY, direction, weatherType, temperature, moisture, humidityPercent) — called on main queue
    var onStateReceived: ((Int, Int, String, Float?, Float?, Int?, Int?, Float?, Int?, Int?) -> Void)?

    /// Callback: pixel art data received — (size, rows) — called on main queue
    var onPixelArtReceived: ((Int, [String]) -> Void)?

    /// Callback: chat message received — (role, text) — called on main queue
    var onChatMessageReceived: ((String, String) -> Void)?

    func start() {
        // Create UDP socket
        socketFD = socket(AF_INET, SOCK_DGRAM, 0)
        guard socketFD >= 0 else {
            print("[UDP] Failed to create socket")
            return
        }

        // Allow address reuse
        var yes: Int32 = 1
        setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, socklen_t(MemoryLayout<Int32>.size))
        setsockopt(socketFD, SOL_SOCKET, SO_REUSEPORT, &yes, socklen_t(MemoryLayout<Int32>.size))

        // Bind to port 19820 on all interfaces
        var addr = sockaddr_in()
        addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        addr.sin_family = sa_family_t(AF_INET)
        addr.sin_port = UInt16(19820).bigEndian
        addr.sin_addr.s_addr = INADDR_ANY.bigEndian

        let bindResult = withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockPtr in
                bind(socketFD, sockPtr, socklen_t(MemoryLayout<sockaddr_in>.size))
            }
        }

        guard bindResult == 0 else {
            print("[UDP] Failed to bind: \(String(cString: strerror(errno)))")
            Darwin.close(socketFD)
            socketFD = -1
            return
        }

        print("[UDP] Listening on port 19820")

        // Start background receive thread
        running = true
        listenThread = Thread { [weak self] in
            self?.receiveLoop()
        }
        listenThread?.qualityOfService = .userInteractive
        listenThread?.start()
    }

    func stop() {
        running = false
        if socketFD >= 0 {
            Darwin.close(socketFD)
            socketFD = -1
        }
    }

    private func receiveLoop() {
        var buf = [UInt8](repeating: 0, count: 1024)  // Larger buffer for pixel art packets
        var srcAddr = sockaddr_in()
        var srcAddrLen = socklen_t(MemoryLayout<sockaddr_in>.size)

        while running && socketFD >= 0 {
            let n = withUnsafeMutablePointer(to: &srcAddr) { addrPtr in
                addrPtr.withMemoryRebound(to: sockaddr.self, capacity: 1) { sockPtr in
                    recvfrom(socketFD, &buf, buf.count, 0, sockPtr, &srcAddrLen)
                }
            }

            if n > 0 {
                // Extract source IP
                let ipBytes = srcAddr.sin_addr.s_addr
                let ip = String(format: "%d.%d.%d.%d",
                                ipBytes & 0xFF,
                                (ipBytes >> 8) & 0xFF,
                                (ipBytes >> 16) & 0xFF,
                                (ipBytes >> 24) & 0xFF)

                // Update ESP32 address on main queue for thread safety (skip localhost / broadcast)
                if ip != "127.0.0.1" && ip != "255.255.255.255" {
                    let newIP = ip
                    DispatchQueue.main.async { [weak self] in
                        if self?.esp32Address != newIP {
                            self?.esp32Address = newIP
                            print("[UDP] ESP32 address: \(newIP)")
                        }
                    }
                }

                let data = Data(buf[0..<n])
                parsePacket(data)
            } else if n < 0 && errno != EAGAIN {
                print("[UDP] recv error: \(String(cString: strerror(errno)))")
                break
            }
        }
        print("[UDP] receiveLoop exited")
    }

    private func parsePacket(_ data: Data) {
        // Expect compact JSON
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return
        }

        // Check for "type" field — new message types
        if let type = json["type"] as? String {
            switch type {
            case "pixelart":
                if let size = json["size"] as? Int,
                   let rows = json["rows"] as? [String] {
                    let callback = onPixelArtReceived
                    DispatchQueue.main.async {
                        callback?(size, rows)
                    }
                }
            case "chat":
                if let role = json["role"] as? String,
                   let text = json["text"] as? String {
                    let callback = onChatMessageReceived
                    DispatchQueue.main.async {
                        callback?(role, text)
                    }
                }
            default:
                break
            }
            return
        }

        // Legacy state sync packet (no "type" field)
        guard let state = json["s"] as? Int,
              let frame = json["f"] as? Int,
              let mode = json["m"] as? String else {
            return
        }

        let normX = (json["x"] as? NSNumber)?.floatValue
        let normY = (json["y"] as? NSNumber)?.floatValue
        let direction = json["d"] as? Int
        let weatherType = json["w"] as? Int
        let temperature = (json["t"] as? NSNumber)?.floatValue
        let moisture = json["h"] as? Int
        let humidityPct = json["rh"] as? Int

        let callback = onStateReceived
        DispatchQueue.main.async {
            callback?(state, frame, mode, normX, normY, direction, weatherType, temperature, moisture, humidityPct)
        }
    }
}
