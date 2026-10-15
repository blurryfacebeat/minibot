
#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class MicrophoneController {
public:
    bool begin();
    void update();

    bool ready() const;
    bool calibrated() const;
    bool isActive() const;

    float rms() const;
    float noiseFloor() const;
    float threshold() const;

private:
    static constexpr size_t BUFFER_WORDS = 256;

    int32_t audioBuffer_[BUFFER_WORDS] = {};

    bool ready_ = false;
    bool calibrated_ = false;
    bool active_ = false;
    bool hasNoiseFloor_ = false;

    float rms_ = 0.0f;
    float noiseFloor_ = 0.0f;

    int64_t sum_ = 0;
    uint64_t sumSquares_ = 0;
    uint32_t sampleCount_ = 0;

    uint32_t startedAt_ = 0;
    uint32_t lastWindowAt_ = 0;
    uint32_t lastLoudAt_ = 0;

    uint8_t consecutiveLoud_ = 0;

    void processWindow(uint32_t now);
};
