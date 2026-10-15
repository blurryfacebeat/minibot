
#include "MicrophoneController.h"
#include <math.h>

namespace {
    // INMP441
    constexpr int MIC_SCK = 26;
    constexpr int MIC_WS = 25;
    constexpr int MIC_SD = 33;

    constexpr int SAMPLE_RATE = 16000;

    // Длительность измерительного окна
    constexpr uint32_t WINDOW_MS = 100;

    // Начальная калибровка
    constexpr uint32_t CALIBRATION_MS = 3000;
    constexpr uint32_t STARTUP_IGNORE_MS = 300;

    // Чувствительность
    constexpr float MIN_THRESHOLD = 190.0f;
    constexpr float NOISE_MULTIPLIER = 1.75f;
    constexpr float NOISE_MARGIN = 35.0f;

    // Система накопления активности
    constexpr uint8_t ACTIVITY_SCORE_MAX = 6;
    constexpr uint8_t ACTIVITY_SCORE_START = 4;

    // Пауза после обнаруженного звука
    constexpr uint32_t HOLD_MS = 1100;

    // Адаптация фона
    constexpr float NOISE_RISE_ALPHA = 0.008f;
    constexpr float NOISE_FALL_ALPHA = 0.08f;
}

bool MicrophoneController::begin() {
    i2s_config_t config = {};

    config.mode = (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_RX
    );

    config.sample_rate = SAMPLE_RATE;

    config.bits_per_sample =
        I2S_BITS_PER_SAMPLE_32BIT;

    config.channel_format =
        I2S_CHANNEL_FMT_RIGHT_LEFT;

    config.communication_format =
        I2S_COMM_FORMAT_STAND_I2S;

    config.intr_alloc_flags = 0;
    config.dma_buf_count = 4;
    config.dma_buf_len = 256;
    config.use_apll = false;

    i2s_pin_config_t pins = {};

    pins.bck_io_num = MIC_SCK;
    pins.ws_io_num = MIC_WS;
    pins.data_in_num = MIC_SD;
    pins.data_out_num = I2S_PIN_NO_CHANGE;

    esp_err_t result = i2s_driver_install(
        I2S_NUM_0,
        &config,
        0,
        nullptr
    );

    if (result != ESP_OK) {
        Serial.printf(
            "I2S install error: %d\n",
            result
        );

        return false;
    }

    result = i2s_set_pin(I2S_NUM_0, &pins);

    if (result != ESP_OK) {
        Serial.printf(
            "I2S pin error: %d\n",
            result
        );

        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);

    startedAt_ = millis();
    lastWindowAt_ = startedAt_;

    ready_ = true;
    calibrated_ = false;
    active_ = false;
    hasNoiseFloor_ = false;

    rms_ = 0;
    noiseFloor_ = 0;

    sum_ = 0;
    sumSquares_ = 0;
    sampleCount_ = 0;

    consecutiveLoud_ = 0;
    lastLoudAt_ = 0;

    Serial.println("Microphone initialized!");
    Serial.println("Calibrating for 3 seconds...");

    return true;
}

bool MicrophoneController::ready() const {
    return ready_;
}

bool MicrophoneController::calibrated() const {
    return calibrated_;
}

bool MicrophoneController::isActive() const {
    return active_;
}

float MicrophoneController::rms() const {
    return rms_;
}

float MicrophoneController::noiseFloor() const {
    return noiseFloor_;
}

float MicrophoneController::threshold() const {
    return fmaxf(
        MIN_THRESHOLD,
        noiseFloor_ * NOISE_MULTIPLIER
            + NOISE_MARGIN
    );
}

void MicrophoneController::update() {
    if (!ready_) {
        return;
    }

    size_t bytesRead = 0;

    esp_err_t result = i2s_read(
        I2S_NUM_0,
        audioBuffer_,
        sizeof(audioBuffer_),
        &bytesRead,
        pdMS_TO_TICKS(10)
    );

    uint32_t now = millis();

    if (result == ESP_OK && bytesRead > 0) {
        int count =
            bytesRead / sizeof(int32_t);

        // По нашим тестам звук находится
        // в первом I2S-слоте (канал A).
        for (int i = 0; i + 1 < count; i += 2) {
            int32_t raw = audioBuffer_[i];

            int32_t sample = raw >> 16;
            int64_t value = sample;

            sum_ += value;

            sumSquares_ +=
                (uint64_t)(value * value);

            sampleCount_++;
        }
    }

    if (now - lastWindowAt_ >= WINDOW_MS) {
        processWindow(now);
        lastWindowAt_ = now;
    }
}

