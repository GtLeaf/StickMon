#include "core/SaveManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include "game/Species.h"

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* CLOCK_KEY = "clock_min";
constexpr const char* MAIN_SCENE_VIEW_KEY = "main_view";
constexpr uint32_t MAIN_SCENE_VIEW_MAGIC = 0x4D565354; // MVST
constexpr uint16_t MAIN_SCENE_VIEW_VERSION = 1;
constexpr uint32_t MAIN_SCENE_VIEW_MAX_REMAINING_MS = 24UL * 60UL * 60UL * 1000UL;
constexpr float MAIN_SCENE_VIEW_COORD_LIMIT = 8192.0f;
constexpr uint16_t LEGACY_SAVE_VERSION_V8 = 8;
constexpr uint16_t LEGACY_SAVE_VERSION_V9 = 9;
constexpr uint16_t LEGACY_SAVE_VERSION_V10 = 10;
constexpr uint16_t LEGACY_SAVE_VERSION_V11 = 11;
constexpr uint16_t LEGACY_SAVE_VERSION_V12 = 12;
constexpr uint16_t LEGACY_SAVE_VERSION_V13 = 13;
constexpr uint16_t LEGACY_SAVE_VERSION_V14 = 14;
constexpr uint16_t LEGACY_SAVE_VERSION_V15 = 15;
constexpr uint16_t LEGACY_SAVE_VERSION_V16 = 16;
constexpr uint16_t LEGACY_SAVE_VERSION_V17 = 17;
constexpr uint16_t LEGACY_SAVE_VERSION_V18 = 18;
constexpr uint16_t GAME_MINUTES_PER_DAY = 24U * 60U;

struct MainSceneViewRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t checksum;
    uint16_t speciesId;
    uint8_t valid;
    uint8_t aiMode;
    uint8_t pmdAction;
    uint8_t pmdDirection;
    uint8_t pmdFrame;
    uint8_t facingRight;
    uint8_t faintRestActive;
    uint8_t reserved;
    float monsterX;
    float monsterY;
    float targetX;
    float targetY;
    uint32_t nextDecisionRemainingMs;
    uint32_t postFeedAwakeRemainingMs;
};

struct LegacyRoomStateV12 {
    uint8_t food[Game::ROOM_FOOD_COUNT] = {2, 0};
    uint8_t selectedFood = 0;
    uint8_t roomStyle = 0;
    uint8_t activeToy = 0;
    uint8_t ownedToys = 0;
    uint16_t ownedFurniture = 0;
    uint16_t placedFurniture = 0;
};

struct LegacyRoomStateV13 {
    uint8_t food[Game::ROOM_FOOD_COUNT] = {2, 0};
    uint8_t selectedFood = 0;
    uint8_t bowlFood = 0;
    uint8_t bowlCount = 0;
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

struct LegacyMonsterRuntimeV15 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    uint8_t move1Id = 0;
    uint8_t move2Id = 0;
    uint8_t move3Id = 0;
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

struct LegacyMonsterRuntimeV16 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    Game::MoveId move1Id = 0;
    Game::MoveId move2Id = 0;
    Game::MoveId move3Id = 0;
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

struct LegacyMonsterRuntimeV17 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    Game::MoveId move1Id = 0;
    Game::MoveId move2Id = 0;
    Game::MoveId move3Id = 0;
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
    uint32_t lastExploredAt = 0;
    uint32_t lastWindowGazeAt = 0;
};

struct LegacyMonsterRuntimeV18 {
    uint16_t speciesId = 1;
    uint8_t level = 5;
    uint32_t exp = 0;
    uint16_t hpCur = 20;
    uint16_t hpMax = 20;
    Game::MoveId move1Id = 0;
    Game::MoveId move2Id = 0;
    Game::MoveId move3Id = 0;
    uint32_t ivPacked = 0;
    Game::StatLine ev;
    uint8_t nature = 0;
    uint8_t affection = 30;
    uint8_t mood = 70;
    uint8_t satiety = 75;
    uint8_t moveProficiency[Game::MOVE_SLOT_COUNT] = {};
    uint8_t statusBits = Game::STATUS_NONE;
    uint8_t metArea = Game::MET_AREA_UNKNOWN;
    uint8_t petCountToday = 0;
    Game::Origin origin = Game::Origin::STARTER;
    bool fainted = false;
    uint32_t caughtAt = 0;
    uint32_t lastSeenAt = 0;
    uint32_t lastPettedAt = 0;
    uint32_t lastExploredAt = 0;
    uint32_t lastWindowGazeAt = 0;
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
    LegacyMonsterRuntimeV15 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV15 storage[Game::STORAGE_CAP];
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

struct LegacyGameStateV13 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V13;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV15 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV15 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    LegacyRoomStateV13 room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV14 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V14;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV15 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV15 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV15 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V15;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV15 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV15 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    uint8_t pendingMoveId = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV16 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V16;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV16 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV16 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    Game::MoveId pendingMoveId = 0;
    uint16_t pendingMoveCursor = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV17 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V17;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV17 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV17 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    Game::MoveId pendingMoveId = 0;
    uint16_t pendingMoveCursor = 0;
    Game::PlayerSettings settings;
};

