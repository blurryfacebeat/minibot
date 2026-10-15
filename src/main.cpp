
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "FaceController.h"
#include "MicrophoneController.h"
#include "WifiSecrets.h"

// Основные компоненты робота
FaceController face;
MicrophoneController microphone;

WebServer server(80);

// Автоматическая реакция на звук
bool autoSoundMode = true;
bool faceIsListening = false;

// Команды через USB
char commandBuffer[40];
uint8_t commandLength = 0;

// Для диагностических сообщений
uint32_t lastAudioLogAt = 0;

// Веб-интерфейс
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
      color: #fff;
      font-family: system-ui, sans-serif;
    }

    main {
      max-width: 480px;
      margin: 0 auto;
    }

    h1 {
      margin-bottom: 8px;
    }

    p {
      color: #9ca3af;
    }

    h2 {
      font-size: 18px;
      margin-top: 32px;
    }

    .card {
      background: #1d222c;
      padding: 18px;
      border-radius: 14px;
      margin-top: 20px;
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
      margin-top: 16px;
    }

    button {
      border: 1px solid #343a46;
      border-radius: 12px;
      background: #242833;
      color: white;
      padding: 15px 8px;
      font-size: 15px;
      cursor: pointer;
    }

    button:active {
      background: #3b4252;
    }

    .primary {
      background: #2563eb;
    }

    .level {
      height: 16px;
      background: #343a46;
      border-radius: 8px;
      overflow: hidden;
      margin: 12px 0;
    }

    .bar {
      height: 100%;
      width: 0%;
      background: #22c55e;
      transition: width 100ms;
    }

    .status {
      font-size: 14px;
      color: #a7f3d0;
      margin-top: 12px;
    }

    .muted {
      font-size: 13px;
      color: #9ca3af;
    }
  </style>
</head>

<body>
  <main>
    <h1>🤖 MiniBot v0.7</h1>
    <p>Voice Activity Control</p>

    <div class="card">
      <h2>🎤 Микрофон</h2>

      <div id="activity">
        Подключаемся...
      </div>

      <div class="level">
        <div class="bar" id="levelBar"></div>
      </div>

      <div id="rms" class="muted">
        RMS: —
      </div>

      <div id="noise" class="muted">
        Noise: —
      </div>

      <div id="threshold" class="muted">
        Threshold: —
      </div>
    </div>

    <div class="card">
      <h2>Автоматическая реакция</h2>

      <div class="grid">
        <button class="primary"
                onclick="send('auto on')">
          Включить
        </button>

        <button onclick="send('auto off')">
          Отключить
        </button>
      </div>

      <div id="autoStatus"
           class="status">
        Режим: —
      </div>
    </div>

    <h2>Эмоции</h2>

    <div class="grid">
      <button onclick="send('boot')">
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

    <h2>Управление глазами</h2>

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
        Центр
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

    <div id="commandStatus"
         class="status">
      Готов к управлению
    </div>
  </main>

  <script>
    async function send(command) {
      const status =
        document.getElementById('commandStatus');

      try {
        const response = await fetch(
          '/api/command',
          {
            method: 'POST',
            headers: {
              'Content-Type':
                'application/x-www-form-urlencoded'
            },
            body: new URLSearchParams({
              cmd: command
            })
          }
        );

        if (!response.ok) {
          throw new Error(await response.text());
        }

        status.textContent =
          'Выполнено: ' + command;

        updateStatus();
      } catch (error) {
        status.textContent =
          'Ошибка: ' + error.message;
      }
    }

    async function updateStatus() {
      try {
        const response = await fetch(
          '/api/status',
          { cache: 'no-store' }
        );

        if (!response.ok) {
          throw new Error('HTTP ' + response.status);
        }

        const data = await response.json();

        const activity =
          document.getElementById('activity');

        if (!data.microphone) {
          activity.textContent =
            'Микрофон недоступен';
        } else if (!data.calibrated) {
          activity.textContent =
            'Калибровка микрофона...';
        } else if (data.active) {
          activity.textContent =
            '🎤 Обнаружен звук!';
        } else {
          activity.textContent =
            '🔇 Ожидание звука';
        }

        document.getElementById('rms')
          .textContent =
          'RMS: ' + data.rms.toFixed(1);

        document.getElementById('noise')
          .textContent =
          'Noise: ' + data.noise.toFixed(1);

        document.getElementById('threshold')
          .textContent =
          'Threshold: ' + data.threshold.toFixed(1);

        document.getElementById('autoStatus')
          .textContent =
          'Режим: ' +
          (data.auto ? 'Автоматический' : 'Ручной');

        // Индикатор относительно порога
        const percent = Math.min(
          100,
          data.rms / Math.max(1, data.threshold) * 65
        );

        document.getElementById('levelBar')
          .style.width = percent + '%';

      } catch (error) {
        document.getElementById('activity')
          .textContent = 'Нет связи с MiniBot';
      }
    }

    updateStatus();
    setInterval(updateStatus, 800);
  </script>
