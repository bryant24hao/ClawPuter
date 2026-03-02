#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>
#include "utils.h"
#include "config.h"
#include "companion.h"
#include "chat.h"
#include "ai_client.h"

// ── Globals ──
M5Canvas canvas(&M5Cardputer.Display);
Companion companion;
Chat chat;
AIClient aiClient;

enum class AppMode { SETUP, COMPANION, CHAT };
static AppMode appMode = AppMode::SETUP;

// ── Setup mode state ──
enum class SetupStep { SSID, PASSWORD, API_KEY, CONNECTING };
static SetupStep setupStep = SetupStep::SSID;
static String setupInput;

// ── NTP config ──
static const char* NTP_SERVER = "pool.ntp.org";
static const long  GMT_OFFSET_SEC = 8 * 3600;  // UTC+8 for China
static const int   DAYLIGHT_OFFSET_SEC = 0;

// ── Forward declarations ──
void enterSetupMode();
void updateSetupMode();
void handleSetupKey(char key, bool enter, bool backspace);
void connectWiFi();
void enterCompanionMode();
void enterChatMode();

// ══════════════════════════════════════════════════════════════
void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Screen setup
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(80);
    canvas.createSprite(SCREEN_W, SCREEN_H);
    canvas.setTextWrap(false);

    // Try loading saved config
    if (Config::load() && Config::isValid()) {
        // Show connecting screen
        canvas.fillScreen(Color::BG_DAY);
        canvas.setTextColor(Color::CLOCK_TEXT);
        canvas.setTextSize(1);
        canvas.drawString("Connecting to WiFi...", 40, 60);
        canvas.pushSprite(0, 0);

        connectWiFi();
    } else {
        enterSetupMode();
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
                char key = 0;
                if (keys.word.size() > 0) key = keys.word[0];
                handleSetupKey(key, enter, backspace);
            }
            updateSetupMode();
            break;

        case AppMode::COMPANION:
            if (keyPressed) {
                if (keys.tab) {
                    enterChatMode();
                    break;
                }
                // Fn+R = reset config
                if (keys.fn && keys.word.size() > 0 && keys.word[0] == 'r') {
                    Config::reset();
                    enterSetupMode();
                    break;
                }
                // Any other key → companion handles it
                char key = 0;
                if (keys.enter) key = '\n';
                else if (keys.word.size() > 0) key = keys.word[0];
                if (key) companion.handleKey(key);
            }
            companion.update(canvas);
            canvas.pushSprite(0, 0);
            break;

        case AppMode::CHAT:
            if (keyPressed) {
                if (keys.tab) {
                    enterCompanionMode();
                    break;
                }
                if (keys.enter) {
                    chat.handleEnter();
                } else if (keys.del) {
                    chat.handleBackspace();
                } else if (keys.word.size() > 0) {
                    chat.handleKey(keys.word[0]);
                }
            }

            // Check if chat has a message to send
            if (chat.hasPendingMessage() && !aiClient.isBusy()) {
                String msg = chat.takePendingMessage();
                companion.triggerTalk();

                aiClient.sendMessage(msg,
                    // onToken
                    [](const String& token) {
                        chat.appendAIToken(token);
                        // Redraw while streaming
                        chat.update(canvas);
                        canvas.pushSprite(0, 0);
                    },
                    // onDone
                    []() {
                        chat.onAIResponseComplete();
                        companion.triggerIdle();
                    },
                    // onError
                    [](const String& error) {
                        chat.appendAIToken("[Error: " + error + "]");
                        chat.onAIResponseComplete();
                        companion.triggerIdle();
                    }
                );
            }

            chat.update(canvas);
            canvas.pushSprite(0, 0);
            break;
    }

    delay(16); // ~60fps cap
}

// ══════════════════════════════════════════════════════════════
// Setup Mode
// ══════════════════════════════════════════════════════════════

void enterSetupMode() {
    appMode = AppMode::SETUP;
    setupStep = SetupStep::SSID;
    setupInput = "";
}

