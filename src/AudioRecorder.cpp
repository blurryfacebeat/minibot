
#include "AudioRecorder.h"

namespace {
    constexpr int MIC_SCK = 26;
    constexpr int MIC_WS = 25;
    constexpr int MIC_SD = 33;
}

bool AudioRecorder::begin() {
    i2s_config_t config = {};

    config.mode = (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_RX
    );

    config.sample_rate = SAMPLE_RATE;

    config.bits_per_sample =
        I2S_BITS_PER_SAMPLE_32BIT;

    // Используем проверенную конфигурацию.
    // Полезный звук находится в первом слоте.
    config.channel_format =
        I2S_CHANNEL_FMT_RIGHT_LEFT;

    config.communication_format =
        I2S_COMM_FORMAT_STAND_I2S;

    config.intr_alloc_flags = 0;

    // Увеличиваем DMA-буферы для записи по Wi-Fi
    config.dma_buf_count = 8;
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

    ready_ = true;

    Serial.println("AudioRecorder initialized!");

    return true;
}

bool AudioRecorder::ready() const {
    return ready_;
}

bool AudioRecorder::stream(
    WiFiClient& client,
    void (*tick)()
) {
    if (!ready_) {
        return false;
    }

    // Перезапускаем I2S, чтобы не отправлять
    // старые данные из DMA-буферов.
    if (i2s_stop(I2S_NUM_0) != ESP_OK) {
        return false;
    }

    if (i2s_start(I2S_NUM_0) != ESP_OK) {
        return false;
    }

    uint32_t remaining = TOTAL_SAMPLES;
    uint32_t lastProgressAt = millis();

    Serial.println("Recording started!");

    while (remaining > 0) {
        if (!client.connected()) {
            Serial.println("Client disconnected");
            return false;
        }

        size_t bytesRead = 0;

        esp_err_t result = i2s_read(
            I2S_NUM_0,
            rawBuffer_,
            sizeof(rawBuffer_),
            &bytesRead,
            pdMS_TO_TICKS(200)
        );

        if (result != ESP_OK) {
            Serial.printf(
                "I2S read error: %d\n",
                result
            );

            return false;
        }

        size_t wordCount =
            bytesRead / sizeof(int32_t);

        // Два слова I2S на один моносэмпл
        size_t sampleCount = wordCount / 2;

        if (sampleCount == 0) {
            if (millis() - lastProgressAt > 3000) {
                Serial.println("I2S timeout");
                return false;
            }

            continue;
        }

        if (sampleCount > remaining) {
            sampleCount = remaining;
        }

        // Преобразуем 32-битные I2S-слова
        // в 16-битный PCM.
        for (size_t i = 0; i < sampleCount; i++) {
            int32_t raw = rawBuffer_[i * 2];

            pcmBuffer_[i] = (int16_t)(raw >> 16);
        }

        const uint8_t* data =
            reinterpret_cast<const uint8_t*>(
                pcmBuffer_
            );

        size_t totalBytes =
            sampleCount * sizeof(int16_t);

        size_t sent = 0;

        // TCP может принять не весь буфер
        // за один вызов write().
        while (sent < totalBytes) {
            if (!client.connected()) {
                return false;
            }

            size_t written = client.write(
                data + sent,
                totalBytes - sent
            );

            if (written == 0) {
                Serial.println("Network write failed");
                return false;
            }

            sent += written;
        }

        remaining -= sampleCount;
        lastProgressAt = millis();

        // Обновляем OLED во время записи
        if (tick != nullptr) {
            tick();
        }
    }

    client.flush();

    Serial.println("Recording completed!");

    return true;
}
