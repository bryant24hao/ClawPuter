#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>
#include "utils.h"
#include "config.h"
#include "companion.h"
#include "chat.h"
#include "ai_client.h"
#include "state_broadcast.h"
#include "cmd_server.h"
#include "voice_input.h"
#include "tts_playback.h"
#include "weather_client.h"

// ── Build-time defaults (may be empty if not set in .env) ──
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef CLAUDE_API_KEY
#define CLAUDE_API_KEY ""
#endif
#ifndef OPENCLAW_HOST
#define OPENCLAW_HOST ""
#endif
#ifndef OPENCLAW_PORT
#define OPENCLAW_PORT ""
#endif
#ifndef OPENCLAW_TOKEN
#define OPENCLAW_TOKEN ""
#endif
#ifndef STT_PROXY_HOST
#define STT_PROXY_HOST ""
#endif
#ifndef STT_PROXY_PORT
#define STT_PROXY_PORT "8090"
#endif
#ifndef WIFI_SSID2
#define WIFI_SSID2 ""
#endif
#ifndef WIFI_PASS2
#define WIFI_PASS2 ""
#endif
#ifndef OPENCLAW_HOST2
#define OPENCLAW_HOST2 ""
#endif
#ifndef DEFAULT_CITY
#define DEFAULT_CITY "Beijing"
#endif

// ── Globals ──
M5Canvas canvas(&M5Cardputer.Display);
Companion companion;
Chat chat;
AIClient aiClient;
VoiceInput voiceInput;
TTSPlayback ttsPlayback;
WeatherClient weatherClient;
CmdServer cmdServer;

enum class AppMode { SETUP, COMPANION, CHAT };
static AppMode appMode = AppMode::SETUP;
static bool offlineMode = false;

// ── Setup mode state ──
enum class SetupStep { SSID, PASSWORD, GATEWAY_HOST, GATEWAY_PORT, GATEWAY_TOKEN, STT_HOST, CONNECTING };
static SetupStep setupStep = SetupStep::SSID;
static String setupInput;

// ── NTP config ──
static const char* NTP_SERVER = "pool.ntp.org";
static const long  GMT_OFFSET_SEC = 8 * 3600;  // UTC+8 for China
static const int   DAYLIGHT_OFFSET_SEC = 0;

// ── Forward declarations ──
void fillBuildTimeDefaults();
void enterSetupMode();
void updateSetupMode();
void handleSetupKey(char key, bool enter, bool backspace, bool tab);
bool tryConnect(const String& ssid, const String& pass);
void connectWiFi();
void initOnlineServices(bool usedSecondary);
void enterCompanionMode();
void enterChatMode();

// ══════════════════════════════════════════════════════════════
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Speaker.setVolume(255);
    Serial.begin(115200);
    delay(500);
    Serial.println("[BOOT] Starting...");

    // Screen setup
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(80);
    canvas.createSprite(SCREEN_W, SCREEN_H);
    canvas.setTextWrap(false);

    // Load NVS, then fill empty fields with build-time defaults
    Config::load();
    fillBuildTimeDefaults();
    Config::save();

    // Play boot animation
    playBootAnimation(canvas);

    if (Config::isValid()) {
        connectWiFi();
    } else {
        enterSetupMode();
    }
}

