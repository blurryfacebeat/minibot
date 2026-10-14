#include <Arduino.h>
#include <U8g2lib.h>

// Пины подключения дисплея
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;

// Создаём объект дисплея
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(
    U8G2_R0,
    U8X8_PIN_NONE,
    OLED_SCL,
    OLED_SDA
);

// Время последнего кадра
uint32_t lastFrame = 0;

// Рисуем глаза
void drawEyes(int offset, bool blink) {
    display.clearBuffer();

    if (blink) {
        // Закрытые глаза
        display.drawRBox(24, 32, 30, 4, 2);
        display.drawRBox(74, 32, 30, 4, 2);
    } else {
        // Открытые глаза
        display.drawRBox(
            24 + offset, 15, 30, 35, 10
        );

        display.drawRBox(
            74 + offset, 15, 30, 35, 10
        );
    }

    display.sendBuffer();
}

void setup() {
    Serial.begin(115200);

    // Адрес 0x3C в формате U8g2
    display.setI2CAddress(0x3C * 2);

    // Запускаем дисплей
    display.begin();

    Serial.println("MiniBot eyes started!");
}

void loop() {
    uint32_t now = millis();

    // Около 30 кадров в секунду
    if (now - lastFrame < 33) {
        return;
    }

    lastFrame = now;

    // Моргание раз в 3,5 секунды
    bool blink = (now % 3500) < 150;

    // Направление взгляда
    int direction = (now / 2000) % 3;

    int offset = 0;

    if (direction == 1) offset = -7;
    if (direction == 2) offset = 7;

    drawEyes(offset, blink);
}
