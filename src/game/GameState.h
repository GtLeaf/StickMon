#pragma once

#include <cstdint>

namespace Game {

static constexpr uint8_t TEAM_CAP = 2;
static constexpr uint8_t STORAGE_CAP = 20;
static constexpr uint32_t SAVE_MAGIC = 0x534D4F4E; // SMON
static constexpr uint16_t SAVE_VERSION = 5;
static constexpr uint8_t STAT_COUNT = 6;
static constexpr uint8_t NATURE_COUNT = 25;
static constexpr uint8_t LEVEL_MAX = 50;
static constexpr uint8_t IV_MAX = 31;
static constexpr uint8_t EV_MAX = 252;
static constexpr uint16_t EV_TOTAL_MAX = 510;
static constexpr uint32_t HATCH_MAGIC = 0x48415443; // HATC

enum class ItemId : uint8_t {
    POKE_BALL = 0,
    GREAT_BALL,
    HEAVY_BALL,
    TIMER_BALL,
    NORMAL_FOOD,
    POTION,
    SUPER_POTION,
    ANTIDOTE,
    CANDY,
    COUNT,
};

enum StatusBits : uint8_t {
    STATUS_NONE = 0,
    STATUS_POISON = 1 << 0,
    STATUS_PARALYSIS = 1 << 1,
    STATUS_SLEEP = 1 << 2,
    STATUS_BURN = 1 << 3,
    STATUS_FREEZE = 1 << 4,
    STATUS_CONFUSION = 1 << 5,
};

enum class Origin : uint8_t {
    STARTER = 0,
    HATCHED,
    CAPTURED,
    TRADED,
    GIFT,
    UNKNOWN = 0xFF,
};

struct StatLine {
    uint8_t hp = 0;
    uint8_t atk = 0;
    uint8_t def = 0;
    uint8_t spa = 0;
    uint8_t spd = 0;
    uint8_t spe = 0;
};

inline uint8_t ivAt(uint32_t packed, uint8_t index) {
    if (index >= STAT_COUNT) return 0;
    return (packed >> (index * 5)) & 0x1F;
}

inline void setIv(uint32_t& packed, uint8_t index, uint8_t value) {
    if (index >= STAT_COUNT) return;
    uint32_t shift = index * 5;
    packed &= ~(0x1FUL << shift);
    packed |= ((uint32_t)(value > IV_MAX ? IV_MAX : value) & 0x1F) << shift;
}

inline uint8_t evAt(const StatLine& ev, uint8_t index) {
    switch (index) {
    case 0: return ev.hp;
    case 1: return ev.atk;
    case 2: return ev.def;
    case 3: return ev.spa;
    case 4: return ev.spd;
    case 5: return ev.spe;
    default: return 0;
    }
}

inline uint16_t evTotal(const StatLine& ev) {
    return ev.hp + ev.atk + ev.def + ev.spa + ev.spd + ev.spe;
}

struct MonsterRuntime {
    uint16_t speciesId = 906;
    uint8_t level = 5;
    uint16_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    uint32_t ivPacked = 0;
    StatLine ev;
    uint8_t nature = 0;
    uint8_t affection = 30;
    uint8_t mood = 70;
    uint8_t satiety = 75;
    uint8_t proficiency = 0;
    uint8_t statusBits = STATUS_NONE;
    uint8_t petCountToday = 0;
    Origin origin = Origin::STARTER;
    bool fainted = false;
    uint32_t caughtAt = 0;
    uint32_t lastSeenAt = 0;
    uint32_t lastPettedAt = 0;
};

struct BagState {
    uint8_t pokeBall = 5;
    uint8_t greatBall = 0;
    uint8_t heavyBall = 0;
    uint8_t timerBall = 0;
    uint8_t normalFood = 2;
    uint8_t potion = 2;
    uint8_t superPotion = 0;
    uint8_t antidote = 0;
    uint8_t candy = 1;
};

struct PlayerSettings {
    uint8_t brightness = 128;
    uint8_t speedIndex = 0; // 1x, 2x, 4x, 8x
    uint16_t longPressMs = 500;
    uint16_t doubleClickMs = 300;
    uint8_t volume = 0;
    bool vibrationOn = false;
    uint8_t idleTimeoutIndex = 0; // 0=30s, 1=2min, 2=5min, 3=10min, 4=never
    bool leftHanded = false;
    uint8_t language = 0; // 0 = zh-CN
};

struct GameState {
    uint32_t magic = SAVE_MAGIC;
    uint16_t version = SAVE_VERSION;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    MonsterRuntime team[TEAM_CAP];
    uint8_t storageCount = 0;
    MonsterRuntime storage[STORAGE_CAP];
    BagState bag;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint32_t dayStamp = 0;
    PlayerSettings settings;
};

struct HatchProgress {
    uint32_t magic = HATCH_MAGIC;
    uint16_t checksum = 0;
    uint16_t elapsedSeconds = 0;
    uint16_t pokeCount = 0;
    uint16_t wipeCount = 0;
};

} // namespace Game