struct LegacyGameStateV18 {
    uint32_t magic = Game::SAVE_MAGIC;
    uint16_t version = LEGACY_SAVE_VERSION_V18;
    uint16_t checksum = 0;
    bool oobeDone = false;
    uint16_t hatchSeconds = 180;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 1;
    LegacyMonsterRuntimeV18 team[Game::TEAM_CAP];
    uint8_t storageCount = 0;
    LegacyMonsterRuntimeV18 storage[Game::STORAGE_CAP];
    Game::BagState bag;
    Game::RoomState room;
    uint32_t coins = 50;
    uint16_t stepsToday = 0;
    uint16_t walkExpToday = 0;
    uint16_t careExpToday = 0;
    uint16_t careDay = 0;
    uint32_t gameMinutesTotal = 0;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    Game::MoveId pendingMoveId = 0;
    uint16_t pendingMoveCursor = 0;
    Game::PlayerSettings settings;
};

static_assert(sizeof(LegacyGameStateV13) != sizeof(Game::GameState),
              "v13 and current save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV14) != sizeof(Game::GameState),
              "v14 and current save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV15) != sizeof(Game::GameState),
              "v15 and current save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV16) != sizeof(Game::GameState),
              "v16 and current save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV17) != sizeof(Game::GameState),
              "v17 and current save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV18) != sizeof(LegacyGameStateV17),
              "v17 and v18 save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV17) != sizeof(LegacyGameStateV16),
              "v16 and v17 save layouts must be distinguishable");
static_assert(sizeof(LegacyGameStateV15) != sizeof(LegacyGameStateV14),
              "v14 and v15 save layouts must be distinguishable");

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

bool mainSceneCoordinateValid(float value) {
    return std::isfinite(value) && std::fabs(value) <= MAIN_SCENE_VIEW_COORD_LIMIT;
}

bool mainSceneViewValuesValid(const MainSceneViewState& viewState) {
    if (!viewState.valid) return true;
    return findSpecies(viewState.speciesId) != nullptr &&
           mainSceneCoordinateValid(viewState.monsterX) &&
           mainSceneCoordinateValid(viewState.monsterY) &&
           mainSceneCoordinateValid(viewState.targetX) &&
           mainSceneCoordinateValid(viewState.targetY);
}

