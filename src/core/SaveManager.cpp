#include "core/SaveManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include "game/Species.h"

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* CLOCK_KEY = "clock_min";
constexpr uint16_t LEGACY_SAVE_VERSION_V8 = 8;
constexpr uint16_t LEGACY_SAVE_VERSION_V9 = 9;
constexpr uint16_t GAME_MINUTES_PER_DAY = 24U * 60U;

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
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV9 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V9;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    Game::MonsterRuntime team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    Game::MonsterRuntime storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

template <typename T>
uint16_t checksumObject(T object) {
    object.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&object);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(object); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
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
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    uint32_t day = next.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    next.careDay = (uint16_t)(day > 0xFFFF ? 0xFFFF : day);
    next.careExpToday = 0;
    next.settings = legacy.settings;
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) next.team[i] = legacy.team[i];
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) next.storage[i] = legacy.storage[i];
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    uint32_t day = next.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    next.careDay = (uint16_t)(day > 0xFFFF ? 0xFFFF : day);
    next.careExpToday = 0;
    next.settings = legacy.settings;
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
        if (len == sizeof(LegacyGameStateV9)) {
            LegacyGameStateV9 legacy;
            size_t read = prefs.getBytes(NVS_KEY, &legacy, sizeof(legacy));
            prefs.end();
            if (read == sizeof(legacy) && migrateLegacyV9(legacy, state)) {
                Serial.println("[SaveManager] migrated state v9 -> v10");
                save(state);
                return true;
            }
            reset(state);
            return false;
        }
        if (len == sizeof(LegacyGameStateV8)) {
            LegacyGameStateV8 legacy;
            size_t read = prefs.getBytes(NVS_KEY, &legacy, sizeof(legacy));
            prefs.end();
            if (read == sizeof(legacy) && migrateLegacyV8(legacy, state)) {
                Serial.println("[SaveManager] migrated state v8 -> v10");
                save(state);
                return true;
            }
            reset(state);
            return false;
        }
        Serial.printf("[SaveManager] state size mismatch: %u != %u\n", (unsigned)len, (unsigned)sizeof(Game::GameState));
        prefs.end();
        reset(state);
        return false;
    }

    Game::GameState loaded;
    size_t read = prefs.getBytes(NVS_KEY, &loaded, sizeof(loaded));
    prefs.end();
    if (read != sizeof(loaded) ||
        loaded.magic != Game::SAVE_MAGIC ||
        loaded.version != Game::SAVE_VERSION ||
        loaded.checksum != checksum(loaded)) {
        Serial.printf("[SaveManager] state invalid read=%u magic=%08lx version=%u checksum=%04x/%04x\n",
                      (unsigned)read,
                      (unsigned long)loaded.magic,
                      loaded.version,
                      loaded.checksum,
                      checksum(loaded));
        reset(state);
        return false;
    }

    state = loaded;
    return true;
}

bool SaveManager::save(const Game::GameState& state) {
    Game::GameState copy = state;
    copy.magic = Game::SAVE_MAGIC;
    copy.version = Game::SAVE_VERSION;
    copy.checksum = checksum(copy);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putBytes(NVS_KEY, &copy, sizeof(copy));
    prefs.end();
    return written == sizeof(copy);
}

void SaveManager::reset(Game::GameState& state) {
    state = Game::GameState{};
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
    Game::GameState copy = state;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(copy); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

uint16_t SaveManager::checksum(const Game::HatchProgress& progress) {
    Game::HatchProgress copy = progress;
    copy.checksum = 0;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&copy);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(copy); ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}
