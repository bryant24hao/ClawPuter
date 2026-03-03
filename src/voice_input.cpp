#include "voice_input.h"
#include "config.h"
#include <WiFiClient.h>

#ifndef STT_PROXY_HOST
#define STT_PROXY_HOST ""
#endif
#ifndef STT_PROXY_PORT
#define STT_PROXY_PORT "8090"
#endif

void VoiceInput::begin() {
    // Only compute size — buffer allocated on demand in startRecording()
    // and freed after STT in stopRecording() to keep ~190KB free during chat.
    maxSamples = (size_t)(SAMPLE_RATE * MAX_RECORD_SEC);
    Serial.printf("[VOICE] Ready (buffer will be %zu bytes on demand)\n",
                  maxSamples * sizeof(int16_t));
}

bool VoiceInput::allocBuffer() {
    if (recordBuffer) return true;  // already allocated
    size_t bytes = maxSamples * sizeof(int16_t);

    recordBuffer = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!recordBuffer) {
        recordBuffer = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }

    if (recordBuffer) {
        Serial.printf("[VOICE] Buffer allocated: %zu bytes, heap=%u\n", bytes, ESP.getFreeHeap());
        return true;
    }
    Serial.printf("[VOICE] Buffer allocation failed! heap=%u\n", ESP.getFreeHeap());
    return false;
}

void VoiceInput::freeBuffer() {
    if (recordBuffer) {
        heap_caps_free(recordBuffer);
        recordBuffer = nullptr;
        Serial.printf("[VOICE] Buffer freed, heap=%u\n", ESP.getFreeHeap());
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
    if (recording || maxSamples == 0) return;

    // Allocate buffer on demand (freed after STT completes)
    if (!allocBuffer()) return;

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

    // Too short? Ignore — but still free buffer
    if (elapsed < MIN_RECORD_SEC) {
        Serial.println("[VOICE] Too short, ignoring");
        freeBuffer();
        result = "";
        return false;
    }

    // Send to STT — sendToSTT() frees the buffer internally after upload,
    // so ~190KB is available while waiting for Whisper response.
    transcribing = true;
    result = sendToSTT(recordBuffer, samplesRecorded);
    transcribing = false;
    // Ensure buffer is freed even if sendToSTT had an early exit
    freeBuffer();

    if (result.length() > 0) {
        Serial.printf("[VOICE] STT result: %s\n", result.c_str());
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

    // Build WAV header on stack
    uint8_t wavHeader[44];
    writeWavHeader(wavHeader, dataSize);

    // Multipart parts as stack char arrays — no heap allocation
    const char* boundary = "----VoiceBoundary1234";
    char partHeader[160];
    snprintf(partHeader, sizeof(partHeader),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"recording.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);

    char partFooter[128];
    snprintf(partFooter, sizeof(partFooter),
        "\r\n--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "whisper-large-v3-turbo\r\n"
        "--%s--\r\n", boundary, boundary);

    size_t totalSize = strlen(partHeader) + 44 + dataSize + strlen(partFooter);

    // Connect to local STT proxy over HTTP (proxy forwards to Groq HTTPS)
    WiFiClient client;
    client.setTimeout(30);

    int port = atoi(STT_PROXY_PORT);
    Serial.printf("[VOICE] Connecting to STT proxy %s:%d...\n", STT_PROXY_HOST, port);

    if (!client.connect(STT_PROXY_HOST, port)) {
        Serial.println("[VOICE] Connection to STT proxy failed");
        return "";
    }

    // Send HTTP headers + multipart file part header + WAV header
    client.printf("POST /v1/audio/transcriptions HTTP/1.1\r\n"
                  "Host: %s:%s\r\n"
                  "Content-Type: multipart/form-data; boundary=%s\r\n"
                  "Content-Length: %u\r\n"
                  "Connection: close\r\n\r\n",
                  STT_PROXY_HOST, STT_PROXY_PORT, boundary, totalSize);
    client.print(partHeader);
    client.write(wavHeader, 44);

    // Send PCM data in chunks
    const uint8_t* pcmBytes = (const uint8_t*)data;
    size_t remaining = dataSize;
    size_t offset = 0;
    while (remaining > 0) {
        size_t toSend = (remaining > 4096) ? 4096 : remaining;
        client.write(pcmBytes + offset, toSend);
        offset += toSend;
        remaining -= toSend;
        yield();
    }

    client.print(partFooter);
    client.flush();

    Serial.printf("[VOICE] Audio sent (%u bytes)\n", totalSize);

    // ── Free 160KB buffer NOW — data is fully sent, buffer no longer needed ──
    freeBuffer();

    // ── Wait for response (~190KB free heap now) ──
    unsigned long deadline = millis() + 30000;
    while (!client.available() && client.connected() && millis() < deadline) {
        delay(10);
    }

    if (!client.available()) {
        Serial.println("[VOICE] STT timeout");
        client.stop();
        return "";
    }

    // Read response into stack buffer — skip headers, read body
    // STT JSON is small: {"text":"..."} typically < 200 bytes
    char responseBuf[512];
    int respLen = 0;
    bool headersDone = false;
    char lineBuf[256];
    int lineLen = 0;

    while ((client.connected() || client.available()) && millis() < deadline) {
        if (!client.available()) { delay(5); continue; }
        char c = client.read();
        if (c == '\n') {
            lineBuf[lineLen] = '\0';
            if (lineLen > 0 && lineBuf[lineLen-1] == '\r') lineBuf[--lineLen] = '\0';
            if (!headersDone) {
                if (lineLen == 0) headersDone = true;
            } else {
                // Append to response body
                int copyLen = lineLen;
                if (respLen + copyLen >= (int)sizeof(responseBuf) - 1)
                    copyLen = sizeof(responseBuf) - 1 - respLen;
                if (copyLen > 0) {
                    memcpy(responseBuf + respLen, lineBuf, copyLen);
                    respLen += copyLen;
                }
            }
            lineLen = 0;
        } else if (lineLen < (int)sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        }
    }
    responseBuf[respLen] = '\0';

    client.stop();

    Serial.printf("[VOICE] Response: %s\n", responseBuf);

    // Extract "text" from JSON without ArduinoJson — zero heap allocation
    // Format: {"text":"the transcribed text"}
    const char* textKey = strstr(responseBuf, "\"text\":\"");
    if (!textKey) {
        textKey = strstr(responseBuf, "\"text\": \"");
        if (textKey) textKey += 9;
    } else {
        textKey += 8;
    }

    if (!textKey) {
        Serial.println("[VOICE] No 'text' field in response");
        return "";
    }

    // Extract value, handling JSON escapes
    char textBuf[256];
    int ti = 0;
    const char* p = textKey;
    while (*p && *p != '"' && ti < (int)sizeof(textBuf) - 1) {
        if (*p == '\\' && p[1]) {
            switch (p[1]) {
                case '"':  textBuf[ti++] = '"';  p += 2; break;
                case '\\': textBuf[ti++] = '\\'; p += 2; break;
                case 'n':  textBuf[ti++] = '\n'; p += 2; break;
                case '/':  textBuf[ti++] = '/';  p += 2; break;
                default:   p += 2; break;
            }
        } else {
            textBuf[ti++] = *p++;
        }
    }
    textBuf[ti] = '\0';

    if (ti > 0) {
        return filterForDisplay(String(textBuf));
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

    static const char* msgs[] = {
        "Transcribing", "Transcribing.", "Transcribing..", "Transcribing..."
    };
    canvas.drawString(msgs[(millis() / 400) % 4], 4, barY + 4);
}