bool mainSceneViewRecordValid(const MainSceneViewRecord& record) {
    if (record.magic != MAIN_SCENE_VIEW_MAGIC ||
        record.version != MAIN_SCENE_VIEW_VERSION ||
        record.checksum != checksumObject(record) ||
        record.valid > 1 || record.facingRight > 1 || record.faintRestActive > 1) {
        return false;
    }
    if (!record.valid) return true;
    return findSpecies(record.speciesId) != nullptr &&
           mainSceneCoordinateValid(record.monsterX) &&
           mainSceneCoordinateValid(record.monsterY) &&
           mainSceneCoordinateValid(record.targetX) &&
           mainSceneCoordinateValid(record.targetY);
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

    if (!isBasicFirstMoveForSpecies(*species, mon.move1Id)) {
        mon.move1Id = basicMoveIdForSpecies(*species);
    }
    uint8_t move2Level = moveLearnLevelForSpecies(*species, mon.move2Id);
    if (mon.move2Id != 0 &&
        (!canLearnAsSpecialMove(*species, mon.move2Id) || mon.level < move2Level)) {
        mon.move2Id = 0;
    }
    uint8_t move3Level = moveLearnLevelForSpecies(*species, mon.move3Id);
    if (mon.move3Id != 0 &&
        (!canLearnAsSpecialMove(*species, mon.move3Id) || mon.level < move3Level ||
         mon.move3Id == mon.move2Id)) {
        mon.move3Id = 0;
    }

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
    const Game::MoveId moves[Game::MOVE_SLOT_COUNT] = {
        mon.move1Id,
        mon.move2Id,
        mon.move3Id,
    };
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        if (slot == 0) mon.moveProficiency[slot] = Game::MOVE_PROFICIENCY_MAX;
        else if (moves[slot] == 0) mon.moveProficiency[slot] = 0;
        else if (mon.moveProficiency[slot] > Game::MOVE_PROFICIENCY_MAX) {
            mon.moveProficiency[slot] = Game::MOVE_PROFICIENCY_MAX;
        }
    }
    if (static_cast<uint8_t>(mon.majorStatus) >
        static_cast<uint8_t>(Game::MajorStatus::FREEZE)) {
        mon.majorStatus = Game::MajorStatus::NONE;
    }
    if (mon.majorStatus == Game::MajorStatus::SLEEP) {
        if (mon.majorStatusTurns == 0 || mon.majorStatusTurns > 7) mon.majorStatusTurns = 2;
    } else {
        mon.majorStatusTurns = 0;
    }
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
    clampU8(state.bag.pokeBall, Game::ITEM_STACK_CAP);
    clampU8(state.bag.greatBall, Game::ITEM_STACK_CAP);
    clampU8(state.bag.heavyBall, Game::ITEM_STACK_CAP);
    clampU8(state.bag.timerBall, Game::ITEM_STACK_CAP);
    clampU8(state.bag.potion, Game::ITEM_STACK_CAP);
    clampU8(state.bag.superPotion, Game::ITEM_STACK_CAP);
    clampU8(state.bag.antidote, Game::ITEM_STACK_CAP);
    clampU8(state.bag.candy, Game::ITEM_STACK_CAP);
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        clampU8(state.room.food[i], Game::ITEM_STACK_CAP);
    }
    if (state.room.selectedFood >= Game::ROOM_FOOD_COUNT) {
        state.room.selectedFood = 0;
        changed = true;
    }
    if (state.room.bowlFood >= Game::ROOM_FOOD_COUNT) {
        state.room.bowlFood = 0;
        state.room.bowlBitesRemaining = 0;
        changed = true;
    }
    if (state.room.bowlCount > Game::ROOM_BOWL_CAPACITY) {
        uint8_t extraServings = state.room.bowlCount - Game::ROOM_BOWL_CAPACITY;
        uint8_t foodIndex = state.room.bowlFood;
        state.room.food[foodIndex] = static_cast<uint8_t>(min<uint16_t>(
            Game::ITEM_STACK_CAP,
            static_cast<uint16_t>(state.room.food[foodIndex]) + extraServings));
        state.room.bowlCount = Game::ROOM_BOWL_CAPACITY;
        changed = true;
    }
    if (state.room.bowlCount == 0) {
        if (state.room.bowlFood != 0) {
            state.room.bowlFood = 0;
            changed = true;
        }
        if (state.room.bowlBitesRemaining != 0) {
            state.room.bowlBitesRemaining = 0;
            changed = true;
        }
    } else {
        uint8_t maxBites = Game::roomFoodBitesPerServing(state.room.bowlFood);
        if (state.room.bowlBitesRemaining == 0 ||
            state.room.bowlBitesRemaining > maxBites) {
            state.room.bowlBitesRemaining = maxBites;
            changed = true;
        }
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
    if (!state.pendingLevelUp) {
        if (state.pendingLevelUpLevel != 0) {
            state.pendingLevelUpLevel = 0;
            changed = true;
        }
    } else if (state.teamCount == 0 ||
               state.pendingLevelUpLevel == 0 ||
               state.pendingLevelUpLevel > state.team[0].level) {
        state.pendingLevelUp = false;
        state.pendingLevelUpLevel = 0;
        changed = true;
    }

    bool pendingMoveValid = false;
    if (state.pendingMoveLearn &&
        state.pendingMoveSlot < state.teamCount &&
        state.pendingMoveSlot < Game::TEAM_CAP) {
        const Game::MonsterRuntime& mon = state.team[state.pendingMoveSlot];
        const Species* species = findSpecies(mon.speciesId);
        uint8_t learnLevel = species
            ? moveLearnLevelForSpecies(*species, state.pendingMoveId)
            : 0;
        const LearnsetEntry* queuedEntry = species && state.pendingMoveCursor > 0
            ? learnsetEntryForSpecies(*species, state.pendingMoveCursor - 1)
            : nullptr;
        pendingMoveValid = state.pendingMoveId != 0 &&
                           species && canLearnAsSpecialMove(*species, state.pendingMoveId) &&
                           learnLevel > 0 && mon.level >= learnLevel &&
                           queuedEntry && queuedEntry->moveId == state.pendingMoveId &&
                           mon.move2Id != state.pendingMoveId &&
                           mon.move3Id != state.pendingMoveId;
    }
    if (!pendingMoveValid &&
        (state.pendingMoveLearn || state.pendingMoveSlot != 0 || state.pendingMoveId != 0 ||
         state.pendingMoveCursor != 0)) {
        state.pendingMoveLearn = false;
        state.pendingMoveSlot = 0;
        state.pendingMoveId = 0;
        state.pendingMoveCursor = 0;
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

void resetLegacyExploreTimestamps(Game::GameState& state) {
    uint32_t nowGameSeconds = gameSecondsForMinutes(state.gameMinutesTotal);
    for (uint8_t i = 0; i < state.teamCount; ++i) {
        state.team[i].lastExploredAt = nowGameSeconds;
        state.team[i].lastWindowGazeAt = nowGameSeconds;
    }
    for (uint8_t i = 0; i < state.storageCount; ++i) {
        state.storage[i].lastExploredAt = nowGameSeconds;
        state.storage[i].lastWindowGazeAt = nowGameSeconds;
    }
}

void convertLegacyRoom(const LegacyRoomStateV12& oldRoom, Game::RoomState& room) {
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) room.food[i] = oldRoom.food[i];
    room.selectedFood = oldRoom.selectedFood;
    room.bowlFood = 0;
    room.bowlCount = 0;
    room.bowlBitesRemaining = 0;
    room.roomStyle = oldRoom.roomStyle;
    room.activeToy = oldRoom.activeToy;
    room.ownedToys = oldRoom.ownedToys;
    room.ownedFurniture = oldRoom.ownedFurniture;
    room.placedFurniture = oldRoom.placedFurniture;
}

void convertLegacyRoom(const LegacyRoomStateV13& oldRoom, Game::RoomState& room) {
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) room.food[i] = oldRoom.food[i];
    room.selectedFood = oldRoom.selectedFood;
    room.bowlFood = oldRoom.bowlFood;
    room.bowlCount = oldRoom.bowlCount;
    room.bowlBitesRemaining = oldRoom.bowlCount > 0
        ? Game::roomFoodBitesPerServing(oldRoom.bowlFood)
        : 0;
    room.roomStyle = oldRoom.roomStyle;
    room.activeToy = oldRoom.activeToy;
    room.ownedToys = oldRoom.ownedToys;
    room.ownedFurniture = oldRoom.ownedFurniture;
    room.placedFurniture = oldRoom.placedFurniture;
}

