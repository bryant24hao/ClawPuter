#include "weather_client.h"
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Timeout for HTTP read operations (10 seconds)
static constexpr unsigned long HTTP_TIMEOUT_MS = 10000;

// Skip HTTP response headers (zero heap allocation).
// Detects \r\n\r\n (standard) or \n\n (lenient) as end-of-headers.
// Also scans for "Transfer-Encoding: chunked" and sets *chunked flag.
static bool skipHeaders(WiFiClientSecure& client, unsigned long deadline, bool* chunked) {
    *chunked = false;
    // State: 0=in content, 1=saw \r, 2=saw \n (line ended), 3=saw \r after line end
    int state = 0;
    // Ring buffer to detect "chunked" keyword
    char ring[7] = {};
    int ringPos = 0;
    while (client.connected() && millis() < deadline) {
        if (!client.available()) { delay(1); continue; }
        char c = client.read();

        // Case-insensitive scan for "chunked"
        ring[ringPos] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        ringPos = (ringPos + 1) % 7;
        static const char target[] = "chunked";
        bool match = true;
        for (int i = 0; i < 7 && match; i++) {
            if (ring[(ringPos + i) % 7] != target[i]) match = false;
        }
        if (match) *chunked = true;

        switch (state) {
            case 0:
                if (c == '\r') state = 1;
                else if (c == '\n') state = 2;
                break;
            case 1:
                if (c == '\n') state = 2;
                else if (c == '\r') state = 1;
                else state = 0;
                break;
            case 2:
                if (c == '\n') return true;
                if (c == '\r') state = 3;
                else state = 0;
                break;
            case 3:
                if (c == '\n') return true;
                else state = 0;
                break;
        }
    }
    return false;
}

// Read HTTP body into buffer with deadline.
// Handles chunked transfer-encoding or reads until connection closes.
static int readBody(WiFiClientSecure& client, char* buf, int bufSize, unsigned long deadline, bool chunked) {
    int len = 0;
    if (chunked) {
        while (len < bufSize - 1 && millis() < deadline) {
            // Read chunk size line
            char sizeBuf[16];
            int sizePos = 0;
            while (sizePos < (int)sizeof(sizeBuf) - 1 && millis() < deadline) {
                if (!client.available()) {
                    if (!client.connected()) goto done;
                    delay(1); continue;
                }
                char c = client.read();
                if (c == '\n') break;
                if (c != '\r') sizeBuf[sizePos++] = c;
            }
            sizeBuf[sizePos] = '\0';
            int chunkSize = (int)strtol(sizeBuf, nullptr, 16);
            if (chunkSize <= 0) break;

            for (int i = 0; i < chunkSize && len < bufSize - 1 && millis() < deadline; i++) {
                while (!client.available() && client.connected() && millis() < deadline) delay(1);
                if (client.available()) buf[len++] = client.read();
            }
            // Skip trailing \r\n
            for (int i = 0; i < 2 && millis() < deadline; i++) {
                while (!client.available() && client.connected() && millis() < deadline) delay(1);
                if (client.available()) client.read();
            }
        }
    } else {
        while (len < bufSize - 1 && millis() < deadline) {
            if (client.available()) {
                buf[len++] = client.read();
            } else if (!client.connected()) {
                break;
            } else {
                delay(1);
            }
        }
    }
done:
    buf[len] = '\0';
    return len;
}

void WeatherClient::begin(const String& city) {
    Serial.printf("[WEATHER] Init with city: %s\n", city.c_str());
    if (resolveCity(city)) {
        fetchWeather();
    }
    Serial.printf("[WEATHER] Init done, heap=%d\n", ESP.getFreeHeap());
}

void WeatherClient::update() {
    if (!hasCoords) return;
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        fetchWeather();
    }
}

