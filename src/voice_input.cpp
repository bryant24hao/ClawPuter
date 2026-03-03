#include "voice_input.h"
#include "config.h"
#include <WiFiClient.h>
#include <ArduinoJson.h>

#ifndef OPENCLAW_HOST
#define OPENCLAW_HOST ""
#endif
#ifndef OPENCLAW_PORT
#define OPENCLAW_PORT ""
#endif
#ifndef OPENCLAW_TOKEN
#define OPENCLAW_TOKEN ""
#endif

void VoiceInput::begin() {
    // Allocate recording buffer for MAX_RECORD_SEC seconds
    maxSamples = (size_t)(SAMPLE_RATE * MAX_RECORD_SEC);
    size_t bytes = maxSamples * sizeof(int16_t);

    // Try PSRAM first, fallback to internal
    recordBuffer = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!recordBuffer) {
        recordBuffer = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }

    if (recordBuffer) {
        Serial.printf("[VOICE] Buffer allocated: %zu bytes (%zu samples)\n", bytes, maxSamples);
    } else {
        Serial.println("[VOICE] ERROR: Failed to allocate recording buffer");
        maxSamples = 0;
    }
}

void VoiceInput::initMic() {
    // Must stop speaker — they share GPIO 43
    M5Cardputer.Speaker.end();

    auto micCfg = M5Cardputer.Mic.config();
    micCfg.sample_rate = SAMPLE_RATE;
    micCfg.magnification = 64;
    micCfg.noise_filter_level = 64;
    micCfg.task_priority = 1;
    M5Cardputer.Mic.config(micCfg);
    M5Cardputer.Mic.begin();

    Serial.println("[VOICE] Mic started");
}

void VoiceInput::deinitMic() {
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin();
    Serial.println("[VOICE] Mic stopped, speaker restored");
}

void VoiceInput::startRecording() {
    if (recording || !recordBuffer || maxSamples == 0) return;

    samplesRecorded = 0;
    recording = true;
    recordStartTime = millis();

    initMic();

    // Start non-blocking recording into the full buffer
    M5Cardputer.Mic.record(recordBuffer, maxSamples, SAMPLE_RATE);

    Serial.println("[VOICE] Recording started");
}

bool VoiceInput::stopRecording() {
    if (!recording) return false;
    recording = false;

    // Calculate how many samples we captured based on elapsed time
    float elapsed = (millis() - recordStartTime) / 1000.0f;
    samplesRecorded = (size_t)(elapsed * SAMPLE_RATE);
    if (samplesRecorded > maxSamples) samplesRecorded = maxSamples;

    // Stop mic and restore speaker
    deinitMic();

    Serial.printf("[VOICE] Recorded %.1fs (%zu samples)\n", elapsed, samplesRecorded);

    // Too short? Ignore
    if (elapsed < MIN_RECORD_SEC) {
        Serial.println("[VOICE] Too short, ignoring");
        result = "";
        return false;
    }

    // Send to STT
    transcribing = true;
    result = sendToSTT(recordBuffer, samplesRecorded);
    transcribing = false;

    if (result.length() > 0) {
        Serial.println("[VOICE] STT result: " + result);
        return true;
    }

    Serial.println("[VOICE] No speech detected");
    return false;
}

String VoiceInput::takeResult() {
    String r = result;
    result = "";
    return r;
}

float VoiceInput::getRecordingDuration() const {
    if (!recording) return 0;
    return (millis() - recordStartTime) / 1000.0f;
}

void VoiceInput::writeWavHeader(uint8_t* h, uint32_t dataSize) {
    uint32_t fileSize = 36 + dataSize;
    uint32_t byteRate = SAMPLE_RATE * 2; // 16-bit mono
    uint16_t blockAlign = 2;

    // "RIFF"
    h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
    // ChunkSize (little-endian)
    h[4] = fileSize & 0xFF;
    h[5] = (fileSize >> 8) & 0xFF;
    h[6] = (fileSize >> 16) & 0xFF;
    h[7] = (fileSize >> 24) & 0xFF;
    // "WAVE"
    h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
    // "fmt "
    h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
    // Subchunk1Size = 16
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
    // AudioFormat = 1 (PCM)
    h[20] = 1; h[21] = 0;
    // NumChannels = 1
    h[22] = 1; h[23] = 0;
    // SampleRate
    h[24] = SAMPLE_RATE & 0xFF;
    h[25] = (SAMPLE_RATE >> 8) & 0xFF;
    h[26] = (SAMPLE_RATE >> 16) & 0xFF;
    h[27] = (SAMPLE_RATE >> 24) & 0xFF;
    // ByteRate
    h[28] = byteRate & 0xFF;
    h[29] = (byteRate >> 8) & 0xFF;
    h[30] = (byteRate >> 16) & 0xFF;
    h[31] = (byteRate >> 24) & 0xFF;
    // BlockAlign
    h[32] = blockAlign & 0xFF;
    h[33] = (blockAlign >> 8) & 0xFF;
    // BitsPerSample = 16
    h[34] = 16; h[35] = 0;
    // "data"
    h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
    // Subchunk2Size
    h[40] = dataSize & 0xFF;
    h[41] = (dataSize >> 8) & 0xFF;
    h[42] = (dataSize >> 16) & 0xFF;
    h[43] = (dataSize >> 24) & 0xFF;
}