// Fill NVS fields that are empty with build-time compile values.
// Does NOT overwrite user-modified values.
void fillBuildTimeDefaults() {
    if (Config::getSSID().length() == 0 && strlen(WIFI_SSID) > 0)
        Config::setSSID(WIFI_SSID);
    if (Config::getPassword().length() == 0 && strlen(WIFI_PASS) > 0)
        Config::setPassword(WIFI_PASS);
    if (Config::getApiKey().length() == 0 && strlen(CLAUDE_API_KEY) > 0)
        Config::setApiKey(CLAUDE_API_KEY);
    if (Config::getGatewayHost().length() == 0 && strlen(OPENCLAW_HOST) > 0)
        Config::setGatewayHost(OPENCLAW_HOST);
    if (Config::getGatewayPort().length() == 0 && strlen(OPENCLAW_PORT) > 0)
        Config::setGatewayPort(OPENCLAW_PORT);
    if (Config::getGatewayToken().length() == 0 && strlen(OPENCLAW_TOKEN) > 0)
        Config::setGatewayToken(OPENCLAW_TOKEN);
    if (Config::getSttHost().length() == 0 && strlen(STT_PROXY_HOST) > 0)
        Config::setSttHost(STT_PROXY_HOST);
    if (Config::getSttPort().length() == 0 && strlen(STT_PROXY_PORT) > 0)
        Config::setSttPort(STT_PROXY_PORT);
    if (Config::getSSID2().length() == 0 && strlen(WIFI_SSID2) > 0)
        Config::setSSID2(WIFI_SSID2);
    if (Config::getPassword2().length() == 0 && strlen(WIFI_PASS2) > 0)
        Config::setPassword2(WIFI_PASS2);
    if (Config::getGatewayHost2().length() == 0 && strlen(OPENCLAW_HOST2) > 0)
        Config::setGatewayHost2(OPENCLAW_HOST2);
    if (Config::getCity().length() == 0) {
        if (strlen(DEFAULT_CITY) > 0)
            Config::setCity(DEFAULT_CITY);
        else
            Config::setCity("Beijing"); // fallback when env var not set
    }
}

