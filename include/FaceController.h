
#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

class FaceController {
public:
    enum class Mode {
        BootSleep,
        BootWake,
        BootSettle,
        Idle,
        Listening,
        Thinking,
        Speaking,
        Sleeping
    };

    FaceController();

    void begin();
    void update();

    void setMode(Mode mode);
    void restartBootSequence();

    void lookAt(
        int x,
        int y,
        uint32_t holdMs = 2000
    );

    void blink();
    void setPupilsEnabled(bool enabled);

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C display_;

    Mode mode_ = Mode::BootSleep;

    uint32_t modeStartedAt_ = 0;
    uint32_t lastFrameAt_ = 0;

    // Моргание
    bool blinking_ = false;
    bool secondBlinkPending_ = false;
    bool secondBlinkReady_ = false;

    uint32_t blinkStartedAt_ = 0;
    uint32_t nextBlinkAt_ = 0;

    // Движение взгляда
    float gazeX_ = 0;
    float gazeY_ = 0;

    float gazeFromX_ = 0;
    float gazeFromY_ = 0;

    float gazeToX_ = 0;
    float gazeToY_ = 0;

    uint32_t gazeStartedAt_ = 0;
    uint32_t gazeDuration_ = 250;
    uint32_t nextGazeAt_ = 0;

    // Зрачки включены по умолчанию
    bool pupilsEnabled_ = true;

    void transitionTo(Mode mode, uint32_t now);

    void updateBlink(uint32_t now);
    void updateGaze(uint32_t now);

    void startGaze(
        float x,
        float y,
        uint32_t duration,
        uint32_t now
    );

    void render(uint32_t now);

    void drawEyes(
        int height,
        int gazeX,
        int gazeY
    );
};
