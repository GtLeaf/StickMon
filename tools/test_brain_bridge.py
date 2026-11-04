#!/usr/bin/env python3
"""Deterministic host checks for the real BrainBridge queue."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
class BrainBridgeTests(unittest.TestCase):
    def test_queue_timeout_notification_and_reuse(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            headers = {
                "esp_err.h": """#pragma once
using esp_err_t = int;
constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;
constexpr int ESP_ERR_INVALID_ARG = -2;
constexpr int ESP_ERR_INVALID_STATE = -3;
constexpr int ESP_ERR_NO_MEM = -4;
""",
                "claw_core.h": """#pragma once
struct claw_core_request_t { int unused; };
struct claw_core_context_t { int kind; char* content; };
constexpr int CLAW_CORE_CONTEXT_KIND_SYSTEM_PROMPT = 1;
""",
                "freertos/FreeRTOS.h": """#pragma once
#include <cstdint>
#include <mutex>
struct portMUX_TYPE {
    std::recursive_mutex mutex;
    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }
};
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) ((mux)->lock())
#define portEXIT_CRITICAL(mux) ((mux)->unlock())
using TickType_t = uint32_t;
using BaseType_t = int;
constexpr int pdFALSE = 0;
constexpr int pdTRUE = 1;
#define pdMS_TO_TICKS(ms) (ms)
""",
                "freertos/semphr.h": """#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include "freertos/FreeRTOS.h"

namespace host_sem {
inline std::mutex hookMutex;
inline std::condition_variable hookCv;
inline bool blockNextGive = false;
inline bool giveEntered = false;
inline int giveCount = 0;

inline void reset() {
    std::lock_guard<std::mutex> lock(hookMutex);
    blockNextGive = false;
    giveEntered = false;
    giveCount = 0;
    hookCv.notify_all();
}
inline void blockNext() {
    std::lock_guard<std::mutex> lock(hookMutex);
    blockNextGive = true;
    giveEntered = false;
}
inline bool waitGiveEntered() {
    std::unique_lock<std::mutex> lock(hookMutex);
    return hookCv.wait_for(lock, std::chrono::seconds(2), [] {
        return giveEntered;
    });
}
inline void release() {
    std::lock_guard<std::mutex> lock(hookMutex);
    blockNextGive = false;
    hookCv.notify_all();
}
inline int gives() {
    std::lock_guard<std::mutex> lock(hookMutex);
    return giveCount;
}
inline void beforeGive() {
    std::unique_lock<std::mutex> lock(hookMutex);
    ++giveCount;
    if (!blockNextGive) return;
    giveEntered = true;
    hookCv.notify_all();
    hookCv.wait(lock, [] { return !blockNextGive; });
}
}  // namespace host_sem

struct HostSemaphore {
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
};
using SemaphoreHandle_t = HostSemaphore*;
inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return new HostSemaphore();
}
inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) {
    delete semaphore;
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t waitMs) {
    std::unique_lock<std::mutex> lock(semaphore->mutex);
    if (waitMs == 0) {
        if (!semaphore->signaled) return pdFALSE;
        semaphore->signaled = false;
        return pdTRUE;
    }
    semaphore->cv.wait_for(lock, std::chrono::milliseconds(waitMs), [&] {
        return semaphore->signaled;
    });
    if (!semaphore->signaled) return pdFALSE;
    semaphore->signaled = false;
    return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    host_sem::beforeGive();
    {
        std::lock_guard<std::mutex> lock(semaphore->mutex);
        semaphore->signaled = true;
    }
    semaphore->cv.notify_all();
    return pdTRUE;
}
""",
            }
            for name, content in headers.items():
                path = base / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            binary = base / "brain_bridge_host"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-pthread", "-DSTICKMON_CLAW_INTERNAL=1",
                "-I" + directory, "-Isrc", "tools/brain_bridge_host.cpp",
                "src/brain/BrainBridge.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
