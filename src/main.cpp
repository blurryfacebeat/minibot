
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <cstring>

#include "FaceController.h"
#include "WifiSecrets.h"

// Наши устройства
FaceController face;
WebServer server(80);

// Буфер команд из Serial Monitor
char commandBuffer[40];
uint8_t commandLength = 0;

// HTML-страница хранится во Flash,
// а не создаётся заново при каждом запросе.
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>MiniBot Control</title>

  <style>
    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      padding: 24px;
      background: #111318;
      color: white;
      font-family: system-ui, sans-serif;
    }

    main {
      max-width: 480px;
      margin: 0 auto;
    }

    h1 {
      font-size: 28px;
      margin-bottom: 8px;
    }

    p {
      color: #9ca3af;
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 24px;
    }

    button {
      border: 1px solid #343a46;
      border-radius: 12px;
      background: #242833;
      color: white;
      padding: 16px 8px;
      font-size: 16px;
      cursor: pointer;
    }

    button:active {
      background: #3b4252;
    }

    .primary {
      background: #2563eb;
    }

    .status {
      margin-top: 24px;
      padding: 14px;
      border-radius: 10px;
      background: #1d222c;
      color: #a7f3d0;
      font-size: 14px;
    }

    h2 {
      font-size: 18px;
      margin-top: 32px;
    }
  </style>
</head>

<body>
  <main>
    <h1>🤖 MiniBot</h1>
    <p>Управление роботом по Wi-Fi</p>

    <h2>Состояние</h2>

    <div class="grid">
      <button class="primary" onclick="send('boot')">
        Проснуться
      </button>

      <button onclick="send('sleep')">
        Спать
      </button>

      <button onclick="send('idle')">
        Обычный
      </button>

      <button onclick="send('listen')">
        Слушает
      </button>

      <button onclick="send('think')">
        Думает
      </button>

      <button onclick="send('speak')">
        Говорит
      </button>
    </div>

    <h2>Глаза</h2>

    <div class="grid">
      <button onclick="send('left')">
        ← Влево
      </button>

      <button onclick="send('right')">
        Вправо →
      </button>

      <button onclick="send('up')">
        ↑ Вверх
      </button>

      <button onclick="send('down')">
        ↓ Вниз
      </button>

      <button onclick="send('center')">
        По центру
      </button>

      <button onclick="send('blink')">
        Моргнуть
      </button>

      <button onclick="send('pupils on')">
        Зрачки ВКЛ
      </button>

      <button onclick="send('pupils off')">
        Зрачки ВЫКЛ
      </button>
    </div>

    <div id="status" class="status">
      Готов к управлению
    </div>
  </main>

  <script>
    async function send(command) {
      const status = document.getElementById('status');

      try {
        const response = await fetch('/api/command', {
          method: 'POST',
          headers: {
            'Content-Type':
              'application/x-www-form-urlencoded'
          },
          body: new URLSearchParams({
            cmd: command
          }).toString()
        });

        const result = await response.text();

        if (!response.ok) {
          throw new Error(result);
        }

        status.textContent = 'Выполнено: ' + command;
      } catch (error) {
        status.textContent =
          'Ошибка: ' + error.message;
      }
    }
  </script>
</body>
</html>
)HTML";

// Единый обработчик команд.
// Им пользуются и HTTP, и Serial.
bool executeCommand(const String& command) {
    if (command == "boot" || command == "wake") {
        face.restartBootSequence();
    }
    else if (command == "idle") {
        face.setMode(
            FaceController::Mode::Idle
        );
    }
    else if (command == "listen") {
        face.setMode(
            FaceController::Mode::Listening
        );
    }
    else if (command == "think") {
        face.setMode(
            FaceController::Mode::Thinking
        );
    }
    else if (command == "speak") {
        face.setMode(
            FaceController::Mode::Speaking
        );
    }
    else if (command == "sleep") {
        face.setMode(
            FaceController::Mode::Sleeping
        );
    }
    else if (command == "blink") {
        face.blink();
    }
    else if (command == "left") {
        face.lookAt(-6, 0);
    }
    else if (command == "right") {
        face.lookAt(6, 0);
    }
    else if (command == "up") {
        face.lookAt(0, -4);
    }
    else if (command == "down") {
        face.lookAt(0, 4);
    }
    else if (command == "center") {
        face.lookAt(0, 0);
    }
    else if (command == "pupils on") {
        face.setPupilsEnabled(true);
    }
    else if (command == "pupils off") {
        face.setPupilsEnabled(false);
    }
    else {
        return false;
    }

    Serial.print("Command executed: ");
    Serial.println(command);

    return true;
}

// Настраиваем маршруты веб-сервера
void setupWebServer() {
    // Главная страница
    server.on("/", HTTP_GET, []() {
        server.send_P(
            200,
            "text/html; charset=utf-8",
            INDEX_HTML
        );
    });

    // API для управления роботом
    server.on("/api/command", HTTP_POST, []() {
        String command = server.arg("cmd");

        if (executeCommand(command)) {
            server.send(
                200,
                "text/plain",
                "OK"
            );
        } else {
            server.send(
                400,
                "text/plain",
                "Unknown command"
            );
        }
    });

    server.onNotFound([]() {
        server.send(
            404,
            "text/plain",
            "Not found"
        );
    });

    server.begin();

    Serial.println("HTTP server started");
}

// Подключаемся к Wi-Fi
void connectWiFi() {
    Serial.print("Connecting to Wi-Fi");

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t startedAt = millis();

    // Ожидаем не больше 15 секунд
    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startedAt < 15000
    ) {
        Serial.print(".");

        // Глаза продолжают работать
        face.update();

        delay(200);
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected!");

        Serial.print("MiniBot IP: ");
        Serial.println(WiFi.localIP());

        setupWebServer();
    } else {
        Serial.println("Wi-Fi connection failed");
        Serial.println("Check SSID and password");
    }
}

// Сохраняем управление через USB
void readSerialCommands() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            if (commandLength == 0) {
                continue;
            }

            commandBuffer[commandLength] = '\0';

            String command(commandBuffer);

            if (!executeCommand(command)) {
                Serial.println("Unknown command");
            }

            commandLength = 0;
        }
        else if (
            commandLength <
            sizeof(commandBuffer) - 1
        ) {
            commandBuffer[commandLength++] = c;
        }
        else {
            commandLength = 0;
            Serial.println("Command too long");
        }
    }
}

void setup() {
    Serial.begin(115200);

    delay(300);

    Serial.println();
    Serial.println("====================");
    Serial.println("     MiniBot v0.5");
    Serial.println("====================");

    // Сначала запускаем глаза
    face.begin();

    // Затем подключаем сеть
    connectWiFi();
}

void loop() {
    // Обработка HTTP-запросов
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    // Сохраняем команды через USB
    readSerialCommands();

    // Обновление глаз
    face.update();

    delay(1);
}
