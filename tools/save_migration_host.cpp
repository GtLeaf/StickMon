#include "core/SaveManager.h"
#include "core/SaveCodec.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"
#include "save_schema1_fabricator.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr uint32_t SAVE_RECORD_MAGIC = 0x3156534D;
constexpr uint32_t MAIN_SCENE_VIEW_MAGIC = 0x4D565354;

template <typename T>
uint16_t checksumObject(const T& object) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&object);
    const uint8_t* checksumBytes =
        reinterpret_cast<const uint8_t*>(&object.checksum);
    size_t checksumOffset = static_cast<size_t>(checksumBytes - bytes);
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < sizeof(object); ++i) {
        uint8_t value =
            (i == checksumOffset || i == checksumOffset + 1) ? 0 : bytes[i];
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

struct ViewV1 {
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

struct ViewV2 {
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

template <typename View>
struct LegacyRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    uint8_t stateBytes[1560];
    View view;
};

void makeLegacyState(uint8_t stateBytes[1560], uint16_t version) {
    Game::GameState state;
    state.version = version;
    state.settings.reservedAudioSetting = 1; // Legacy saves could enable voice calling.
    state.settings.volume = 66;
    state.settings.vibrationOn = true;
    state.coins = 4321;
    state.teamCount = 2;
    state.team[0].speciesId = 1;
    state.team[1] = state.team[0];
    state.team[1].speciesId = 4;
    state.team[1].ivPacked = 0x123456;
    state.team[1].nature = 3;
    state.team[1].metAt = 9876;
    state.team[1].metArea = 2;
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.storageCount = 1;
    state.storage[0] = state.team[0];
    state.checksum = 0;
    memcpy(stateBytes, &state, 1560);
    uint16_t checksum = checksumBytes(
        stateBytes, 1560, offsetof(Game::GameState, checksum));
    memcpy(stateBytes + offsetof(Game::GameState, checksum),
           &checksum, sizeof(checksum));
}

ViewV1 legacyViewV1() {
    ViewV1 view{};
    view.magic = MAIN_SCENE_VIEW_MAGIC;
    view.version = 1;
    view.valid = 1;
    view.speciesId = 1;
    view.monsterX = 84.0f;
    view.monsterY = 73.0f;
    view.targetX = 92.0f;
    view.targetY = 78.0f;
    view.facingRight = 1;
    view.checksum = checksumObject(view);
    return view;
}

void verifyV1Migration(DesktopPlatform& desktop) {
    assert(Platform::blobs().clearNamespace("stickmon"));
    LegacyRecord<ViewV1> legacy{};
    legacy.magic = SAVE_RECORD_MAGIC;
    legacy.version = 1;
    makeLegacyState(legacy.stateBytes, 1);
    legacy.view = legacyViewV1();
    static_assert(sizeof(legacy) == 1612);
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", &legacy, sizeof(legacy)));

    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    bool normalized = true;
    assert(manager.load(loaded, view, &normalized));
    assert(!normalized);
    assert(loaded.version == Game::SAVE_VERSION);
    assert(loaded.coins == 4321);
    assert(loaded.settings.reservedAudioSetting == 0);
    assert(loaded.settings.volume == 66 && loaded.settings.vibrationOn);
    assert(loaded.teamCount == 2 && loaded.team[1].speciesId == 4);
    assert(loaded.team[0].gender ==
           static_cast<uint8_t>(Game::Gender::MALE));
    assert(loaded.team[1].gender ==
           static_cast<uint8_t>(Game::Gender::MALE));
    assert(loaded.storage[0].gender ==
           static_cast<uint8_t>(Game::Gender::MALE));
    assert(view.valid && view.monsterX == 84.0f && view.monsterY == 73.0f);
    assert(!view.secondary.valid);
    size_t migratedLength = Platform::blobs().blobSize("stickmon", "state");
    assert(migratedLength >= SaveCodec::HEADER_BYTES &&
           migratedLength <= SaveCodec::MAX_ENCODED_BYTES);

    Game::GameState reloaded;
    MainSceneViewState reloadedView;
    assert(manager.load(reloaded, reloadedView, &normalized));
    assert(reloaded.coins == loaded.coins);
    assert(reloadedView.monsterX == view.monsterX);
    assert(loaded.normalBossPitySlotIndex ==
           loaded.gameMinutesTotal / 480);
    assert(desktop.logs().find(
               "migrated state v1 -> v" +
               std::to_string(Game::SAVE_VERSION)) != std::string::npos);
}

void verifyTransitionalMigration() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    LegacyRecord<ViewV2> legacy{};
    legacy.magic = SAVE_RECORD_MAGIC;
    legacy.version = 1;
    makeLegacyState(legacy.stateBytes, 1);
    ViewV1 first = legacyViewV1();
    memcpy(&legacy.view, &first, sizeof(first));
    legacy.view.version = 2;
    legacy.view.secondaryValid = 1;
    legacy.view.secondarySpeciesId = 4;
    legacy.view.secondaryIvPacked = 0x123456;
    legacy.view.secondaryMetAt = 9876;
    legacy.view.secondaryNature = 3;
    legacy.view.secondaryMetArea = 2;
    legacy.view.secondaryOrigin =
        static_cast<uint8_t>(Game::Origin::BEFRIENDED);
    legacy.view.secondaryDirection = 1;
    legacy.view.secondaryState = 6;
    legacy.view.secondaryX = 132.0f;
    legacy.view.secondaryY = 91.0f;
    legacy.view.secondaryTargetX = 132.0f;
    legacy.view.secondaryTargetY = 91.0f;
    legacy.view.secondarySleepX = 132.0f;
    legacy.view.secondarySleepY = 91.0f;
    legacy.view.secondarySleepSpotValid = 1;
    legacy.view.checksum = checksumObject(legacy.view);
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", &legacy, sizeof(legacy)));

    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    assert(manager.load(loaded, view));
    assert(loaded.coins == 4321);
    assert(loaded.settings.reservedAudioSetting == 0);
    assert(loaded.settings.volume == 66 && loaded.settings.vibrationOn);
    assert(view.secondary.valid);
    assert(view.secondary.speciesId == 4);
    assert(view.secondary.state == 6);
    assert(view.secondary.x == 132.0f && view.secondary.y == 91.0f);
}

void verifyV2Migration(DesktopPlatform& desktop) {
    assert(Platform::blobs().clearNamespace("stickmon"));
    LegacyRecord<ViewV2> legacy{};
    legacy.magic = SAVE_RECORD_MAGIC;
    legacy.version = 2;
    makeLegacyState(legacy.stateBytes, 2);
    ViewV1 first = legacyViewV1();
    memcpy(&legacy.view, &first, sizeof(first));
    legacy.view.version = 2;
    legacy.view.checksum = checksumObject(legacy.view);
    static_assert(sizeof(legacy) == 1664);
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", &legacy, sizeof(legacy)));

    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    assert(manager.load(loaded, view));
    assert(loaded.version == Game::SAVE_VERSION);
    assert(loaded.coins == 4321);
    assert(loaded.settings.reservedAudioSetting == 0);
    assert(loaded.settings.volume == 66 && loaded.settings.vibrationOn);
    assert(loaded.normalBossPitySlotIndex ==
           loaded.gameMinutesTotal / 480);
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        assert(loaded.normalBossMissCount[area] == 0);
    }
    assert(desktop.logs().find(
               "migrated state v2 -> v" +
               std::to_string(Game::SAVE_VERSION)) != std::string::npos);
}

