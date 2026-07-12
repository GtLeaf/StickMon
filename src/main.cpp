#include <Arduino.h>
#include "core/GameEngine.h"

namespace {
bool gameReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("[Boot] Starting StickMon...");
    randomSeed(esp_random());
    gameReady = GameEngine::ins().begin();
    if (!gameReady) {
        Serial.println("[Boot] ERROR: GameEngine init failed");
        return;
    }
    Serial.printf("[Boot] PSRAM found=%d size=%u free=%u\n",
                  psramFound() ? 1 : 0,
                  ESP.getPsramSize(),
                  ESP.getFreePsram());
    Serial.println("[Boot] Init done");
}

void loop() {
    if (gameReady) {
        GameEngine::ins().run();
    } else {
        delay(1000);
    }
}
