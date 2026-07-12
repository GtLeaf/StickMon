#include "core/SaveManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstdlib>
#include <cstring>
#include <new>
#include "game/Species.h"

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* CLOCK_KEY = "clock_min";
constexpr uint16_t LEGACY_SAVE_VERSION_V8 = 8;
constexpr uint16_t LEGACY_SAVE_VERSION_V9 = 9;
constexpr uint16_t LEGACY_SAVE_VERSION_V10 = 10;
constexpr uint16_t LEGACY_SAVE_VERSION_V11 = 11;
constexpr uint16_t LEGACY_SAVE_VERSION_V12 = 12;
constexpr uint16_t GAME_MINUTES_PER_DAY = 24U * 60U;

struct LegacyRoomStateV12 {
    uint8_t food[Game::ROOM_FOOD_COUNT] = {2, 0};
    uint8_t selectedFood = 0;
    uint8_t roomStyle = 0;
    uint8_t activeToy = 0;
    uint8_t ownedToys = 0;
    uint16_t ownedFurniture = 0;
    uint16_t placedFurniture = 0;
};

struct LegacyMonsterRuntimeV8 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    uint32_t ivPacked = 0;
    Game::StatLine ev;
    uint8_t nature = 0;
    uint8_t affection = 30;
    uint8_t mood = 70;
    uint8_t satiety = 75;
    uint8_t proficiency = 0;
    uint8_t statusBits = Game::STATUS_NONE;
    uint8_t metArea = Game::MET_AREA_UNKNOWN;
    uint8_t petCountToday = 0;
    Game::Origin origin = Game::Origin::STARTER;
    bool fainted = false;
    uint32_t caughtAt = 0;
    uint32_t lastSeenAt = 0;
    uint32_t lastPettedAt = 0;
};

struct LegacyGameStateV8 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V8;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV8 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV8 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    LegacyRoomStateV12 room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyMonsterRuntimeV10 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    uint8_t move1Id = 0;
    uint8_t move2Id = 0;
    uint32_t ivPacked = 0;
    Game::StatLine ev;
    uint8_t nature = 0;
    uint8_t affection = 30;
    uint8_t mood = 70;
    uint8_t satiety = 75;
    uint8_t proficiency = 0;
    uint8_t statusBits = Game::STATUS_NONE;
    uint8_t metArea = Game::MET_AREA_UNKNOWN;
    uint8_t petCountToday = 0;
    Game::Origin origin = Game::Origin::STARTER;
    bool fainted = false;
    uint32_t caughtAt = 0;
    uint32_t lastSeenAt = 0;
    uint32_t lastPettedAt = 0;
};

struct LegacyGameStateV9 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V9;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV10 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV10 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    LegacyRoomStateV12 room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV10 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V10;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV10 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV10 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    LegacyRoomStateV12 room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV12 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V12;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    Game::MonsterRuntime team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    Game::MonsterRuntime storage[Game::STORAGE_CAP];
    Game::BagState bag;
    LegacyRoomStateV12 room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

