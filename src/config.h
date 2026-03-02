#pragma once
#include <Arduino.h>

namespace Config {
    // Load saved config from NVS. Returns true if WiFi credentials exist.
    bool load();

    // Save current config to NVS.
    void save();

    // Clear all saved config.
    void reset();

    // Getters
    const String& getSSID();
    const String& getPassword();
    const String& getApiKey();

    // Setters
    void setSSID(const String& ssid);
    void setPassword(const String& password);
    void setApiKey(const String& key);

    // Check if config is valid (has WiFi credentials)
    bool isValid();
}