void importSharedProficiency(Game::MonsterRuntime& mon, uint8_t proficiency) {
    uint8_t value = min<uint8_t>(proficiency, Game::MOVE_PROFICIENCY_MAX);
    const Game::MoveId moves[Game::MOVE_SLOT_COUNT] = {
        mon.move1Id,
        mon.move2Id,
        mon.move3Id,
    };
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        mon.moveProficiency[slot] = slot == 0
            ? Game::MOVE_PROFICIENCY_MAX
            : (moves[slot] == 0 ? 0 : value);
    }
}

void importLegacyStatus(Game::MonsterRuntime& mon, uint8_t statusBits) {
    mon.majorStatusTurns = 0;
    if (statusBits & Game::STATUS_POISON) {
        mon.majorStatus = Game::MajorStatus::POISON;
    } else if (statusBits & Game::STATUS_PARALYSIS) {
        mon.majorStatus = Game::MajorStatus::PARALYSIS;
    } else if (statusBits & Game::STATUS_SLEEP) {
        mon.majorStatus = Game::MajorStatus::SLEEP;
        mon.majorStatusTurns = 2;
    } else if (statusBits & Game::STATUS_BURN) {
        mon.majorStatus = Game::MajorStatus::BURN;
    } else if (statusBits & Game::STATUS_FREEZE) {
        mon.majorStatus = Game::MajorStatus::FREEZE;
    } else {
        mon.majorStatus = Game::MajorStatus::NONE;
    }
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
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;

    const Species* species = findSpecies(mon.speciesId);
    if (!species) species = &starterSpecies();
    resetMovesForLevel(mon, *species);
    importSharedProficiency(mon, oldMon.proficiency);
}