template <typename T>
uint16_t checksumObject(const T& object) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&object);
    const uint8_t* checksumBytes = reinterpret_cast<const uint8_t*>(&object.checksum);
    size_t checksumOffset = static_cast<size_t>(checksumBytes - bytes);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(object); ++i) {
        uint8_t value = (i == checksumOffset || i == checksumOffset + 1) ? 0 : bytes[i];
        crc ^= value;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

void resetGameState(Game::GameState& state) {
    state.~GameState();
    new (&state) Game::GameState();
}

uint32_t gameSecondsForMinutes(uint32_t minutes) {
    uint64_t seconds = static_cast<uint64_t>(minutes) * 60ULL;
    return seconds > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : static_cast<uint32_t>(seconds);
}

bool sanitizeMonster(Game::MonsterRuntime& mon) {
    Game::MonsterRuntime before = mon;
    const Species* species = findSpecies(mon.speciesId);
    if (!species) {
        species = &starterSpecies();
        mon.speciesId = species->id;
    }

    if (mon.level < 1) mon.level = 1;
    if (mon.level > Game::LEVEL_MAX) mon.level = Game::LEVEL_MAX;
    uint32_t minExp = minimumExpForLevel(species->growthRate, mon.level);
    uint32_t maxExp = minimumExpForLevel(species->growthRate, Game::LEVEL_MAX);
    if (mon.exp < minExp) mon.exp = minExp;
    if (mon.exp > maxExp) mon.exp = maxExp;

    mon.ivPacked &= 0x3FFFFFFFUL;
    uint16_t remainingEv = Game::EV_TOTAL_MAX;
    auto sanitizeEv = [&](uint8_t& value) {
        if (value > Game::EV_MAX) value = Game::EV_MAX;
        if (value > remainingEv) value = static_cast<uint8_t>(remainingEv);
        remainingEv -= value;
    };
    sanitizeEv(mon.ev.hp);
    sanitizeEv(mon.ev.atk);
    sanitizeEv(mon.ev.def);
    sanitizeEv(mon.ev.spa);
    sanitizeEv(mon.ev.spd);
    sanitizeEv(mon.ev.spe);

    if (mon.nature >= Game::NATURE_COUNT) mon.nature = 0;
    if (mon.mood > 100) mon.mood = 100;
    if (mon.satiety > 100) mon.satiety = 100;
    if (mon.proficiency > 100) mon.proficiency = 100;
    mon.statusBits &= Game::STATUS_POISON | Game::STATUS_PARALYSIS | Game::STATUS_SLEEP |
                      Game::STATUS_BURN | Game::STATUS_FREEZE | Game::STATUS_CONFUSION;
    if (mon.petCountToday > 4) mon.petCountToday = 4;
    uint8_t origin = static_cast<uint8_t>(mon.origin);
    if (origin > static_cast<uint8_t>(Game::Origin::GIFT) && mon.origin != Game::Origin::UNKNOWN) {
        mon.origin = Game::Origin::UNKNOWN;
    }

    mon.hpMax = maxHpFor(*species, mon);
    if (mon.hpCur > mon.hpMax) mon.hpCur = mon.hpMax;
    if (mon.hpCur == 0 && !mon.fainted) {
        mon.fainted = true;
        mon.lastSeenAt = 0;
    }
    if (mon.fainted) mon.hpCur = 0;
    return memcmp(&before, &mon, sizeof(mon)) != 0;
}

bool sanitizeState(Game::GameState& state) {
    bool changed = false;
    if (state.hatchSeconds == 0 || state.hatchSeconds > 3600) {
        state.hatchSeconds = 180;
        changed = true;
    }
    if (state.activeSlot != 0) {
        state.activeSlot = 0;
        changed = true;
    }
    if (state.oobeDone && state.teamCount == 0) {
        state.teamCount = 1;
        state.team[0] = Game::MonsterRuntime{};
        changed = true;
    }
    if (state.teamCount > Game::TEAM_CAP) {
        state.teamCount = Game::TEAM_CAP;
        changed = true;
    }
    if (state.storageCount > Game::STORAGE_CAP) {
        state.storageCount = Game::STORAGE_CAP;
        changed = true;
    }
    for (uint8_t i = 0; i < state.teamCount; ++i) changed |= sanitizeMonster(state.team[i]);
    for (uint8_t i = 0; i < state.storageCount; ++i) changed |= sanitizeMonster(state.storage[i]);

    auto clampU8 = [&](uint8_t& value, uint8_t maximum) {
        if (value > maximum) {
            value = maximum;
            changed = true;
        }
    };
    clampU8(state.bag.pokeBall, 99);
    clampU8(state.bag.greatBall, 50);
    clampU8(state.bag.heavyBall, 50);
    clampU8(state.bag.timerBall, 50);
    clampU8(state.bag.potion, 30);
    clampU8(state.bag.superPotion, 20);
    clampU8(state.bag.antidote, 30);
    clampU8(state.bag.candy, 30);
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        clampU8(state.room.food[i], 30);
    }
    if (state.room.selectedFood >= Game::ROOM_FOOD_COUNT) {
        state.room.selectedFood = 0;
        changed = true;
    }
    if (state.room.bowlFood >= Game::ROOM_FOOD_COUNT) {
        state.room.bowlFood = 0;
        changed = true;
    }
    if (state.room.bowlCount > Game::ROOM_BOWL_CAPACITY) {
        state.room.bowlCount = Game::ROOM_BOWL_CAPACITY;
        changed = true;
    }
    if (state.room.bowlCount == 0 && state.room.bowlFood != 0) {
        state.room.bowlFood = 0;
        changed = true;
    }

    if (state.coins > 99999) {
        state.coins = 99999;
        changed = true;
    }
    if (state.stepsToday > 60000) {
        state.stepsToday = 60000;
        changed = true;
    }
    uint16_t maxWalkExp = min<uint16_t>(50, state.stepsToday / 100);
    if (state.walkExpToday > maxWalkExp) {
        state.walkExpToday = maxWalkExp;
        changed = true;
    }
    if (state.careExpToday > 60) {
        state.careExpToday = 60;
        changed = true;
    }
    if (state.settings.brightness < 16) {
        state.settings.brightness = 16;
        changed = true;
    }
    if (state.settings.speedIndex >= 4) {
        state.settings.speedIndex = 0;
        changed = true;
    }
    if (state.settings.longPressMs < 250 || state.settings.longPressMs > 2000) {
        state.settings.longPressMs = 500;
        changed = true;
    }
    if (state.settings.doubleClickMs < 100 || state.settings.doubleClickMs > 1000) {
        state.settings.doubleClickMs = 300;
        changed = true;
    }
    if (state.settings.volume > 100) {
        state.settings.volume = 100;
        changed = true;
    }
    if (state.settings.idleTimeoutIndex >= 5) {
        state.settings.idleTimeoutIndex = 0;
        changed = true;
    }
    if (state.settings.language != 0) {
        state.settings.language = 0;
        changed = true;
    }
    return changed;
}