// ══════════════════════════════════════════════════════════════
void loop() {
    M5Cardputer.update();

    // Handle keyboard
    bool keyPressed = M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
    Keyboard_Class::KeysState keys;
    if (keyPressed) {
        keys = M5Cardputer.Keyboard.keysState();
    }

    switch (appMode) {
        case AppMode::SETUP:
            if (keyPressed) {
                bool enter = keys.enter;
                bool backspace = keys.del;
                bool tab = keys.tab;
                char key = 0;
                if (keys.word.size() > 0) key = keys.word[0];
                handleSetupKey(key, enter, backspace, tab);
            }
            updateSetupMode();
            break;

        case AppMode::COMPANION: {
            // Use keysState() for continuous movement detection (hold-to-move)
            auto ks = M5Cardputer.Keyboard.keysState();

            if (keyPressed) {
                if (ks.tab) {
                    playTransition(canvas, true);
                    enterChatMode();
                    break;
                }
                // Fn+W = toggle weather simulation mode
                if (ks.fn && ks.word.size() > 0 && ks.word[0] == 'w') {
                    companion.toggleWeatherSim();
                    break;
                }
                // Fn+0 = debug: set moisture to 0
                if (ks.fn && ks.word.size() > 0 && ks.word[0] == '0') {
                    companion.debugSetMoisture(0);
                    break;
                }
                // Fn+R = reset config
                if (ks.fn && ks.word.size() > 0 && ks.word[0] == 'r') {
                    WiFi.disconnect(true);
                    Config::reset();
                    fillBuildTimeDefaults();
                    Config::save();
                    enterSetupMode();
                    break;
                }
                // Digit keys 1-8 in weather sim mode
                if (companion.isWeatherSimMode() && ks.word.size() > 0) {
                    char ch = ks.word[0];
                    if (ch >= '1' && ch <= '8') {
                        companion.setSimWeatherType(ch - '0');
                        break;
                    }
                }
                // H key: spray water
                if (ks.word.size() > 0 && ks.word[0] == 'h') {
                    companion.spray();
                }
                // Non-movement keys → companion handles (space/enter for happy, etc.)
                char key = 0;
                if (ks.enter) key = '\n';
                else if (ks.word.size() > 0) {
                    char ch = ks.word[0];
                    // Skip movement keys — handled below via continuous detection
                    if (ch != ';' && ch != '.' && ch != ',' && ch != '/')
                        key = ch;
                }
                if (key) companion.handleKey(key);
            }

            // Continuous direction keys: ;=up .=down ,=left /=right
            // These work even when held down (no isChange required)
            for (char ch : ks.word) {
                switch (ch) {
                    case ';': companion.move(0, -1); break;
                    case '.': companion.move(0,  1); break;
                    case ',': companion.move(-1, 0); break;
                    case '/': companion.move( 1, 0); break;
                }
            }

            if (!offlineMode) weatherClient.update();
            if (!companion.isWeatherSimMode()) {
                companion.setWeather(weatherClient.getData());
            }
            companion.update(canvas);
            companion.drawNotificationOverlay(canvas);
            canvas.pushSprite(0, 0);
            break;
        }

        case AppMode::CHAT: {
            // ── Keyboard state via keysState() + manual edge detection ──
            // We bypass isChange() which fails after blocking STT calls.
            // keysState() always returns fresh data — proven reliable in all rounds.
            auto ks = M5Cardputer.Keyboard.keysState();
            static bool pFn = false, pEnter = false, pDel = false, pTab = false;
            static char pWordChar = 0;
            bool didBlock = false;

            bool fnDown = ks.fn && !pFn;
            bool fnUp = !ks.fn && pFn;
            bool fnAlone = ks.fn && ks.word.size() == 0
                           && !ks.tab && !ks.enter && !ks.del;

            // ── Voice input: Fn push-to-talk (only when online) ──
            if (!offlineMode && fnDown && fnAlone && !voiceInput.isRecording()
                && !aiClient.isBusy() && !voiceInput.isTranscribing()
                && !ttsPlayback.isPlaying()) {
                voiceInput.startRecording();
            }
            if (fnUp && voiceInput.isRecording()) {
                chat.update(canvas);
                voiceInput.drawTranscribingBar(canvas);
                canvas.pushSprite(0, 0);
                if (voiceInput.stopRecording()) {
                    String text = voiceInput.takeResult();
                    if (text.length() > 0) chat.setInput(text);
                }
                didBlock = true;
            }

            // Auto-stop at max duration
            if (!didBlock && voiceInput.isRecording()
                && voiceInput.getRecordingDuration() >= 3.0f) {
                chat.update(canvas);
                voiceInput.drawTranscribingBar(canvas);
                canvas.pushSprite(0, 0);
                if (voiceInput.stopRecording()) {
                    String text = voiceInput.takeResult();
                    if (text.length() > 0) chat.setInput(text);
                }
                didBlock = true;
            }

            // Resync keyboard after blocking STT call
            if (didBlock) {
                M5Cardputer.update();
                ks = M5Cardputer.Keyboard.keysState();
            }

            // Compute edges for regular keys (suppressed on blocking frame)
            bool enterDown = !didBlock && ks.enter && !pEnter;
            bool delDown   = !didBlock && ks.del && !pDel;
            bool tabDown   = !didBlock && ks.tab && !pTab;
            char curWordChar = (ks.word.size() > 0) ? ks.word[0] : 0;
            bool charDown  = !didBlock && curWordChar != 0 && curWordChar != pWordChar;

            // Save baseline for next frame
            pFn = ks.fn; pEnter = ks.enter; pDel = ks.del; pTab = ks.tab;
            pWordChar = curWordChar;

            // ── Normal keyboard input ──
            if (!voiceInput.isRecording()) {
                if (tabDown) {
                    playTransition(canvas, false);
                    enterCompanionMode();
                    break;
                }
                if (enterDown) {
                    chat.handleEnter();
                } else if (delDown) {
                    chat.handleBackspace();
                } else if (charDown) {
                    char key = ks.word[0];
                    if (ks.fn && key == ';') {
                        chat.scrollUp();
                    } else if (ks.fn && key == '/') {
                        chat.scrollDown();
                    } else if (!ks.fn) {
                        // H key: spray when thirsty, type normally otherwise
                        if (key == 'h' && companion.getMoistureLevel() <= 1) {
                            companion.spray();
                        } else {
                            chat.handleKey(key);
                        }
                    }
                }
            }

            // Check if chat has a message to send
            if (chat.hasPendingMessage() && !aiClient.isBusy()) {
                if (offlineMode) {
                    // Offline: show error message instead of attempting AI request
                    String msg = chat.takePendingMessage();
                    chat.appendAIToken("[Offline] No network connection");
                    chat.onAIResponseComplete();
                } else {
                    String msg = chat.takePendingMessage();
                    Serial.printf("[CHAT] Sending: %s\n", msg.c_str());
                    companion.triggerTalk();

                    // Broadcast user message to desktop
                    stateBroadcastChatMsg("user", msg.c_str());

                    // Set pixel art mode if /draw command
                    bool isDrawCmd = chat.isDrawCommand();
                    int drawSz = chat.getDrawSize();
                    aiClient.setPixelArtMode(isDrawCmd, drawSz);

                    // Update AI with companion state
                    AIClient::CompanionContext ctx;
                    ctx.moisture = companion.getMoistureLevel();
                    ctx.weatherType = static_cast<int>(companion.getWeatherType());
                    ctx.temperature = companion.getTemperature();
                    ctx.humidity = companion.getHumidityPercent();
                    aiClient.setCompanionContext(ctx);

                    bool aiError = false;
                    aiClient.sendMessage(msg,
                        // onToken — receives const char* (zero heap allocation)
                        [](const char* token) {
                            chat.appendAIToken(token);
                            // Typing sound — short chirp, throttled
                            static unsigned long lastBeep = 0;
                            if (millis() - lastBeep > 80) {
                                M5Cardputer.Speaker.tone(1800, 15);
                                lastBeep = millis();
                            }
                            // Redraw while streaming
                            chat.update(canvas);
                            canvas.pushSprite(0, 0);
                        },
                        // onDone — don't triggerIdle yet, TTS may follow
                        []() {
                            chat.onAIResponseComplete();
                        },
                        // onError
                        [&aiError](const String& error) {
                            // Build error string on stack to avoid heap allocation
                            char errBuf[64];
                            snprintf(errBuf, sizeof(errBuf), "[Error: %s]", error.c_str());
                            chat.appendAIToken(errBuf);
                            chat.onAIResponseComplete();
                            aiError = true;
                        }
                    );

                    // Broadcast AI response to desktop
                    if (!aiError && aiClient.getLastResponse().length() > 0) {
                        stateBroadcastChatMsg("ai", aiClient.getLastResponse().c_str());
                    }

                    // Broadcast pixel art to desktop if one was just parsed
                    bool hasPixelArt = chat.hasNewPixelArt() || isDrawCmd;
                    if (chat.hasNewPixelArt()) {
                        char rows[16][17];
                        int paSize = chat.getLastPixelArtRows(rows, 16);
                        if (paSize > 0) {
                            const char* rowPtrs[16];
                            for (int i = 0; i < paSize; i++) rowPtrs[i] = rows[i];
                            stateBroadcastPixelArt(paSize, rowPtrs, paSize);
                        }
                        chat.clearNewPixelArt();
                    }

                    // TTS: speak the AI response (skip for pixel art)
                    if (!aiError && !hasPixelArt && aiClient.getLastResponse().length() > 0) {
                        // Show "Speaking..." indicator while downloading PCM
                        chat.update(canvas);
                        ttsPlayback.drawSpeakingBar(canvas);
                        canvas.pushSprite(0, 0);

                        ttsPlayback.requestAndPlay(aiClient.getLastResponse().c_str());
                        // playRaw is non-blocking (DMA queue), returns immediately
                    }
                    companion.triggerIdle();

                    // Resync keyboard after blocking AI + TTS download.
                    // Without this, the Enter that sent the message still has
                    // enterDown=true in this iteration, immediately stopping TTS.
                    M5Cardputer.update();
                    ks = M5Cardputer.Keyboard.keysState();
                    pFn = ks.fn; pEnter = ks.enter; pDel = ks.del; pTab = ks.tab;
                    pWordChar = (ks.word.size() > 0) ? ks.word[0] : 0;
                    enterDown = delDown = tabDown = charDown = false;
                    fnDown = false;
                }
            }

            // ── Stop TTS on any key press ──
            if (ttsPlayback.isPlaying() && (enterDown || delDown || tabDown || charDown || fnDown)) {
                ttsPlayback.stop();
                Serial.println("[TTS] Playback interrupted by key press");
            }

            // ── Sync state + Draw ──
            chat.setMoistureLevel(companion.getMoistureLevel());
            chat.setAIThinking(aiClient.thinkingDetected);
            chat.update(canvas);
            // Override input bar if recording, transcribing, or speaking
            if (voiceInput.isRecording()) {
                voiceInput.drawRecordingBar(canvas);
            } else if (ttsPlayback.isPlaying()) {
                ttsPlayback.drawSpeakingBar(canvas);
            }
            companion.drawNotificationOverlay(canvas);
            canvas.pushSprite(0, 0);
            break;
        }
    }

    // Process incoming TCP commands from desktop app
    if (!offlineMode && appMode != AppMode::SETUP) {
        cmdServer.tick();
    }

    // Broadcast state over UDP for desktop sync (skip if offline or not yet initialized)
    if (!offlineMode && appMode != AppMode::SETUP) {
        const char* modeStr = "COMPANION";
        if (appMode == AppMode::CHAT) modeStr = "CHAT";
        int wType = companion.hasValidWeather() ? static_cast<int>(companion.getWeatherType()) : -1;
        float temp = companion.hasValidWeather() ? companion.getTemperature() : -999;
        stateBroadcastTick(static_cast<int>(companion.getState()),
                           companion.getFrameIndex(), modeStr,
                           companion.getNormX(), companion.getNormY(),
                           companion.isFacingLeft() ? 1 : 0, wType, temp,
                           companion.getMoistureLevel(), companion.getHumidityPercent());
    }

    delay(16); // ~60fps cap
}

