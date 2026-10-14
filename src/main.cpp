
#include <Arduino.h>
#include <cstring>

#include "FaceController.h"

FaceController face;

// Буфер для ввода команд
char commandBuffer[32];
uint8_t commandLength = 0;

void handleCommand(const char* command) {
    if (strcmp(command, "boot") == 0 ||
        strcmp(command, "wake") == 0) {
        face.restartBootSequence();
    }
    else if (strcmp(command, "idle") == 0) {
        face.setMode(FaceController::Mode::Idle);
    }
    else if (strcmp(command, "listen") == 0) {
        face.setMode(FaceController::Mode::Listening);
    }
    else if (strcmp(command, "think") == 0) {
        face.setMode(FaceController::Mode::Thinking);
    }
    else if (strcmp(command, "speak") == 0) {
        face.setMode(FaceController::Mode::Speaking);
    }
    else if (strcmp(command, "sleep") == 0) {
        face.setMode(FaceController::Mode::Sleeping);
    }
    else if (strcmp(command, "blink") == 0) {
        face.blink();
    }
    else if (strcmp(command, "left") == 0) {
        face.lookAt(-6, 0);
    }
    else if (strcmp(command, "right") == 0) {
        face.lookAt(6, 0);
    }
    else if (strcmp(command, "up") == 0) {
        face.lookAt(0, -4);
    }
    else if (strcmp(command, "down") == 0) {
        face.lookAt(0, 4);
    }
    else if (strcmp(command, "center") == 0) {
        face.lookAt(0, 0);
    }
    else if (strcmp(command, "pupils on") == 0) {
        face.setPupilsEnabled(true);
    }
    else if (strcmp(command, "pupils off") == 0) {
        face.setPupilsEnabled(false);
    }
    else if (strcmp(command, "help") == 0) {
        Serial.println("Available commands:");
        Serial.println("boot, wake, idle, listen");
        Serial.println("think, speak, sleep, blink");
        Serial.println("left, right, up, down, center");
        Serial.println("pupils on, pupils off");
    }
    else {
        Serial.println("Unknown command. Type help");
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

            handleCommand(commandBuffer);

            commandLength = 0;
        }
        else if (commandLength < sizeof(commandBuffer) - 1) {
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

    Serial.println("MiniBot v0.4");

    face.begin();

    Serial.println("Type help for commands");
}

void loop() {
    readSerialCommands();
    face.update();

    delay(1);
}