void verifyInvalidLegacyViewKeepsGameState() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    LegacyRecord<ViewV1> legacy{};
    legacy.magic = SAVE_RECORD_MAGIC;
    legacy.version = 1;
    makeLegacyState(legacy.stateBytes, 1);
    legacy.view = legacyViewV1();
    legacy.view.checksum ^= 0xFFFF;
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", &legacy, sizeof(legacy)));

    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    assert(manager.load(loaded, view));
    assert(loaded.coins == 4321);
    assert(loaded.settings.reservedAudioSetting == 0);
    assert(loaded.settings.volume == 66 && loaded.settings.vibrationOn);
    assert(!view.valid && !view.secondary.valid);
}

void verifyNewerVersionIsPreserved() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    struct FutureHeader {
        uint32_t magic = SAVE_RECORD_MAGIC;
        uint16_t version = 99;
        uint16_t reserved = 0;
        uint32_t sentinel = 0xA5A55A5A;
    } future;
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", &future, sizeof(future)));

    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    SaveManager::LoadStatus status = SaveManager::LoadStatus::NOT_FOUND;
    assert(!manager.load(loaded, view, nullptr, &status));
    assert(status == SaveManager::LoadStatus::NEWER_VERSION);
    assert(Platform::blobs().blobSize("stickmon", "state") == sizeof(future));
    FutureHeader preserved{};
    assert(Platform::blobs().readBlob(
        "stickmon", "state", &preserved, sizeof(preserved)));
    assert(preserved.version == 99 && preserved.sentinel == future.sentinel);
}

