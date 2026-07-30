#pragma once

#include <cstdint>

class SaveCoordinator {
public:
    enum class Priority : uint8_t {
        DEFERRED,
        SOON,
    };

    void mark(uint32_t nowMs, Priority priority);
    bool due(uint32_t nowMs) const;
    void recordAttempt(uint32_t nowMs, bool success);
    void reset(uint32_t nowMs = 0);
    bool dirty() const { return dirty_; }

private:
    static constexpr uint32_t SOON_QUIET_MS = 2000UL;
    static constexpr uint32_t SOON_MAX_DELAY_MS = 15000UL;
    static constexpr uint32_t DEFERRED_MAX_DELAY_MS = 300000UL;
    static constexpr uint32_t MIN_INTERVAL_MS = 1000UL;

    uint32_t lastSaveMs_ = 0;
    uint32_t dirtySinceMs_ = 0;
    uint32_t soonSinceMs_ = 0;
    uint32_t lastMutationMs_ = 0;
    bool dirty_ = false;
    bool soon_ = false;
};