bool WeatherClient::resolveCity(const String& city) {
    if (city.length() == 0) {
        Serial.println("[WEATHER] No city configured");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect("geocoding-api.open-meteo.com", 443)) {
        Serial.println("[WEATHER] Geocoding connection failed");
        return false;
    }

    char path[160];
    int written = snprintf(path, sizeof(path), "/v1/search?name=%s&count=1&language=en", city.c_str());
    if (written >= (int)sizeof(path)) {
        Serial.println("[WEATHER] City name too long");
        client.stop();
        return false;
    }

    client.printf("GET %s HTTP/1.1\r\n", path);
    client.println("Host: geocoding-api.open-meteo.com");
    client.println("Connection: close");
    client.println();

    unsigned long deadline = millis() + HTTP_TIMEOUT_MS;

    bool chunked = false;
    if (!skipHeaders(client, deadline, &chunked)) {
        Serial.println("[WEATHER] Geocoding timeout (headers)");
        client.stop();
        return false;
    }

    char body[512];
    int bodyLen = readBody(client, body, sizeof(body), deadline, chunked);
    client.stop();

    if (bodyLen == 0) {
        Serial.println("[WEATHER] Geocoding empty response");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, bodyLen);
    if (err) {
        Serial.printf("[WEATHER] Geocoding JSON error: %s\n", err.c_str());
        return false;
    }

    JsonArray results = doc["results"];
    if (results.isNull() || results.size() == 0) {
        Serial.printf("[WEATHER] City not found: %s\n", city.c_str());
        return false;
    }

    lat = results[0]["latitude"] | 0.0f;
    lon = results[0]["longitude"] | 0.0f;
    hasCoords = true;

    Serial.printf("[WEATHER] Geocoded %s -> %.2f, %.2f\n", city.c_str(), lat, lon);
    return true;
}

bool WeatherClient::fetchWeather() {
    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect("api.open-meteo.com", 443)) {
        Serial.println("[WEATHER] Weather API connection failed");
        return false;
    }

    char path[192];
    snprintf(path, sizeof(path),
        "/v1/forecast?latitude=%.2f&longitude=%.2f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,is_day&timezone=auto",
        lat, lon);

    client.printf("GET %s HTTP/1.1\r\n", path);
    client.println("Host: api.open-meteo.com");
    client.println("Connection: close");
    client.println();

    unsigned long deadline = millis() + HTTP_TIMEOUT_MS;

    bool chunked = false;
    if (!skipHeaders(client, deadline, &chunked)) {
        Serial.println("[WEATHER] Weather timeout (headers)");
        client.stop();
        return false;
    }

    char body[512];
    int bodyLen = readBody(client, body, sizeof(body), deadline, chunked);
    client.stop();

    if (bodyLen == 0) {
        Serial.println("[WEATHER] Weather empty response");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, bodyLen);
    if (err) {
        Serial.printf("[WEATHER] Weather JSON error: %s\n", err.c_str());
        return false;
    }

    JsonObject current = doc["current"];
    if (current.isNull()) {
        Serial.println("[WEATHER] No 'current' field in response");
        return false;
    }

    data.temperature = current["temperature_2m"] | 0.0f;
    data.humidity = current["relative_humidity_2m"] | 0;
    int code = current["weather_code"] | 0;
    data.isDay = (current["is_day"] | 1) != 0;
    data.type = codeToType(code);
    data.valid = true;
    lastUpdate = millis();

    Serial.printf("[WEATHER] %.1f C, %d%% RH, code=%d, type=%d, day=%d\n",
                  data.temperature, data.humidity, code, (int)data.type, data.isDay);
    return true;
}

WeatherType WeatherClient::codeToType(int code) {
    if (code == 0)                         return WeatherType::CLEAR;
    if (code >= 1 && code <= 2)            return WeatherType::PARTLY_CLOUDY;
    if (code == 3)                         return WeatherType::OVERCAST;
    if (code == 45 || code == 48)          return WeatherType::FOG;
    if (code >= 51 && code <= 57)          return WeatherType::DRIZZLE;
    if ((code >= 61 && code <= 67) ||
        (code >= 80 && code <= 82))        return WeatherType::RAIN;
    if ((code >= 71 && code <= 77) ||
        (code >= 85 && code <= 86))        return WeatherType::SNOW;
    if (code >= 95 && code <= 99)          return WeatherType::THUNDER;
    return WeatherType::UNKNOWN;
}
