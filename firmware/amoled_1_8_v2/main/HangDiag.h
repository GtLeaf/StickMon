#pragma once

// Optional main-loop stall detector for field hang reports. Disabled unless the
// build passes -DSTICKMON_HANG_DIAG=ON. When the main loop stays in one
// non-idle stage for HANG_MS, it prints both task states and aborts so the
// panic handler can write a core dump with every task's blocking backtrace.

#include <cstdint>

#if STICKMON_HANG_DIAG
#include <atomic>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace AmoledV2::HangDiag {

enum Stage : uint8_t {
    IDLE_WAIT = 0, LOOP_TOP, POWER, TOUCH, UPDATE, PRE_RENDER, RENDER,
    COLLECT_WAIT, SUBMIT_SYNC, POST, STANDBY,
};
enum WorkerStage : uint8_t { WORKER_WAIT = 0, WORKER_TRANSFER, WORKER_RESULT };

#if STICKMON_HANG_DIAG
inline constexpr uint32_t HANG_MS = 3000;
inline std::atomic<uint8_t> mainStage{IDLE_WAIT};
inline std::atomic<uint8_t> workerStage{WORKER_WAIT};
inline std::atomic<uint32_t> mainSinceMs{0};
inline std::atomic<uint32_t> workerSinceMs{0};
inline TaskHandle_t mainTask = nullptr;
inline TaskHandle_t worker = nullptr;

inline uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

inline void mark(Stage stage) {
    mainSinceMs.store(nowMs());
    mainStage.store(stage);
}
inline void markWorker(WorkerStage stage) {
    workerSinceMs.store(nowMs());
    workerStage.store(stage);
}
inline void setWorker(TaskHandle_t task) { worker = task; }

inline void monitor(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        const uint8_t stage = mainStage.load();
        if (stage == IDLE_WAIT || stage == STANDBY) continue;
        const uint32_t now = nowMs();
        const uint32_t stuck = now - mainSinceMs.load();
        if (stuck < HANG_MS) continue;
        // ROM printf takes no stdout lock that the stuck task might hold.
        esp_rom_printf("\n[HangDiag] main stage=%u stuck=%u ms state=%d hw=%u\n",
                       stage, static_cast<unsigned>(stuck),
                       static_cast<int>(eTaskGetState(mainTask)),
                       static_cast<unsigned>(uxTaskGetStackHighWaterMark(mainTask)));
        if (worker) {
            esp_rom_printf("[HangDiag] worker stage=%u for=%u ms state=%d hw=%u\n",
                           workerStage.load(),
                           static_cast<unsigned>(now - workerSinceMs.load()),
                           static_cast<int>(eTaskGetState(worker)),
                           static_cast<unsigned>(uxTaskGetStackHighWaterMark(worker)));
        }
        esp_rom_printf("[HangDiag] aborting to write core dump\n");
        esp_system_abort("StickMon main loop hang");
    }
}

inline void start() {
    mainTask = xTaskGetCurrentTaskHandle();
    mark(LOOP_TOP);
    xTaskCreate(monitor, "hang_diag", 3072, nullptr, configMAX_PRIORITIES - 2, nullptr);
}
#else
inline void mark(Stage) {}
inline void markWorker(WorkerStage) {}
inline void setWorker(void*) {}
inline void start() {}
#endif

}  // namespace AmoledV2::HangDiag
