#pragma once

#include <cstdint>

namespace Game::Bond {

using Value = int8_t;

static constexpr Value MIN_VALUE = -100;
static constexpr Value MAX_VALUE = 100;
static constexpr Value NEW_CONTACT_VALUE = 10;
static constexpr uint8_t FAINT_LOSS = 10;
static constexpr uint8_t INVITE_LOCK_FLAG = 0x80;
static constexpr uint16_t INVITE_DAY_START_MINUTE = 7U * 60U;

enum class Level : uint8_t {
    AVERSE = 1,
    DISTANT,
    ACQUAINTED,
    FAMILIAR,
    TRUSTED,
    CLOSE,
};

constexpr Level levelFor(Value value) {
    return value >= 75 ? Level::CLOSE
        : value >= 50 ? Level::TRUSTED
        : value >= 25 ? Level::FAMILIAR
        : value >= 0 ? Level::ACQUAINTED
        : value >= -50 ? Level::DISTANT
                       : Level::AVERSE;
}

constexpr uint8_t adventureGain(uint16_t steps) {
    return steps == 0 ? 0
        : steps < 15 ? 1
        : steps < 40 ? 2
        : steps < 80 ? 3
                     : 4;
}

constexpr Value increase(Value value, uint8_t amount) {
    return static_cast<int16_t>(value) + amount > MAX_VALUE
        ? MAX_VALUE
        : static_cast<Value>(value + amount);
}

constexpr Value decrease(Value value, uint8_t amount) {
    return static_cast<int16_t>(value) - amount < MIN_VALUE
        ? MIN_VALUE
        : static_cast<Value>(value - amount);
}

constexpr uint8_t inviteChance(Value value, bool firstInvitation) {
    return firstInvitation ? 100
        : value >= 75 ? 100
        : value >= 50 ? 85
        : value >= 25 ? 60
        : value >= 0 ? 35
        : value >= -50 ? 15
                       : 5;
}

constexpr uint32_t invitationDay(uint32_t gameMinutesTotal) {
    return gameMinutesTotal < INVITE_DAY_START_MINUTE
        ? 0
        : (gameMinutesTotal - INVITE_DAY_START_MINUTE) / (24U * 60U) + 1U;
}

constexpr uint8_t inviteLockMarker(uint32_t day) {
    return static_cast<uint8_t>(
        INVITE_LOCK_FLAG | static_cast<uint8_t>(day & 0x7F));
}

constexpr bool inviteLockedToday(uint8_t marker, uint32_t day) {
    return (marker & INVITE_LOCK_FLAG) != 0 &&
           (marker & 0x7F) == (day & 0x7F);
}

} // namespace Game::Bond