</body>
</html>
)HTML";

// Ручное управление состоянием лица
void setManualMode(FaceController::Mode mode) {
    autoSoundMode = false;
    faceIsListening = false;

    face.setMode(mode);
}

// Выполняем команды из браузера или USB
bool executeCommand(String command) {
    command.trim();
    command.toLowerCase();

    if (command == "auto on") {
        autoSoundMode = true;
        faceIsListening = false;

        face.setMode(
            FaceController::Mode::Idle
        );
    }
    else if (command == "auto off") {
        autoSoundMode = false;
        faceIsListening = false;

        face.setMode(
            FaceController::Mode::Idle
        );
    }
    else if (
        command == "boot" ||
        command == "wake"
    ) {
        autoSoundMode = false;
        faceIsListening = false;

        face.restartBootSequence();
    }
    else if (command == "idle") {
        setManualMode(
            FaceController::Mode::Idle
        );
    }
    else if (command == "listen") {
        setManualMode(
            FaceController::Mode::Listening
        );
    }
    else if (command == "think") {
        setManualMode(
            FaceController::Mode::Thinking
        );
    }
    else if (command == "speak") {
        setManualMode(
            FaceController::Mode::Speaking
        );
    }
    else if (command == "sleep") {
        setManualMode(
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

    Serial.print("Command: ");
    Serial.println(command);

    return true;
}

// Создаём HTTP-маршруты
void setupWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send_P(
            200,
            "text/html; charset=utf-8",
            INDEX_HTML
        );
    });

    server.on(
        "/api/command",
        HTTP_POST,
        []() {
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
        }
    );

    // API с текущим уровнем звука
    server.on(
        "/api/status",
        HTTP_GET,
        []() {
            String json = "{";

            json += "\"microphone\":";
            json += microphone.ready()
                ? "true" : "false";

            json += ",\"calibrated\":";
            json += microphone.calibrated()
                ? "true" : "false";

            json += ",\"active\":";
            json += microphone.isActive()
                ? "true" : "false";

            json += ",\"auto\":";
            json += autoSoundMode
                ? "true" : "false";

            json += ",\"rms\":";
            json += String(
                microphone.rms(), 1
            );

            json += ",\"noise\":";
            json += String(
                microphone.noiseFloor(), 1
            );

            json += ",\"threshold\":";
            json += String(
                microphone.threshold(), 1
            );

            json += "}";

            server.send(
                200,
                "application/json",
                json
            );
        }
    );

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

// Подключаем ESP32 к Wi-Fi
void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
    );

    Serial.print("Connecting to Wi-Fi");

    uint32_t startedAt = millis();

    while (
        WiFi.status() != WL_CONNECTED &&
        millis() - startedAt < 15000
    ) {
        Serial.print(".");

        face.update();

        delay(100);
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi connected!");

        Serial.print("MiniBot IP: ");
        Serial.println(WiFi.localIP());

        setupWebServer();
    } else {
        Serial.println("Wi-Fi connection failed");
    }
}

// Получаем команды через Serial Monitor
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

// Связываем микрофон и глаза
void updateAutomaticFace() {
    if (!autoSoundMode) {
        return;
    }

    if (!microphone.calibrated()) {
        return;
    }

    bool soundActive = microphone.isActive();

    if (soundActive == faceIsListening) {
        return;
    }

    faceIsListening = soundActive;

    if (soundActive) {
        face.setMode(
            FaceController::Mode::Listening
        );
    } else {
        face.setMode(
            FaceController::Mode::Idle
        );
    }
}

// Диагностика микрофона
void logAudioStatus() {
    uint32_t now = millis();

    if (now - lastAudioLogAt < 1000) {
        return;
    }

    lastAudioLogAt = now;

    if (!microphone.ready()) {
        return;
    }

    Serial.printf(
        "Mic RMS: %.0f | Noise: %.0f | "
        "Threshold: %.0f | Active: %s\n",
        microphone.rms(),
        microphone.noiseFloor(),
        microphone.threshold(),
        microphone.isActive() ? "YES" : "NO"
    );
}

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("======================");
    Serial.println("     MiniBot v0.7");
    Serial.println("======================");

    // Запускаем OLED
    face.begin();

    // Подключаем Wi-Fi
    connectWiFi();

    // Запускаем микрофон
    microphone.begin();

    Serial.println("MiniBot ready!");
}

void loop() {
    // 1. Считываем звук
    microphone.update();

    // 2. Реагируем глазами на звук
    updateAutomaticFace();

    // 3. Обновляем анимацию OLED
    face.update();

    // 4. Обрабатываем HTTP
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    // 5. Команды с компьютера по USB
    readSerialCommands();

    // 6. Диагностические сообщения
    logAudioStatus();

    delay(1);
}