void updateSetupMode() {
    canvas.fillScreen(Color::BG_DAY);
    canvas.setTextColor(Color::CLOCK_TEXT);
    canvas.setTextSize(1);

    canvas.drawString("=== Setup ===", 70, 8);

    switch (setupStep) {
        case SetupStep::SSID:
            canvas.drawString("WiFi SSID:", 10, 35);
            canvas.setTextColor(Color::WHITE);
            canvas.drawString((setupInput + "_").c_str(), 10, 50);
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] confirm  [Backspace] delete", 10, 75);
            break;

        case SetupStep::PASSWORD:
            canvas.drawString("WiFi Password:", 10, 35);
            canvas.setTextColor(Color::WHITE);
            // Show asterisks for password
            {
                String masked;
                for (unsigned int i = 0; i < setupInput.length(); i++) masked += '*';
                canvas.drawString((masked + "_").c_str(), 10, 50);
            }
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] confirm", 10, 75);
            break;

        case SetupStep::API_KEY:
            canvas.drawString("Claude API Key:", 10, 35);
            canvas.setTextColor(Color::WHITE);
            {
                String display = setupInput + "_";
                // Show last 20 chars if too long
                if (display.length() > 35) {
                    display = "..." + display.substring(display.length() - 32);
                }
                canvas.drawString(display.c_str(), 10, 50);
            }
            canvas.setTextColor(Color::STATUS_DIM);
            canvas.drawString("[Enter] confirm  (or empty to skip)", 10, 75);
            break;

        case SetupStep::CONNECTING:
            canvas.drawString("Connecting to WiFi...", 50, 55);
            {
                static int dots = 0;
                String d;
                for (int i = 0; i < (dots % 4); i++) d += ".";
                canvas.drawString(d.c_str(), 170, 55);
                dots++;
            }
            break;
    }

    canvas.pushSprite(0, 0);
}

void handleSetupKey(char key, bool enter, bool backspace) {
    if (backspace && setupInput.length() > 0) {
        setupInput.remove(setupInput.length() - 1);
        return;
    }

    if (key && !enter) {
        setupInput += key;
        return;
    }

    if (!enter) return;

    switch (setupStep) {
        case SetupStep::SSID:
            if (setupInput.length() > 0) {
                Config::setSSID(setupInput);
                setupInput = "";
                setupStep = SetupStep::PASSWORD;
            }
            break;

        case SetupStep::PASSWORD:
            Config::setPassword(setupInput);
            setupInput = "";
            setupStep = SetupStep::API_KEY;
            break;

        case SetupStep::API_KEY:
            Config::setApiKey(setupInput);
            Config::save();
            setupInput = "";
            setupStep = SetupStep::CONNECTING;
            connectWiFi();
            break;

        default:
            break;
    }
}

void connectWiFi() {
    WiFi.begin(Config::getSSID().c_str(), Config::getPassword().c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;

        // Update connecting screen
        canvas.fillScreen(Color::BG_DAY);
        canvas.setTextColor(Color::CLOCK_TEXT);
        canvas.setTextSize(1);
        String msg = "Connecting" + String("....").substring(0, (attempts % 4) + 1);
        canvas.drawString(msg.c_str(), 70, 55);
        canvas.pushSprite(0, 0);
    }

    if (WiFi.status() == WL_CONNECTED) {
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

        // Init AI client
        if (Config::getApiKey().length() > 0) {
            aiClient.begin(Config::getApiKey());
        }

        enterCompanionMode();
    } else {
        // Connection failed
        canvas.fillScreen(Color::BG_DAY);
        canvas.setTextColor(rgb565(220, 80, 80));
        canvas.setTextSize(1);
        canvas.drawString("WiFi failed!", 75, 50);
        canvas.drawString("Press any key to retry", 50, 70);
        canvas.pushSprite(0, 0);

        // Wait for keypress then restart setup
        while (true) {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                enterSetupMode();
                return;
            }
            delay(50);
        }
    }
}

void enterCompanionMode() {
    appMode = AppMode::COMPANION;
    companion.begin(canvas);
}

void enterChatMode() {
    appMode = AppMode::CHAT;
    chat.begin(canvas);
}