void resetLegacyFaintTimestamps(Game::GameState& state) {
    uint32_t nowGameSeconds = gameSecondsForMinutes(state.gameMinutesTotal);
    for (uint8_t i = 0; i < state.teamCount; ++i) {
        if (state.team[i].fainted) state.team[i].lastSeenAt = nowGameSeconds;
    }
    for (uint8_t i = 0; i < state.storageCount; ++i) {
        if (state.storage[i].fainted) state.storage[i].lastSeenAt = nowGameSeconds;
    }
}

void convertLegacyRoom(const LegacyRoomStateV12& oldRoom, Game::RoomState& room) {
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) room.food[i] = oldRoom.food[i];
    room.selectedFood = oldRoom.selectedFood;
    room.bowlFood = 0;
    room.bowlCount = 0;
    room.roomStyle = oldRoom.roomStyle;
    room.activeToy = oldRoom.activeToy;
    room.ownedToys = oldRoom.ownedToys;
    room.ownedFurniture = oldRoom.ownedFurniture;
    room.placedFurniture = oldRoom.placedFurniture;
}

void convertLegacyMonster(const LegacyMonsterRuntimeV8& oldMon, Game::MonsterRuntime& mon) {
    mon.speciesId = oldMon.speciesId;
    mon.level = oldMon.level;
    mon.exp = oldMon.exp;
    mon.hpCur = oldMon.hpCur;
    mon.hpMax = oldMon.hpMax;
    mon.ivPacked = oldMon.ivPacked;
    mon.ev = oldMon.ev;
    mon.nature = oldMon.nature;
    mon.affection = oldMon.affection;
    mon.mood = oldMon.mood;
    mon.satiety = oldMon.satiety;
    mon.proficiency = oldMon.proficiency;
    mon.statusBits = oldMon.statusBits;
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;

    const Species* species = findSpecies(mon.speciesId);
    if (!species) species = &starterSpecies();
    mon.move1Id = basicMoveIdForSpecies(*species);
    uint8_t secondLevel = secondMoveLearnLevelForSpecies(*species);
    mon.move2Id = (secondLevel > 0 && mon.level >= secondLevel) ? secondMoveIdForSpecies(*species) : 0;
    mon.move3Id = 0;
}

void convertLegacyMonster(const LegacyMonsterRuntimeV10& oldMon, Game::MonsterRuntime& mon) {
    mon.speciesId = oldMon.speciesId;
    mon.level = oldMon.level;
    mon.exp = oldMon.exp;
    mon.hpCur = oldMon.hpCur;
    mon.hpMax = oldMon.hpMax;
    mon.move1Id = oldMon.move1Id;
    mon.move2Id = oldMon.move2Id;
    mon.move3Id = 0;
    mon.ivPacked = oldMon.ivPacked;
    mon.ev = oldMon.ev;
    mon.nature = oldMon.nature;
    mon.affection = oldMon.affection;
    mon.mood = oldMon.mood;
    mon.satiety = oldMon.satiety;
    mon.proficiency = oldMon.proficiency;
    mon.statusBits = oldMon.statusBits;
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;
}