void verifyCodecAbRecovery() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    SaveManager manager;
    Game::GameState state;
    MainSceneViewState view;
    state.coins = 111;
    assert(manager.saveSnapshot(state, view));
    state.coins = 222;
    assert(manager.saveSnapshot(state, view));

    size_t slotBLength = Platform::blobs().blobSize("stickmon", "state_b");
    assert(slotBLength > SaveCodec::HEADER_BYTES);
    uint8_t* corrupted = new uint8_t[slotBLength];
    assert(Platform::blobs().readBlob(
        "stickmon", "state_b", corrupted, slotBLength));
    corrupted[slotBLength - 1] ^= 0x80;
    assert(Platform::blobs().writeBlob(
        "stickmon", "state_b", corrupted, slotBLength));
    delete[] corrupted;

    Game::GameState loaded;
    MainSceneViewState loadedView;
    assert(manager.load(loaded, loadedView));
    assert(loaded.coins == 222);

    size_t mirrorLength = Platform::blobs().blobSize("stickmon", "state");
    assert(mirrorLength == slotBLength);
    uint8_t* corruptedMirror = new uint8_t[mirrorLength];
    assert(Platform::blobs().readBlob(
        "stickmon", "state", corruptedMirror, mirrorLength));
    corruptedMirror[mirrorLength - 1] ^= 0x40;
    assert(Platform::blobs().writeBlob(
        "stickmon", "state", corruptedMirror, mirrorLength));
    delete[] corruptedMirror;

    assert(manager.load(loaded, loadedView));
    assert(loaded.coins == 111);
}

void verifyVisitorIsNotPersisted() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    SaveManager manager;
    Game::GameState state;
    MainSceneViewState view;
    state.teamCount = 2;
    state.team[1].speciesId = 25;
    state.team[1].origin = Game::Origin::VISITOR;
    assert(manager.saveSnapshot(state, view));
    assert(state.teamCount == 2);

    size_t length = Platform::blobs().blobSize("stickmon", "state");
    assert(length > SaveCodec::HEADER_BYTES);
    uint8_t* bytes = new uint8_t[length];
    assert(Platform::blobs().readBlob("stickmon", "state", bytes, length));
    SaveCodec::Snapshot snapshot;
    assert(SaveCodec::decode(bytes, length, snapshot));
    delete[] bytes;
    assert(snapshot.state.teamCount == 1);
    assert(snapshot.state.team[0].origin != Game::Origin::VISITOR);
}
void verifyCodecSchema1Migration(DesktopPlatform& desktop) {
    assert(Platform::blobs().clearNamespace("stickmon"));
    SaveManager manager;
    Game::GameState state;
    MainSceneViewState view;
    state.coins = 7777;
    uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
    size_t length = 0;
    assert(SaveCodec::encode(state, view, 5, encoded, sizeof(encoded), length));
    std::vector<uint8_t> legacy;
    assert(SaveTestUtil::fabricateSchema1Blob(encoded, length, legacy));
    assert(Platform::blobs().writeBlob(
        "stickmon", "state_a", legacy.data(), legacy.size()));

    Game::GameState loaded;
    MainSceneViewState loadedView;
    assert(manager.load(loaded, loadedView));
    assert(loaded.coins == 7777);
    assert(loaded.version == Game::SAVE_VERSION);
    assert(loaded.debugMotionFlags == 0);
    assert(desktop.logs().find(
               "migrated state v3 -> v" +
               std::to_string(Game::SAVE_VERSION)) != std::string::npos);

    // The mirror blob was rewritten in the current schema.
    size_t mirrorLength = Platform::blobs().blobSize("stickmon", "state");
    assert(mirrorLength == length);
    std::vector<uint8_t> mirror(mirrorLength);
    assert(Platform::blobs().readBlob(
        "stickmon", "state", mirror.data(), mirror.size()));
    assert(mirror[4] == SaveCodec::SCHEMA_VERSION);

    // The new field survives a save/load round trip.
    loaded.debugMotionFlags =
        Game::DEBUG_MOTION_TILT | Game::DEBUG_MOTION_WALK_BOUNDARY;
    assert(manager.saveSnapshot(loaded, loadedView));
    Game::GameState reloaded;
    MainSceneViewState reloadedView;
    assert(manager.load(reloaded, reloadedView));
    assert(reloaded.debugMotionFlags ==
           (Game::DEBUG_MOTION_TILT | Game::DEBUG_MOTION_WALK_BOUNDARY));
}