void convertLegacyMonster(const LegacyMonsterRuntimeV10& oldMon, Game::MonsterRuntime& mon) {
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
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;

    const Species* species = findSpecies(mon.speciesId);
    if (!species) species = &starterSpecies();
    resetMovesForLevel(mon, *species);
    importSharedProficiency(mon, oldMon.proficiency);
}

void convertLegacyMonster(const LegacyMonsterRuntimeV15& oldMon, Game::MonsterRuntime& mon) {
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
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;

    const Species* species = findSpecies(mon.speciesId);
    if (!species) species = &starterSpecies();
    resetMovesForLevel(mon, *species);
    importSharedProficiency(mon, oldMon.proficiency);
}

void convertLegacyMonster(const LegacyMonsterRuntimeV16& oldMon, Game::MonsterRuntime& mon) {
    mon.speciesId = oldMon.speciesId;
    mon.level = oldMon.level;
    mon.exp = oldMon.exp;
    mon.hpCur = oldMon.hpCur;
    mon.hpMax = oldMon.hpMax;
    mon.move1Id = oldMon.move1Id;
    mon.move2Id = oldMon.move2Id;
    mon.move3Id = oldMon.move3Id;
    mon.ivPacked = oldMon.ivPacked;
    mon.ev = oldMon.ev;
    mon.nature = oldMon.nature;
    mon.affection = oldMon.affection;
    mon.mood = oldMon.mood;
    mon.satiety = oldMon.satiety;
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;
    importSharedProficiency(mon, oldMon.proficiency);
}

void convertLegacyMonster(const LegacyMonsterRuntimeV17& oldMon, Game::MonsterRuntime& mon) {
    mon.speciesId = oldMon.speciesId;
    mon.level = oldMon.level;
    mon.exp = oldMon.exp;
    mon.hpCur = oldMon.hpCur;
    mon.hpMax = oldMon.hpMax;
    mon.move1Id = oldMon.move1Id;
    mon.move2Id = oldMon.move2Id;
    mon.move3Id = oldMon.move3Id;
    mon.ivPacked = oldMon.ivPacked;
    mon.ev = oldMon.ev;
    mon.nature = oldMon.nature;
    mon.affection = oldMon.affection;
    mon.mood = oldMon.mood;
    mon.satiety = oldMon.satiety;
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;
    mon.lastExploredAt = oldMon.lastExploredAt;
    mon.lastWindowGazeAt = oldMon.lastWindowGazeAt;
    importSharedProficiency(mon, oldMon.proficiency);
}

void convertLegacyMonster(const LegacyMonsterRuntimeV18& oldMon, Game::MonsterRuntime& mon) {
    mon.speciesId = oldMon.speciesId;
    mon.level = oldMon.level;
    mon.exp = oldMon.exp;
    mon.hpCur = oldMon.hpCur;
    mon.hpMax = oldMon.hpMax;
    mon.move1Id = oldMon.move1Id;
    mon.move2Id = oldMon.move2Id;
    mon.move3Id = oldMon.move3Id;
    mon.ivPacked = oldMon.ivPacked;
    mon.ev = oldMon.ev;
    mon.nature = oldMon.nature;
    mon.affection = oldMon.affection;
    mon.mood = oldMon.mood;
    mon.satiety = oldMon.satiety;
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        mon.moveProficiency[slot] = oldMon.moveProficiency[slot];
    }
    importLegacyStatus(mon, oldMon.statusBits);
    mon.metArea = oldMon.metArea;
    mon.petCountToday = oldMon.petCountToday;
    mon.origin = oldMon.origin;
    mon.fainted = oldMon.fainted;
    mon.caughtAt = oldMon.caughtAt;
    mon.lastSeenAt = oldMon.lastSeenAt;
    mon.lastPettedAt = oldMon.lastPettedAt;
    mon.lastExploredAt = oldMon.lastExploredAt;
    mon.lastWindowGazeAt = oldMon.lastWindowGazeAt;
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
    resetLegacyExploreTimestamps(next);
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
    resetLegacyExploreTimestamps(next);
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
    resetLegacyExploreTimestamps(next);
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
    resetLegacyExploreTimestamps(next);
    sanitizeState(next);
    if (legacy.version == LEGACY_SAVE_VERSION_V11) resetLegacyFaintTimestamps(next);
    state = next;
    return true;
}

