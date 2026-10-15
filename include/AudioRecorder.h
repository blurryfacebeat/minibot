
#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <driver/i2s.h>

class AudioRecorder {
public:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t DURATION_SECONDS = 5;

    static constexpr uint32_t TOTAL_SAMPLES =
        SAMPLE_RATE * DURATION_SECONDS;

    static constexpr size_t PCM_BYTES =
        TOTAL_SAMPLES * sizeof(int16_t);

    bool begin();
    bool ready() const;

    // Записывает звук прямо в TCP-соединение.
    // tick() позволяет обновлять глаза во время записи.
    bool stream(
        WiFiClient& client,
        void (*tick)()
    );

private:
    static constexpr size_t BUFFER_WORDS = 512;

    // Сырые стереоданные I2S
    int32_t rawBuffer_[BUFFER_WORDS] = {};

    // Моно PCM для передачи
    int16_t pcmBuffer_[BUFFER_WORDS / 2] = {};

    bool ready_ = false;
};
