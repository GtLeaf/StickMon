#include "core/SaveManager.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <new>
#include "game/BondSystem.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/FriendshipPity.h"
#include "game/Species.h"
#include "platform/api/PlatformServices.h"

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* ENCOUNTER_KEY = "encounters";
constexpr uint32_t SAVE_RECORD_MAGIC = 0x3156534D; // MSV1
constexpr uint16_t SAVE_RECORD_VERSION = Game::SAVE_VERSION;
constexpr uint32_t ENCOUNTER_RECORD_MAGIC = 0x31454D53; // SME1
constexpr uint16_t ENCOUNTER_RECORD_VERSION = 1;
static_assert(SAVE_RECORD_VERSION == Game::SAVE_VERSION,
              "save record and game-state versions must advance together");
constexpr uint32_t MAIN_SCENE_VIEW_MAGIC = 0x4D565354; // MVST
constexpr uint16_t MAIN_SCENE_VIEW_VERSION = 1;
constexpr uint32_t MAIN_SCENE_VIEW_MAX_REMAINING_MS = 24UL * 60UL * 60UL * 1000UL;
constexpr float MAIN_SCENE_VIEW_COORD_LIMIT = 8192.0f;

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

struct SaveRecord {
    uint32_t magic = SAVE_RECORD_MAGIC;
    uint16_t version = SAVE_RECORD_VERSION;
    uint16_t reserved = 0;
    Game::GameState state;
    MainSceneViewRecord view{};
};

struct SaveRecordHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
};

struct EncounterRecord {
    uint32_t magic = ENCOUNTER_RECORD_MAGIC;
    uint16_t version = ENCOUNTER_RECORD_VERSION;
    uint16_t checksum = 0;
    Game::EncounterHistory history;
};

static_assert(sizeof(SaveRecordHeader) == 8, "unexpected save record header size");

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

