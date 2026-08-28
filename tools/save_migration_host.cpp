#include "core/SaveManager.h"
#include "core/SaveCodec.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

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
    assert(loaded.teamCount == 2 && loaded.team[1].speciesId == 4);
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
    assert(desktop.logs().find("migrated state v1 -> v3") !=
           std::string::npos);
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
    assert(loaded.normalBossPitySlotIndex ==
           loaded.gameMinutesTotal / 480);
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        assert(loaded.normalBossMissCount[area] == 0);
    }
    assert(desktop.logs().find("migrated state v2 -> v3") !=
           std::string::npos);
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
}  // namespace

int main() {
    DesktopPlatform desktop(".");
    Platform::bind(desktop.serviceBundle());
    assert(Platform::blobs().initialize());
    verifyV1Migration(desktop);
    verifyTransitionalMigration();
    verifyV2Migration(desktop);
    verifyInvalidLegacyViewKeepsGameState();
    verifyNewerVersionIsPreserved();
    verifyCodecAbRecovery();
    return 0;
}