bool migrateLegacyV13(const LegacyGameStateV13& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V13 ||
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
    resetLegacyExploreTimestamps(next);
    sanitizeState(next);
    state = next;
    return true;
}

bool migrateLegacyV14(const LegacyGameStateV14& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V14 ||
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) convertLegacyMonster(legacy.team[i], next.team[i]);
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) convertLegacyMonster(legacy.storage[i], next.storage[i]);
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.settings = legacy.settings;
    resetLegacyExploreTimestamps(next);
    sanitizeState(next);
    state = next;
    return true;
}

bool migrateLegacyV15(const LegacyGameStateV15& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V15 ||
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) {
        convertLegacyMonster(legacy.team[i], next.team[i]);
    }
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) {
        convertLegacyMonster(legacy.storage[i], next.storage[i]);
    }
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.pendingLevelUp = legacy.pendingLevelUp;
    next.pendingLevelUpLevel = legacy.pendingLevelUpLevel;
    next.settings = legacy.settings;
    resetLegacyExploreTimestamps(next);
    sanitizeState(next);
    state = next;
    return true;
}

bool migrateLegacyV16(const LegacyGameStateV16& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V16 ||
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) {
        convertLegacyMonster(legacy.team[i], next.team[i]);
    }
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) {
        convertLegacyMonster(legacy.storage[i], next.storage[i]);
    }
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.pendingLevelUp = legacy.pendingLevelUp;
    next.pendingLevelUpLevel = legacy.pendingLevelUpLevel;
    next.pendingMoveLearn = legacy.pendingMoveLearn;
    next.pendingMoveSlot = legacy.pendingMoveSlot;
    next.pendingMoveId = legacy.pendingMoveId;
    next.pendingMoveCursor = legacy.pendingMoveCursor;
    next.settings = legacy.settings;
    resetLegacyExploreTimestamps(next);
    sanitizeState(next);
    state = next;
    return true;
}

bool migrateLegacyV17(const LegacyGameStateV17& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V17 ||
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) {
        convertLegacyMonster(legacy.team[i], next.team[i]);
    }
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) {
        convertLegacyMonster(legacy.storage[i], next.storage[i]);
    }
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.pendingLevelUp = legacy.pendingLevelUp;
    next.pendingLevelUpLevel = legacy.pendingLevelUpLevel;
    next.pendingMoveLearn = legacy.pendingMoveLearn;
    next.pendingMoveSlot = legacy.pendingMoveSlot;
    next.pendingMoveId = legacy.pendingMoveId;
    next.pendingMoveCursor = legacy.pendingMoveCursor;
    next.settings = legacy.settings;
    sanitizeState(next);
    state = next;
    return true;
}

