#include "core/GameClockService.h"

void GameClockService::start(uint32_t nowMs, uint32_t gameMinutes) {
    set(nowMs, gameMinutes);
}

uint32_t GameClockService::minutesAt(uint32_t nowMs, float speed) const {
    uint32_t elapsedMs = nowMs - anchorMs_;
    uint32_t scaledMinutes = static_cast<uint32_t>(
        static_cast<double>(elapsedMs) * static_cast<double>(speed) / 60000.0);
    return anchorMinutes_ + scaledMinutes;
}

bool GameClockService::sync(uint32_t nowMs, float speed,
                            uint32_t& gameMinutes) {
    uint32_t total = minutesAt(nowMs, speed);
    if (total == gameMinutes) return false;
    gameMinutes = total;
    return true;
}

void GameClockService::set(uint32_t nowMs, uint32_t gameMinutes) {
    anchorMs_ = nowMs;
    anchorMinutes_ = gameMinutes;
}
