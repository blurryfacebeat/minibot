
#include "FaceController.h"
#include <math.h>

namespace {
    constexpr uint32_t FRAME_MS = 50;

    constexpr uint32_t BOOT_SLEEP_MS = 500;
    constexpr uint32_t BOOT_WAKE_MS = 800;
    constexpr uint32_t BOOT_SETTLE_MS = 450;

    constexpr int EYE_WIDTH = 34;
    constexpr int IDLE_HEIGHT = 32;
    constexpr int WAKE_HEIGHT = 42;
    constexpr int EYE_RADIUS = 6;

    // Глаза полностью в нижней цветной зоне OLED.
    // Даже при высоте 44px верх будет на Y=18.
    constexpr int EYE_CENTER_Y = 40;

    // Верхняя жёлтая зона OLED.
    // Здесь рисуем только крошечные штрихи.
    constexpr int ACCENT_Y = 14;
    constexpr int ACCENT_WIDTH = 2;

    float clamp01(float value) {
        if (value < 0.0f) return 0.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    }

    float smoothstep(float t) {
        t = clamp01(t);
        return t * t * (3.0f - 2.0f * t);
    }

    float lerp(float from, float to, float t) {
        return from + (to - from) * t;
    }

    bool isDue(uint32_t now, uint32_t target) {
        return (int32_t)(now - target) >= 0;
    }
}

FaceController::FaceController()
    : display_(
        U8G2_R0,
        U8X8_PIN_NONE,
        22,
        21
    ) {
}

void FaceController::begin() {
    display_.setI2CAddress(0x3C << 1);
    display_.begin();
    display_.setBusClock(400000);

    randomSeed(micros());

    uint32_t now = millis();

    mode_ = Mode::BootSleep;
    modeStartedAt_ = now;
    lastFrameAt_ = now - FRAME_MS;

    nextBlinkAt_ = now + 3500;
    nextGazeAt_ = now + 1500;

    Serial.println("FaceController started");

    render(now);
}

void FaceController::transitionTo(
    Mode mode,
    uint32_t now
) {
    mode_ = mode;
    modeStartedAt_ = now;

    blinking_ = false;
    secondBlinkPending_ = false;
    secondBlinkReady_ = false;

    nextBlinkAt_ = now + random(2800, 5500);

    if (mode == Mode::Listening) {
        startGaze(0, 0, 300, now);
    }

    if (mode == Mode::Thinking) {
        startGaze(4, -3, 350, now);
    }

    if (mode == Mode::Idle) {
        startGaze(0, 0, 300, now);
    }

    nextGazeAt_ = now + 1800;

    Serial.printf(
        "Face mode changed: %d\n",
        static_cast<int>(mode)
    );
}

void FaceController::setMode(Mode mode) {
    if (mode == mode_) return;

    transitionTo(mode, millis());
}

void FaceController::restartBootSequence() {
    uint32_t now = millis();

    gazeX_ = 0;
    gazeY_ = 0;

    gazeFromX_ = 0;
    gazeFromY_ = 0;

    gazeToX_ = 0;
    gazeToY_ = 0;

    gazeStartedAt_ = now;

    transitionTo(Mode::BootSleep, now);
}

void FaceController::setPupilsEnabled(bool enabled) {
    pupilsEnabled_ = enabled;
}

void FaceController::blink() {
    blinking_ = true;
    blinkStartedAt_ = millis();

    secondBlinkPending_ = false;
    secondBlinkReady_ = false;
}

void FaceController::lookAt(
    int x,
    int y,
    uint32_t holdMs
) {
    uint32_t now = millis();

    x = constrain(x, -6, 6);
    y = constrain(y, -4, 4);

    startGaze(x, y, 300, now);

    nextGazeAt_ = now + 300 + holdMs;
}

void FaceController::startGaze(
    float x,
    float y,
    uint32_t duration,
    uint32_t now
) {
    gazeFromX_ = gazeX_;
    gazeFromY_ = gazeY_;

    gazeToX_ = x;
    gazeToY_ = y;

    gazeStartedAt_ = now;
    gazeDuration_ = duration;
}

void FaceController::updateGaze(uint32_t now) {
    float progress = clamp01(
        (float)(now - gazeStartedAt_) /
        (float)gazeDuration_
    );

    float eased = smoothstep(progress);

    gazeX_ = lerp(
        gazeFromX_,
        gazeToX_,
        eased
    );

    gazeY_ = lerp(
        gazeFromY_,
        gazeToY_,
        eased
    );

    if (!isDue(now, nextGazeAt_)) {
        return;
    }

    int x = 0;
    int y = 0;

    switch (mode_) {
        case Mode::Idle:
            x = random(-2, 3) * 3;
            y = random(-1, 2);
            break;

        case Mode::Thinking:
            x = random(-5, 6);
            y = -3;
            break;

        case Mode::Speaking:
            x = random(-2, 3);
            break;

        case Mode::Listening:
            x = 0;
            y = 0;
            break;

        default:
            return;
    }

    uint32_t duration = random(200, 401);

    startGaze(x, y, duration, now);

    nextGazeAt_ =
        now + duration + random(1300, 3200);
}

void FaceController::updateBlink(uint32_t now) {
    if (
        mode_ == Mode::Sleeping ||
        mode_ == Mode::BootSleep ||
        mode_ == Mode::BootWake ||
        mode_ == Mode::BootSettle
    ) {
        return;
    }

    if (blinking_) {
        if (now - blinkStartedAt_ >= 230) {
            blinking_ = false;

            if (secondBlinkPending_) {
                secondBlinkPending_ = false;
                secondBlinkReady_ = true;
                nextBlinkAt_ = now + 110;
            } else {
                nextBlinkAt_ =
                    now + random(2800, 6200);
            }
        }

        return;
    }

    if (isDue(now, nextBlinkAt_)) {
        blinking_ = true;
        blinkStartedAt_ = now;

        if (secondBlinkReady_) {
            secondBlinkReady_ = false;
        } else {
            secondBlinkPending_ =
                random(100) < 13;
        }
    }
}