String VoiceInput::sendToSTT(const int16_t* data, size_t sampleCount) {
    uint32_t dataSize = sampleCount * sizeof(int16_t);

    // Build WAV in memory: 44 byte header + PCM data
    uint8_t wavHeader[44];
    writeWavHeader(wavHeader, dataSize);

    // Multipart form-data boundary
    const char* boundary = "----VoiceBoundary1234";

    // Build the multipart body parts
    String partHeader = String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"recording.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";

    String partFooter = String("\r\n--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "whisper-1\r\n"
        "--" + boundary + "--\r\n";

    size_t totalSize = partHeader.length() + 44 + dataSize + partFooter.length();

    // Use raw WiFiClient for streaming upload (body too large for HTTPClient)
    WiFiClient client;
    client.setTimeout(30);

    int port = atoi(OPENCLAW_PORT);
    Serial.printf("[VOICE] Connecting to %s:%d\n", OPENCLAW_HOST, port);

    if (!client.connect(OPENCLAW_HOST, port)) {
        Serial.println("[VOICE] Connection failed");
        return "";
    }

    // Send HTTP request line and headers
    client.print(String("POST /v1/audio/transcriptions HTTP/1.1\r\n") +
                 "Host: " + OPENCLAW_HOST + ":" + OPENCLAW_PORT + "\r\n" +
                 "Authorization: Bearer " + OPENCLAW_TOKEN + "\r\n" +
                 "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                 "Content-Length: " + String(totalSize) + "\r\n" +
                 "Connection: close\r\n\r\n");

    // Send multipart body: file part header + WAV header + PCM data + footer
    client.print(partHeader);
    client.write(wavHeader, 44);

    // Send PCM data in chunks to avoid buffer overflow
    const uint8_t* pcmBytes = (const uint8_t*)data;
    size_t remaining = dataSize;
    size_t offset = 0;
    const size_t CHUNK_SIZE = 4096;

    while (remaining > 0) {
        size_t toSend = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        client.write(pcmBytes + offset, toSend);
        offset += toSend;
        remaining -= toSend;
        yield(); // let WiFi stack process
    }

    client.print(partFooter);
    client.flush();

    // Wait for response (server processes audio, may take a few seconds)
    unsigned long deadline = millis() + 30000;
    while (!client.available() && client.connected() && millis() < deadline) {
        delay(10);
    }

    if (!client.available()) {
        Serial.println("[VOICE] STT timeout — no response");
        client.stop();
        return "";
    }

    // Read response: skip HTTP headers, then read body
    String response;
    bool headersDone = false;
    while (client.connected() || client.available()) {
        if (millis() > deadline) {
            Serial.println("[VOICE] Response read timeout");
            break;
        }
        if (client.available()) {
            String line = client.readStringUntil('\n');
            if (!headersDone) {
                // Empty line (just \r) marks end of headers
                if (line == "\r" || line.length() == 0) {
                    headersDone = true;
                }
            } else {
                response += line;
            }
        } else {
            delay(5);
        }
    }

    client.stop();

    Serial.println("[VOICE] Response: " + response);

    // Parse JSON: {"text": "..."}
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.printf("[VOICE] JSON parse error: %s\n", err.c_str());
        return "";
    }

    const char* text = doc["text"];
    if (text && strlen(text) > 0) {
        return String(text);
    }

    return "";
}

void VoiceInput::drawRecordingBar(M5Canvas& canvas) {
    int barY = SCREEN_H - INPUT_BAR_H;
    uint16_t recColor = rgb565(160, 40, 40);  // Dark red
    canvas.fillRect(0, barY, SCREEN_W, INPUT_BAR_H, recColor);
    canvas.drawFastHLine(0, barY, SCREEN_W, rgb565(220, 60, 60));

    canvas.setTextColor(Color::WHITE);
    canvas.setTextSize(1);

    float dur = getRecordingDuration();
    char buf[32];
    snprintf(buf, sizeof(buf), "Recording... %.1fs", dur);
    canvas.drawString(buf, 4, barY + 4);

    // Pulsing dot
    if ((millis() / 500) % 2 == 0) {
        canvas.fillCircle(SCREEN_W - 12, barY + INPUT_BAR_H / 2, 4, rgb565(255, 60, 60));
    }

    // Max time indicator
    char maxBuf[8];
    snprintf(maxBuf, sizeof(maxBuf), "/%.0fs", MAX_RECORD_SEC);
    canvas.setTextColor(rgb565(200, 150, 150));
    canvas.drawString(maxBuf, SCREEN_W - 40, barY + 4);
}

void VoiceInput::drawTranscribingBar(M5Canvas& canvas) {
    int barY = SCREEN_H - INPUT_BAR_H;
    canvas.fillRect(0, barY, SCREEN_W, INPUT_BAR_H, Color::INPUT_BG);
    canvas.drawFastHLine(0, barY, SCREEN_W, Color::GROUND_TOP);

    canvas.setTextColor(Color::CHAT_AI);
    canvas.setTextSize(1);

    int dots = (millis() / 400) % 4;
    String msg = "Transcribing";
    for (int i = 0; i < dots; i++) msg += ".";
    canvas.drawString(msg.c_str(), 4, barY + 4);
}