bool makeMainSceneViewRecord(const MainSceneViewState& viewState,
                             MainSceneViewRecord& record) {
    if (!mainSceneViewValuesValid(viewState)) return false;
    record = MainSceneViewRecord{};
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
    return true;
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

void loadMainSceneViewRecord(const MainSceneViewRecord& record,
                             MainSceneViewState& viewState) {
    viewState = MainSceneViewState{};
    if (!record.valid) return;
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
}

void resetGameState(Game::GameState& state) {
    state = Game::GameState{};
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
    if (mon.move2Id != 0 &&
        (!canRetainSpecialMove(*species, mon.move2Id, mon.level) ||
         mon.move2Id == mon.move1Id)) {
        mon.move2Id = 0;
    }
    if (mon.move3Id != 0 &&
        (!canRetainSpecialMove(*species, mon.move3Id, mon.level) ||
         mon.move3Id == mon.move1Id || mon.move3Id == mon.move2Id)) {
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
    if (mon.bond > Game::Bond::MAX_VALUE) mon.bond = Game::Bond::MAX_VALUE;
    if (mon.bond < Game::Bond::MIN_VALUE) mon.bond = Game::Bond::MIN_VALUE;
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
    if ((mon.petCountToday & Game::Bond::INVITE_LOCK_FLAG) == 0 &&
        mon.petCountToday > 4) {
        mon.petCountToday = 4;
    }
    uint8_t origin = static_cast<uint8_t>(mon.origin);
    if (origin > static_cast<uint8_t>(Game::Origin::VISITOR) && mon.origin != Game::Origin::UNKNOWN) {
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
    // Visiting guests never survive a reboot; drop them from the team on load.
    for (uint8_t i = 0; i < state.teamCount;) {
        if (state.team[i].origin == Game::Origin::VISITOR) {
            for (uint8_t j = i + 1; j < state.teamCount; ++j) {
                state.team[j - 1] = state.team[j];
            }
            state.teamCount--;
            if (state.teamCount < Game::TEAM_CAP) {
                state.team[state.teamCount] = Game::MonsterRuntime{};
            }
            changed = true;
        } else {
            ++i;
        }
    }
    if (state.oobeDone && state.teamCount == 0) {
        state.teamCount = 1;
        state.team[0] = Game::MonsterRuntime{};
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
    clampU8(state.bag.potion, Game::ITEM_STACK_CAP);
    clampU8(state.bag.superPotion, Game::ITEM_STACK_CAP);
    clampU8(state.bag.antidote, Game::ITEM_STACK_CAP);
    clampU8(state.bag.candy, Game::ITEM_STACK_CAP);
    clampU8(state.bag.paralyzeHeal, Game::ITEM_STACK_CAP);
    clampU8(state.bag.awakening, Game::ITEM_STACK_CAP);
    clampU8(state.bag.burnHeal, Game::ITEM_STACK_CAP);
    clampU8(state.bag.iceHeal, Game::ITEM_STACK_CAP);
    clampU8(state.bag.maxPotion, Game::ITEM_STACK_CAP);
    clampU8(state.bag.fullRestore, Game::ITEM_STACK_CAP);
    clampU8(state.bag.fullHeal, Game::ITEM_STACK_CAP);
    clampU8(state.bag.fireStone, Game::ITEM_STACK_CAP);
    clampU8(state.bag.waterStone, Game::ITEM_STACK_CAP);
    clampU8(state.bag.thunderStone, Game::ITEM_STACK_CAP);
    clampU8(state.bag.revive, Game::ITEM_STACK_CAP);
    clampU8(state.bag.maxRepel, Game::ITEM_STACK_CAP);
    clampU8(state.bag.honey, Game::ITEM_STACK_CAP);
    clampU8(state.bag.nugget, Game::ITEM_STACK_CAP);
    clampU8(state.bag.bigPearl, Game::ITEM_STACK_CAP);
    clampU8(state.bag.starPiece, Game::ITEM_STACK_CAP);
    clampU8(state.bag.heartScale, Game::ITEM_STACK_CAP);
    for (uint8_t i = 0; i < Game::SOAP_VARIANT_COUNT; ++i) {
        clampU8(state.bag.soap[i], Game::ITEM_STACK_CAP);
    }
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
    if (state.pairMoodRewardsToday > 3) {
        state.pairMoodRewardsToday = 3;
        changed = true;
    }
    if (state.candyPurchasesToday > Game::DAILY_CANDY_PURCHASE_CAP) {
        state.candyPurchasesToday = Game::DAILY_CANDY_PURCHASE_CAP;
        changed = true;
    }
    if (!state.pendingLevelUp) {
        if (state.pendingLevelUpLevel != 0) {
            state.pendingLevelUpLevel = 0;
            changed = true;
        }
    } else {
        uint8_t highestTeamLevel = 0;
        for (uint8_t slot = 0;
             slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
            highestTeamLevel = max(highestTeamLevel, state.team[slot].level);
        }
        if (state.teamCount == 0 ||
            state.pendingLevelUpLevel == 0 ||
            state.pendingLevelUpLevel > highestTeamLevel) {
            state.pendingLevelUp = false;
            state.pendingLevelUpLevel = 0;
            changed = true;
        }
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
    if (state.settings.brightness < 32) {
        state.settings.brightness = 32;
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
    for (uint8_t i = 0; i < Game::FRIENDSHIP_PITY_TRACKED_COUNT; ++i) {
        if (state.friendshipPityFailCounts[i] >
            FriendshipPity::MAX_FAIL_COUNT) {
            state.friendshipPityFailCounts[i] =
                FriendshipPity::MAX_FAIL_COUNT;
            changed = true;
        }
    }
    uint8_t specialMask = static_cast<uint8_t>(
        state.specialBossDefeatedMask & ExploreSpecial::VALID_DEFEATED_MASK);
    if (specialMask != state.specialBossDefeatedMask) {
        state.specialBossDefeatedMask = specialMask;
        changed = true;
    }
    return changed;
}

}

bool SaveManager::begin() {
    return Platform::blobs().initialize();
}

bool SaveManager::load(Game::GameState& state,
                       MainSceneViewState& viewState,
                       bool* normalized) {
    if (normalized) *normalized = false;
    viewState = MainSceneViewState{};
    size_t len = Platform::blobs().blobSize(NVS_NS, NVS_KEY);
    if (len != sizeof(SaveRecord)) {
        Serial.printf("[SaveManager] unsupported state size=%u expected=%u; reset to v%u\n",
                      (unsigned)len,
                      (unsigned)sizeof(SaveRecord),
                      Game::SAVE_VERSION);
        reset(state);
        return false;
    }

    auto* raw = new (std::nothrow) uint8_t[len];
    if (!raw) {
        Serial.println("[SaveManager] state load allocation failed");
        reset(state);
        return false;
    }
    bool readOk = Platform::blobs().readBlob(NVS_NS, NVS_KEY, raw, len);
    size_t read = readOk ? len : 0;
    if (!readOk) {
        Serial.printf("[SaveManager] state read failed read=%u expected=%u\n",
                      (unsigned)read,
                      (unsigned)len);
        delete[] raw;
        reset(state);
        return false;
    }

    SaveRecordHeader header{};
    memcpy(&header, raw, sizeof(header));
    if (header.magic != SAVE_RECORD_MAGIC || header.reserved != 0) {
        Serial.printf("[SaveManager] invalid state header magic=%08lx version=%u reserved=%u\n",
                      (unsigned long)header.magic,
                      header.version,
                      header.reserved);
        delete[] raw;
        reset(state);
        return false;
    }

    if (header.version != SAVE_RECORD_VERSION || len != sizeof(SaveRecord)) {
        Serial.printf("[SaveManager] unsupported state version=%u size=%u expected=v%u/%u\n",
                      header.version,
                      (unsigned)len,
                      Game::SAVE_VERSION,
                      (unsigned)sizeof(SaveRecord));
        delete[] raw;
        reset(state);
        return false;
    }

    auto* record = new (std::nothrow) SaveRecord{};
    if (!record) {
        delete[] raw;
        Serial.println("[SaveManager] state load allocation failed");
        reset(state);
        return false;
    }
    memcpy(record, raw, sizeof(*record));
    delete[] raw;
    uint16_t expectedChecksum = checksum(record->state);
    if (record->magic != SAVE_RECORD_MAGIC ||
        record->version != SAVE_RECORD_VERSION ||
        record->reserved != 0 ||
        record->state.magic != Game::SAVE_MAGIC ||
        record->state.version != Game::SAVE_VERSION ||
        record->state.checksum != expectedChecksum) {
        Serial.printf("[SaveManager] unsupported/invalid state read=%u magic=%08lx version=%u checksum=%04x/%04x; reset to v%u\n",
                      (unsigned)read,
                      (unsigned long)record->state.magic,
                      record->state.version,
                      record->state.checksum,
                      expectedChecksum,
                      Game::SAVE_VERSION);
        delete record;
        reset(state);
        return false;
    }

    state = record->state;
    bool viewValid = mainSceneViewRecordValid(record->view);
    if (viewValid) {
        loadMainSceneViewRecord(record->view, viewState);
    } else {
        Serial.println("[SaveManager] invalid main scene view state; reset view");
    }
    delete record;

    bool changed = sanitizeState(state);
    if (changed) {
        Serial.println("[SaveManager] normalized state fields");
    }
    if (normalized) *normalized = changed || !viewValid;
    return true;
}

bool SaveManager::saveSnapshot(const Game::GameState& state,
                               const MainSceneViewState& viewState) {
    auto* record = new (std::nothrow) SaveRecord{};
    if (!record) {
        Serial.println("[SaveManager] snapshot allocation failed");
        return false;
    }
    if (!makeMainSceneViewRecord(viewState, record->view)) {
        Serial.println("[SaveManager] refused invalid main scene view state");
        delete record;
        return false;
    }
    record->state = state;
    record->state.magic = Game::SAVE_MAGIC;
    record->state.version = Game::SAVE_VERSION;
    record->state.checksum = checksum(record->state);

    bool result = Platform::blobs().writeBlob(
        NVS_NS, NVS_KEY, record, sizeof(*record));
    delete record;

    if (!result) {
        Serial.println("[SaveManager] snapshot write failed");
    }
    return result;
}

bool SaveManager::loadEncounterHistory(Game::EncounterHistory& history,
                                       bool* normalized) {
    if (normalized) *normalized = false;
    history.clear();

    size_t len = Platform::blobs().blobSize(NVS_NS, ENCOUNTER_KEY);
    if (len == 0) {
        return false;
    }
    if (len != sizeof(EncounterRecord)) {
        Serial.printf("[SaveManager] invalid encounter history size=%u expected=%u\n",
                      static_cast<unsigned>(len),
                      static_cast<unsigned>(sizeof(EncounterRecord)));
        return false;
    }

    EncounterRecord record{};
    size_t read = Platform::blobs().readBlob(
        NVS_NS, ENCOUNTER_KEY, &record, sizeof(record))
        ? sizeof(record) : 0;
    uint16_t expectedChecksum = checksumObject(record);
    if (read != sizeof(record) ||
        record.magic != ENCOUNTER_RECORD_MAGIC ||
        record.version != ENCOUNTER_RECORD_VERSION ||
        record.checksum != expectedChecksum) {
        Serial.println("[SaveManager] invalid encounter history; reset history");
        return false;
    }

    history = record.history;
    bool changed = history.sanitize();
    if (normalized) *normalized = changed;
    return true;
}

bool SaveManager::saveEncounterHistory(
    const Game::EncounterHistory& history) {
    EncounterRecord record{};
    record.history = history;
    record.history.sanitize();
    record.checksum = checksumObject(record);

    size_t written = Platform::blobs().writeBlob(
        NVS_NS, ENCOUNTER_KEY, &record, sizeof(record))
        ? sizeof(record) : 0;
    if (written != sizeof(record)) {
        Serial.printf(
            "[SaveManager] encounter history write failed written=%u expected=%u\n",
            static_cast<unsigned>(written),
            static_cast<unsigned>(sizeof(record)));
        return false;
    }
    return true;
}

void SaveManager::reset(Game::GameState& state) {
    resetGameState(state);
}

bool SaveManager::clearAll() {
    return Platform::blobs().clearNamespace(NVS_NS);
}

bool SaveManager::loadHatchProgress(Game::HatchProgress& progress) {
    size_t len = Platform::blobs().blobSize(NVS_NS, HATCH_KEY);
    if (len != sizeof(Game::HatchProgress)) {
        progress = Game::HatchProgress{};
        return false;
    }

    Game::HatchProgress loaded;
    size_t read = Platform::blobs().readBlob(
        NVS_NS, HATCH_KEY, &loaded, sizeof(loaded))
        ? sizeof(loaded) : 0;
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

    return Platform::blobs().writeBlob(
        NVS_NS, HATCH_KEY, &copy, sizeof(copy));
}

void SaveManager::clearHatchProgress() {
    Platform::blobs().removeBlob(NVS_NS, HATCH_KEY);
}

uint16_t SaveManager::checksum(const Game::GameState& state) {
    return checksumObject(state);
}

uint16_t SaveManager::checksum(const Game::HatchProgress& progress) {
    return checksumObject(progress);
}
