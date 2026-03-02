#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static String ssid;
static String password;
static String apiKey;

bool Config::load() {
    prefs.begin("companion", true); // read-only
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    apiKey = prefs.getString("apikey", "");
    prefs.end();
    return ssid.length() > 0;
}

void Config::save() {
    prefs.begin("companion", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.putString("apikey", apiKey);
    prefs.end();
}

void Config::reset() {
    prefs.begin("companion", false);
    prefs.clear();
    prefs.end();
    ssid = "";
    password = "";
    apiKey = "";
}

const String& Config::getSSID() { return ssid; }
const String& Config::getPassword() { return password; }
const String& Config::getApiKey() { return apiKey; }

void Config::setSSID(const String& s) { ssid = s; }
void Config::setPassword(const String& p) { password = p; }
void Config::setApiKey(const String& k) { apiKey = k; }

bool Config::isValid() { return ssid.length() > 0; }
