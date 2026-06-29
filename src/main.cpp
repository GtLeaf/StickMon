#include <Arduino.h>
#include "core/GameEngine.h"

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("[Boot] Starting StickMon...");
    randomSeed(esp_random());
    GameEngine::ins().begin();
    Serial.printf("[Boot] PSRAM found=%d size=%u free=%u\n",
                  psramFound() ? 1 : 0,
                  ESP.getPsramSize(),
                  ESP.getFreePsram());
    Serial.println("[Boot] Init done");
}

void loop() {
    GameEngine::ins().run();
}