void MicrophoneController::processWindow(
    uint32_t now
) {
    if (sampleCount_ == 0) {
        return;
    }

    // Расчёт RMS с удалением DC offset
    double mean =
        (double)sum_ / sampleCount_;

    double meanSquare =
        (double)sumSquares_ / sampleCount_;

    double variance =
        meanSquare - mean * mean;

    if (variance < 0) {
        variance = 0;
    }

    rms_ = (float)sqrt(variance);

    // Очищаем накопленные сэмплы
    sum_ = 0;
    sumSquares_ = 0;
    sampleCount_ = 0;

    uint32_t uptime = now - startedAt_;

    // Пропускаем стартовые помехи
    if (uptime < STARTUP_IGNORE_MS) {
        return;
    }

    // ==================================
    // 1. КАЛИБРОВКА
    // ==================================

    if (!calibrated_) {
        if (!hasNoiseFloor_) {
            noiseFloor_ = rms_;
            hasNoiseFloor_ = true;
        } else {
            // Ограничиваем влияние
            // громких одиночных всплесков.
            float cappedRms = fminf(
                rms_,
                fmaxf(
                    noiseFloor_ * 1.8f,
                    noiseFloor_ + 100.0f
                )
            );

            float alpha =
                cappedRms < noiseFloor_
                ? 0.25f
                : 0.10f;

            noiseFloor_ +=
                (cappedRms - noiseFloor_) * alpha;
        }

        if (uptime >= CALIBRATION_MS) {
            calibrated_ = true;

            Serial.println(
                "Microphone calibrated!"
            );

            Serial.printf(
                "Noise floor: %.1f\n",
                noiseFloor_
            );

            Serial.printf(
                "Threshold: %.1f\n",
                threshold()
            );
        }

        return;
    }

    // ==================================
    // 2. ОЦЕНИВАЕМ ЗВУКОВУЮ АКТИВНОСТЬ
    // ==================================

    float currentThreshold = threshold();

    bool loud = rms_ >= currentThreshold;

    // Используем накопительный счётчик.
    // Громкое окно добавляет 2 балла.
    // Тихое окно убирает 1 балл.
    //
    // Один случайный всплеск
    // не должен включить Listening.

    if (loud) {
        consecutiveLoud_ = min(
            (int)ACTIVITY_SCORE_MAX,
            (int)consecutiveLoud_ + 2
        );
    } else {
        if (consecutiveLoud_ > 0) {
            consecutiveLoud_--;
        }
    }

    // ==================================
    // 3. НАЧАЛО АКТИВНОСТИ
    // ==================================

    if (
        !active_ &&
        consecutiveLoud_ >= ACTIVITY_SCORE_START
    ) {
        active_ = true;
        lastLoudAt_ = now;

        Serial.println("Sound detected!");

        Serial.printf(
            "RMS: %.1f | Noise: %.1f | "
            "Threshold: %.1f\n",
            rms_,
            noiseFloor_,
            currentThreshold
        );
    }

    // Обновляем время последнего
    // громкого измерения
    if (active_ && loud) {
        lastLoudAt_ = now;
    }

    // ==================================
    // 4. ЗАВЕРШЕНИЕ АКТИВНОСТИ
    // ==================================

    if (
        active_ &&
        now - lastLoudAt_ >= HOLD_MS
    ) {
        active_ = false;
        consecutiveLoud_ = 0;

        Serial.println("Sound ended.");
    }

    // ==================================
    // 5. ОБНОВЛЕНИЕ ФОНОВОГО ШУМА
    // ==================================

    // Не обновляем фон во время речи
    // и пока накапливаются громкие окна.
    if (
        !active_ &&
        !loud &&
        consecutiveLoud_ == 0
    ) {
        float alpha =
            rms_ > noiseFloor_
            ? NOISE_RISE_ALPHA
            : NOISE_FALL_ALPHA;

        // Ограничиваем влияние
        // случайных высоких значений.
        float target = fminf(
            rms_,
            noiseFloor_ * 1.3f + 30.0f
        );

        noiseFloor_ +=
            (target - noiseFloor_) * alpha;

        if (noiseFloor_ < 1.0f) {
            noiseFloor_ = 1.0f;
        }
    }
}