// ══════════════════════════════════════════════════════════════
// Setup Mode — redesigned with default value display + Tab cancel
// ══════════════════════════════════════════════════════════════

void enterSetupMode() {
    appMode = AppMode::SETUP;
    setupStep = SetupStep::SSID;
    setupInput = "";
}

// Helper: get display hint for current value (for setup screen)
static void getDefaultHint(char* buf, int bufSize, const String& value, bool isPassword) {
    if (value.length() == 0) {
        snprintf(buf, bufSize, "(empty)");
    } else if (isPassword) {
        snprintf(buf, bufSize, "[%d chars set]", value.length());
    } else {
        snprintf(buf, bufSize, "[%s]", value.c_str());
    }
}

void updateSetupMode() {
    canvas.fillScreen(Color::BG_DAY);
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(1);

    canvas.drawString("=== Setup ===", 70, 4);

    char hint[64];

    switch (setupStep) {
        case SetupStep::SSID:
            canvas.drawString("WiFi SSID:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getSSID(), false);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 80, 25);
            canvas.setTextColor(Color::WHITE);
            canvas.drawString((setupInput + "_").c_str(), 10, 42);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::PASSWORD:
            canvas.drawString("WiFi Password:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getPassword(), true);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 100, 25);
            canvas.setTextColor(Color::WHITE);
            {
                char masked[64];
                int len = setupInput.length();
                if (len > 62) len = 62;
                memset(masked, '*', len);
                masked[len] = '_';
                masked[len + 1] = '\0';
                canvas.drawString(masked, 10, 42);
            }
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::GATEWAY_HOST:
            canvas.drawString("Gateway Host:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getGatewayHost(), false);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 100, 25);
            canvas.setTextColor(Color::WHITE);
            canvas.drawString((setupInput + "_").c_str(), 10, 42);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::GATEWAY_PORT:
            canvas.drawString("Gateway Port:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getGatewayPort(), false);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 100, 25);
            canvas.setTextColor(Color::WHITE);
            canvas.drawString((setupInput + "_").c_str(), 10, 42);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::GATEWAY_TOKEN:
            canvas.drawString("Gateway Token:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getGatewayToken(), true);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 105, 25);
            canvas.setTextColor(Color::WHITE);
            {
                String display = setupInput + "_";
                if (display.length() > 35) {
                    display = "..." + display.substring(display.length() - 32);
                }
                canvas.drawString(display.c_str(), 10, 42);
            }
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::STT_HOST:
            canvas.drawString("STT Proxy Host:", 10, 25);
            getDefaultHint(hint, sizeof(hint), Config::getSttHost(), false);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString(hint, 110, 25);
            canvas.setTextColor(Color::WHITE);
            canvas.drawString((setupInput + "_").c_str(), 10, 42);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] keep/confirm", 10, 62);
            canvas.drawString("[Tab] cancel", 170, 62);
            break;

        case SetupStep::CONNECTING:
            canvas.drawString("Connecting to WiFi...", 50, 55);
            {
                static int dots = 0;
                static const char* dotStr[] = {"", ".", "..", "..."};
                canvas.drawString(dotStr[dots % 4], 170, 55);
                dots++;
            }
            break;
    }

    canvas.pushSprite(0, 0);
}

void handleSetupKey(char key, bool enter, bool backspace, bool tab) {
    // Tab = exit setup, go back to companion
    if (tab) {
        if (WiFi.status() != WL_CONNECTED) offlineMode = true;
        enterCompanionMode();
        return;
    }

    if (backspace && setupInput.length() > 0) {
        setupInput.remove(setupInput.length() - 1);
        return;
    }

    if (key && !enter) {
        setupInput += key;
        return;
    }

    if (!enter) return;

    // Enter pressed — empty input means "keep current value"
    switch (setupStep) {
        case SetupStep::SSID:
            if (setupInput.length() > 0) {
                Config::setSSID(setupInput);
            }
            // If empty and current SSID is also empty, stay on this step
            if (Config::getSSID().length() == 0) break;
            setupInput = "";
            setupStep = SetupStep::PASSWORD;
            break;

        case SetupStep::PASSWORD:
            if (setupInput.length() > 0) {
                Config::setPassword(setupInput);
            }
            setupInput = "";
            setupStep = SetupStep::GATEWAY_HOST;
            break;

        case SetupStep::GATEWAY_HOST:
            if (setupInput.length() > 0) {
                Config::setGatewayHost(setupInput);
            }
            setupInput = "";
            setupStep = SetupStep::GATEWAY_PORT;
            break;

        case SetupStep::GATEWAY_PORT:
            if (setupInput.length() > 0) {
                Config::setGatewayPort(setupInput);
            }
            setupInput = "";
            setupStep = SetupStep::GATEWAY_TOKEN;
            break;

        case SetupStep::GATEWAY_TOKEN:
            if (setupInput.length() > 0) {
                Config::setGatewayToken(setupInput);
            }
            setupInput = "";
            setupStep = SetupStep::STT_HOST;
            break;

        case SetupStep::STT_HOST:
            if (setupInput.length() > 0) {
                Config::setSttHost(setupInput);
            }
            Config::save();
            setupInput = "";
            setupStep = SetupStep::CONNECTING;
            connectWiFi();
            break;

        default:
            break;
    }
}

// ══════════════════════════════════════════════════════════════
// WiFi connection — dual WiFi + failure handling
// ══════════════════════════════════════════════════════════════

// Try connecting to a single WiFi network. Returns true on success.
bool tryConnect(const String& ssid, const String& pass) {
    Serial.printf("[WIFI] Trying %s...\n", ssid.c_str());

    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {  // 15 seconds
        delay(500);
        attempts++;

        // Update connecting screen
        canvas.fillScreen(Color::BG_DAY);
        canvas.setTextColor(Color::CLOCK_TEXT);
        canvas.setTextSize(1);

        char msg[64];
        static const char* dotSuffix[] = {".", "..", "...", "...."};
        snprintf(msg, sizeof(msg), "Connecting to %s%s", ssid.c_str(), dotSuffix[attempts % 4]);
        // Truncate if too long for screen, respecting UTF-8 boundaries
        if (strlen(msg) > 38) {
            int cut = 38;
            while (cut > 0 && (msg[cut] & 0xC0) == 0x80) cut--;
            msg[cut] = '\0';
        }
        canvas.drawString(msg, 10, 55);
        canvas.pushSprite(0, 0);
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[WIFI] %s: %s\n", ssid.c_str(), connected ? "OK" : "FAILED");
    return connected;
}

void connectWiFi() {
    offlineMode = false;

    while (true) {
        // Try primary WiFi
        bool connected = tryConnect(Config::getSSID(), Config::getPassword());

        // Try secondary WiFi if primary failed and secondary is configured
        bool usedSecondary = false;
        if (!connected && Config::getSSID2().length() > 0) {
            connected = tryConnect(Config::getSSID2(), Config::getPassword2());
            if (connected) usedSecondary = true;
        }

        if (connected) {
            initOnlineServices(usedSecondary);
            enterCompanionMode();
            return;
        }

        // WiFi failed — show options menu
        canvas.fillScreen(Color::BG_DAY);
        canvas.setTextColor(rgb565(220, 80, 80));
        canvas.setTextSize(1);
        canvas.drawString("WiFi failed!", 80, 20);

        canvas.setTextColor(Color::CLOCK_TEXT);
        canvas.drawString("[Enter]  Retry", 60, 48);
        canvas.drawString("[Fn+R]   Setup wizard", 60, 63);
        canvas.drawString("[Tab]    Offline mode", 60, 78);
        canvas.pushSprite(0, 0);

        // Wait for user choice
        bool retry = false;
        while (true) {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                auto ks = M5Cardputer.Keyboard.keysState();

                if (ks.enter) {
                    retry = true;
                    break;  // break inner loop, outer loop retries
                }
                if (ks.fn && ks.word.size() > 0 && ks.word[0] == 'r') {
                    // Reset + setup wizard
                    WiFi.disconnect(true);
                    Config::reset();
                    fillBuildTimeDefaults();
                    Config::save();
                    enterSetupMode();
                    return;
                }
                if (ks.tab) {
                    // Enter offline mode
                    offlineMode = true;
                    Serial.println("[WIFI] Entering offline mode");
                    enterCompanionMode();
                    return;
                }
            }
            delay(50);
        }
        if (!retry) return;  // safety: should not reach here
    }
}

// Initialize NTP, state broadcast, AI client, and voice input
void initOnlineServices(bool usedSecondary) {
    // Sync time
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    // Show success briefly
    canvas.fillScreen(Color::BG_DAY);
    canvas.setTextColor(Color::CHAT_AI);
    canvas.setTextSize(1);
    canvas.drawString("WiFi connected!", 70, 50);
    canvas.drawString(WiFi.localIP().toString().c_str(), 80, 65);
    canvas.pushSprite(0, 0);
    delay(1000);

    // Use secondary gateway host if connected via secondary WiFi
    String gwHost = Config::getGatewayHost();
    String sttHost = Config::getSttHost();
    if (usedSecondary && Config::getGatewayHost2().length() > 0) {
        String primaryGwHost = gwHost;
        gwHost = Config::getGatewayHost2();
        Serial.printf("[WIFI] Using secondary gateway: %s\n", gwHost.c_str());
        // If STT host was same as primary gateway, switch it too
        if (sttHost == primaryGwHost) {
            sttHost = gwHost;
            Serial.printf("[WIFI] STT host also switched to: %s\n", sttHost.c_str());
        }
    }

    // Init state broadcast (UDP) — broadcast + unicast to gateway host
    stateBroadcastBegin(gwHost.c_str());
    String gwPort = Config::getGatewayPort();
    String gwToken = Config::getGatewayToken();
    String sttPort = Config::getSttPort();

    // Init AI client
    aiClient.begin(Config::getApiKey(), gwHost, gwPort, gwToken);

    // Init weather BEFORE voice buffer — TLS needs heap headroom for handshake.
    // Uses setBufferSizes(1024,512) to minimize TLS heap fragmentation so that
    // the 160KB voice buffer can still find a contiguous block afterward.
    weatherClient.begin(Config::getCity());

    // Init voice input — allocates 160KB contiguous buffer
    voiceInput.begin(sttHost, sttPort);

    // Init TTS playback — reuses voice input's buffer (mic & speaker share GPIO 43)
    ttsPlayback.begin(sttHost, sttPort,
                      voiceInput.getBuffer(), voiceInput.getMaxSamples());

    // Init TCP command server for desktop bidirectional communication
    cmdServer.begin();
    cmdServer.onAnimate([](const char* state) {
        Serial.printf("[CMD] Animate: %s\n", state);
        if (strcmp(state, "happy") == 0) companion.triggerHappy();
        else if (strcmp(state, "idle") == 0) companion.triggerIdle();
        else if (strcmp(state, "sleep") == 0) companion.triggerSleep();
        else if (strcmp(state, "talk") == 0) companion.triggerTalk();
    });
    cmdServer.onText([](const char* text, bool autoSend) {
        Serial.printf("[CMD] Text: '%s' autoSend=%d\n", text, autoSend);
        if (aiClient.isBusy()) return;  // Don't inject while AI is processing
        chat.setInput(String(text));
        if (autoSend && appMode == AppMode::CHAT) {
            chat.handleEnter();
        }
    });
    cmdServer.onNotify([](const char* app, const char* title, const char* body) {
        Serial.printf("[CMD] Notify: [%s] %s - %s\n", app, title, body);
        companion.showNotification(app, title, body);
    });
}

void enterCompanionMode() {
    appMode = AppMode::COMPANION;
    companion.begin(canvas);
}

void enterChatMode() {
    appMode = AppMode::CHAT;
    chat.begin(canvas);
}
