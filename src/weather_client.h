#pragma once
#include <Arduino.h>

enum class WeatherType : uint8_t {
    CLEAR,
    PARTLY_CLOUDY,
    OVERCAST,
    FOG,
    DRIZZLE,
    RAIN,
    SNOW,
    THUNDER,
    UNKNOWN
};

struct WeatherData {
    float temperature = 0;
    int humidity = 0;       // relative humidity %
    WeatherType type = WeatherType::UNKNOWN;
    bool isDay = true;
    bool valid = false;
};

class WeatherClient {
public:
    void begin(const String& city);
    void update();
    const WeatherData& getData() const { return data; }

private:
    float lat = 0, lon = 0;
    bool hasCoords = false;
    WeatherData data;
    unsigned long lastUpdate = 0;
    static constexpr unsigned long UPDATE_INTERVAL = 15 * 60 * 1000; // 15 min

    bool resolveCity(const String& city);
    bool fetchWeather();
    static WeatherType codeToType(int code);
};
