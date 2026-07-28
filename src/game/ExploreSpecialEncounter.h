#pragma once

#include <cstdint>

// Special exploration encounters described in
// doc/探索模式精灵分布方案_v1.0.md §八.
// This module is deterministic and hardware-independent so the firmware and
// host-side tests share exactly the same scheduling rules.
namespace ExploreSpecial {

static constexpr uint8_t AREA_COUNT = 6;
static constexpr uint8_t INVALID_AREA = 0xFF;
static constexpr uint8_t ROAMER_COUNT = 2;
static constexpr uint32_t SLOT_PERIOD_MINUTES = 480;
static constexpr uint8_t MEW_WINDOW_PERCENT = 20;
static constexpr uint8_t ROAMING_LEVEL = 70;

static constexpr uint16_t SNORLAX = 143;
static constexpr uint16_t MEW = 151;
static constexpr uint16_t LATIAS = 380;
static constexpr uint16_t LATIOS = 381;

static constexpr uint8_t SNORLAX_AREA = 2;
static constexpr uint8_t LATIAS_AREA = 4;
static constexpr uint8_t MEW_AREA = 4;
static constexpr uint8_t LATIOS_AREA = 5;

static constexpr uint8_t SNORLAX_DEFEATED = 1U << 0;
static constexpr uint8_t LATIAS_DEFEATED = 1U << 1;
static constexpr uint8_t LATIOS_DEFEATED = 1U << 2;
static constexpr uint8_t VALID_DEFEATED_MASK =
    SNORLAX_DEFEATED | LATIAS_DEFEATED | LATIOS_DEFEATED;

enum class Kind : uint8_t {
    NONE = 0,
    FIRST_SNORLAX,
    FIRST_LATIAS,
    FIRST_LATIOS,
    ROAMING_LATIAS,
    ROAMING_LATIOS,
    MEW_EVENT,
};

struct Config {
    uint16_t speciesId;
    uint8_t area;
    uint8_t level;
    uint16_t experiencePercent;
    bool optional;
    bool allowsFriendship;
};

static constexpr Config NO_CONFIG =
    {0, INVALID_AREA, 0, 100, false, false};
static constexpr Config FIRST_SNORLAX_CONFIG =
    {SNORLAX, SNORLAX_AREA, 27, 200, false, true};
static constexpr Config FIRST_LATIAS_CONFIG =
    {LATIAS, LATIAS_AREA, 53, 200, false, false};
static constexpr Config FIRST_LATIOS_CONFIG =
    {LATIOS, LATIOS_AREA, 67, 200, false, false};
static constexpr Config ROAMING_LATIAS_CONFIG =
    {LATIAS, INVALID_AREA, ROAMING_LEVEL, 200, true, true};
static constexpr Config ROAMING_LATIOS_CONFIG =
    {LATIOS, INVALID_AREA, ROAMING_LEVEL, 200, true, true};
static constexpr Config MEW_EVENT_CONFIG =
    {MEW, MEW_AREA, ROAMING_LEVEL, 200, true, true};

constexpr Config configFor(Kind kind) {
    return kind == Kind::FIRST_SNORLAX
        ? FIRST_SNORLAX_CONFIG
        : (kind == Kind::FIRST_LATIAS
            ? FIRST_LATIAS_CONFIG
            : (kind == Kind::FIRST_LATIOS
                ? FIRST_LATIOS_CONFIG
                : (kind == Kind::ROAMING_LATIAS
                    ? ROAMING_LATIAS_CONFIG
                    : (kind == Kind::ROAMING_LATIOS
                        ? ROAMING_LATIOS_CONFIG
                        : (kind == Kind::MEW_EVENT
                            ? MEW_EVENT_CONFIG
                            : NO_CONFIG)))));
}

constexpr bool isFirstBoss(Kind kind) {
    return kind == Kind::FIRST_SNORLAX ||
           kind == Kind::FIRST_LATIAS ||
           kind == Kind::FIRST_LATIOS;
}

constexpr bool isRoaming(Kind kind) {
    return kind == Kind::ROAMING_LATIAS ||
           kind == Kind::ROAMING_LATIOS;
}

constexpr int8_t roamingIndex(Kind kind) {
    return kind == Kind::ROAMING_LATIAS
        ? 0
        : (kind == Kind::ROAMING_LATIOS ? 1 : -1);
}

constexpr uint8_t defeatedBit(Kind kind) {
    return kind == Kind::FIRST_SNORLAX
        ? SNORLAX_DEFEATED
        : (kind == Kind::FIRST_LATIAS
            ? LATIAS_DEFEATED
            : (kind == Kind::FIRST_LATIOS ? LATIOS_DEFEATED : 0));
}

inline Kind firstBossForArea(uint8_t area, uint8_t defeatedMask) {
    if (area == SNORLAX_AREA &&
        (defeatedMask & SNORLAX_DEFEATED) == 0) {
        return Kind::FIRST_SNORLAX;
    }
    if (area == LATIAS_AREA &&
        (defeatedMask & LATIAS_DEFEATED) == 0) {
        return Kind::FIRST_LATIAS;
    }
    if (area == LATIOS_AREA &&
        (defeatedMask & LATIOS_DEFEATED) == 0) {
        return Kind::FIRST_LATIOS;
    }
    return Kind::NONE;
}

inline uint32_t mixSeed(uint32_t slotIndex, uint16_t speciesId,
                        uint8_t rerollCount) {
    uint32_t seed = slotIndex * 0x9E3779B9UL ^
                    static_cast<uint32_t>(speciesId + 1U) * 0x85EBCA6BUL ^
                    static_cast<uint32_t>(rerollCount + 1U) * 0xC2B2AE35UL;
    seed ^= seed >> 16;
    seed *= 0x7FEB352DUL;
    seed ^= seed >> 15;
    seed *= 0x846CA68BUL;
    seed ^= seed >> 16;
    return seed == 0 ? 1 : seed;
}

inline uint32_t slotIndexFor(uint32_t gameMinutesTotal) {
    return gameMinutesTotal / SLOT_PERIOD_MINUTES;
}

inline uint8_t rawRoamingArea(uint32_t slotIndex, uint16_t speciesId,
                              uint8_t rerollCount) {
    return static_cast<uint8_t>(
        mixSeed(slotIndex, speciesId, rerollCount) % AREA_COUNT);
}

// Replays at most 255 deterministic migrations. Each migration is guaranteed
// to differ from the species' previous position without storing an area byte.
inline uint8_t roamingArea(uint32_t slotIndex, uint16_t speciesId,
                           uint8_t rerollCount) {
    uint8_t area = rawRoamingArea(slotIndex, speciesId, 0);
    for (uint16_t step = 1; step <= rerollCount; ++step) {
        uint32_t seed = mixSeed(
            slotIndex, speciesId, static_cast<uint8_t>(step));
        uint8_t candidate = static_cast<uint8_t>(seed % AREA_COUNT);
        if (candidate == area) {
            uint8_t delta = static_cast<uint8_t>(1 + ((seed >> 8) % 5));
            candidate = static_cast<uint8_t>((candidate + delta) % AREA_COUNT);
        }
        area = candidate;
    }
    return area;
}

inline void resolveRoamingAreas(uint8_t defeatedMask, uint32_t slotIndex,
                                const uint8_t rerollCounts[ROAMER_COUNT],
                                uint8_t outAreas[ROAMER_COUNT]) {
    outAreas[0] = INVALID_AREA;
    outAreas[1] = INVALID_AREA;
    if ((defeatedMask & LATIAS_DEFEATED) != 0) {
        outAreas[0] = roamingArea(slotIndex, LATIAS, rerollCounts[0]);
    }
    if ((defeatedMask & LATIOS_DEFEATED) != 0) {
        outAreas[1] = roamingArea(slotIndex, LATIOS, rerollCounts[1]);
        if (outAreas[0] != INVALID_AREA && outAreas[1] == outAreas[0]) {
            outAreas[1] = static_cast<uint8_t>(
                (outAreas[1] + 1U) % AREA_COUNT);
        }
    }
}

inline bool mewWindowActive(uint8_t defeatedMask, uint32_t slotIndex,
                            bool ownsMew) {
    const uint8_t required = LATIAS_DEFEATED | LATIOS_DEFEATED;
    if ((defeatedMask & required) != required || ownsMew) return false;
    return mixSeed(slotIndex, MEW, 0) % 100U < MEW_WINDOW_PERCENT;
}

// Priority: first special boss > roaming Latias/Latios > Mew event.
inline Kind kindForArea(uint8_t area, uint8_t defeatedMask,
                        uint32_t slotIndex,
                        const uint8_t rerollCounts[ROAMER_COUNT],
                        bool ownsMew) {
    if (area >= AREA_COUNT) return Kind::NONE;
    Kind first = firstBossForArea(area, defeatedMask);
    if (first != Kind::NONE) return first;

    uint8_t roamingAreas[ROAMER_COUNT] = {};
    resolveRoamingAreas(defeatedMask, slotIndex, rerollCounts, roamingAreas);
    if (roamingAreas[0] == area) return Kind::ROAMING_LATIAS;
    if (roamingAreas[1] == area) return Kind::ROAMING_LATIOS;
    if (area == MEW_AREA &&
        mewWindowActive(defeatedMask, slotIndex, ownsMew)) {
        return Kind::MEW_EVENT;
    }
    return Kind::NONE;
}

static_assert(configFor(Kind::FIRST_SNORLAX).level == 27 &&
                  configFor(Kind::FIRST_LATIAS).level == 53 &&
                  configFor(Kind::FIRST_LATIOS).level == 67,
              "first special boss levels must match area progression");
static_assert(configFor(Kind::ROAMING_LATIAS).level == ROAMING_LEVEL &&
                  configFor(Kind::ROAMING_LATIOS).level == ROAMING_LEVEL &&
                  configFor(Kind::MEW_EVENT).level == ROAMING_LEVEL,
              "roaming legendary and mythical encounters must stay level 70");

} // namespace ExploreSpecial
