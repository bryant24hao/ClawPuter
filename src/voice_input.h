#pragma once
#include <M5Cardputer.h>
#include "utils.h"

class VoiceInput {
public:
    void begin();

    // Push-to-talk: call when Fn pressed/released
    void startRecording();
    bool stopRecording();  // returns true if STT result available

    bool isRecording() const { return recording; }
    bool isTranscribing() const { return transcribing; }

    // Get transcription result (clears after call)
    String takeResult();

    // Recording duration in seconds
    float getRecordingDuration() const;

    // Draw recording indicator over the input bar area
    void drawRecordingBar(M5Canvas& canvas);
    void drawTranscribingBar(M5Canvas& canvas);

private:
    int16_t* recordBuffer = nullptr;
    size_t maxSamples = 0;
    size_t samplesRecorded = 0;
    bool recording = false;
    bool transcribing = false;
    unsigned long recordStartTime = 0;
    String result;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr float MAX_RECORD_SEC = 5.0f;
    static constexpr float MIN_RECORD_SEC = 0.3f;
    static constexpr int INPUT_BAR_H = 16;

    void initMic();
    void deinitMic();
    bool allocBuffer();
    void freeBuffer();
    String sendToSTT(const int16_t* data, size_t sampleCount);
    void writeWavHeader(uint8_t* header, uint32_t dataSize);
};