void verifyV4CareMigration() {
    for (int format = 0; format < 3; ++format) {
        assert(Platform::blobs().clearNamespace("stickmon"));
        Game::GameState original;
        original.oobeDone = true;
        original.coins = 8765;
        original.bag.soap[2] = 8;
        original.team[0].petCountToday = 3;
        original.debugMotionFlags = Game::DEBUG_MOTION_TILT;
        original.normalBossMissCount[2] = 2;
        if (format == 0) {
            uint8_t current[SaveCodec::MAX_ENCODED_BYTES];
            size_t length = 0;
            assert(SaveCodec::encode(original, {}, 9, current, sizeof(current), length));
            std::vector<uint8_t> legacy;
            assert(SaveTestUtil::fabricateSchema2Blob(current, length, legacy));
            assert(Platform::blobs().writeBlob("stickmon", "state_a", legacy.data(), legacy.size()));
        } else {
            struct FrozenRecord {
                uint32_t magic;
                uint16_t version, reserved;
                uint8_t stateBytes[1572];
                ViewV2 view;
            } legacy{};
            static_assert(sizeof(legacy) == 1676);
            original.version = format == 1 ? 3 : 4;
            legacy.magic = SAVE_RECORD_MAGIC;
            legacy.version = original.version;
            std::memcpy(legacy.stateBytes, &original, 1572);
            const uint16_t checksum = checksumBytes(legacy.stateBytes, 1572, 6);
            std::memcpy(legacy.stateBytes + 6, &checksum, 2);
            assert(Platform::blobs().writeBlob("stickmon", "state", &legacy, sizeof(legacy)));
        }
        SaveManager manager;
        Game::GameState migrated;
        MainSceneViewState view;
        assert(manager.load(migrated, view));
        assert(migrated.version == Game::SAVE_VERSION);
        assert(migrated.coins == original.coins && migrated.bag.soap[2] == 8);
        assert(migrated.team[0].petCountToday == 3);
        assert(migrated.normalBossMissCount[2] == 2);
        assert(migrated.debugMotionFlags == (format == 1 ? 0 : Game::DEBUG_MOTION_TILT));
        assert(migrated.careRewardDay == migrated.gameMinutesTotal / 1440);
        assert(migrated.careRewards[0].valid && migrated.careRewards[0].budget == 18);
        assert(migrated.careRewards[0].awarded[0] == 0);
        Game::GameState reloaded;
        assert(manager.load(reloaded, view));
        assert(reloaded.careRewards[0].budget == 18);
    }
}

// Forward every operation to the real host store except selected snapshot
// writes, allowing the production loader to decide which copy is durable.
class SnapshotWriteFailures final : public Platform::IBlobStore {
public:
    explicit SnapshotWriteFailures(Platform::IBlobStore& backing) : backing_(backing) {}
    bool failSlots = false;
    bool failMirror = false;
    bool initialize() override { return backing_.initialize(); }
    size_t blobSize(const char* ns, const char* key) override { return backing_.blobSize(ns,key); }
    bool readBlob(const char* ns, const char* key, void* output, size_t length) override {
        return backing_.readBlob(ns,key,output,length);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t length) override {
        if ((failMirror && std::strcmp(key,"state") == 0) ||
            (failSlots && (std::strcmp(key,"state_a") == 0 || std::strcmp(key,"state_b") == 0))) return false;
        return backing_.writeBlob(ns,key,data,length);
    }
    bool removeBlob(const char* ns, const char* key) override { return backing_.removeBlob(ns,key); }
    bool clearNamespace(const char* ns) override { return backing_.clearNamespace(ns); }
private:
    Platform::IBlobStore& backing_;
};

