#pragma once

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_pm.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include "esp_private/esp_clk.h"
#endif

namespace Amoled {

// One UI owner; peripheral drivers hold their own locks during transactions.
class LockPower {
public:
    bool begin() {
#if CONFIG_PM_ENABLE
        esp_err_t error = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0,
                                             "room_awake_cpu", &cpuLock_);
        if (error == ESP_OK)
            error = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0,
                                       "room_awake", &awakeLock_);
        if (error == ESP_OK) error = esp_pm_lock_acquire(cpuLock_);
        if (error == ESP_OK) error = esp_pm_lock_acquire(awakeLock_);
        if (error != ESP_OK) {
            ESP_LOGE("LockPower", "PM locks unavailable: %s", esp_err_to_name(error));
            return false;
        }
        esp_pm_config_t config{};
        config.max_freq_mhz = 240;
        config.min_freq_mhz = 80;
        config.light_sleep_enable = true;
        error = esp_pm_configure(&config);
        ready_ = error == ESP_OK;
        ESP_LOGI("LockPower", "PM 80-240MHz automatic light sleep: %s",
                 esp_err_to_name(error));
        return ready_;
#else
        ESP_LOGW("LockPower", "PM disabled; lock cadence remains active");
        return false;
#endif
    }

    void setLockedRoom(bool lockedRoom, bool touchWakeReady) {
#if CONFIG_PM_ENABLE
        if (!ready_) return;
        if (lockedRoom != locked_) {
            if (lockedRoom) esp_pm_lock_release(cpuLock_);
            else esp_pm_lock_acquire(cpuLock_);
            locked_ = lockedRoom;
            ESP_LOGI("LockPower", "room=%s cpu=%s brightness=unchanged",
                     locked_ ? "locked" : "awake", locked_ ? "80MHz idle" : "240MHz");
        }
        const bool allowSleep = lockedRoom && touchWakeReady;
        if (allowSleep != sleepAllowed_) {
            if (allowSleep) esp_pm_lock_release(awakeLock_);
            else esp_pm_lock_acquire(awakeLock_);
            sleepAllowed_ = allowSleep;
        }
#endif
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    void report() const {
        // The running diagnostic task holds the RTOS CPU-max lock. Use PM
        // profiling mode-time deltas to measure idle frequency residency.
        ESP_LOGI("LockPower", "[PowerDiag] locked=%u sleepAllowed=%u cpu=%dMHz",
                 locked_ ? 1U : 0U, sleepAllowed_ ? 1U : 0U,
                 esp_clk_cpu_freq() / 1000000);
        esp_pm_dump_locks(stdout);
    }
#endif

private:
#if CONFIG_PM_ENABLE
    esp_pm_lock_handle_t cpuLock_ = nullptr;
    esp_pm_lock_handle_t awakeLock_ = nullptr;
#endif
    bool ready_ = false;
    bool locked_ = false;
    bool sleepAllowed_ = false;
};

} // namespace Amoled