bool migrateLegacyV8(const LegacyGameStateV8& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V8 ||
        legacy.checksum != checksumObject(legacy)) {
        return false;
    }

    Game::GameState next;
    next.magic = Game::SAVE_MAGIC;
    next.version = Game::SAVE_VERSION;
    next.checksum = 0;
    next.oobeDone = legacy.oobeDone;
    next.hatchSeconds = legacy.hatchSeconds;
    next.activeSlot = 0;
    next.teamCount = legacy.teamCount > Game::TEAM_CAP ? Game::TEAM_CAP : legacy.teamCount;
    next.storageCount = legacy.storageCount > Game::STORAGE_CAP ? Game::STORAGE_CAP : legacy.storageCount;
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) convertLegacyMonster(legacy.team[i], next.team[i]);
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) convertLegacyMonster(legacy.storage[i], next.storage[i]);
    next.bag = legacy.bag;
    convertLegacyRoom(legacy.room, next.room);
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    uint32_t day = next.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    next.careDay = (uint16_t)(day > 0xFFFF ? 0xFFFF : day);
    next.careExpToday = 0;
    next.settings = legacy.settings;
    sanitizeState(next);
    resetLegacyFaintTimestamps(next);
    state = next;
    return true;
}

bool migrateLegacyV9(const LegacyGameStateV9& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V9 ||
        legacy.checksum != checksumObject(legacy)) {
        return false;
    }

    Game::GameState next;
    next.magic = Game::SAVE_MAGIC;
    next.version = Game::SAVE_VERSION;
    next.checksum = 0;
    next.oobeDone = legacy.oobeDone;
    next.hatchSeconds = legacy.hatchSeconds;
    next.activeSlot = 0;
    next.teamCount = legacy.teamCount > Game::TEAM_CAP ? Game::TEAM_CAP : legacy.teamCount;
    next.storageCount = legacy.storageCount > Game::STORAGE_CAP ? Game::STORAGE_CAP : legacy.storageCount;
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) convertLegacyMonster(legacy.team[i], next.team[i]);
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) convertLegacyMonster(legacy.storage[i], next.storage[i]);
    next.bag = legacy.bag;
    convertLegacyRoom(legacy.room, next.room);
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    uint32_t day = next.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    next.careDay = (uint16_t)(day > 0xFFFF ? 0xFFFF : day);
    next.careExpToday = 0;
    next.settings = legacy.settings;
    sanitizeState(next);
    resetLegacyFaintTimestamps(next);
    state = next;
    return true;
}

bool migrateLegacyV10(const LegacyGameStateV10& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V10 ||
        legacy.checksum != checksumObject(legacy)) {
        return false;
    }

    Game::GameState next;
    next.magic = Game::SAVE_MAGIC;
    next.version = Game::SAVE_VERSION;
    next.checksum = 0;
    next.oobeDone = legacy.oobeDone;
    next.hatchSeconds = legacy.hatchSeconds;
    next.activeSlot = 0;
    next.teamCount = legacy.teamCount > Game::TEAM_CAP ? Game::TEAM_CAP : legacy.teamCount;
    next.storageCount = legacy.storageCount > Game::STORAGE_CAP ? Game::STORAGE_CAP : legacy.storageCount;
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) convertLegacyMonster(legacy.team[i], next.team[i]);
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) convertLegacyMonster(legacy.storage[i], next.storage[i]);
    next.bag = legacy.bag;
    convertLegacyRoom(legacy.room, next.room);
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.settings = legacy.settings;
    sanitizeState(next);
    resetLegacyFaintTimestamps(next);
    state = next;
    return true;
}

bool migrateLegacyV12(const LegacyGameStateV12& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        (legacy.version != LEGACY_SAVE_VERSION_V11 && legacy.version != LEGACY_SAVE_VERSION_V12) ||
        legacy.checksum != checksumObject(legacy)) {
        return false;
    }

    Game::GameState next;
    next.magic = Game::SAVE_MAGIC;
    next.version = Game::SAVE_VERSION;
    next.checksum = 0;
    next.oobeDone = legacy.oobeDone;
    next.hatchSeconds = legacy.hatchSeconds;
    next.activeSlot = legacy.activeSlot;
    next.teamCount = legacy.teamCount;
    next.storageCount = legacy.storageCount;
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) next.team[i] = legacy.team[i];
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) next.storage[i] = legacy.storage[i];
    next.bag = legacy.bag;
    convertLegacyRoom(legacy.room, next.room);
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.settings = legacy.settings;
    sanitizeState(next);
    if (legacy.version == LEGACY_SAVE_VERSION_V11) resetLegacyFaintTimestamps(next);
    state = next;
    return true;
}
}