void verifySnapshotSuccessMatchesDurableCopy(DesktopPlatform& desktop) {
    auto& originalServices = desktop.serviceBundle();
    SnapshotWriteFailures failures(originalServices.blobs);
    Platform::Services services{
        originalServices.lifecycle, originalServices.clock, originalServices.logger,
        originalServices.input, originalServices.display, originalServices.audio,
        originalServices.imu, originalServices.power, failures, originalServices.resources,
        originalServices.memory, originalServices.peers};
    Platform::bind(services);
    // Exercise a committed state_a and state_b, with either copy failing and
    // with both failing. Old copies remain present throughout the attempt.
    for (unsigned baselineWrites : {1,2}) for (unsigned failureMask : {0,1,2,3}) {
        failures.failSlots = failures.failMirror = false;
        assert(Platform::blobs().clearNamespace("stickmon"));
        SaveManager manager;
        Game::GameState before;
        before.teamCount = 0;
        before.coins = 5000;
        before.careRewardDay = before.gameMinutesTotal / 1440;
        before.marketSeason.periodDay = 0;
        before.marketSeason.items[0] = static_cast<uint8_t>(Game::ItemId::HONEY);
        before.marketSeason.stock[0] = 3;
        before.marketSeason.boxRoll = 87;
        MainSceneViewState view;
        for (unsigned i = 0; i < baselineWrites; ++i) assert(manager.saveSnapshot(before,view));
        auto purchased = before;
        purchased.coins -= 600;
        purchased.bag.heartScale = 1;
        purchased.marketTutorGifted = 1;
        purchased.marketSeason.stock[0] = 2;
        purchased.marketItemStock[0] = 1;
        failures.failSlots = (failureMask & 1) != 0;
        failures.failMirror = (failureMask & 2) != 0;
        const bool saved = manager.saveSnapshot(purchased,view);
        assert(saved == (failureMask != 3));
        // Model the caller's transaction: it rolls back only after false.
        const auto visible = saved ? purchased : before;
        failures.failSlots = failures.failMirror = false;
        Game::GameState reloaded;
        assert(manager.load(reloaded,view));
        assert(reloaded.coins == visible.coins);
        assert(reloaded.bag.heartScale == visible.bag.heartScale);
        assert(reloaded.marketTutorGifted == visible.marketTutorGifted);
        assert(reloaded.marketSeason.stock[0] == visible.marketSeason.stock[0]);
        assert(reloaded.marketSeason.boxRoll == visible.marketSeason.boxRoll);
        assert(reloaded.marketItemStock[0] == visible.marketItemStock[0]);
    }
    Platform::bind(originalServices);
}

void verifySchema7MigrationAndFutureSchemaPreservation() {
    assert(Platform::blobs().clearNamespace("stickmon"));
    Game::GameState original;
    original.coins = 8765;
    original.bag.honey = 7;
    original.marketShop.periodDay = 0;
    original.marketShop.items[0] = uint8_t(Game::ItemId::HONEY);
    original.marketShop.items[1] = uint8_t(Game::ItemId::POTION);
    original.marketShop.stock[0] = 0;
    original.marketShop.stock[1] = 1;
    original.marketShop.boxBought = 1;
    original.marketShop.boxRoll = 98;
    original.marketCare.periodDay = 0;
    original.marketCare.used = 1;
    original.marketCare.served[0].ivPacked = 321;
    original.marketCare.served[0].metAt = 654;
    uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
    size_t length = 0;
    assert(SaveCodec::encode(original, {}, 5, encoded, sizeof(encoded), length));
    std::vector<uint8_t> legacy;
    assert(SaveTestUtil::fabricateLegacySchemaBlob(encoded, length, legacy, 7));
    assert(Platform::blobs().writeBlob("stickmon", "state", legacy.data(), legacy.size()));
    SaveManager manager;
    Game::GameState loaded;
    MainSceneViewState view;
    assert(manager.load(loaded, view));
    assert(loaded.coins == 8765 && loaded.bag.honey == 7);
    assert(loaded.marketShop.stock[0] == 0 && loaded.marketShop.stock[1] == 1);
    assert(loaded.marketShop.boxBought == 1 && loaded.marketShop.boxRoll == 98);
    assert(loaded.marketCare.used == 1 && loaded.marketCare.served[0].ivPacked == 321);
    assert(loaded.marketCare.served[0].metAt == 654);
    assert(loaded.marketSeason.periodDay == Game::MARKET_PERIOD_NONE);
    assert(loaded.marketTutorGifted == 0 && loaded.marketItemStock[0] == 0);
    std::vector<uint8_t> rewritten(length);
    assert(Platform::blobs().readBlob("stickmon", "state", rewritten.data(), rewritten.size()));
    assert(rewritten[4] == 8);
    assert(manager.load(loaded, view));
    assert(loaded.marketShop.boxBought == 1 && loaded.marketCare.used == 1);

    assert(Platform::blobs().clearNamespace("stickmon"));
    encoded[4] = SaveCodec::SCHEMA_VERSION + 1;
    const uint16_t crc = SaveTestUtil::blobCrc16(encoded, length);
    encoded[14] = static_cast<uint8_t>(crc);
    encoded[15] = static_cast<uint8_t>(crc >> 8);
    assert(Platform::blobs().writeBlob("stickmon", "state", encoded, length));
    SaveManager::LoadStatus status = SaveManager::LoadStatus::NOT_FOUND;
    assert(!manager.load(loaded, view, nullptr, &status));
    assert(status == SaveManager::LoadStatus::NEWER_VERSION);
    assert(Platform::blobs().readBlob("stickmon", "state", rewritten.data(), rewritten.size()));
    assert(std::memcmp(encoded, rewritten.data(), length) == 0);
}

