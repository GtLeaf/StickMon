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

enum class NaturalVisitEvent : uint8_t {
    PLAY,
    GIFT,
    EXPLORE,
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

constexpr bool naturalVisitEligible(Value value) {
    return value >= 0;
}

constexpr uint8_t naturalVisitDailyChance(Level highestLevel) {
    return highestLevel == Level::CLOSE ? 20
        : highestLevel == Level::TRUSTED ? 14
        : highestLevel == Level::FAMILIAR ? 8
        : highestLevel == Level::ACQUAINTED ? 4
                                           : 0;
}

constexpr uint8_t naturalVisitLevelWeight(Level level) {
    return level == Level::CLOSE ? 36
        : level == Level::TRUSTED ? 12
        : level == Level::FAMILIAR ? 4
        : level == Level::ACQUAINTED ? 1
                                    : 0;
}

constexpr uint8_t naturalVisitLevelMask(Level level) {
    return level >= Level::ACQUAINTED && level <= Level::CLOSE
        ? static_cast<uint8_t>(
              1U << (static_cast<uint8_t>(level) -
                     static_cast<uint8_t>(Level::ACQUAINTED)))
        : 0;
}

inline uint8_t naturalVisitTotalWeight(uint8_t levelMask) {
    uint8_t total = 0;
    for (uint8_t raw = static_cast<uint8_t>(Level::ACQUAINTED);
         raw <= static_cast<uint8_t>(Level::CLOSE); ++raw) {
        Level level = static_cast<Level>(raw);
        if ((levelMask & naturalVisitLevelMask(level)) != 0) {
            total = static_cast<uint8_t>(
                total + naturalVisitLevelWeight(level));
        }
    }
    return total;
}

inline Level naturalVisitLevelForRoll(uint8_t levelMask, uint16_t roll) {
    uint8_t total = naturalVisitTotalWeight(levelMask);
    if (total == 0) return Level::ACQUAINTED;
    roll %= total;
    for (uint8_t raw = static_cast<uint8_t>(Level::ACQUAINTED);
         raw <= static_cast<uint8_t>(Level::CLOSE); ++raw) {
        Level level = static_cast<Level>(raw);
        if ((levelMask & naturalVisitLevelMask(level)) == 0) continue;
        uint8_t weight = naturalVisitLevelWeight(level);
        if (roll < weight) return level;
        roll -= weight;
    }
    return Level::CLOSE;
}

inline NaturalVisitEvent naturalVisitEvent(Level level, uint8_t roll) {
    roll %= 100;
    if (level == Level::CLOSE) {
        return roll < 45 ? NaturalVisitEvent::PLAY
            : roll < 80 ? NaturalVisitEvent::GIFT
                        : NaturalVisitEvent::EXPLORE;
    }
    if (level == Level::TRUSTED) {
        return roll < 55 ? NaturalVisitEvent::PLAY
            : roll < 90 ? NaturalVisitEvent::GIFT
                        : NaturalVisitEvent::EXPLORE;
    }
    if (level == Level::FAMILIAR) {
        return roll < 80 ? NaturalVisitEvent::PLAY
                         : NaturalVisitEvent::GIFT;
    }
    return NaturalVisitEvent::PLAY;
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
