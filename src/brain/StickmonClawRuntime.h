#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "brain/ClawStatusLog.h"

namespace Stickmon {

// Point-in-time summary of the ESP-Claw connection state, shared by the
// device setup page (电脑 -> ESP-Claw) and the phone setup portal.
struct ClawStatus {
    bool staConnected = false;
    char staIp[16] = {};
    bool portalActive = false;
    bool phoneJoined = false;
    bool clawStarted = false;
    char wechatPhase[16] = {};  // Raw esp-claw status string, e.g. "waiting_scan".
    bool wechatPersisted = false;
};

class ClawRuntime {
public:
    static ClawRuntime& instance();

    bool begin();
    void beginAsync();
    void update(uint32_t nowMs);
    void notePlayerActivity(uint32_t nowMs);
    bool playerActive(uint32_t nowMs) const;
    uint32_t idleSeconds(uint32_t nowMs) const;
    bool agentAllowed(uint32_t nowMs) const;
    bool started() const;
    bool networkConnected() const;
    // An autonomous request is queued or being processed by ESP-Claw.
    bool autonomyActive() const;
    bool startSetupPortal();
    void stopSetupPortal();
    bool setupPortalActive() const;
    bool setupPortalInfo(char* ssid, size_t ssidSize, char* password,
                         size_t passwordSize, char* ip, size_t ipSize) const;

    // Appends a formatted line to the shared status log (64-entry ring).
    void logf(ClawStatusLog::Level level, const char* format, ...);
    uint32_t logGeneration() const;
    // Copies the newest entries (oldest first). Returns the entry count.
    size_t copyLog(ClawStatusLog::Entry* out, size_t maxCount) const;
    // Copies entries appended after generation `since` (oldest first). When
    // generationOut is set it receives the generation read under the same
    // lock, so portals can use it as their next `since` cursor.
    size_t copyLogSince(uint32_t since, ClawStatusLog::Entry* out,
                        size_t maxCount,
                        uint32_t* generationOut = nullptr) const;
    bool setupPhoneJoined() const;
    void statusSnapshot(ClawStatus& status) const;
    // Wi-Fi event hooks used by the shared event handler in the .cpp.
    void noteStaIp(const char* ip);
    void notePhoneJoined(bool joined);

    static constexpr uint32_t AUTONOMY_IDLE_MS = 5UL * 60UL * 1000UL;
    static constexpr uint32_t PLAYER_ACTIVE_GRACE_MS = 1500;
    // The autonomous cadence starts at one minute and backs off after each
    // successful round until it reaches one round per day.
    static constexpr uint32_t AUTONOMY_COOLDOWN_MS = 60UL * 1000UL;
    static constexpr uint32_t AUTONOMY_MAX_COOLDOWN_MS = 24UL * 60UL * 60UL * 1000UL;

private:
    ClawRuntime() = default;
    ClawRuntime(const ClawRuntime&) = delete;
    ClawRuntime& operator=(const ClawRuntime&) = delete;

    static void beginTaskEntry(void* context);
    void beginTask();
    bool startSetupPortalImpl();
    void pollWechatLogin(uint32_t nowMs);

    bool started_ = false;
    bool initializing_ = false;
    bool networkConnected_ = false;
    bool setupPortalActive_ = false;
    bool phoneJoined_ = false;
    uint32_t lastPlayerActivityMs_ = 0;
    uint32_t nextAutonomyAtMs_ = 0;
    uint32_t autonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    uint32_t activeAutonomyCooldownMs_ = AUTONOMY_COOLDOWN_MS;
    uint32_t activeAutonomyRequestId_ = 0;
    uint32_t nextRequestId_ = 1;
    uint32_t activityGeneration_ = 0;
    bool autonomySubmitInFlight_ = false;
    bool playerActivityKnown_ = false;
    char setupSsid_[33] = {};
    char setupPassword_[65] = {};
    char setupIp_[16] = {};
    char staIp_[16] = {};
    char lastWechatStatus_[32] = {};
    bool lastWechatPersisted_ = false;
    uint32_t lastWechatPollMs_ = 0;
    ClawStatusLog log_;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace Stickmon