bool SaveManager::begin() {
    Preferences prefs;
    bool ok = prefs.begin(NVS_NS, false);
    prefs.end();
    return ok;
}

bool SaveManager::load(Game::GameState& state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    size_t len = prefs.getBytesLength(NVS_KEY);
    if (len != sizeof(Game::GameState)) {
        prefs.end();
        Serial.printf("[SaveManager] unsupported state size=%u expected=%u; reset to v13\n",
                      (unsigned)len,
                      (unsigned)sizeof(Game::GameState));
        reset(state);
        return false;
    }

    size_t read = prefs.getBytes(NVS_KEY, &state, sizeof(state));
    prefs.end();
    uint16_t expectedChecksum = checksum(state);
    if (read != sizeof(state) ||
        state.magic != Game::SAVE_MAGIC ||
        state.version != Game::SAVE_VERSION ||
        state.checksum != expectedChecksum) {
        Serial.printf("[SaveManager] unsupported/invalid state read=%u magic=%08lx version=%u checksum=%04x/%04x; reset to v13\n",
                      (unsigned)read,
                      (unsigned long)state.magic,
                      state.version,
                      state.checksum,
                      expectedChecksum);
        reset(state);
        return false;
    }

    if (sanitizeState(state)) {
        Serial.println("[SaveManager] normalized state fields");
        save(state);
    }
    return true;
}

bool SaveManager::save(const Game::GameState& state) {
    auto* copy = static_cast<Game::GameState*>(std::malloc(sizeof(Game::GameState)));
    if (!copy) {
        Serial.println("[SaveManager] state save allocation failed");
        return false;
    }
    memcpy(copy, &state, sizeof(*copy));
    copy->magic = Game::SAVE_MAGIC;
    copy->version = Game::SAVE_VERSION;
    copy->checksum = checksum(*copy);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        std::free(copy);
        return false;
    }
    size_t written = prefs.putBytes(NVS_KEY, copy, sizeof(*copy));
    prefs.end();
    std::free(copy);
    return written == sizeof(Game::GameState);
}

void SaveManager::reset(Game::GameState& state) {
    resetGameState(state);
}

bool SaveManager::clearAll() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    bool cleared = prefs.clear();
    prefs.end();
    return cleared;
}

bool SaveManager::loadClock(uint32_t& gameMinutesTotal) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    bool exists = prefs.isKey(CLOCK_KEY);
    if (exists) gameMinutesTotal = prefs.getUInt(CLOCK_KEY, 0);
    prefs.end();
    return exists;
}

bool SaveManager::saveClock(uint32_t gameMinutesTotal) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putUInt(CLOCK_KEY, gameMinutesTotal);
    prefs.end();
    return written == sizeof(gameMinutesTotal);
}

bool SaveManager::loadHatchProgress(Game::HatchProgress& progress) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    size_t len = prefs.getBytesLength(HATCH_KEY);
    if (len != sizeof(Game::HatchProgress)) {
        prefs.end();
        progress = Game::HatchProgress{};
        return false;
    }

    Game::HatchProgress loaded;
    size_t read = prefs.getBytes(HATCH_KEY, &loaded, sizeof(loaded));
    prefs.end();
    if (read != sizeof(loaded) ||
        loaded.magic != Game::HATCH_MAGIC ||
        loaded.checksum != checksum(loaded)) {
        progress = Game::HatchProgress{};
        return false;
    }

    progress = loaded;
    return true;
}

bool SaveManager::saveHatchProgress(const Game::HatchProgress& progress) {
    Game::HatchProgress copy = progress;
    copy.magic = Game::HATCH_MAGIC;
    copy.checksum = checksum(copy);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putBytes(HATCH_KEY, &copy, sizeof(copy));
    prefs.end();
    return written == sizeof(copy);
}

void SaveManager::clearHatchProgress() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return;
    prefs.remove(HATCH_KEY);
    prefs.end();
}

uint16_t SaveManager::checksum(const Game::GameState& state) {
    return checksumObject(state);
}

uint16_t SaveManager::checksum(const Game::HatchProgress& progress) {
    return checksumObject(progress);
}
