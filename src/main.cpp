#include <Arduino.h>
#include <esp_sleep.h>
#include "core/GameEngine.h"

namespace {
bool gameReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(100);

    // 深度睡眠定时静默唤醒：不启动游戏，只跑照护逻辑后立即再次深睡
    // （runSilentCareWake 内部重新武装定时器并调用 esp_deep_sleep_start，不返回）。
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        GameEngine::ins().runSilentCareWake();
    }

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
