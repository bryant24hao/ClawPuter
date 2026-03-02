#include "ai_client.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Anthropic API root certificate (ISRG Root X1 - Let's Encrypt)
// This covers api.anthropic.com
static const char* ROOT_CA PROGMEM = R"(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6
UA5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+s
WT8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qy
HB5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+U
CvdQWIKs/0qS2CGy1FYnSbUzg7VjvRoQA6zV1a0nnWlgsrCMuS5xCi1DAFBsRN7f
J0gJCDGBRaIh3JOaR26Vmlzm1MbSyKIpZ5YOPxkN8FNRNbUgIlGL1mel1FUXuNMe
rC1MfPuNBjKEL8YnRScbaGenGqUH9M8dePAq3KZtJJXgCLO0K9ljbJwKcGNOFSan
T36+4fV2KR12hlkuVh2e1VBOeEK8rmT+ly3FM2hNXKOniEi8FC4OCNF+GiX8FKXB
JqralCBMRsCLORmZKh1OE8/xMqGSg77WBBJnJaSfObMFJkEbwMx+UlKEAyMv5Scn
y+MKheV4jLVaqCCp7Yp2xda1AgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogZiUwcgkuFdMaBklm2NQ09ubI5TBkLam9hIIIAiNMmGBIN
L3mhStKYR8NtKoJ2sUlgRM1DRWWAiShXPcf1eMGNKpkGLDPY01BOAJcR0b1L6y0l
F12aOUtGDKSMAqxOJZp7FJV7i/5WjvrJvNPGP0sKUOZHCg6APQIIRyQQ5FnJR6hg
L0dSNKCejuT05xe59oWpRBB6fg6MPrx4JIBafLQkXx5+2KsZ7HqDN2JDK2H/Y8nC
E86PjzVbXwt6kDOToGAsuMJaBwiaHdMP1B6+d1PpC4IpBtqHxFnvJM4d77eI/V+1
LNuE3qdC7B0mFVZkqhq6AgRqjDRaAhVqChEvJn/JFre99d9+BuEq/PILfdxQ+dTt
mBCAZyxDoyPBkhcEdAHerTFz2LNFM3v0Fbmi2q4YR3dMXMjFt/UyHo1G2t4gmUHm
FIi+ITGkHZdKku+CjiIHAmHBJfcaMxJwh26x8b6s/UbFSceAxl/QUJDXH6Fe9Ck=
-----END CERTIFICATE-----
)";

void AIClient::begin(const String& key) {
    apiKey = key;
    historyCount = 0;
}

void AIClient::sendMessage(const String& userMessage,
                           TokenCallback onToken,
                           DoneCallback onDone,
                           ErrorCallback onError) {
    if (busy) {
        if (onError) onError("Already processing");
        return;
    }
    busy = true;

    WiFiClientSecure client;
    client.setCACert(ROOT_CA);
    client.setTimeout(30);

    HTTPClient http;
    if (!http.begin(client, "https://api.anthropic.com/v1/messages")) {
        busy = false;
        if (onError) onError("Connection failed");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", apiKey);
    http.addHeader("anthropic-version", "2023-06-01");

    String body = buildRequestBody(userMessage);

    int httpCode = http.POST(body);
    if (httpCode != 200) {
        String errMsg = "HTTP " + String(httpCode);
        http.end();
        busy = false;
        if (onError) onError(errMsg);
        return;
    }

    // Read streaming response
    WiFiClient* stream = http.getStreamPtr();
    String fullResponse;
    String lineBuffer;

    while (stream->connected() || stream->available()) {
        while (stream->available()) {
            char c = stream->read();
            if (c == '\n') {
                // Process SSE line
                if (lineBuffer.startsWith("data: ")) {
                    String data = lineBuffer.substring(6);
                    if (data == "[DONE]") {
                        goto done;
                    }

                    JsonDocument doc;
                    DeserializationError err = deserializeJson(doc, data);
                    if (!err) {
                        const char* type = doc["type"];
                        if (type && strcmp(type, "content_block_delta") == 0) {
                            const char* text = doc["delta"]["text"];
                            if (text) {
                                String token(text);
                                fullResponse += token;
                                if (onToken) onToken(token);
                            }
                        }
                    }
                }
                lineBuffer = "";
            } else {
                lineBuffer += c;
            }
        }
        delay(1);
    }

done:
    http.end();

    // Save to history
    addToHistory(userMessage, fullResponse);

    busy = false;
    if (onDone) onDone();
}

void AIClient::update() {
    // Currently unused - streaming is synchronous in sendMessage
    // Future: could move to async processing
}

void AIClient::addToHistory(const String& user, const String& assistant) {
    if (historyCount < MAX_HISTORY) {
        history[historyCount].user = user;
        history[historyCount].assistant = assistant;
        historyCount++;
    } else {
        // Shift history
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            history[i] = history[i + 1];
        }
        history[MAX_HISTORY - 1].user = user;
        history[MAX_HISTORY - 1].assistant = assistant;
    }
}

String AIClient::buildRequestBody(const String& userMessage) {
    JsonDocument doc;
    doc["model"] = "claude-haiku-4-5-20251001";
    doc["max_tokens"] = 150;
    doc["stream"] = true;
    doc["system"] = "You are a tiny pixel companion living inside a Cardputer device. "
                    "Keep responses very short (1-2 sentences max) since the screen is tiny (240x135). "
                    "Be friendly, cute, and playful. Use simple words.";

    JsonArray messages = doc["messages"].to<JsonArray>();

    // Add history
    for (int i = 0; i < historyCount; i++) {
        JsonObject userMsg = messages.add<JsonObject>();
        userMsg["role"] = "user";
        userMsg["content"] = history[i].user;

        JsonObject assistMsg = messages.add<JsonObject>();
        assistMsg["role"] = "assistant";
        assistMsg["content"] = history[i].assistant;
    }

    // Add current message
    JsonObject currentMsg = messages.add<JsonObject>();
    currentMsg["role"] = "user";
    currentMsg["content"] = userMessage;

    String body;
    serializeJson(doc, body);
    return body;
}