void FaceController::update() {
    uint32_t now = millis();
    uint32_t elapsed = now - modeStartedAt_;

    switch (mode_) {
        case Mode::BootSleep:
            if (elapsed >= BOOT_SLEEP_MS) {
                transitionTo(Mode::BootWake, now);
            }
            break;

        case Mode::BootWake:
            if (elapsed >= BOOT_WAKE_MS) {
                transitionTo(Mode::BootSettle, now);
            }
            break;

        case Mode::BootSettle:
            if (elapsed >= BOOT_SETTLE_MS) {
                transitionTo(Mode::Idle, now);
            }
            break;

        default:
            break;
    }

    updateGaze(now);
    updateBlink(now);

    if (now - lastFrameAt_ < FRAME_MS) {
        return;
    }

    lastFrameAt_ = now;
    render(now);
}

void FaceController::drawEyes(
    int height,
    int gazeX,
    int gazeY
) {
    height = constrain(height, 2, 44);

    const int leftX = 22;
    const int rightX = 72;

    int eyeShiftX = pupilsEnabled_ ? 0 : gazeX;
    int eyeShiftY = pupilsEnabled_ ? 0 : gazeY;

    int top =
        EYE_CENTER_Y - height / 2 + eyeShiftY;

    int radius = min(
        EYE_RADIUS,
        height / 2
    );

    display_.setDrawColor(1);

    // Основная форма глаз.
    // Теперь они находятся в синей зоне OLED.
    display_.drawRBox(
        leftX + eyeShiftX,
        top,
        EYE_WIDTH,
        height,
        radius
    );

    display_.drawRBox(
        rightX + eyeShiftX,
        top,
        EYE_WIDTH,
        height,
        radius
    );

    // Крошечные жёлтые акценты.
    // Две полоски по 2 пикселя над каждым глазом.
    // Они не соединяются с основной формой.
    if (height >= 24) {
        const int positions[2] = {
            leftX + eyeShiftX,
            rightX + eyeShiftX
        };

        for (int i = 0; i < 2; i++) {
            int x = positions[i];

            display_.drawHLine(
                x + 11,
                ACCENT_Y,
                ACCENT_WIDTH
            );

            display_.drawHLine(
                x + 22,
                ACCENT_Y,
                ACCENT_WIDTH
            );
        }
    }

    // Зрачки
    if (!pupilsEnabled_ || height < 22) {
        return;
    }

    const int pupilW = 8;
    const int pupilH = 14;

    int pupilY =
        EYE_CENTER_Y - pupilH / 2 + gazeY;

    pupilY = constrain(
        pupilY,
        top + 3,
        top + height - pupilH - 3
    );

    display_.setDrawColor(0);

    const int positions[2] = {
        leftX,
        rightX
    };

    for (int i = 0; i < 2; i++) {
        int x = positions[i];

        int pupilX =
            x + EYE_WIDTH / 2
            - pupilW / 2
            + gazeX;

        pupilX = constrain(
            pupilX,
            x + 4,
            x + EYE_WIDTH - pupilW - 4
        );

        display_.drawRBox(
            pupilX,
            pupilY,
            pupilW,
            pupilH,
            3
        );
    }

    display_.setDrawColor(1);
}

void FaceController::render(uint32_t now) {
    display_.clearBuffer();

    uint32_t elapsed = now - modeStartedAt_;

    int height = IDLE_HEIGHT;

    switch (mode_) {
        case Mode::BootSleep:
            height = 2;
            break;

        case Mode::BootWake: {
            float t = clamp01(
                (float)elapsed / BOOT_WAKE_MS
            );

            float eased =
                1.0f - powf(1.0f - t, 3.0f);

            height = (int)roundf(
                lerp(2, WAKE_HEIGHT, eased)
            );
            break;
        }

        case Mode::BootSettle: {
            float t = smoothstep(
                (float)elapsed /
                BOOT_SETTLE_MS
            );

            height = (int)roundf(
                lerp(
                    WAKE_HEIGHT,
                    IDLE_HEIGHT,
                    t
                )
            );
            break;
        }

        case Mode::Idle:
            height = IDLE_HEIGHT;
            break;

        case Mode::Listening:
            height = 38;
            break;

        case Mode::Thinking:
            height = 26;
            break;

        case Mode::Speaking:
            height = IDLE_HEIGHT +
                (int)roundf(
                    sinf(elapsed / 170.0f) * 2
                );
            break;

        case Mode::Sleeping: {
            float t = smoothstep(
                (float)elapsed / 400.0f
            );

            height = (int)roundf(
                lerp(IDLE_HEIGHT, 2, t)
            );
            break;
        }
    }

    // Плавное моргание
    if (blinking_) {
        uint32_t blinkElapsed =
            now - blinkStartedAt_;

        float closed = 0;

        if (blinkElapsed < 75) {
            closed = smoothstep(
                blinkElapsed / 75.0f
            );
        }
        else if (blinkElapsed < 110) {
            closed = 1.0f;
        }
        else if (blinkElapsed < 230) {
            closed = 1.0f - smoothstep(
                (blinkElapsed - 110) / 120.0f
            );
        }

        height = max(
            2,
            (int)roundf(
                height * (1.0f - closed)
            )
        );
    }

    drawEyes(
        height,
        (int)roundf(gazeX_),
        (int)roundf(gazeY_)
    );

    display_.sendBuffer();
}
