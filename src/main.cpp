#include <Arduino.h>
#include "core/GameEngine.h"
#include "game/GameRandom.h"
#include "platform/api/PlatformServices.h"
#include "platform/m5stick_s3/M5StickS3Platform.h"

namespace {
bool gameReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(100);
    bindM5StickS3Platform();

    // 深度睡眠定时静默唤醒：不启动游戏，只跑照护逻辑后立即再次深睡
    // （runSilentCareWake 内部重新武装定时器并调用 esp_deep_sleep_start，不返回）。
    if (Platform::power().wakeReason() == Platform::WakeReason::TIMER) {
        GameEngine::ins().runSilentCareWake();
    }

    Serial.println("[Boot] Starting StickMon...");
    const uint32_t randomSeedValue = Platform::power().hardwareRandom();
    randomSeed(randomSeedValue);
    GameRandom::seed(randomSeedValue);
    gameReady = GameEngine::ins().begin();
    if (!gameReady) {
        Serial.println("[Boot] ERROR: GameEngine init failed");
        return;
    }
    Serial.printf("[Boot] PSRAM found=%d size=%u free=%u\n",
                  Platform::power().externalMemorySize() > 0 ? 1 : 0,
                  Platform::power().externalMemorySize(),
                  Platform::power().externalMemoryFree());
    Serial.println("[Boot] Init done");
}

void loop() {
    if (gameReady) {
        GameEngine::ins().run();
    } else {
        Platform::clock().sleepMs(1000);
    }
}
