#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static String ssid;
static String password;
static String apiKey;
static String gatewayHost;
static String gatewayPort;
static String gatewayToken;
static String sttHost;
static String sttPort;
static String ssid2;
static String password2;
static String gatewayHost2;
static String city;
static String wifiLocalIp;
static String wifiGateway;
static String wifiSubnet;
static String wifiDns1;
static String wifiDns2;
static uint8_t brightness = 80;
static uint8_t volume = 100;
static uint16_t screenOffTimeoutSec = 0;

bool Config::load() {
    prefs.begin("companion", true); // read-only
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    apiKey = prefs.getString("apikey", "");
    gatewayHost = prefs.getString("gw_host", "");
    gatewayPort = prefs.getString("gw_port", "");
    gatewayToken = prefs.getString("gw_token", "");
    sttHost = prefs.getString("stt_host", "");
    sttPort = prefs.getString("stt_port", "");
    ssid2 = prefs.getString("ssid2", "");
    password2 = prefs.getString("pass2", "");
    gatewayHost2 = prefs.getString("gw_host2", "");
    city = prefs.getString("city", "");
    wifiLocalIp = prefs.getString("wifi_ip", "");
    wifiGateway = prefs.getString("wifi_gw", "");
    wifiSubnet = prefs.getString("wifi_subnet", "");
    wifiDns1 = prefs.getString("wifi_dns1", "");
    wifiDns2 = prefs.getString("wifi_dns2", "");
    brightness = prefs.getUChar("brightness", 80);
    volume = prefs.getUChar("volume", 100);
    if (volume > 100) volume = 100;
    screenOffTimeoutSec = prefs.getUShort("scr_timeout", 0);
    prefs.end();
    return ssid.length() > 0;
}

void Config::save() {
    prefs.begin("companion", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.putString("apikey", apiKey);
    prefs.putString("gw_host", gatewayHost);
    prefs.putString("gw_port", gatewayPort);
    prefs.putString("gw_token", gatewayToken);
    prefs.putString("stt_host", sttHost);
    prefs.putString("stt_port", sttPort);
    prefs.putString("ssid2", ssid2);
    prefs.putString("pass2", password2);
    prefs.putString("gw_host2", gatewayHost2);
    prefs.putString("city", city);
    prefs.putString("wifi_ip", wifiLocalIp);
    prefs.putString("wifi_gw", wifiGateway);
    prefs.putString("wifi_subnet", wifiSubnet);
    prefs.putString("wifi_dns1", wifiDns1);
    prefs.putString("wifi_dns2", wifiDns2);
    prefs.putUChar("brightness", brightness);
    prefs.putUChar("volume", volume);
    prefs.putUShort("scr_timeout", screenOffTimeoutSec);
    prefs.end();
}

void Config::reset() {
    ssid = "";
    password = "";
    apiKey = "";
    gatewayHost = "";
    gatewayPort = "";
    gatewayToken = "";
    sttHost = "";
    sttPort = "";
    ssid2 = "";
    password2 = "";
    gatewayHost2 = "";
    city = "";
    wifiLocalIp = "";
    wifiGateway = "";
    wifiSubnet = "";
    wifiDns1 = "";
    wifiDns2 = "";
    brightness = 80;
    volume = 100;
    screenOffTimeoutSec = 0;
    save();
}

const String& Config::getSSID() { return ssid; }
const String& Config::getPassword() { return password; }
const String& Config::getApiKey() { return apiKey; }
const String& Config::getGatewayHost() { return gatewayHost; }
const String& Config::getGatewayPort() { return gatewayPort; }
const String& Config::getGatewayToken() { return gatewayToken; }
const String& Config::getSttHost() { return sttHost; }
const String& Config::getSttPort() { return sttPort; }
const String& Config::getSSID2() { return ssid2; }
const String& Config::getPassword2() { return password2; }
const String& Config::getGatewayHost2() { return gatewayHost2; }
const String& Config::getCity() { return city; }
const String& Config::getWifiLocalIp() { return wifiLocalIp; }
const String& Config::getWifiGateway() { return wifiGateway; }
const String& Config::getWifiSubnet() { return wifiSubnet; }
const String& Config::getWifiDns1() { return wifiDns1; }
const String& Config::getWifiDns2() { return wifiDns2; }
uint8_t Config::getBrightness() { return brightness; }
uint8_t Config::getVolume() { return volume; }
uint16_t Config::getScreenOffTimeoutSec() { return screenOffTimeoutSec; }

void Config::setSSID(const String& s) { ssid = s; }
void Config::setPassword(const String& p) { password = p; }
void Config::setApiKey(const String& k) { apiKey = k; }
void Config::setGatewayHost(const String& h) { gatewayHost = h; }
void Config::setGatewayPort(const String& p) { gatewayPort = p; }
void Config::setGatewayToken(const String& t) { gatewayToken = t; }
void Config::setSttHost(const String& h) { sttHost = h; }
void Config::setSttPort(const String& p) { sttPort = p; }
void Config::setSSID2(const String& s) { ssid2 = s; }
void Config::setPassword2(const String& p) { password2 = p; }
void Config::setGatewayHost2(const String& h) { gatewayHost2 = h; }
void Config::setCity(const String& c) { city = c; }
void Config::setWifiLocalIp(const String& ip) { wifiLocalIp = ip; }
void Config::setWifiGateway(const String& g) { wifiGateway = g; }
void Config::setWifiSubnet(const String& s) { wifiSubnet = s; }
void Config::setWifiDns1(const String& d) { wifiDns1 = d; }
void Config::setWifiDns2(const String& d) { wifiDns2 = d; }
void Config::setBrightness(uint8_t value) { brightness = value; }
void Config::setVolume(uint8_t value) { volume = value > 100 ? 100 : value; }
void Config::setScreenOffTimeoutSec(uint16_t seconds) { screenOffTimeoutSec = seconds; }

bool Config::isValid() { return ssid.length() > 0; }
