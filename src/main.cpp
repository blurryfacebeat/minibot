
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "FaceController.h"
#include "AudioRecorder.h"
#include "WifiSecrets.h"

FaceController face;
AudioRecorder recorder;

WebServer server(80);

char commandBuffer[40];
size_t commandLength = 0;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>MiniBot</title>

  <style>
    body {
      margin: 0;
      padding: 24px;
      background: #111318;
      color: white;
      font-family: system-ui, sans-serif;
    }

    main {
      max-width: 480px;
      margin: auto;
    }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }

    button {
      padding: 16px;
      border: 1px solid #394150;
      border-radius: 12px;
      background: #242833;
      color: white;
      font-size: 15px;
      cursor: pointer;
    }

    .status {
      margin: 20px 0;
      color: #86efac;
    }

    p {
      color: #9ca3af;
    }
  </style>
</head>

<body>
  <main>
    <h1>🤖 MiniBot v0.8</h1>

    <p>
      ESP32 подключена к Wi-Fi.
      Запись аудио запускается
      через сервер на Windows.
    </p>

    <h2>Управление глазами</h2>

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

      <button onclick="send('left')">
        Влево
      </button>

      <button onclick="send('right')">
        Вправо
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

    <div id="status" class="status">
      Готов к работе
    </div>
  </main>

  <script>
    async function send(cmd) {
      const status =
        document.getElementById('status');

      try {
        const response = await fetch(
          '/api/command',
          {
            method: 'POST',
            headers: {
              'Content-Type':
                'application/x-www-form-urlencoded'
            },
            body: new URLSearchParams({ cmd })
          }
        );

        if (!response.ok) {
          throw new Error(await response.text());
        }

        status.textContent = 'Выполнено: ' + cmd;
      } catch (error) {
        status.textContent = error.message;
      }
    }
  </script>
</body>
</html>
)HTML";

bool executeCommand(String command) {
    command.trim();
    command.toLowerCase();

    if (command == "boot" || command == "wake") {
        face.restartBootSequence();
    }
    else if (command == "idle") {
        face.setMode(FaceController::Mode::Idle);
    }
    else if (command == "listen") {
        face.setMode(FaceController::Mode::Listening);
    }
    else if (command == "think") {
        face.setMode(FaceController::Mode::Thinking);
    }
    else if (command == "speak") {
        face.setMode(FaceController::Mode::Speaking);
    }
    else if (command == "sleep") {
        face.setMode(FaceController::Mode::Sleeping);
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

void updateFaceDuringRecording() {
    face.update();
}

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
                server.send(200, "text/plain", "OK");
            } else {
                server.send(
                    400,
                    "text/plain",
                    "Unknown command"
                );
            }
        }
    );

    // Новый маршрут записи
    server.on(
        "/api/record",
        HTTP_POST,
        []() {
            if (!recorder.ready()) {
                server.send(
                    503,
                    "text/plain",
                    "Microphone unavailable"
                );

                return;
            }

            face.setMode(
                FaceController::Mode::Listening
            );

            Serial.println(
                "HTTP audio recording requested"
            );

            // Сообщаем точную длину потока.
            server.sendHeader(
                "Cache-Control",
                "no-store"
            );

            server.setContentLength(
                AudioRecorder::PCM_BYTES
            );

            server.send(
                200,
                "application/octet-stream",
                ""
            );

            // Отправляем PCM прямо клиенту.
            WiFiClient client = server.client();

            bool success = recorder.stream(
                client,
                updateFaceDuringRecording
            );

            // Восстанавливаем обычную
            // обработку следующих HTTP-ответов.
            server.setContentLength(
                CONTENT_LENGTH_NOT_SET
            );

            face.setMode(
                FaceController::Mode::Idle
            );

            if (success) {
                Serial.println(
                    "Audio transfer finished"
                );
            } else {
                Serial.println(
                    "Audio transfer failed"
                );
            }
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
        face.update();
        delay(100);
        Serial.print(".");
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

void readSerialCommands() {
    while (Serial.available()) {
        char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            if (commandLength == 0) {
                continue;
            }

            commandBuffer[commandLength] = '\0';

            if (!executeCommand(commandBuffer)) {
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
    Serial.println("    MiniBot v0.8");
    Serial.println("====================");

    face.begin();

    recorder.begin();

    connectWiFi();

    Serial.println("MiniBot ready!");
}

void loop() {
    face.update();

    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    readSerialCommands();

    delay(1);
}