void verifyPeriodicMarketNormalization() {
    const uint8_t capacities[] = {3,3,1,2,3,2,1,1};
    // Every stock boundary is accepted; one over any boundary closes this
    // period so re-entry cannot restore purchased stock.
    for (unsigned damage = 0; damage < 15; ++damage) {
        assert(Platform::blobs().clearNamespace("stickmon"));
        Game::GameState original;
        original.coins = 4321;
        original.bag.honey = 4;
        original.gameMinutesTotal = 10 * 1440;
        original.careRewardDay = 10;
        original.teamCount = 0;
        original.marketShop.periodDay = 10;
        original.marketShop.stock[0] = 1;
        original.marketShop.items[0] = uint8_t(Game::ItemId::HONEY);
        original.marketCare.periodDay = 10;
        original.marketCare.used = 1;
        original.marketCare.served[0].ivPacked = 123;
        original.marketCare.served[0].metAt = 456;
        original.marketSeason.periodDay = 10;
        for (unsigned i = 0; i < 8; ++i) {
            original.marketSeason.items[i] = uint8_t(Game::ItemId::POTION);
            original.marketSeason.stock[i] = capacities[i];
        }
        original.marketSeason.items[7] = 255;
        original.marketSeason.boxRoll = 100;
        for (auto& stock : original.marketItemStock) stock = damage == 14 ? 255 : 99;
        original.marketTutorGifted = damage == 14 ? 255 : 1;
        if (damage >= 1 && damage <= 8) ++original.marketSeason.stock[damage-1];
        if (damage == 9) original.marketSeason.items[2] = uint8_t(Game::ItemId::COUNT);
        if (damage == 10) original.marketSeason.boxRoll = 101;
        if (damage == 11) original.marketSeason.boxRoll = 0;
        if (damage == 12) original.marketSeason.periodDay = Game::MARKET_PERIOD_NONE;
        if (damage == 13) original.marketSeason.periodDay = 11;
        uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES] = {};
        size_t length = 0;
        assert(SaveCodec::encode(original, {}, 1, encoded, sizeof(encoded), length));
        assert(Platform::blobs().writeBlob("stickmon", "state", encoded, length));
        SaveManager manager;
        Game::GameState loaded;
        MainSceneViewState view;
        bool normalized = false;
        assert(manager.load(loaded, view, &normalized));
        assert(normalized == (damage != 0));
        assert(loaded.coins == 4321 && loaded.bag.honey == 4);
        assert(loaded.marketCare.used == 1 && loaded.marketCare.served[0].ivPacked == 123);
        assert(loaded.marketCare.served[0].metAt == 456);
        assert(loaded.marketShop.stock[0] == 1);
        assert(loaded.marketSeason.periodDay == 10);
        for (unsigned i = 0; i < 8; ++i)
            assert(loaded.marketSeason.stock[i] == ((damage == 0 || damage == 14) ? capacities[i] : 0));
        for (auto stock : loaded.marketItemStock) assert(stock == 99);
        assert(loaded.marketTutorGifted == 1);
        assert(manager.saveSnapshot(loaded, view));
        assert(manager.load(loaded, view));
        assert(loaded.marketSeason.periodDay == 10);
        if (damage >= 1 && damage <= 13) for (auto stock : loaded.marketSeason.stock) assert(stock == 0);
    }
    // An absent extension is a healthy migration default, not a damaged shelf.
    assert(Platform::blobs().clearNamespace("stickmon"));
    SaveManager manager;
    Game::GameState original, loaded;
    original.careRewardDay = original.gameMinutesTotal / 1440;
    original.teamCount = 0;
    MainSceneViewState view;
    assert(manager.saveSnapshot(original, view));
    bool normalized = false;
    assert(manager.load(loaded, view, &normalized));
    assert(!normalized && loaded.marketSeason.periodDay == Game::MARKET_PERIOD_NONE);
}

