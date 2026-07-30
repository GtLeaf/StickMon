#include "core/SaveCoordinator.h"

void SaveCoordinator::mark(uint32_t nowMs, Priority priority) {
    if (!dirty_) dirtySinceMs_ = nowMs;
    dirty_ = true;
    lastMutationMs_ = nowMs;
    if (priority == Priority::SOON && !soon_) {
        soon_ = true;
        soonSinceMs_ = nowMs;
    }
}

bool SaveCoordinator::due(uint32_t nowMs) const {
    if (!dirty_ || nowMs - lastSaveMs_ < MIN_INTERVAL_MS) return false;
    bool soonDue = soon_ &&
        (nowMs - lastMutationMs_ >= SOON_QUIET_MS ||
         nowMs - soonSinceMs_ >= SOON_MAX_DELAY_MS);
    return soonDue || nowMs - dirtySinceMs_ >= DEFERRED_MAX_DELAY_MS;
}

void SaveCoordinator::recordAttempt(uint32_t nowMs, bool success) {
    lastSaveMs_ = nowMs;
    if (success) {
        dirty_ = false;
        soon_ = false;
        dirtySinceMs_ = 0;
        soonSinceMs_ = 0;
        lastMutationMs_ = 0;
        return;
    }
    dirty_ = true;
    soon_ = true;
    if (dirtySinceMs_ == 0) dirtySinceMs_ = nowMs;
    soonSinceMs_ = nowMs;
    lastMutationMs_ = nowMs;
}

void SaveCoordinator::reset(uint32_t nowMs) {
    lastSaveMs_ = nowMs;
    dirtySinceMs_ = 0;
    soonSinceMs_ = 0;
    lastMutationMs_ = 0;
    dirty_ = false;
    soon_ = false;
}
