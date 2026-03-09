import Foundation

/// Listens for UDP broadcast packets from the Cardputer firmware using BSD sockets
class UDPListener {
    private var socketFD: Int32 = -1
    private var listenThread: Thread?
    private var running = false

    /// Callback: (state, frameIndex, appMode, normX, normY, direction, weatherType, temperature) — called on main queue
    var onStateReceived: ((Int, Int, String, Float?, Float?, Int?, Int?, Float?) -> Void)?

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
            close(socketFD)
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
            close(socketFD)
            socketFD = -1
        }
    }

    private func receiveLoop() {
        var buf = [UInt8](repeating: 0, count: 256)

        while running && socketFD >= 0 {
            let n = recv(socketFD, &buf, buf.count, 0)
            if n > 0 {
                let data = Data(buf[0..<n])
                if let str = String(data: data, encoding: .utf8) {
                    print("[UDP] Received: \(str)")
                }
                parsePacket(data)
            } else if n < 0 && errno != EAGAIN {
                print("[UDP] recv error: \(String(cString: strerror(errno)))")
                break
            }
        }
        print("[UDP] receiveLoop exited")
    }

    private func parsePacket(_ data: Data) {
        // Expect compact JSON: {"s":0,"f":1,"m":"COMPANION"}
        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let state = json["s"] as? Int,
              let frame = json["f"] as? Int,
              let mode = json["m"] as? String else {
            return
        }

        let normX = (json["x"] as? NSNumber)?.floatValue
        let normY = (json["y"] as? NSNumber)?.floatValue
        let direction = json["d"] as? Int
        let weatherType = json["w"] as? Int
        let temperature = (json["t"] as? NSNumber)?.floatValue

        let callback = onStateReceived
        DispatchQueue.main.async {
            callback?(state, frame, mode, normX, normY, direction, weatherType, temperature)
        }
    }
}
