#include "state_broadcast.h"
#include <WiFiUdp.h>
#include "utils.h"

static WiFiUDP udp;
static Timer broadcastTimer{200}; // 5Hz

static constexpr uint16_t BROADCAST_PORT = 19820;
static char unicastHost[64] = {0};

void stateBroadcastBegin(const char* target) {
    udp.begin(BROADCAST_PORT);
    if (target && strlen(target) > 0) {
        strncpy(unicastHost, target, sizeof(unicastHost) - 1);
        unicastHost[sizeof(unicastHost) - 1] = '\0';
    }
}

void stateBroadcastTick(int state, int frame, const char* mode,
                        float normX, float normY, int direction,
                        int weatherType, float temperature) {
    if (!broadcastTimer.tick()) return;

    char buf[128];
    int len;
    if (temperature > -999) {
        len = snprintf(buf, sizeof(buf),
            "{\"s\":%d,\"f\":%d,\"m\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"d\":%d,\"w\":%d,\"t\":%.1f}",
            state, frame, mode, normX, normY, direction, weatherType, temperature);
    } else {
        len = snprintf(buf, sizeof(buf),
            "{\"s\":%d,\"f\":%d,\"m\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"d\":%d,\"w\":%d}",
            state, frame, mode, normX, normY, direction, weatherType);
    }

    // Broadcast (works on normal routers)
    udp.beginPacket("255.255.255.255", BROADCAST_PORT);
    udp.write((const uint8_t*)buf, len);
    udp.endPacket();

    // Unicast to gateway host (works on iPhone hotspot)
    if (unicastHost[0]) {
        udp.beginPacket(unicastHost, BROADCAST_PORT);
        udp.write((const uint8_t*)buf, len);
        udp.endPacket();
    }
}