void verifyRawV8ReservedBoxBytes() {
    for (uint8_t boxBought : {0, 1}) {
        assert(Platform::blobs().clearNamespace("stickmon"));
        struct RawV8 {
            uint32_t magic = SAVE_RECORD_MAGIC;
            uint16_t version = 8, reserved = 0;
            uint8_t stateBytes[2480];
            ViewV2 view{};
        } raw{};
        static_assert(sizeof(raw) == 2584);
        Game::GameState original;
        original.version = 8;
        original.coins = 4321;
        original.bag.honey = 4;
        original.marketShop.periodDay = 0;
        original.marketShop.items[0] = uint8_t(Game::ItemId::HONEY);
        original.marketShop.items[1] = uint8_t(Game::ItemId::POTION);
        original.marketShop.stock[0] = 0;
        original.marketShop.stock[1] = 1;
        original.marketShop.boxBought = boxBought;
        original.marketShop.boxRoll = boxBought ? 99 : 0;
        original.marketCare.periodDay = 0;
        original.marketCare.used = 1;
        original.marketCare.served[0].ivPacked = 123;
        original.marketCare.served[0].metAt = 456;
        std::memcpy(raw.stateBytes, &original, sizeof(raw.stateBytes));
        const uint16_t crc = checksumBytes(raw.stateBytes, sizeof(raw.stateBytes), 6);
        std::memcpy(raw.stateBytes + 6, &crc, 2);
        const auto first = legacyViewV1();
        std::memcpy(&raw.view, &first, sizeof(first));
        raw.view.version = 2;
        raw.view.checksum = checksumObject(raw.view);
        assert(Platform::blobs().writeBlob("stickmon", "state", &raw, sizeof(raw)));
        SaveManager manager; Game::GameState loaded; MainSceneViewState view;
        assert(manager.load(loaded, view));
        assert(loaded.version == Game::SAVE_VERSION);
        assert(loaded.coins == 4321 && loaded.bag.honey == 4);
        assert(loaded.marketShop.periodDay == 0 && !loaded.marketShop.stock[0] && loaded.marketShop.stock[1] == 1);
        assert(loaded.marketShop.boxBought == boxBought && loaded.marketShop.boxRoll == (boxBought ? 99 : 0));
        assert(loaded.marketCare.used == 1 && loaded.marketCare.served[0].ivPacked == 123);
        assert(loaded.marketCare.served[0].metAt == 456);
        assert(loaded.marketSeason.periodDay == Game::MARKET_PERIOD_NONE);
        assert(loaded.marketItemStock[0] == 0 && loaded.marketTutorGifted == 0);
        assert(view.valid && view.speciesId == first.speciesId);
        assert(view.monsterX == first.monsterX);
        assert(manager.load(loaded, view));
        assert(loaded.bag.honey == 4 && loaded.marketShop.boxBought == boxBought);
    }
}

}  // namespace

int main() {
    DesktopPlatform desktop(".");
    Platform::bind(desktop.serviceBundle());
    assert(Platform::blobs().initialize());
    verifyV1Migration(desktop);
    verifyTransitionalMigration();
    verifyV2Migration(desktop);
    verifyCodecSchema1Migration(desktop);
    verifyV4CareMigration();
    verifyInvalidLegacyViewKeepsGameState();
    verifyNewerVersionIsPreserved();
    verifyCodecAbRecovery();
    verifySnapshotSuccessMatchesDurableCopy(desktop);
    verifyVisitorIsNotPersisted();
    verifyRawV8ReservedBoxBytes();
    verifyPeriodicMarketNormalization();
    verifySchema7MigrationAndFutureSchemaPreservation();
    return 0;
}