bool migrateLegacyV18(const LegacyGameStateV18& legacy, Game::GameState& state) {
    if (legacy.magic != Game::SAVE_MAGIC ||
        legacy.version != LEGACY_SAVE_VERSION_V18 ||
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
    for (uint8_t i = 0; i < Game::TEAM_CAP; ++i) {
        convertLegacyMonster(legacy.team[i], next.team[i]);
    }
    for (uint8_t i = 0; i < Game::STORAGE_CAP; ++i) {
        convertLegacyMonster(legacy.storage[i], next.storage[i]);
    }
    next.bag = legacy.bag;
    next.room = legacy.room;
    next.coins = legacy.coins;
    next.stepsToday = legacy.stepsToday;
    next.walkExpToday = legacy.walkExpToday;
    next.careExpToday = legacy.careExpToday;
    next.careDay = legacy.careDay;
    next.gameMinutesTotal = legacy.gameMinutesTotal;
    next.pendingLevelUp = legacy.pendingLevelUp;
    next.pendingLevelUpLevel = legacy.pendingLevelUpLevel;
    next.pendingMoveLearn = legacy.pendingMoveLearn;
    next.pendingMoveSlot = legacy.pendingMoveSlot;
    next.pendingMoveId = legacy.pendingMoveId;
    next.pendingMoveCursor = legacy.pendingMoveCursor;
    next.settings = legacy.settings;
    sanitizeState(next);
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
    if (len == sizeof(LegacyGameStateV18)) {
        auto* legacy = static_cast<LegacyGameStateV18*>(std::malloc(sizeof(LegacyGameStateV18)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v18 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        bool isV18 = read == sizeof(*legacy) && legacy->version == LEGACY_SAVE_VERSION_V18;
        if (isV18) {
            prefs.end();
            bool migrated = migrateLegacyV18(*legacy, state);
            std::free(legacy);
            if (!migrated) {
                Serial.println("[SaveManager] invalid v18 state; reset to current version");
                reset(state);
                return false;
            }
            bool saved = save(state);
            Serial.printf("[SaveManager] migrated v18 to v%u saved=%u\n",
                          Game::SAVE_VERSION, saved ? 1 : 0);
            return true;
        }
        std::free(legacy);
    }
    if (len == sizeof(LegacyGameStateV17)) {
        auto* legacy = static_cast<LegacyGameStateV17*>(std::malloc(sizeof(LegacyGameStateV17)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v17 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        prefs.end();
        bool migrated = read == sizeof(*legacy) && migrateLegacyV17(*legacy, state);
        std::free(legacy);
        if (!migrated) {
            Serial.println("[SaveManager] invalid v17 state; reset to current version");
            reset(state);
            return false;
        }
        bool saved = save(state);
        Serial.printf("[SaveManager] migrated v17 to v%u saved=%u\n",
                      Game::SAVE_VERSION, saved ? 1 : 0);
        return true;
    }
    if (len == sizeof(LegacyGameStateV16)) {
        auto* legacy = static_cast<LegacyGameStateV16*>(std::malloc(sizeof(LegacyGameStateV16)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v16 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        prefs.end();
        bool migrated = read == sizeof(*legacy) && migrateLegacyV16(*legacy, state);
        std::free(legacy);
        if (!migrated) {
            Serial.println("[SaveManager] invalid v16 state; reset to current version");
            reset(state);
            return false;
        }
        bool saved = save(state);
        Serial.printf("[SaveManager] migrated v16 to v%u saved=%u\n",
                      Game::SAVE_VERSION, saved ? 1 : 0);
        return true;
    }
    if (len == sizeof(LegacyGameStateV15)) {
        auto* legacy = static_cast<LegacyGameStateV15*>(std::malloc(sizeof(LegacyGameStateV15)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v15 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        prefs.end();
        bool migrated = read == sizeof(*legacy) && migrateLegacyV15(*legacy, state);
        std::free(legacy);
        if (!migrated) {
            Serial.println("[SaveManager] invalid v15 state; reset to current version");
            reset(state);
            return false;
        }
        bool saved = save(state);
        Serial.printf("[SaveManager] migrated v15 to v%u saved=%u\n",
                      Game::SAVE_VERSION, saved ? 1 : 0);
        return true;
    }
    if (len == sizeof(LegacyGameStateV14)) {
        auto* legacy = static_cast<LegacyGameStateV14*>(std::malloc(sizeof(LegacyGameStateV14)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v14 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        prefs.end();
        bool migrated = read == sizeof(*legacy) && migrateLegacyV14(*legacy, state);
        std::free(legacy);
        if (!migrated) {
            Serial.println("[SaveManager] invalid v14 state; reset to current version");
            reset(state);
            return false;
        }
        bool saved = save(state);
        Serial.printf("[SaveManager] migrated v14 to v%u saved=%u\n",
                      Game::SAVE_VERSION, saved ? 1 : 0);
        return true;
    }
    if (len == sizeof(LegacyGameStateV13)) {
        auto* legacy = static_cast<LegacyGameStateV13*>(std::malloc(sizeof(LegacyGameStateV13)));
        if (!legacy) {
            prefs.end();
            Serial.println("[SaveManager] v13 migration allocation failed");
            return false;
        }
        size_t read = prefs.getBytes(NVS_KEY, legacy, sizeof(*legacy));
        prefs.end();
        bool migrated = read == sizeof(*legacy) && migrateLegacyV13(*legacy, state);
        std::free(legacy);
        if (!migrated) {
            Serial.println("[SaveManager] invalid v13 state; reset to current version");
            reset(state);
            return false;
        }
        bool saved = save(state);
        Serial.printf("[SaveManager] migrated v13 to v%u saved=%u\n",
                      Game::SAVE_VERSION, saved ? 1 : 0);
        return true;
    }
    if (len != sizeof(Game::GameState)) {
        prefs.end();
        Serial.printf("[SaveManager] unsupported state size=%u expected=%u; reset to v%u\n",
                      (unsigned)len,
                      (unsigned)sizeof(Game::GameState),
                      Game::SAVE_VERSION);
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
        Serial.printf("[SaveManager] unsupported/invalid state read=%u magic=%08lx version=%u checksum=%04x/%04x; reset to v%u\n",
                      (unsigned)read,
                      (unsigned long)state.magic,
                      state.version,
                      state.checksum,
                      expectedChecksum,
                      Game::SAVE_VERSION);
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

bool SaveManager::loadMainSceneView(MainSceneViewState& viewState) {
    viewState = MainSceneViewState{};
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) return false;
    size_t len = prefs.getBytesLength(MAIN_SCENE_VIEW_KEY);
    if (len != sizeof(MainSceneViewRecord)) {
        prefs.end();
        return false;
    }

    MainSceneViewRecord record{};
    size_t read = prefs.getBytes(MAIN_SCENE_VIEW_KEY, &record, sizeof(record));
    prefs.end();
    if (read != sizeof(record) || !mainSceneViewRecordValid(record)) {
        Serial.println("[SaveManager] invalid main scene view state");
        return false;
    }

    if (!record.valid) return true;
    viewState.valid = true;
    viewState.speciesId = record.speciesId;
    viewState.monsterX = record.monsterX;
    viewState.monsterY = record.monsterY;
    viewState.targetX = record.targetX;
    viewState.targetY = record.targetY;
    viewState.aiMode = record.aiMode;
    viewState.pmdAction = record.pmdAction;
    viewState.pmdDirection = record.pmdDirection;
    viewState.pmdFrame = record.pmdFrame;
    viewState.facingRight = record.facingRight != 0;
    viewState.faintRestActive = record.faintRestActive != 0;
    viewState.nextDecisionRemainingMs =
        record.nextDecisionRemainingMs > MAIN_SCENE_VIEW_MAX_REMAINING_MS
            ? MAIN_SCENE_VIEW_MAX_REMAINING_MS
            : record.nextDecisionRemainingMs;
    viewState.postFeedAwakeRemainingMs =
        record.postFeedAwakeRemainingMs > MAIN_SCENE_VIEW_MAX_REMAINING_MS
            ? MAIN_SCENE_VIEW_MAX_REMAINING_MS
            : record.postFeedAwakeRemainingMs;
    Serial.printf("[SaveManager] main view loaded species=%u mode=%u\n",
                  viewState.speciesId, viewState.aiMode);
    return true;
}

bool SaveManager::saveMainSceneView(const MainSceneViewState& viewState) {
    if (!mainSceneViewValuesValid(viewState)) {
        Serial.println("[SaveManager] refused invalid main scene view state");
        return false;
    }

    MainSceneViewRecord record{};
    record.magic = MAIN_SCENE_VIEW_MAGIC;
    record.version = MAIN_SCENE_VIEW_VERSION;
    record.speciesId = viewState.speciesId;
    record.valid = viewState.valid ? 1 : 0;
    record.aiMode = viewState.aiMode;
    record.pmdAction = viewState.pmdAction;
    record.pmdDirection = viewState.pmdDirection;
    record.pmdFrame = viewState.pmdFrame;
    record.facingRight = viewState.facingRight ? 1 : 0;
    record.faintRestActive = viewState.faintRestActive ? 1 : 0;
    record.monsterX = viewState.monsterX;
    record.monsterY = viewState.monsterY;
    record.targetX = viewState.targetX;
    record.targetY = viewState.targetY;
    record.nextDecisionRemainingMs = viewState.nextDecisionRemainingMs;
    record.postFeedAwakeRemainingMs = viewState.postFeedAwakeRemainingMs;
    record.checksum = checksumObject(record);

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    size_t written = prefs.putBytes(MAIN_SCENE_VIEW_KEY, &record, sizeof(record));
    prefs.end();
    return written == sizeof(record);
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
