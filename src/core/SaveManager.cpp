#include "core/SaveManager.h"
#include "core/MathUtil.h"
#include "core/SaveCodec.h"
#include <cmath>
#include <cstring>
#include <new>
#include "game/BondSystem.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/FriendshipPity.h"
#include "game/Species.h"
#include "platform/api/PlatformServices.h"

namespace {
constexpr const char* NVS_NS = "stickmon";
constexpr const char* NVS_KEY = "state";
constexpr const char* NVS_KEY_A = "state_a";
constexpr const char* NVS_KEY_B = "state_b";
constexpr const char* HATCH_KEY = "hatch";
constexpr const char* ENCOUNTER_KEY = "encounters";
constexpr uint32_t SAVE_RECORD_MAGIC = 0x3156534D; // MSV1
constexpr uint16_t SAVE_RECORD_VERSION = Game::SAVE_VERSION;
constexpr uint16_t LEGACY_SAVE_RECORD_VERSION_V1 = 1;
constexpr uint16_t LEGACY_SAVE_RECORD_VERSION_V2 = 2;
constexpr uint32_t ENCOUNTER_RECORD_MAGIC = 0x31454D53; // SME1
constexpr uint16_t ENCOUNTER_RECORD_VERSION = 1;
static_assert(SAVE_RECORD_VERSION == Game::SAVE_VERSION,
              "save record and game-state versions must advance together");
constexpr uint32_t MAIN_SCENE_VIEW_MAGIC = 0x4D565354; // MVST
constexpr uint16_t MAIN_SCENE_VIEW_VERSION = 2;
constexpr uint32_t MAIN_SCENE_VIEW_MAX_REMAINING_MS = 24UL * 60UL * 60UL * 1000UL;
constexpr float MAIN_SCENE_VIEW_COORD_LIMIT = 8192.0f;
constexpr size_t LEGACY_GAME_STATE_V1_V2_SIZE = 1560;
constexpr uint8_t MAIN_SCENE_AI_MODE_MAX = 8; // RESTING
constexpr uint8_t MAIN_SCENE_PMD_ACTION_MAX = 3; // SLEEPING
constexpr uint8_t MAIN_SCENE_PMD_DIRECTION_MAX = 7; // DOWN_RIGHT
constexpr uint8_t SECONDARY_SCENE_STATE_MAX = 6; // SLEEPING
constexpr uint8_t SECONDARY_SCENE_DIRECTION_MAX = 3; // RIGHT

struct MainSceneViewRecordV1 {
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
    uint16_t secondarySpeciesId;
    uint8_t secondaryValid;
    uint8_t secondaryState;
    uint32_t secondaryIvPacked;
    uint32_t secondaryMetAt;
    uint8_t secondaryNature;
    uint8_t secondaryMetArea;
    uint8_t secondaryOrigin;
    uint8_t secondaryDirection;
    uint8_t secondaryFrameIndex;
    uint8_t secondaryFacingRight;
    uint8_t secondarySleepSpotValid;
    uint8_t secondaryReserved;
    float secondaryX;
    float secondaryY;
    float secondaryTargetX;
    float secondaryTargetY;
    float secondarySleepX;
    float secondarySleepY;
    uint32_t secondaryStateRemainingMs;
    uint32_t secondaryFoodRetryRemainingMs;
};

struct SaveRecord {
    uint32_t magic = SAVE_RECORD_MAGIC;
    uint16_t version = SAVE_RECORD_VERSION;
    uint16_t reserved = 0;
    Game::GameState state;
    MainSceneViewRecord view{};
};

// v1 used the same GameState byte layout as v2. These two envelopes freeze
// both v1 variants that reached development devices: the original view record
// and the transitional view-v2 record produced before the outer version bump.
struct SaveRecordV1 {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint8_t stateBytes[LEGACY_GAME_STATE_V1_V2_SIZE];
    MainSceneViewRecordV1 view;
};

struct SaveRecordV1WithViewV2 {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint8_t stateBytes[LEGACY_GAME_STATE_V1_V2_SIZE];
    MainSceneViewRecord view;
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
static_assert(offsetof(Game::GameState, normalBossPitySlotIndex) ==
                  LEGACY_GAME_STATE_V1_V2_SIZE,
              "v3 fields must be appended after the frozen v1/v2 state");
static_assert(sizeof(MainSceneViewRecordV1) == 44,
              "v1 main-scene view layout changed");
static_assert(sizeof(SaveRecordV1) == 1612,
              "v1 save envelope layout changed");
static_assert(sizeof(SaveRecordV1WithViewV2) == 1664,
              "v1/v2 view-v2 save envelope layout changed");

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

uint16_t checksumBytes(const uint8_t* bytes, size_t length,
                       size_t checksumOffset) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint8_t value =
            (i == checksumOffset || i == checksumOffset + 1) ? 0 : bytes[i];
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

bool monsterIdentityFieldsValid(uint32_t ivPacked, uint8_t nature,
                                uint8_t metArea, uint8_t origin) {
    bool validMetArea = metArea < Game::EXPLORE_AREA_COUNT ||
                        metArea == Game::MET_AREA_STARTER ||
                        metArea == Game::MET_AREA_HATCHED ||
                        metArea == Game::MET_AREA_UNKNOWN;
    bool validOrigin = origin <= static_cast<uint8_t>(Game::Origin::VISITOR) ||
                       origin == static_cast<uint8_t>(Game::Origin::UNKNOWN);
    return (ivPacked & ~0x3FFFFFFFUL) == 0 &&
           nature < Game::NATURE_COUNT && validMetArea && validOrigin;
}

bool mainSceneViewValuesValid(const MainSceneViewState& viewState) {
    if (viewState.secondary.valid && !viewState.valid) return false;
    if (viewState.valid &&
        (findSpecies(viewState.speciesId) == nullptr ||
         viewState.aiMode > MAIN_SCENE_AI_MODE_MAX ||
         viewState.pmdAction > MAIN_SCENE_PMD_ACTION_MAX ||
         viewState.pmdDirection > MAIN_SCENE_PMD_DIRECTION_MAX ||
         !mainSceneCoordinateValid(viewState.monsterX) ||
         !mainSceneCoordinateValid(viewState.monsterY) ||
         !mainSceneCoordinateValid(viewState.targetX) ||
         !mainSceneCoordinateValid(viewState.targetY))) {
        return false;
    }
    const SecondarySceneViewState& secondary = viewState.secondary;
    return !secondary.valid ||
           (findSpecies(secondary.speciesId) != nullptr &&
            secondary.state <= SECONDARY_SCENE_STATE_MAX &&
            secondary.direction <= SECONDARY_SCENE_DIRECTION_MAX &&
            monsterIdentityFieldsValid(
                secondary.ivPacked, secondary.nature,
                secondary.metArea, secondary.origin) &&
            mainSceneCoordinateValid(secondary.x) &&
            mainSceneCoordinateValid(secondary.y) &&
            mainSceneCoordinateValid(secondary.targetX) &&
            mainSceneCoordinateValid(secondary.targetY) &&
            mainSceneCoordinateValid(secondary.sleepX) &&
            mainSceneCoordinateValid(secondary.sleepY));
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
    const SecondarySceneViewState& secondary = viewState.secondary;
    record.secondarySpeciesId = secondary.speciesId;
    record.secondaryValid = secondary.valid ? 1 : 0;
    record.secondaryState = secondary.state;
    record.secondaryIvPacked = secondary.ivPacked;
    record.secondaryMetAt = secondary.metAt;
    record.secondaryNature = secondary.nature;
    record.secondaryMetArea = secondary.metArea;
    record.secondaryOrigin = secondary.origin;
    record.secondaryDirection = secondary.direction;
    record.secondaryFrameIndex = secondary.frameIndex;
    record.secondaryFacingRight = secondary.facingRight ? 1 : 0;
    record.secondarySleepSpotValid = secondary.sleepSpotValid ? 1 : 0;
    record.secondaryX = secondary.x;
    record.secondaryY = secondary.y;
    record.secondaryTargetX = secondary.targetX;
    record.secondaryTargetY = secondary.targetY;
    record.secondarySleepX = secondary.sleepX;
    record.secondarySleepY = secondary.sleepY;
    record.secondaryStateRemainingMs = secondary.stateRemainingMs;
    record.secondaryFoodRetryRemainingMs = secondary.foodRetryRemainingMs;
    record.checksum = checksumObject(record);
    return true;
}

bool mainSceneViewRecordValid(const MainSceneViewRecord& record) {
    if (record.magic != MAIN_SCENE_VIEW_MAGIC ||
        record.version != MAIN_SCENE_VIEW_VERSION ||
        record.checksum != checksumObject(record) ||
        record.valid > 1 || record.facingRight > 1 || record.faintRestActive > 1 ||
        record.secondaryValid > 1 || record.secondaryFacingRight > 1 ||
        record.secondarySleepSpotValid > 1) {
        return false;
    }
    if (record.secondaryValid && !record.valid) return false;
    if (record.valid &&
        (findSpecies(record.speciesId) == nullptr ||
         record.aiMode > MAIN_SCENE_AI_MODE_MAX ||
         record.pmdAction > MAIN_SCENE_PMD_ACTION_MAX ||
         record.pmdDirection > MAIN_SCENE_PMD_DIRECTION_MAX ||
         !mainSceneCoordinateValid(record.monsterX) ||
         !mainSceneCoordinateValid(record.monsterY) ||
         !mainSceneCoordinateValid(record.targetX) ||
         !mainSceneCoordinateValid(record.targetY))) {
        return false;
    }
    return !record.secondaryValid ||
           (findSpecies(record.secondarySpeciesId) != nullptr &&
            record.secondaryState <= SECONDARY_SCENE_STATE_MAX &&
            record.secondaryDirection <= SECONDARY_SCENE_DIRECTION_MAX &&
            monsterIdentityFieldsValid(
                record.secondaryIvPacked, record.secondaryNature,
                record.secondaryMetArea, record.secondaryOrigin) &&
            mainSceneCoordinateValid(record.secondaryX) &&
            mainSceneCoordinateValid(record.secondaryY) &&
            mainSceneCoordinateValid(record.secondaryTargetX) &&
            mainSceneCoordinateValid(record.secondaryTargetY) &&
            mainSceneCoordinateValid(record.secondarySleepX) &&
            mainSceneCoordinateValid(record.secondarySleepY));
}

bool mainSceneViewRecordV1Valid(const MainSceneViewRecordV1& record) {
    if (record.magic != MAIN_SCENE_VIEW_MAGIC || record.version != 1 ||
        record.checksum != checksumObject(record) ||
        record.valid > 1 || record.facingRight > 1 ||
        record.faintRestActive > 1) {
        return false;
    }
    if (!record.valid) return true;
    return findSpecies(record.speciesId) != nullptr &&
           record.aiMode <= MAIN_SCENE_AI_MODE_MAX &&
           record.pmdAction <= MAIN_SCENE_PMD_ACTION_MAX &&
           record.pmdDirection <= MAIN_SCENE_PMD_DIRECTION_MAX &&
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
    if (!record.secondaryValid) return;
    SecondarySceneViewState& secondary = viewState.secondary;
    secondary.valid = true;
    secondary.speciesId = record.secondarySpeciesId;
    secondary.ivPacked = record.secondaryIvPacked;
    secondary.metAt = record.secondaryMetAt;
    secondary.nature = record.secondaryNature;
    secondary.metArea = record.secondaryMetArea;
    secondary.origin = record.secondaryOrigin;
    secondary.x = record.secondaryX;
    secondary.y = record.secondaryY;
    secondary.targetX = record.secondaryTargetX;
    secondary.targetY = record.secondaryTargetY;
    secondary.sleepX = record.secondarySleepX;
    secondary.sleepY = record.secondarySleepY;
    secondary.state = record.secondaryState;
    secondary.direction = record.secondaryDirection;
    secondary.frameIndex = record.secondaryFrameIndex;
    secondary.facingRight = record.secondaryFacingRight != 0;
    secondary.sleepSpotValid = record.secondarySleepSpotValid != 0;
    secondary.stateRemainingMs =
        record.secondaryStateRemainingMs > MAIN_SCENE_VIEW_MAX_REMAINING_MS
            ? MAIN_SCENE_VIEW_MAX_REMAINING_MS
            : record.secondaryStateRemainingMs;
    secondary.foodRetryRemainingMs =
        record.secondaryFoodRetryRemainingMs > MAIN_SCENE_VIEW_MAX_REMAINING_MS
            ? MAIN_SCENE_VIEW_MAX_REMAINING_MS
            : record.secondaryFoodRetryRemainingMs;
}

void loadMainSceneViewRecordV1(const MainSceneViewRecordV1& record,
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

bool loadLegacyGameState(const uint8_t* stateBytes, uint16_t version,
                         Game::GameState& state) {
    uint32_t magic = 0;
    uint16_t stateVersion = 0;
    uint16_t storedChecksum = 0;
    memcpy(&magic, stateBytes + offsetof(Game::GameState, magic), sizeof(magic));
    memcpy(&stateVersion, stateBytes + offsetof(Game::GameState, version),
           sizeof(stateVersion));
    memcpy(&storedChecksum, stateBytes + offsetof(Game::GameState, checksum),
           sizeof(storedChecksum));
    uint16_t expectedChecksum = checksumBytes(
        stateBytes, LEGACY_GAME_STATE_V1_V2_SIZE,
        offsetof(Game::GameState, checksum));
    if (magic != Game::SAVE_MAGIC || stateVersion != version ||
        storedChecksum != expectedChecksum) {
        return false;
    }

    state = Game::GameState{};
    memcpy(&state, stateBytes, LEGACY_GAME_STATE_V1_V2_SIZE);
    // Legacy v1/v2 records did not define gender. Use the requested stable
    // migration default for every team and storage monster.
    constexpr uint8_t LEGACY_GENDER =
        static_cast<uint8_t>(Game::Gender::MALE);
    for (Game::MonsterRuntime& monster : state.team) {
        monster.gender = LEGACY_GENDER;
    }
    for (Game::MonsterRuntime& monster : state.storage) {
        monster.gender = LEGACY_GENDER;
    }
    state.version = Game::SAVE_VERSION;
    state.checksum = 0;
    state.normalBossPitySlotIndex = ExploreSpecial::slotIndexFor(
        state.gameMinutesTotal);
    memset(state.normalBossMissCount, 0,
           sizeof(state.normalBossMissCount));
    return true;
}

bool migrateLegacySaveRecord(const uint8_t* raw, size_t len,
                             uint16_t version,
                             Game::GameState& state,
                             MainSceneViewState& viewState,
                             bool& viewValid) {
    viewState = MainSceneViewState{};
    viewValid = false;
    if (version == LEGACY_SAVE_RECORD_VERSION_V1 &&
        len == sizeof(SaveRecordV1)) {
        const uint8_t* stateBytes = raw + offsetof(SaveRecordV1, stateBytes);
        if (!loadLegacyGameState(stateBytes, version, state)) return false;
        MainSceneViewRecordV1 view{};
        memcpy(&view, raw + offsetof(SaveRecordV1, view), sizeof(view));
        viewValid = mainSceneViewRecordV1Valid(view);
        if (viewValid) loadMainSceneViewRecordV1(view, viewState);
        return true;
    }
    if (len == sizeof(SaveRecordV1WithViewV2)) {
        const uint8_t* stateBytes =
            raw + offsetof(SaveRecordV1WithViewV2, stateBytes);
        if (!loadLegacyGameState(stateBytes, version, state)) return false;
        MainSceneViewRecord view{};
        memcpy(&view, raw + offsetof(SaveRecordV1WithViewV2, view),
               sizeof(view));
        viewValid = mainSceneViewRecordValid(view);
        if (viewValid) loadMainSceneViewRecord(view, viewState);
        return true;
    }
    return false;
}

struct CodecLoadResult {
    bool found = false;
    bool newerVersion = false;
    SaveCodec::Snapshot snapshot;
    uint32_t sequence = 0;
};

bool sequenceIsNewer(uint32_t candidate, uint32_t current) {
    return static_cast<int32_t>(candidate - current) > 0;
}

void loadLatestCodecSnapshot(CodecLoadResult& result) {
    constexpr const char* KEYS[] = {NVS_KEY_A, NVS_KEY_B, NVS_KEY};
    // Scratch snapshot lives on the heap: this function is called from
    // saveSnapshot() during legacy migration, deep inside SaveManager::load's
    // call chain on the 8KB Arduino loopTask stack. Two on-stack Snapshot
    // instances (~1.7KB each) plus the printf error path were enough to
    // overflow it on devices without state_a/state_b slots.
    auto* scratch = new (std::nothrow) SaveCodec::Snapshot;
    if (!scratch) return;
    for (const char* key : KEYS) {
        size_t length = Platform::blobs().blobSize(NVS_NS, key);
        if (length < SaveCodec::HEADER_BYTES ||
            length > SaveCodec::MAX_ENCODED_BYTES) {
            continue;
        }
        auto* raw = new (std::nothrow) uint8_t[length];
        if (!raw) continue;
        bool readOk = Platform::blobs().readBlob(NVS_NS, key, raw, length);
        if (!readOk) {
            delete[] raw;
            continue;
        }
        uint32_t magic = 0;
        std::memcpy(&magic, raw, sizeof(magic));
        if (magic != SaveCodec::MAGIC) {
            delete[] raw;
            continue;
        }
        uint16_t schema = static_cast<uint16_t>(raw[4]) |
                          static_cast<uint16_t>(raw[5]) << 8;
        if (schema > SaveCodec::SCHEMA_VERSION) {
            result.newerVersion = true;
            delete[] raw;
            continue;
        }
        uint32_t sequence = 0;
        if (SaveCodec::decode(raw, length, *scratch, &sequence) &&
            (!result.found || sequenceIsNewer(sequence, result.sequence))) {
            result.found = true;
            result.sequence = sequence;
            result.snapshot = *scratch;
        }
        delete[] raw;
    }
    delete scratch;
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
    if (mon.gender == static_cast<uint8_t>(Game::Gender::UNKNOWN) ||
        mon.gender > static_cast<uint8_t>(Game::Gender::FEMALE)) {
        // Records written before gender support decode as UNKNOWN. Treat
        // those historical values as the migration default as well.
        mon.gender = static_cast<uint8_t>(Game::Gender::MALE);
    }
    mon.statusFlags &= 0x07U;
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
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        clampU8(state.normalBossMissCount[area],
                ExploreBossPity::MAX_MISSES);
    }
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
        state.room.food[foodIndex] = static_cast<uint8_t>(MathUtil::min<uint16_t>(
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
    uint16_t maxWalkExp = MathUtil::min<uint16_t>(50, state.stepsToday / 100);
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
            highestTeamLevel = MathUtil::max(highestTeamLevel, state.team[slot].level);
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
                       bool* normalized,
                       LoadStatus* status) {
    if (normalized) *normalized = false;
    if (status) *status = LoadStatus::NOT_FOUND;
    viewState = MainSceneViewState{};

    // Heap-allocated: CodecLoadResult embeds a full Snapshot (~1.7KB) and the
    // migration path below chains into saveSnapshot on the 8KB loopTask stack.
    auto* codecHeap = new (std::nothrow) CodecLoadResult;
    if (!codecHeap) {
        Platform::logLine("[SaveManager] codec snapshot allocation failed");
        reset(state);
        if (status) *status = LoadStatus::INVALID;
        return false;
    }
    loadLatestCodecSnapshot(*codecHeap);
    if (codecHeap->found) {
        state = codecHeap->snapshot.state;
        viewState = codecHeap->snapshot.view;
        bool changed = sanitizeState(state);
        state.checksum = checksum(state);
        if (changed && normalized) *normalized = true;
        if (status) *status = LoadStatus::LOADED;
        delete codecHeap;
        return true;
    }
    if (codecHeap->newerVersion) {
        Platform::logLine(
            "[SaveManager] codec snapshot is newer than this firmware; write protection required");
        reset(state);
        if (status) *status = LoadStatus::NEWER_VERSION;
        delete codecHeap;
        return false;
    }
    delete codecHeap;

    size_t len = Platform::blobs().blobSize(NVS_NS, NVS_KEY);
    if (len < sizeof(SaveRecordHeader)) {
        if (len != 0) {
            Platform::logf("[SaveManager] truncated state size=%u\n",
                           static_cast<unsigned>(len));
            if (status) *status = LoadStatus::INVALID;
        }
        reset(state);
        return false;
    }

    auto* raw = new (std::nothrow) uint8_t[len];
    if (!raw) {
        Platform::logLine("[SaveManager] state load allocation failed");
        if (status) *status = LoadStatus::INVALID;
        reset(state);
        return false;
    }
    bool readOk = Platform::blobs().readBlob(NVS_NS, NVS_KEY, raw, len);
    size_t read = readOk ? len : 0;
    if (!readOk) {
        Platform::logf("[SaveManager] state read failed read=%u expected=%u\n",
                      (unsigned)read,
                      (unsigned)len);
        delete[] raw;
        if (status) *status = LoadStatus::INVALID;
        reset(state);
        return false;
    }

    SaveRecordHeader header{};
    memcpy(&header, raw, sizeof(header));
    if (header.magic != SAVE_RECORD_MAGIC || header.reserved != 0) {
        Platform::logf("[SaveManager] invalid state header magic=%08lx version=%u reserved=%u\n",
                      (unsigned long)header.magic,
                      header.version,
                      header.reserved);
        delete[] raw;
        if (status) *status = LoadStatus::INVALID;
        reset(state);
        return false;
    }

    if (header.version > SAVE_RECORD_VERSION) {
        Platform::logf(
            "[SaveManager] state v%u is newer than firmware v%u; write protection required\n",
            header.version, Game::SAVE_VERSION);
        delete[] raw;
        if (status) *status = LoadStatus::NEWER_VERSION;
        reset(state);
        return false;
    }

    bool migrated = false;
    bool viewValid = false;
    bool loaded = false;
    if (header.version == LEGACY_SAVE_RECORD_VERSION_V1 ||
        header.version == LEGACY_SAVE_RECORD_VERSION_V2) {
        loaded = migrateLegacySaveRecord(
            raw, len, header.version, state, viewState, viewValid);
        migrated = loaded;
    } else if (header.version == SAVE_RECORD_VERSION &&
               len == sizeof(SaveRecord)) {
        auto* record = new (std::nothrow) SaveRecord{};
        if (record) {
            memcpy(record, raw, sizeof(*record));
            uint16_t expectedChecksum = checksum(record->state);
            loaded = record->state.magic == Game::SAVE_MAGIC &&
                     record->state.version == Game::SAVE_VERSION &&
                     record->state.checksum == expectedChecksum;
            if (loaded) {
                state = record->state;
                viewValid = mainSceneViewRecordValid(record->view);
                if (viewValid) {
                    loadMainSceneViewRecord(record->view, viewState);
                }
            }
            delete record;
        } else {
            Platform::logLine("[SaveManager] state load allocation failed");
        }
    } else {
        Platform::logf(
            "[SaveManager] unsupported state version=%u size=%u supported=%u-%u\n",
            header.version, static_cast<unsigned>(len),
            Game::MIN_SUPPORTED_SAVE_VERSION, Game::SAVE_VERSION);
    }
    delete[] raw;

    if (!loaded) {
        Platform::logf(
            "[SaveManager] invalid state version=%u size=%u; keeping blob and resetting runtime\n",
            header.version, static_cast<unsigned>(len));
        reset(state);
        if (status) *status = LoadStatus::INVALID;
        return false;
    }
    if (migrated) {
        Platform::logf("[SaveManager] migrated state v%u -> v%u\n",
                       header.version, Game::SAVE_VERSION);
    }
    if (!viewValid) {
        Platform::logLine("[SaveManager] invalid main scene view state; reset view");
    }

    bool changed = sanitizeState(state);
    if (changed) {
        Platform::logLine("[SaveManager] normalized state fields");
    }
    bool needsRewrite = migrated || changed || !viewValid;
    if (migrated) {
        if (saveSnapshot(state, viewState)) {
            Platform::logLine("[SaveManager] migration committed");
            needsRewrite = false;
        } else {
            Platform::logLine(
                "[SaveManager] migration commit failed; legacy blob retained for retry");
        }
    }
    if (normalized) *normalized = needsRewrite;
    if (status) *status = LoadStatus::LOADED;
    return true;
}

bool SaveManager::saveSnapshot(const Game::GameState& state,
                               const MainSceneViewState& viewState) {
    MainSceneViewRecord viewRecord{};
    if (!makeMainSceneViewRecord(viewState, viewRecord)) {
        Platform::logLine("[SaveManager] refused invalid main scene view state");
        return false;
    }

    auto* encoded = new (std::nothrow) uint8_t[SaveCodec::MAX_ENCODED_BYTES];
    auto* previous = new (std::nothrow) CodecLoadResult;
    auto* copy = new (std::nothrow) Game::GameState;
    if (!encoded || !previous || !copy) {
        delete[] encoded;
        delete previous;
        delete copy;
        Platform::logLine("[SaveManager] snapshot allocation failed");
        return false;
    }
    loadLatestCodecSnapshot(*previous);
    uint32_t sequence = previous->found ? previous->sequence + 1U : 1U;
    *copy = state;
    copy->magic = Game::SAVE_MAGIC;
    copy->version = Game::SAVE_VERSION;
    copy->checksum = checksum(*copy);
    size_t encodedLength = 0;
    if (!SaveCodec::encode(*copy, viewState, sequence, encoded,
                           SaveCodec::MAX_ENCODED_BYTES, encodedLength)) {
        delete[] encoded;
        delete previous;
        delete copy;
        Platform::logLine("[SaveManager] snapshot encode failed");
        return false;
    }

    const char* slotKey = (sequence & 1U) != 0 ? NVS_KEY_A : NVS_KEY_B;
    bool slotResult = Platform::blobs().writeBlob(
        NVS_NS, slotKey, encoded, encodedLength);
    bool mirrorResult = Platform::blobs().writeBlob(
        NVS_NS, NVS_KEY, encoded, encodedLength);
    delete[] encoded;
    delete previous;
    delete copy;

    if (!slotResult || !mirrorResult) {
        Platform::logLine("[SaveManager] snapshot write failed");
    }
    return slotResult && mirrorResult;
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
        Platform::logf("[SaveManager] invalid encounter history size=%u expected=%u\n",
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
        Platform::logLine("[SaveManager] invalid encounter history; reset history");
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
        Platform::logf(
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
