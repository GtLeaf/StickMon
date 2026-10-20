#include "AmoledApp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "HomeScreen.h"
#include "ui/UiCommon.h"
#include "ui/ExploreMapRenderer.h"
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/AudioManager.h"
#include "core/CryPlayer.h"
#include "core/FontResource.h"
#include "core/RoomMovementArea.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "game/ExploreItemProgression.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEncounters.h"
#include "game/BathService.h"
#include "game/BattleSystem.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExperienceService.h"
#include "game/FriendshipService.h"
#include "game/GameRandom.h"
#include "game/HatchService.h"
#include "game/HomeCare.h"
#include "game/HomeHud.h"
#include "game/ItemInventory.h"
#include "game/HomeChase.h"
#include "game/MonsterFactory.h"
#include "game/ShopService.h"
#include "game/Species.h"
#include "game/SpeciesBehavior.h"
#include "game/TeamRoster.h"
#if STICKMON_HAS_CLAW
#include "brain/StickmonClawRuntime.h"
#endif
#include "platform/api/PlatformServices.h"
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

namespace AmoledV1 {
using namespace BattleTiming;
namespace {

constexpr int TAP_SLOP = 12;
constexpr int DRAG_START_SLOP = 6;
constexpr int MENU_HEADER_HEIGHT = 56;
constexpr uint32_t MIND_UPDATE_MS = 400;
constexpr uint32_t MOTION_FRAME_MS = 140;
constexpr uint32_t TURN_PAUSE_MS = 160;
constexpr uint32_t PET_WANDER_RETRY_MIN_MS = 700;
constexpr uint32_t PET_WANDER_RETRY_MAX_MS = 1400;
constexpr uint32_t PET_DEBUG_LOG_INTERVAL_MS = 10000;
constexpr uint32_t COMPANION_NIGHT_FOOD_RETRY_MS = 60000UL;
constexpr uint16_t HOME_SLEEP_FRAME_MS = 800;

bool isEvolutionStone(Game::ItemId item) {
    return item == Game::ItemId::FIRE_STONE ||
           item == Game::ItemId::WATER_STONE ||
           item == Game::ItemId::THUNDER_STONE;
}
constexpr int PAIR_TALK_MIN_GAP_PX = 20;
// Actor x/y is rendered as the sprite rectangle's bottom-center anchor.
// Keep the host close to the doorway while leaving its full footprint inside
// the room walk polygon.
constexpr float VISITOR_WELCOME_OFFSET_X = 10.0f;
constexpr float VISITOR_WELCOME_OFFSET_Y = -5.0f;

int pairIdleWidthPx(uint16_t speciesId) {
    PokemonSprites::PetAnimationProfile profile{};
    if (!PokemonSprites::petAnimationProfile(speciesId, profile)) return 80;
    int widest = 0;
    for (uint16_t direction = 0; direction < 4; ++direction) {
        for (uint8_t index = 0; index < profile.idleFrames; ++index) {
            const auto kind = static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(profile.idleBase) +
                direction * profile.idleFrames + index);
            const auto* frame = PokemonSprites::findSpeciesSprite(speciesId, kind);
            if (frame) widest = std::max<int>(
                widest, PokemonSprites::frameVisibleWidth(frame) *
                            AmoledUi::RESOURCE_SCALE);
        }
    }
    return widest > 0 ? widest : 80;
}

constexpr uint32_t CARE_TICK_MS = 60000;
constexpr uint32_t PERIODIC_SAVE_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t EXPLORE_DOOR_PHASE_TIMEOUT_MS = 8000;
constexpr float EXPLORE_DOOR_CROSS_SPEED = 42.0f;
constexpr float EXPLORE_DOOR_CLEAR_SPEED = 30.0f;
constexpr int EXPLORE_ROUTE_VIEW_HEIGHT = HOME_STATUS_TOP;
constexpr uint32_t EXPLORE_PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t EXPLORE_PREVIEW_MOVE_MS = 500;
constexpr uint32_t EXPLORE_PREVIEW_HOLD_MS =
    EXPLORE_PREVIEW_CYCLE_MS - EXPLORE_PREVIEW_MOVE_MS;
constexpr float EXPLORE_AREA_CURSOR_LERP = 0.5f;
constexpr uint32_t EXPLORE_PREVIEW_LOAD_DELAY_MS = 80;
constexpr uint32_t EXPLORE_PREVIEW_BACKGROUND_LOAD_MS = 80;
// During the carousel only the three preview sprites change. Keep their
// invalidation band separate from the full page used by area changes.
constexpr uint16_t EXPLORE_PREVIEW_RENDER_TOP = EXPLORE_PREVIEW_TOP;
constexpr uint16_t EXPLORE_PREVIEW_RENDER_BOTTOM = EXPLORE_PREVIEW_BOTTOM;
constexpr float FALLBACK_ROOM_MIN_X = 56.0f;
constexpr float FALLBACK_ROOM_MAX_X = 122.0f;
constexpr float FALLBACK_ROOM_MIN_Y = 126.0f;
constexpr float FALLBACK_ROOM_MAX_Y = 151.0f;
constexpr float FALLBACK_FOOD_APPROACH_X = 121.0f;
constexpr float FALLBACK_FOOD_APPROACH_Y = 149.0f;
constexpr uint32_t MOOD_BURST_DURATION_MS = 420;
constexpr uint32_t MOOD_BURST_FRAME_MS = 50;
#if STICKMON_ENABLE_DEBUG_FEATURES
constexpr float DEBUG_TILT_DEADZONE = 0.08f;
constexpr float DEBUG_TILT_MAX = 0.62f;
constexpr float DEBUG_TILT_SPEED = 58.0f;
#endif

#if STICKMON_HAS_CLAW
// Render-time snapshot of the shared ClawStatusLog. Keep this copy in .bss so
// rendering the Claw log does not add another large automatic buffer.
Stickmon::ClawStatusLog::Entry s_clawLogEntries[Stickmon::ClawStatusLog::CAPACITY];
#endif

uint8_t moodHeartCountFor(uint8_t mood) {
    return std::min<uint8_t>(5, static_cast<uint8_t>(mood / 20));
}

constexpr float FOOD_FEED_OFFSET_X = 12.0f;
constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
constexpr float CAMERA_SAFE_LEFT = 62.0f;
constexpr float CAMERA_SAFE_RIGHT = 122.0f;
constexpr float CAMERA_SAFE_TOP = 58.0f;
constexpr float CAMERA_SAFE_BOTTOM = 126.0f;

constexpr RoomResource::Point FALLBACK_WALK_POLYGON[] = {
    {static_cast<int16_t>(FALLBACK_ROOM_MIN_X),
     static_cast<int16_t>(FALLBACK_ROOM_MIN_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MAX_X),
     static_cast<int16_t>(FALLBACK_ROOM_MIN_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MAX_X),
     static_cast<int16_t>(FALLBACK_ROOM_MAX_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MIN_X),
     static_cast<int16_t>(FALLBACK_ROOM_MAX_Y)},
};

int16_t gHomeNavParent[RoomNavigator::MAX_NODES] = {};
uint16_t gHomeNavQueue[RoomNavigator::MAX_NODES] = {};

float speciesGroundOffset(uint16_t speciesId) {
    PokemonSprites::PetAnimationProfile profile{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    if (PokemonSprites::petAnimationProfile(speciesId, profile)) {
        frame = PokemonSprites::findSpeciesSprite(speciesId, profile.idleBase);
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    return frame
        ? static_cast<float>(PokemonSprites::frameGroundOffsetY(frame))
        : 18.0f;
}

PokemonSprites::WalkDirection mainDirectionFromView(uint8_t direction) {
    if (direction == 3 || direction == 4 || direction == 5) {
        return PokemonSprites::WalkDirection::UP;
    }
    if (direction == 1 || direction == 2) {
        return PokemonSprites::WalkDirection::LEFT;
    }
    if (direction == 6 || direction == 7) {
        return PokemonSprites::WalkDirection::RIGHT;
    }
    return PokemonSprites::WalkDirection::DOWN;
}

uint8_t mainDirectionForView(PokemonSprites::WalkDirection direction) {
    switch (direction) {
    case PokemonSprites::WalkDirection::LEFT: return 2;
    case PokemonSprites::WalkDirection::UP: return 4;
    case PokemonSprites::WalkDirection::RIGHT: return 6;
    case PokemonSprites::WalkDirection::DOWN:
    default: return 0;
    }
}

PokemonSprites::WalkDirection petDirectionForDelta(float dx, float dy) {
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                          : PokemonSprites::WalkDirection::LEFT;
    }
    return dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                     : PokemonSprites::WalkDirection::UP;
}

PokemonSprites::WalkDirection foodFacingDirection(float x, float y) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available() ? room.foodX() : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available() ? room.foodY() : FALLBACK_FOOD_APPROACH_Y;
    return petDirectionForDelta(foodX - x, foodY - y);
}

GameAssets::Kind battleBackgroundForArea(uint8_t area) {
    return ExploreAreaCatalog::battleBackground(area);
}

ExplorePool::Pool buildExplorePreviewPool(const Game::GameState& state,
                                          uint8_t area) {
    return ExploreEncounters::poolForArea(
        area, state.gameMinutesTotal,
        area < Game::EXPLORE_AREA_COUNT
            ? state.explorePoolRerollCounts[area] : 0);
}

uint8_t collectExplorePreviewSpecies(const Game::GameState& state,
                                     uint16_t* speciesIds, uint8_t capacity,
                                     uint8_t priorityArea) {
    if (!speciesIds || capacity == 0) return 0;
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(state);
    uint8_t count = 0;
    if (priorityArea < Game::EXPLORE_AREA_COUNT &&
        priorityArea <= unlockedArea) {
        count = ExplorePool::appendUniqueSpecies(
            buildExplorePreviewPool(state, priorityArea), speciesIds,
            count, capacity);
    }
    for (uint8_t area = 0;
         area < Game::EXPLORE_AREA_COUNT && area <= unlockedArea; ++area) {
        if (area == priorityArea) continue;
        count = ExplorePool::appendUniqueSpecies(
            buildExplorePreviewPool(state, area), speciesIds, count,
            capacity);
    }
    return count;
}

bool repairZeroIndividualValues(Game::MonsterRuntime& monster) {
    if (monster.ivPacked != 0) return false;

    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;
    const uint16_t oldMax = monster.hpMax;
    const uint16_t oldCurrent = monster.hpCur;
    Game::MonsterFactory::rollIndividualValues(monster);
    monster.hpMax = maxHpFor(*species, monster);
    if (monster.fainted || oldCurrent == 0) {
        monster.hpCur = 0;
    } else if (monster.hpMax > oldMax) {
        monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
            monster.hpMax,
            static_cast<uint32_t>(oldCurrent) + monster.hpMax - oldMax));
    } else {
        monster.hpCur = std::min<uint16_t>(oldCurrent, monster.hpMax);
    }
    return true;
}

uint8_t repairOwnedZeroIndividualValues(Game::GameState& state) {
    uint8_t repaired = 0;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (repairZeroIndividualValues(state.team[slot])) ++repaired;
    }
    for (uint8_t slot = 0;
         slot < state.storageCount && slot < Game::STORAGE_CAP; ++slot) {
        if (repairZeroIndividualValues(state.storage[slot])) ++repaired;
    }
    return repaired;
}

}  // namespace

void AmoledApp::setMusicContext(MusicContext context) {
    if (musicContext == context) return;
    musicContext = context;

    MusicTrack track = MusicTrack::HOME;
    switch (context) {
    case MusicContext::EXPLORE:
        track = MusicTrack::EXPLORE;
        break;
    case MusicContext::BATTLE:
        track = MusicTrack::BATTLE;
        break;
    case MusicContext::BATTLE_SPECIAL:
        track = MusicTrack::BATTLE_SPECIAL;
        break;
    case MusicContext::HOME:
    default:
        break;
    }
    AudioManager::ins().setMusic(track);
}

void AmoledApp::startVisitHost() {
    if (gameState.teamCount == 1 && !visitRadioExclusive) {
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime::instance().enterPeerSession();
#endif
        visitRadioExclusive = true;
    }
    visitSession.startHost();
    if (visitSession.state() ==
        Communication::VisitSessionService::State::FAILED) {
        releaseVisitRadioSession();
    }
}

void AmoledApp::startVisitSearch() {
    if (gameState.teamCount > 0 && !visitRadioExclusive) {
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime::instance().enterPeerSession();
#endif
        visitRadioExclusive = true;
    }
    visitSession.startSearch();
    if (visitSession.state() ==
        Communication::VisitSessionService::State::FAILED) {
        releaseVisitRadioSession();
    }
}

void AmoledApp::releaseVisitRadioSession() {
    if (!visitRadioExclusive) return;
    EspNowLink::ins().end();
#if STICKMON_HAS_CLAW
    Stickmon::ClawRuntime::instance().leavePeerSession();
#endif
    visitRadioExclusive = false;
}

void AmoledApp::begin(uint32_t nowMs) {
    battleSession_.bindClock(+[] { return Platform::clock().millis(); });
    saveCoordinator_.reset(nowMs);
    lastPersistMs = nowMs;
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugBattleScenarioActive = false;
#endif
    renderCaches_.release();
    homeMainActor.reset(Home::ActorRole::LEADER, 0, nowMs);
    homeMainActor.x = homeMainActor.targetX = 92.0f;
    homeMainActor.y = homeMainActor.targetY = 151.0f;
    homeMainActor.geometry.footprint = {9.0f, 4.0f};
    homeMainActor.nextDecisionMs = 0;
    petStopping = false;
    homeCompanionActor = Home::Actor{};
    homeRuntime.attach(homeMainActor);
    homeRuntime.reset(nowMs);
    homeRuntimeReady = false;
    expeditionFade.reset();
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    pendingExpedition = false;
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    GameRandom::seed(Platform::power().hardwareRandom() ^ nowMs);
    explorePresenter_.reset();
    batterySampled = false;
    updateBattery(nowMs);
    visitSession.attach(&gameState);
    visitArrival.reset();
    visitRadioExclusive = false;
    storageReady = saveManager.begin();
    bool normalized = false;
    SaveManager::LoadStatus loadStatus = SaveManager::LoadStatus::NOT_FOUND;
    bool loaded = storageReady &&
                  saveManager.load(gameState, mainViewState, &normalized, &loadStatus);
    startupStateLoaded_ = loaded;
    saveWritesBlocked = loadStatus == SaveManager::LoadStatus::NEWER_VERSION ||
                        loadStatus == SaveManager::LoadStatus::INVALID;
    if (!loaded) {
        Game::HatchService::initializeNewGame(gameState);
        mainViewState = MainSceneViewState{};
    } else {
        const uint8_t repaired = repairOwnedZeroIndividualValues(gameState);
        if (repaired > 0) {
            normalized = true;
            Platform::logf(
                "[AmoledApp] repaired zero individual values: %u monster(s)\n",
                repaired);
        }
    }

    hatchResult.reset();
    hatchController_.reset(nowMs);
    // The normal startup path also explicitly puts a previously active IMU to sleep.
    Platform::imu().setAccelerationEnabled(false);
    if (!gameState.oobeDone) {
        if (gameState.teamCount != 0) normalized = true;
        gameState.teamCount = 0;
        Game::HatchProgress progress;
        if (storageReady) saveManager.loadHatchProgress(progress);
        hatchController_.restoreProgress(progress, gameState.hatchSeconds, nowMs);
    }

    bool normalizedEncounterHistory = false;
    bool loadedEncounterHistory = storageReady &&
        saveManager.loadEncounterHistory(
            encounterHistory, &normalizedEncounterHistory);
    if (!loadedEncounterHistory) encounterHistory.clear();
    encounterHistoryDirty = normalizedEncounterHistory ||
                            syncOwnedSpeciesToEncounterHistory();

#if STICKMON_ENABLE_DEBUG_FEATURES
    debugTiltControl =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_TILT) != 0;
    debugWalkBoundaryVisible =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_WALK_BOUNDARY) != 0;
    debugTalkPointsVisible =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_TALK_POINTS) != 0;
#endif

    if (storageReady && (!loaded || normalized || encounterHistoryDirty)) {
        saveState();
    }
    uint16_t teamSpecies[Game::TEAM_CAP] = {};
    uint8_t teamCount = std::min<uint8_t>(gameState.teamCount,
                                          Game::TEAM_CAP);
    for (uint8_t slot = 0; slot < teamCount; ++slot) {
        teamSpecies[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(teamSpecies, teamCount);
#if defined(__EMSCRIPTEN__)
    // Start browser sessions at full brightness, including restored saves.
    gameState.settings.brightness = 255;
#endif
    Platform::display().setBrightness(gameState.settings.brightness);
    Platform::audio().setVolume(gameState.settings.volume);
    AudioManager::ins().setMusic(MusicTrack::HOME);
    Platform::logf("[AmoledApp] save=%s species=%u level=%u food=%u\n",
                   loaded ? "loaded" : "created",
                   gameState.team[0].speciesId,
                   gameState.team[0].level,
                   gameState.room.food[gameState.room.selectedFood]);
    gameClock.start(nowMs, gameState.gameMinutesTotal);
    lastCareMs = nowMs;
    lastInteractionMs = nowMs;
    lastPetUpdateMs = nowMs;
    homeMainActor.nextMindUpdateMs = nowMs;
    if (!gameState.oobeDone) {
        RoomResource::ins().begin();
        FontResource::ins().begin();
        sceneFlow = AppSceneFlow::Controller(AppSceneFlow::Scene::HATCH);
        syncHatchImu(nowMs);
        requestFullRender();
        Platform::logLine("[AmoledApp] begin: hatch pending");
        return;
    }
    PetMotion restoredMotion = PetMotion::IDLE;
    petDirection = PokemonSprites::WalkDirection::DOWN;
    homeFlow_.reset();
    petFrame = 0;
    nextPetFrameMs = nowMs + 520;
    homeMainActor.mind.reset(nowMs);
    schedulePairInteraction(nowMs);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        homeMainActor.behavior = behaviorProfileFor(*species, gameState.team[0]);
    }
    homeFlow_.scheduleAttention(nowMs, true);
    homeFlow_.scheduleSpecialAction(nowMs);
    RoomResource::ins().begin();
    Platform::logf("[AmoledApp] begin: room resource ready\n");
    FontResource::ins().begin();
    Platform::logf("[AmoledApp] begin: font resource ready\n");
    homeMainActor.faintRestActive = gameState.teamCount > 0 &&
                 (gameState.team[0].fainted || gameState.team[0].hpCur == 0);
    moodHeartCount = gameState.teamCount > 0
        ? moodHeartCountFor(gameState.team[0].mood) : 0;
    moodBurstHeart = 0xFF;
    moodBurstStartedMs = 0;
    moodBurstUntilMs = 0;
    nextMoodBurstFrameMs = 0;
    Platform::logf("[AmoledApp] begin: pet resting=%u\n",
                   petIsResting() ? 1U : 0U);
    if (petIsResting()) {
        RoomResource& room = RoomResource::ins();
        homeMainActor.x = homeMainActor.targetX = room.available() ? static_cast<float>(room.bedX())
                                              : 76.0f;
        homeMainActor.y = homeMainActor.targetY = room.available() ? static_cast<float>(room.bedY())
                                              : 99.0f;
        startSleepAnimation(nowMs, 700);
    }
    Platform::logf("[AmoledApp] begin: resting position ready\n");
    updatePetFootprint();
    Platform::logf("[AmoledApp] begin: footprint ready rx=%.2f ry=%.2f\n",
                   static_cast<double>(homeMainActor.geometry.footprint.radiusX),
                   static_cast<double>(homeMainActor.geometry.footprint.radiusY));
    if (!petIsResting() && mainViewState.valid &&
        mainViewState.speciesId == gameState.team[0].speciesId) {
        const Game::MonsterRuntime& savedMonster = gameState.team[0];
        const Game::SpeciesCareProfile savedCare =
            Game::speciesCareProfileFor(savedMonster.speciesId);
        const bool savedSleepTime = savedCare.usesBed &&
            Game::isSleepCareTime(gameState.gameMinutesTotal,
                                  savedMonster.nature);
        const float groundOffset = speciesGroundOffset(
            gameState.team[0].speciesId);
        homeMainActor.x = mainViewState.monsterX;
        homeMainActor.y = mainViewState.monsterY + groundOffset;
        homeMainActor.targetX = mainViewState.targetX;
        homeMainActor.targetY = mainViewState.targetY + groundOffset;
        petDirection = mainDirectionFromView(mainViewState.pmdDirection);
        petFrame = mainViewState.pmdFrame;
        if (mainViewState.aiMode == 1) {
            restoredMotion = PetMotion::WANDERING;
        } else if (mainViewState.aiMode == 3) {
            if (savedCare.needsFood && gameState.room.bowlCount > 0 &&
                savedMonster.satiety < MONSTER_FEED_TARGET_SATIETY) {
                restoredMotion = PetMotion::SEEKING_FOOD;
            }
        } else if (mainViewState.aiMode == 4) {
            if (savedSleepTime) restoredMotion = PetMotion::SEEKING_SLEEP;
        } else if (mainViewState.aiMode == 8 && savedSleepTime) {
            restoredMotion = PetMotion::SLEEPING;
            startSleepAnimation(nowMs, HOME_SLEEP_FRAME_MS);
        }
        if (restoredMotion == PetMotion::IDLE) {
            homeMainActor.targetX = homeMainActor.x;
            homeMainActor.targetY = homeMainActor.y;
            petFrame = 0;
        }
        homeMainActor.nextDecisionMs = nowMs +
            mainViewState.nextDecisionRemainingMs;
    }
    if (!petFootprintInsideWalkArea(homeMainActor.x, homeMainActor.y)) {
        Platform::logf("[AmoledApp] begin: pet outside walk area\n");
        float x = homeMainActor.x;
        float y = homeMainActor.y;
        if (chooseWanderTarget(x, y, false)) {
            homeMainActor.x = homeMainActor.targetX = x;
            homeMainActor.y = homeMainActor.targetY = y;
        }
    }
    Platform::logf("[AmoledApp] begin: walk target ready x=%.2f y=%.2f\n",
                   static_cast<double>(homeMainActor.x), static_cast<double>(homeMainActor.y));
    syncHomeActors(nowMs);
    if (homeMainActor.faintRestActive) {
        homeRuntime.transition(0, Home::Task::FAINTED, nowMs, 0, true);
    } else if (restoredMotion == PetMotion::WANDERING) {
        homeRuntime.transition(0, Home::Task::WANDER, nowMs, 0, true);
    } else if (restoredMotion == PetMotion::SEEKING_FOOD) {
        homeRuntime.transition(0, Home::Task::SEEK_FOOD, nowMs, 0, true);
        homeRuntime.acquire(Home::Resource::BOWL, 0,
                            Home::Task::SEEK_FOOD, nowMs);
    } else if (restoredMotion == PetMotion::SEEKING_SLEEP ||
               restoredMotion == PetMotion::SLEEPING) {
        homeRuntime.transition(
            0, restoredMotion == PetMotion::SLEEPING
                   ? Home::Task::SLEEPING : Home::Task::SEEK_SLEEP,
            nowMs, 0, true);
        homeRuntime.acquire(Home::Resource::BED, 0,
                            restoredMotion == PetMotion::SLEEPING
                                ? Home::Task::SLEEPING
                                : Home::Task::SEEK_SLEEP,
                            nowMs);
    }
    if ((restoredMotion == PetMotion::WANDERING ||
         restoredMotion == PetMotion::SEEKING_FOOD ||
         restoredMotion == PetMotion::SEEKING_SLEEP) &&
        !homeRuntime.planRoute(
            0, homeMainActor.targetX, homeMainActor.targetY, false,
            homeCompanionActor.active)) {
        restoredMotion = PetMotion::IDLE;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        homeRuntime.stop(0, nowMs, 700);
    }
    updateCamera();
    Platform::logf("[AmoledApp] begin: camera ready\n");
    if (homeMainActor.nextDecisionMs == 0) schedulePetDecision(nowMs);
    Platform::logf("[AmoledApp] begin: decision scheduled\n");
    requestFullRender();
    Platform::logf("[AmoledApp] begin: full render requested\n");
}

bool AmoledApp::persistHatchProgress(bool force) {
    if (gameState.oobeDone) return true;
    if (!storageReady || saveWritesBlocked || gameResetPending) return false;
    Game::HatchProgress progress;
    if (!hatchController_.progressToSave(force, progress)) return true;
    if (!saveManager.saveHatchProgress(progress)) return false;
    hatchController_.didSaveProgress(progress.elapsedSeconds);
    return true;
}

void AmoledApp::completeHatch(uint32_t nowMs) {
    if (!storageReady || saveWritesBlocked || gameResetPending) return;
    if (!hatchController_.beginCompletionAttempt(nowMs)) return;
    if (!hatchResult) {
        gameClock.sync(nowMs, gameSpeed(), gameState.gameMinutesTotal);
        hatchResult.reset(new (std::nothrow) Game::GameState(gameState));
        if (hatchResult) {
            Game::HatchService::finish(*hatchResult,
                hatchController_.starterStyle());
        }
    }
    // Commit the completed game before deleting the egg's resumable progress.
    // Keep the same rolled starter for retries after a temporary storage failure.
    if (!hatchResult || !saveManager.saveSnapshot(*hatchResult, MainSceneViewState{})) {
        setToast(Ui::Hatch::SAVE_FAILED, nowMs);
        return;
    }
    gameState = *hatchResult;
    hatchResult.reset();
    mainViewState = MainSceneViewState{};
    saveManager.clearHatchProgress();
    sceneFlow = AppSceneFlow::Controller(AppSceneFlow::Scene::HOME);
    pointerDown = false;
    hatchController_.cancelTouch();
    begin(nowMs);
    setToast(Ui::Hatch::DONE, nowMs);
    CryPlayer::ins().replay(gameState.team[0].speciesId);
}

void AmoledApp::syncHatchImu(uint32_t nowMs) {
    const bool enabled = !gameResetPending && !gameState.oobeDone &&
        sceneFlow.current() == AppSceneFlow::Scene::HATCH &&
        !hatchController_.finished(gameState.hatchSeconds);
    const auto request = hatchController_.syncImu(enabled, nowMs);
    if (request.roomChanged) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    if (request.apply) {
        hatchController_.didApplyImu(
            Platform::imu().setAccelerationEnabled(request.enabled), nowMs);
    }
}

void AmoledApp::interactWithHatch(bool shake, uint32_t nowMs) {
    if (!hatchController_.interact(shake, nowMs, gameState.hatchSeconds,
                                   static_cast<bool>(hatchResult))) return;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    if (!persistHatchProgress(true)) setToast(Ui::Hatch::SAVE_FAILED, nowMs);
}

void AmoledApp::updateHatch(uint32_t nowMs) {
    const bool active = !gameState.oobeDone &&
        sceneFlow.current() == AppSceneFlow::Scene::HATCH;
    hatchController_.advance(nowMs, gameState.hatchSeconds, gameSpeed(), active);
    if (!active) return;
    syncHatchImu(nowMs);
    persistHatchProgress(false);
    if (hatchController_.pollImu(nowMs)) {
        float x, y, z;
        if (Platform::imu().readAcceleration(x, y, z) &&
            hatchController_.sampleAcceleration(x, y, z, nowMs)) {
            interactWithHatch(true, nowMs);
        }
    }
    const auto feedback = hatchController_.updateFeedback(nowMs, gameState.hatchSeconds);
    if (feedback.statusChanged) requestRenderRows(HOME_STATUS_TOP, 448);
    if (feedback.roomChanged) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    if (hatchController_.finished(gameState.hatchSeconds)) completeHatch(nowMs);
}

void AmoledApp::handleHatchTouch(const TouchEvent& event) {
    if (event.type == TouchEventType::DOWN) pointerDown = true;
    const auto action = hatchController_.handleTouch(event);
    if (event.type == TouchEventType::UP) pointerDown = false;
    if (action == HatchController::TouchAction::OPEN_MENU) {
        saveState();
        sceneFlow.openMenu();
        syncHatchImu(event.timestampMs);
        menuScroll = menuVelocity = 0.0f;
        toast = nullptr;
        requestFullRender();
    } else if (action == HatchController::TouchAction::POKE) {
        interactWithHatch(false, event.timestampMs);
    }
}

#if STICKMON_HAS_CLAW
bool AmoledApp::brainSnapshot(Stickmon::BrainBridge::Snapshot& out) const {
    out = Stickmon::BrainBridge::Snapshot{};
    if (gameResetPending) {
        out.actionLocked = true;
        return false;
    }
    out.initialized = true;
    out.oobeDone = gameState.oobeDone;
    out.visitActive = visitSession.active();
    out.teamCount = gameState.teamCount;
    out.exploreArea = selectedExploreArea;
    out.battery = batteryPercent;
    out.coins = gameState.coins;
    out.unlockedArea = ExploreItemProgression::unlockedArea(gameState);
    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index) {
        out.foodCounts[index] = gameState.room.food[index];
    }
    for (uint8_t index = 0; index < static_cast<uint8_t>(Game::ItemId::COUNT); ++index) {
        out.inventoryCounts[index] = Game::ItemInventory::count(
            gameState, static_cast<Game::ItemId>(index));
    }
    out.bowlFood = gameState.room.bowlFood;
    out.bowlBitesRemaining = gameState.room.bowlBitesRemaining;
    // Autonomous actions are only allowed while the normal home state machine
    // is visible and no interaction/communication flow owns the app.
    // `autonomyActive()` is deliberately not part of action_locked. It
    // describes ownership of the current Agent turn, not a gameplay lock;
    // including it would make the first stickmon_get_context call report a
    // locked state and the autonomous prompt would never take an action.
    out.actionLocked = !gameState.oobeDone || pointerDown ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        visitSession.busy() || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        homeFlow_.state().action != RoomAction::NONE || homeRuntime.pairActive();
    if (pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
        out.explorePhase = 1;  // departing
    } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        out.explorePhase = 2;  // active
    }
    if (gameState.teamCount > 0) {
        const Game::MonsterRuntime& monster = gameState.team[0];
        out.speciesId = monster.speciesId;
        const Species* species = findSpecies(monster.speciesId);
        if (species && species->name) {
            std::snprintf(out.speciesName, sizeof(out.speciesName), "%s",
                          species->name);
        }
        out.nature = monster.nature;
        std::snprintf(out.natureName, sizeof(out.natureName), "%s",
                      natureName(monster.nature));
        out.gender = monster.gender;
        out.level = monster.level;
        out.hp = monster.hpCur;
        out.hpMax = monster.hpMax;
        out.satiety = monster.satiety;
        out.mood = monster.mood;
    }
    return true;
}

#endif  // STICKMON_HAS_CLAW

bool AmoledApp::queueExploreDeparture(uint8_t area, bool autoWalk) {
    if (area >= Game::EXPLORE_AREA_COUNT || gameState.teamCount == 0) {
        return false;
    }
    if (!ExploreItemProgression::isAreaUnlocked(area, gameState)) {
        return false;
    }
    const uint8_t leader = ExploreSession::leaderSlot(gameState);
    if (!ExploreSession::healthy(gameState.team[leader])) {
        setToast(Ui::Menu::PET_REST, Platform::clock().millis());
        return false;
    }
    if (expeditionFade.active()) return false;
    expeditionFade.reset();
    pendingExpeditionArea = area;
    pendingExpeditionAutoWalk = autoWalk;
    pendingExpedition = true;
    exploreSessionActive = true;
    expeditionMainHidden = false;
    expeditionCompanionDeparting = false;
    expeditionCompanionHidden = false;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    const uint32_t nowMs = Platform::clock().millis();
    lockRequested = false;
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    // Attach/initialize the companion actor now. The departure phase machine
    // short-circuits updatePet (and therefore syncHomeActors), so without
    // this a freshly joined visitor is never active when the departure
    // evaluates expeditionCompanionDeparting and never walks to the door.
    syncHomeActors(nowMs);
    placeFaintedHomeActors(nowMs);
    visitorFlow_.cancelEntryForDeparture();
    lastInteractionMs = nowMs;
    // The departure animation always starts from the room, regardless of the
    // page that was visible when the request arrived.
    sceneFlow.goHome();
    requestFullRender();
    return true;
}

#if STICKMON_ENABLE_DEBUG_FEATURES
bool AmoledApp::debugBattleScenario(uint8_t scenario, uint32_t nowMs) {
    if (scenario == 0) {
        if (!debugBattleScenarioActive) return false;
        Platform::logLine("[BattleDiag] stop; rebooting to the saved game");
        Platform::power().restart();
        return true;
    }
    if (scenario > 12 || !gameState.oobeDone || visitSession.busy()) return false;
    if (!debugBattleScenarioActive) {
        if (sceneFlow.current() != AppSceneFlow::Scene::HOME &&
            sceneFlow.current() != AppSceneFlow::Scene::ROOM &&
            sceneFlow.current() != AppSceneFlow::Scene::DEBUG) return false;
        if (!saveState()) return false;
        debugBattleScenarioActive = true;
    }
    // Isolate deterministic fixtures from the player's persistent game.
    GameRandom::seed(12345);
    gameState.teamCount = 1;
    gameState.activeSlot = 0;
    gameState.team[0] = Game::MonsterFactory::create(scenario == 9 ? 129 : 4, 35);
    uint16_t wildSpecies = scenario == 6 ? 134 : scenario == 7 ? 25
        : scenario == 12 ? 92 : 7;
    Game::MoveId moveId = scenario == 5 ? 130 : scenario == 6 ? 55
        : scenario == 8 ? 256 : scenario == 9 ? 150 : scenario == 10 ? 525 : 10;
    const uint8_t specialSlot = scenario == 9 ? BattleSystem::SPECIAL_SLOT_NONE : 0;
    if (scenario != 9) gameState.team[0].move2Id = moveId;
    selectedExploreArea = 0;
    debugBattleActive = true;
    exploreSession_.battleClosed();
    if (!beginExploreEncounter(nowMs, true, wildSpecies, 35)) return false;
    battleSession_.configureDiagnostic(gameState, scenario, moveId, specialSlot);
    Platform::logf("[BattleDiag] scenario=%u start saves=%lu writes=blocked\n",
        scenario, static_cast<unsigned long>(diagnosticSaveCount));
    GameRandom::setRangeProvider([](uint32_t minimum, uint32_t) { return minimum; });
    if (scenario == 1) performBattleAttack(nowMs);
    else performBattlePlannedAction(nowMs);
    GameRandom::setRangeProvider(nullptr);
    return true;
}

void AmoledApp::debugReportSaveState() {
    const auto& route = exploreSession_.state();
    const auto& presentation = explorePresenter_.state();
    Platform::logf(
        "[ExploreState] now=%lu scene=%u area=%u block=%u index=%u steps=%u "
        "moving=%u paused=%u exiting=%u complete=%u world=(%.2f,%.2f) "
        "follower=(%.2f,%.2f) camera=(%d,%d) frames=%u/%u/%u\n",
        static_cast<unsigned long>(Platform::clock().millis()),
        unsigned(sceneFlow.current()), unsigned(selectedExploreArea),
        unsigned(route.exploreRouteMapBlock), unsigned(route.exploreRouteIndex),
        unsigned(route.exploreRouteSteps), unsigned(route.exploreRouteMoving),
        unsigned(route.exploreRoutePaused), unsigned(route.exploreRouteExiting),
        unsigned(route.exploreRouteComplete),
        static_cast<double>(presentation.exploreRouteWorldX),
        static_cast<double>(presentation.exploreRouteWorldY),
        static_cast<double>(presentation.exploreRouteFollowerWorldX),
        static_cast<double>(presentation.exploreRouteFollowerWorldY),
        int(presentation.exploreRouteCameraX), int(presentation.exploreRouteCameraY),
        unsigned(presentation.exploreRoutePetFrame),
        unsigned(presentation.exploreRouteFollowerFrame),
        unsigned(presentation.exploreRouteMapFrame));
    Game::GameState saved;
    MainSceneViewState savedView;
    const bool loaded = saveManager.load(saved, savedView);
    const auto report = [&](const char* source, const Game::GameState& state) {
        Platform::logf(
            "[SaveDiag] source=%s scene=%u phase=%u saves=%lu steps=%u "
            "coins=%lu team=%u storage=%u hp=%u/%u level=%u/%u potion=%u\n",
            source, static_cast<unsigned>(sceneFlow.current()),
            static_cast<unsigned>(battleSession_.state().battlePhase),
            static_cast<unsigned long>(diagnosticSaveCount), state.stepsToday,
            static_cast<unsigned long>(state.coins), state.teamCount,
            state.storageCount, state.team[0].hpCur, state.team[1].hpCur,
            state.team[0].level, state.team[1].level,
            Game::ItemInventory::count(state, Game::ItemId::POTION));
    };
    debugReportHomeState();
    report("ram", gameState);
    if (loaded) report("disk", saved);
    else Platform::logLine("[SaveDiag] source=disk unavailable");
}

void AmoledApp::debugReportHomeState() const {
    const uint32_t nowMs = Platform::clock().millis();
    const auto home = homeHouseholdObservation(nowMs);
    const auto selected = Home::ActorController::selectBowlActor(home);
    Platform::logf(
        "[HomeState] now=%lu scene=%u gameMinute=%lu bowl=%u bites=%u "
        "bowlOwner=%d bedOwner=%d clearing=%u preferred=%d "
        "pending=%u arbitrationIn=%ld pair=%u visitorMotion=%u roomAction=%u "
        "motion=%u sleeping=%u decisionIn=%ld\n",
        static_cast<unsigned long>(nowMs), static_cast<unsigned>(sceneFlow.current()),
        static_cast<unsigned long>(gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY),
        gameState.room.bowlCount, gameState.room.bowlBitesRemaining,
        static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
        static_cast<int>(homeRuntime.owner(Home::Resource::BED)),
        home.bowlClearing ? 1U : 0U, static_cast<int>(selected.actorId),
        homeFoodArbitrationPending ? 1U : 0U,
        static_cast<long>(static_cast<int32_t>(nextHomeFoodArbitrationMs - nowMs)),
        homeRuntime.pairActive() ? 1U : 0U, static_cast<unsigned>(visitorFlow_.state().motion),
        static_cast<unsigned>(homeFlow_.state().action), static_cast<unsigned>(petMotion()),
        (homeMainActor.task == Home::Task::SLEEPING) ? 1U : 0U,
        static_cast<long>(static_cast<int32_t>(homeMainActor.nextDecisionMs - nowMs)));
    for (uint8_t slot = 0; slot < home.actorCount; ++slot) {
        const auto& actor = home.actors[slot];
        const auto* runtime = homeRuntime.actor(slot);
        const auto& monster = gameState.team[slot];
        Platform::logf(
            "[HomeState] actor=%u species=%u satiety=%u hp=%u/%u "
            "task=%u resume=%u desire=%u intent=%u pos=%.1f,%.1f "
            "active=%u control=%u foodAction=%u canEat=%u "
            "canMove=%u needsFood=%u visitor=%u fainted=%u statusSleep=%u "
            "sleepTime=%u sleeping=%u wakeFood=%u retryReady=%u route=%u/%u\n",
            slot, monster.speciesId, monster.satiety, monster.hpCur, monster.hpMax,
            static_cast<unsigned>(actor.task), static_cast<unsigned>(actor.resumeTask),
            static_cast<unsigned>(actor.desire),
            static_cast<unsigned>(Home::ActorController::survivalIntent(actor, home.bowlHasFood)),
            runtime ? runtime->x : 0.0f, runtime ? runtime->y : 0.0f,
            actor.active ? 1U : 0U, actor.controlAvailable ? 1U : 0U,
            actor.foodActionAvailable ? 1U : 0U,
            Home::ActorController::foodEligible(actor, home.bowlHasFood) ? 1U : 0U,
            actor.canMove ? 1U : 0U, actor.needsFood ? 1U : 0U,
            actor.visitor ? 1U : 0U, actor.fainted ? 1U : 0U,
            actor.statusSleeping ? 1U : 0U, actor.sleepTime ? 1U : 0U,
            actor.sleeping ? 1U : 0U, actor.wakeForFood ? 1U : 0U,
            actor.foodRetryReady ? 1U : 0U,
            runtime ? runtime->route.index : 0U, runtime ? runtime->route.count : 0U);
    }
}

bool AmoledApp::debugStartExplore(uint8_t area) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        pendingExpedition || expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        gameState.teamCount == 0 || gameState.team[0].fainted ||
        gameState.team[0].hpCur == 0) return false;
    return queueExploreDeparture(area, true);
}

bool AmoledApp::debugTriggerContact(uint8_t kind) {
    if (kind < 1 || kind > 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    const uint32_t nowMs = Platform::clock().millis();
    if (!beginDebugContactEvent(kind, nowMs)) return false;
    onWake(nowMs);
    acceptDebugContact(nowMs, true);
    return debugContactActive;
}

bool AmoledApp::debugPromptContact(uint8_t kind) {
    if (kind < 1 || kind > 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    const uint32_t nowMs = Platform::clock().millis();
    if (!beginDebugContactEvent(kind, nowMs)) return false;
    onWake(nowMs);
    return true;
}

bool AmoledApp::debugAcceptContact() {
    if (!debugContactPending ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    acceptDebugContact(Platform::clock().millis(), true);
    return debugContactActive;
}

bool AmoledApp::debugStartPairTalk() {
    if (!debugContactActive ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        visitorFlow_.state().motion != VisitorMotion::ACTIVE) return false;
    const uint32_t nowMs = Platform::clock().millis();
    return startPairInteraction(nowMs, false, true);
}

bool AmoledApp::debugReturnContactExplore() {
    if (!debugContactActive || debugContactKind != 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return false;
    Platform::logf("[FriendDiag] return requested kind=3 team=%u\n",
                   static_cast<unsigned>(gameState.teamCount));
    leaveExploreRoute();
    return expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_OUT;
}
#endif

void AmoledApp::cancelExploreDeparture() {
    expeditionFade.reset();
    if (expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR) {
        homeMainActor.x = expeditionDoorInsideX;
        homeMainActor.y = expeditionDoorInsideY;
    }
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    if (expeditionCompanionDeparting) {
        homeRuntime.stop(1, Platform::clock().millis());
    }
    expeditionCompanionDeparting = false;
    pendingExpedition = false;
    exploreSessionActive = false;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    homeRuntime.stop(0, Platform::clock().millis());
    homeMainActor.motion = Home::MotionPhase::STILL;
    petStopping = false;
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    petFrame = 0;
    nextPetFrameMs = Platform::clock().millis() + 520;
    updateCamera();
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

bool AmoledApp::updateExploreDeparture(uint32_t nowMs) {
    if (!pendingExpedition &&
        expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
        return false;
    }
    expeditionFade.update(nowMs);
    const bool departingFromRoom =
        expeditionDeparturePhase == ExpeditionDeparturePhase::NONE ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_OUT;
    if (departingFromRoom && sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        sceneFlow.goHome();
        requestFullRender();
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_OUT) {
        if (!expeditionFade.complete()) {
            requestFullRender();
            return true;
        }
        completeExploreReturn();
        expeditionMainHidden = false;
        expeditionCompanionHidden = false;
        RoomResource& room = RoomResource::ins();
        const float outsideX = room.available()
            ? static_cast<float>(room.doorwayOutsideX()) : homeMainActor.x;
        const float outsideY = room.available()
            ? static_cast<float>(room.doorwayOutsideY()) : homeMainActor.y;
        expeditionDoorInsideX = room.available()
            ? static_cast<float>(room.doorwayInsideX()) : homeMainActor.x;
        expeditionDoorInsideY = room.available()
            ? static_cast<float>(room.doorwayInsideY()) : homeMainActor.y;
        const RoomResource::Point* walkPolygon = room.available()
            ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
        const uint8_t walkPolygonCount = room.available()
            ? room.walkPolygonCount()
            : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                                   sizeof(FALLBACK_WALK_POLYGON[0]));
        auto chooseInsidePose = [&](const Home::Actor& actor,
                                    float& x, float& y) {
            auto valid = [&](float candidateX, float candidateY) {
                return RoomMovementArea::containsFootprint(
                    walkPolygon, walkPolygonCount,
                    candidateX, candidateY,
                    actor.geometry.footprint);
            };
            if (valid(x, y)) return true;
            const float anchorX = x;
            const float anchorY = y;
            for (int radius = 2; radius <= 48; radius += 2) {
                for (int offset = -radius; offset <= radius; offset += 2) {
                    const float candidates[][2] = {
                        {anchorX + radius, anchorY + offset},
                        {anchorX - radius, anchorY + offset},
                        {anchorX + offset, anchorY + radius},
                        {anchorX + offset, anchorY - radius},
                    };
                    for (const auto& candidate : candidates) {
                        if (!valid(candidate[0], candidate[1])) continue;
                        x = candidate[0];
                        y = candidate[1];
                        return true;
                    }
                }
            }
            return false;
        };
        chooseInsidePose(homeMainActor,
                         expeditionDoorInsideX,
                         expeditionDoorInsideY);
        const bool mainFainted = gameState.team[0].fainted ||
                                 gameState.team[0].hpCur == 0;
        if (!mainFainted) {
            homeMainActor.faintRestActive = false;
            homeMainActor.x = outsideX;
            homeMainActor.y = outsideY;
            homeMainActor.targetX = expeditionDoorInsideX;
            homeMainActor.targetY = expeditionDoorInsideY;
            petDirection = petDirectionForDelta(
                homeMainActor.targetX - homeMainActor.x, homeMainActor.targetY - homeMainActor.y);
            homeMainActor.route.clear();
            homeMainActor.velocityX = 0.0f;
            homeMainActor.velocityY = 0.0f;
            homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
        }
        const bool hasCompanion = gameState.teamCount > 1 &&
            homeCompanionActor.active && gameState.team[1].speciesId != 0 &&
            !gameState.team[1].fainted && gameState.team[1].hpCur > 0;
        if (hasCompanion) {
            expeditionCompanionDoorInsideX = room.available()
                ? static_cast<float>(room.doorwayInsideX())
                : expeditionDoorInsideX;
            expeditionCompanionDoorInsideY = room.available()
                ? static_cast<float>(room.doorwayInsideY())
                : expeditionDoorInsideY;
            chooseInsidePose(homeCompanionActor,
                             expeditionCompanionDoorInsideX,
                             expeditionCompanionDoorInsideY);
            homeCompanionActor.x = outsideX;
            homeCompanionActor.y = outsideY;
            homeCompanionActor.targetX = expeditionCompanionDoorInsideX;
            homeCompanionActor.targetY = expeditionCompanionDoorInsideY;
            homeCompanionActor.route.clear();
            homeCompanionActor.velocityX = 0.0f;
            homeCompanionActor.velocityY = 0.0f;
            homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
            companionDirection = petDirectionForDelta(
                expeditionCompanionDoorInsideX - outsideX,
                expeditionCompanionDoorInsideY - outsideY);
            companionFrame = 0;
            nextCompanionFrameMs = nowMs;
        }
        expeditionCompanionDeparting = hasCompanion;
        placeFaintedHomeActors(nowMs);
        // Stick serializes the doorway: keep the companion hidden outside
        // until the leader has entered and cleared the door.
        expeditionCompanionHidden = hasCompanion;
        if (!mainFainted) {
            homeMainActor.motion = Home::MotionPhase::ROUTE;
            petStopping = false;
            petFrame = 0;
        }
        expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_FADE_IN;
        expeditionDoorPhaseStartedMs = nowMs;
        expeditionFade.beginIn();
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_WALK_IN) {
        const uint32_t elapsedMs = std::min<uint32_t>(nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float step = EXPLORE_DOOR_CROSS_SPEED * (elapsedMs / 1000.0f);

        auto moveTowardsDoor = [step](float& x, float& y,
                                       float targetX, float targetY) {
            const float dx = targetX - x;
            const float dy = targetY - y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= 0.8f || step >= distance) {
                x = targetX;
                y = targetY;
                return true;
            }
            if (step > 0.0f) {
                x += dx / distance * step;
                y += dy / distance * step;
            }
            return false;
        };

        bool mainArrived = moveTowardsDoor(
            homeMainActor.x, homeMainActor.y, homeMainActor.targetX, homeMainActor.targetY);
        if (!mainArrived &&
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            homeMainActor.x = homeMainActor.targetX;
            homeMainActor.y = homeMainActor.targetY;
            mainArrived = true;
            Platform::logLine(
                "[AmoledExplore] return leader cross timeout fallback");
        }
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (mainArrived && !expeditionCompanionDeparting) {
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            homeRuntime.stop(0, nowMs, 700);
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            exploreSessionActive = false;
            lastInteractionMs = nowMs;
            homeMainActor.mind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            Platform::logf(
                "[AmoledExplore] return entered main=%.1f,%.1f companion=0\n",
                homeMainActor.x, homeMainActor.y);
        } else if (mainArrived) {
            float inwardX = expeditionDoorInsideX - expeditionDoorOutsideX;
            float inwardY = expeditionDoorInsideY - expeditionDoorOutsideY;
            const float inwardLength = std::sqrt(
                inwardX * inwardX + inwardY * inwardY);
            if (inwardLength > 0.001f) {
                inwardX /= inwardLength;
                inwardY /= inwardLength;
            } else {
                inwardX = 0.0f;
                inwardY = -1.0f;
            }
            const float tangentX = -inwardY;
            const float tangentY = inwardX;
            const float separation = std::max(
                26.0f,
                homeMainActor.geometry.footprint.radiusX +
                    homeCompanionActor.geometry.footprint.radiusX + 8.0f);
            float clearX = homeMainActor.x;
            float clearY = homeMainActor.y;
            bool clearTargetFound = false;
            static constexpr float INWARD_OFFSETS[] = {14.0f, 20.0f, 28.0f};
            static constexpr float SIDE_SCALES[] = {1.0f, -1.0f, 1.25f, -1.25f};
            for (float inwardOffset : INWARD_OFFSETS) {
                for (float sideScale : SIDE_SCALES) {
                    const float candidateX = expeditionDoorInsideX +
                        inwardX * inwardOffset +
                        tangentX * separation * sideScale;
                    const float candidateY = expeditionDoorInsideY +
                        inwardY * inwardOffset +
                        tangentY * separation * sideScale;
                    if (!petFootprintInsideWalkArea(candidateX, candidateY)) {
                        continue;
                    }
                    clearX = candidateX;
                    clearY = candidateY;
                    clearTargetFound = true;
                    break;
                }
                if (clearTargetFound) break;
            }
            if (!clearTargetFound) {
                for (uint8_t attempt = 0; attempt < 8; ++attempt) {
                    float candidateX = homeMainActor.x;
                    float candidateY = homeMainActor.y;
                    if (!chooseWanderTarget(candidateX, candidateY, false)) {
                        continue;
                    }
                    const float dx = candidateX - expeditionDoorInsideX;
                    const float dy = candidateY - expeditionDoorInsideY;
                    if (dx * dx + dy * dy < separation * separation) continue;
                    clearX = candidateX;
                    clearY = candidateY;
                    clearTargetFound = true;
                    break;
                }
            }
            const bool clearRouteReady = clearTargetFound &&
                homeRuntime.planRoute(0, clearX, clearY, true, false) &&
                homeRuntime.transitionPreparedRoute(
                    0, Home::Task::DOOR_ACTION, nowMs, 0, true);
            homeMainActor.targetX = clearX;
            homeMainActor.targetY = clearY;
            expeditionDoorPhaseStartedMs = nowMs;
            if (clearRouteReady) {
                expeditionDeparturePhase =
                    ExpeditionDeparturePhase::RETURN_CLEAR_MAIN;
            } else {
                if (clearTargetFound) {
                    homeMainActor.x = clearX;
                    homeMainActor.y = clearY;
                    homeMainActor.x = clearX;
                    homeMainActor.y = clearY;
                }
                homeRuntime.stop(0, nowMs, 700);
                homeMainActor.motion = Home::MotionPhase::STILL;
                petStopping = false;
                expeditionCompanionHidden = false;
                expeditionDeparturePhase =
                    ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN;
                Platform::logLine(
                    "[AmoledExplore] return leader clearance fallback");
            }
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_CLEAR_MAIN) {
        const uint32_t elapsedMs = std::min<uint32_t>(
            nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float previousX = homeMainActor.x;
        const float previousY = homeMainActor.y;
        Home::RouteStep routeStep = homeRuntime.advanceRoute(
            0, nowMs, EXPLORE_DOOR_CLEAR_SPEED, elapsedMs / 1000.0f,
            1.0f, false);
        if (routeStep == Home::RouteStep::BLOCKED ||
            routeStep == Home::RouteStep::NO_ROUTE ||
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            homeMainActor.x = homeMainActor.targetX;
            homeMainActor.y = homeMainActor.targetY;
            homeMainActor.route.clear();
            routeStep = Home::RouteStep::ARRIVED;
            Platform::logLine(
                "[AmoledExplore] return leader clearance timeout fallback");
        }
        if (std::fabs(homeMainActor.x - previousX) > 0.01f ||
            std::fabs(homeMainActor.y - previousY) > 0.01f) {
            petDirection = petDirectionForDelta(
                homeMainActor.x - previousX, homeMainActor.y - previousY);
        }
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (routeStep == Home::RouteStep::ARRIVED) {
            homeRuntime.stop(0, nowMs, 700);
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            expeditionCompanionHidden = false;
            expeditionDeparturePhase =
                ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN;
            expeditionDoorPhaseStartedMs = nowMs;
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase ==
        ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN) {
        const uint32_t elapsedMs = std::min<uint32_t>(
            nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float step = EXPLORE_DOOR_CROSS_SPEED *
            (elapsedMs / 1000.0f);
        const float dx = homeCompanionActor.targetX - homeCompanionActor.x;
        const float dy = homeCompanionActor.targetY - homeCompanionActor.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        bool arrived = distance <= 0.8f || step >= distance;
        if (!arrived &&
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            arrived = true;
            Platform::logLine(
                "[AmoledExplore] return companion cross timeout fallback");
        }
        if (arrived) {
            homeCompanionActor.x = homeCompanionActor.targetX;
            homeCompanionActor.y = homeCompanionActor.targetY;
        } else if (distance > 0.0f && step > 0.0f) {
            homeCompanionActor.x += dx / distance * step;
            homeCompanionActor.y += dy / distance * step;
            companionDirection = petDirectionForDelta(dx, dy);
        }
        homeCompanionActor.velocityX = 0.0f;
        homeCompanionActor.velocityY = 0.0f;
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            companionFrame = static_cast<uint8_t>((companionFrame + 1) % 3);
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (arrived) {
            stopCompanion(nowMs, 700);
            expeditionCompanionDeparting = false;
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            exploreSessionActive = false;
            lastInteractionMs = nowMs;
            homeMainActor.mind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            schedulePairInteraction(nowMs, true);
            Platform::logf(
                "[AmoledExplore] return entered main=%.1f,%.1f companion=1\n",
                homeMainActor.x, homeMainActor.y);
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_IN) {
        if (expeditionFade.complete()) {
            expeditionFade.reset();
            const bool mainFainted = gameState.team[0].fainted ||
                                     gameState.team[0].hpCur == 0;
            if (mainFainted) {
                expeditionCompanionHidden = false;
                expeditionDeparturePhase = expeditionCompanionDeparting
                    ? ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN
                    : ExpeditionDeparturePhase::NONE;
                if (!expeditionCompanionDeparting) {
                    exploreSessionActive = false;
                    lastInteractionMs = nowMs;
                }
            } else {
                expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_WALK_IN;
            }
            lastPetUpdateMs = nowMs;
            nextPetFrameMs = nowMs;
            expeditionDoorPhaseStartedMs = nowMs;
        }
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_OUT) {
        if (!expeditionFade.complete()) {
            requestFullRender();
            return true;
        }

        uint8_t area = pendingExpeditionArea;
        bool autoWalk = pendingExpeditionAutoWalk;
        bool started = false;
        if (area < Game::EXPLORE_AREA_COUNT) {
            if (area != selectedExploreArea) selectExploreArea(area, nowMs);
            started = startExploreRoute(nowMs);
        }
        pendingExpedition = false;
        exploreSession_.setAutoWalk(started && autoWalk);
        autonomousExpedition = started && autoWalk;
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugContactActive && debugContactKind == 3) {
            Platform::logf("[FriendDiag] route kind=3 started=%u team=%u auto=%u\n",
                           started ? 1U : 0U,
                           static_cast<unsigned>(gameState.teamCount),
                           autoWalk ? 1U : 0U);
        }
#endif
        // Start at black after loading, including the failure path back to home.
        expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_IN;
        expeditionFade.beginIn();
        if (!started) {
            expeditionMainHidden = false;
            expeditionCompanionHidden = false;
            homeMainActor.x = homeMainActor.targetX = expeditionDoorInsideX;
            homeMainActor.y = homeMainActor.targetY = expeditionDoorInsideY;
            updateCamera();
        }
        Platform::logf(
            "[AmoledExplore] departure complete area=%u route=%u auto=%u\n",
            static_cast<unsigned>(area), started ? 1U : 0U,
            autoWalk ? 1U : 0U);
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_IN) {
        if (expeditionFade.complete()) {
            expeditionFade.reset();
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            lastInteractionMs = nowMs;
            if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
                exploreSessionActive = false;
            }
        }
        requestFullRender();
        return true;
    }

    auto startCompanionDeparture = [&]() {
        homeCompanionActor.targetX = expeditionCompanionDoorInsideX;
        homeCompanionActor.targetY = expeditionCompanionDoorInsideY;
        homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
        homeCompanionActor.route.clear();
        companionDirection = petDirectionForDelta(
            homeCompanionActor.targetX - homeCompanionActor.x,
            homeCompanionActor.targetY - homeCompanionActor.y);
        companionFrame = 0;
        nextCompanionFrameMs = nowMs;
        lastPetUpdateMs = nowMs;
        expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    };

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
        RoomResource& room = RoomResource::ins();
        if (!room.available() || room.doorwayPolygonCount() < 3) {
            // A malformed room pack cannot provide a doorway path, but the
            // scene transition must still remain coherent.
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
            return true;
        }
        expeditionDoorInsideX = static_cast<float>(room.doorwayInsideX());
        expeditionDoorInsideY = static_cast<float>(room.doorwayInsideY());
        expeditionDoorOutsideX = static_cast<float>(room.doorwayOutsideX());
        expeditionDoorOutsideY = static_cast<float>(room.doorwayOutsideY());
        expeditionCompanionDeparting = gameState.teamCount > 1 &&
            homeCompanionActor.active && gameState.team[1].speciesId != 0 &&
            !gameState.team[1].fainted && gameState.team[1].hpCur > 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf(
            "[HomeDiag] departure companion departing=%d team=%u active=%d "
            "species=%u fainted=%d hp=%u visitorMotion=%d\n",
            expeditionCompanionDeparting ? 1 : 0,
            static_cast<unsigned>(gameState.teamCount),
            homeCompanionActor.active ? 1 : 0,
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].speciesId) : 0U,
            gameState.teamCount > 1 && gameState.team[1].fainted ? 1 : 0,
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].hpCur) : 0U,
            static_cast<int>(visitorFlow_.state().motion));
#endif
        if (expeditionCompanionDeparting) {
            expeditionCompanionDoorInsideX = expeditionDoorInsideX;
            expeditionCompanionDoorInsideY = expeditionDoorInsideY;
            expeditionCompanionDoorOutsideX = expeditionDoorOutsideX;
            expeditionCompanionDoorOutsideY = expeditionDoorOutsideY;
            homeRuntime.transition(
                1, Home::Task::DOOR_ACTION, nowMs, 0, true);
        }
        if (!ExploreSession::healthy(gameState.team[0])) {
            // The leader stays in its faint-rest pose. Only the healthy
            // companion participates in the doorway animation.
            if (expeditionCompanionDeparting) {
                startCompanionDeparture();
            } else {
                cancelExploreDeparture();
                placeFaintedHomeActors(nowMs);
                setToast(Ui::Menu::PET_REST, nowMs);
            }
            return true;
        }
        homeMainActor.faintRestActive = false;
        homeMainActor.taskUntilMs = 0;
        nextFeedBiteMs = 0;
        homeMainActor.targetX = expeditionDoorInsideX;
        homeMainActor.targetY = expeditionDoorInsideY;
        homeMainActor.route.clear();
        homeMainActor.velocityX = 0.0f;
        homeMainActor.velocityY = 0.0f;
        homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
        petDirection = petDirectionForDelta(homeMainActor.targetX - homeMainActor.x,
                                            homeMainActor.targetY - homeMainActor.y);
        petLongMove = true;
        homeMainActor.motion = Home::MotionPhase::ROUTE;
        petStopping = false;
        petFrame = 0;
        expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_TO_DOOR;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        Platform::logf("[AmoledExplore] departure started area=%u from=%.1f,%.1f door=%.1f,%.1f\n",
                       static_cast<unsigned>(pendingExpeditionArea), homeMainActor.x, homeMainActor.y,
                       expeditionDoorInsideX, expeditionDoorInsideY);
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR) {
        const float speed = expeditionDeparturePhase ==
            ExpeditionDeparturePhase::CROSS_COMPANION_DOOR ? 42.0f : 30.0f;
        uint32_t elapsedMs = nowMs - lastPetUpdateMs;
        if (lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs) elapsedMs = 0;
        lastPetUpdateMs = nowMs;
        float step = speed * (std::min<uint32_t>(elapsedMs, 120) / 1000.0f);
        float dx = homeCompanionActor.targetX - homeCompanionActor.x;
        float dy = homeCompanionActor.targetY - homeCompanionActor.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        bool reached = distance <= 0.8f || step >= distance;
        if (reached) {
            homeCompanionActor.x = homeCompanionActor.targetX;
            homeCompanionActor.y = homeCompanionActor.targetY;
            homeCompanionActor.velocityX = homeCompanionActor.velocityY = 0.0f;
            if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR) {
                homeCompanionActor.targetX = expeditionCompanionDoorOutsideX;
                homeCompanionActor.targetY = expeditionCompanionDoorOutsideY;
                companionDirection = petDirectionForDelta(
                    homeCompanionActor.targetX - homeCompanionActor.x,
                    homeCompanionActor.targetY - homeCompanionActor.y);
                expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_COMPANION_DOOR;
                companionFrame = 0;
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
                return true;
            }
            expeditionCompanionHidden = true;
            homeCompanionActor.route.clear();
            expeditionCompanionDeparting = false;
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
            return true;
        }
        if (step > 0.0f) {
            homeCompanionActor.x += dx / distance * step;
            homeCompanionActor.y += dy / distance * step;
            homeCompanionActor.velocityX = dx / distance * speed;
            homeCompanionActor.velocityY = dy / distance * speed;
            companionDirection = petDirectionForDelta(dx, dy);
        }
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            companionFrame = static_cast<uint8_t>((companionFrame + 1) % 3);
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        updateCamera();
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }

    const float speed = expeditionDeparturePhase ==
        ExpeditionDeparturePhase::CROSS_DOOR ? 42.0f : 30.0f;
    uint32_t elapsedMs = nowMs - lastPetUpdateMs;
    if (lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs) elapsedMs = 0;
    lastPetUpdateMs = nowMs;
    float step = speed * (std::min<uint32_t>(elapsedMs, 120) / 1000.0f);
    float dx = homeMainActor.targetX - homeMainActor.x;
    float dy = homeMainActor.targetY - homeMainActor.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.8f || step >= distance) {
        homeMainActor.x = homeMainActor.targetX;
        homeMainActor.y = homeMainActor.targetY;
        if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_TO_DOOR) {
            homeMainActor.targetX = expeditionDoorOutsideX;
            homeMainActor.targetY = expeditionDoorOutsideY;
            petDirection = petDirectionForDelta(homeMainActor.targetX - homeMainActor.x,
                                                homeMainActor.targetY - homeMainActor.y);
            expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_DOOR;
            petFrame = 0;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return true;
        }

        expeditionMainHidden = true;
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        if (expeditionCompanionDeparting) {
            startCompanionDeparture();
#if STICKMON_ENABLE_DEBUG_FEATURES
            Platform::logf("[HomeDiag] companion walk-to-door from=%.1f,%.1f to=%.1f,%.1f\n",
                           homeCompanionActor.x, homeCompanionActor.y,
                           homeCompanionActor.targetX,
                           homeCompanionActor.targetY);
#endif
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else {
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
        }
        return true;
    }
    if (distance > 0.0f && step > 0.0f) {
        homeMainActor.x += dx / distance * step;
        homeMainActor.y += dy / distance * step;
    }
    // Departure bypasses the normal home update loop, so keep the actor used
    // by RenderSnapshot in lockstep with the presentation coordinates.
    homeMainActor.velocityX = distance > 0.0f
        ? dx / distance * speed : 0.0f;
    homeMainActor.velocityY = distance > 0.0f
        ? dy / distance * speed : 0.0f;
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    updateCamera();
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::beginExploreReturn(uint32_t nowMs) {
    if (expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return;
    lockRequested = false;
    lastInteractionMs = nowMs;
    // Close route menus before fading; never expose the area selector on return.
    sceneFlow.enterExploreRoute();
    expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_FADE_OUT;
    expeditionFade.beginOut();
    autonomousExpedition = false;
    exploreSession_.stopRoute();
    toast = nullptr;
    requestFullRender();
}

#if STICKMON_HAS_CLAW

bool AmoledApp::brainStartExpedition(uint8_t area) {
    if (gameResetPending || !gameState.oobeDone) return false;
    if (gameState.teamCount == 0) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：队伍为空 area=%u", static_cast<unsigned>(area));
        return false;
    }
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(gameState);
    if (!ExploreItemProgression::isAreaUnlocked(area, gameState)) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：区域未解锁 requested=%u unlocked=%u",
            static_cast<unsigned>(area), static_cast<unsigned>(unlockedArea));
        return false;
    }
    AppSceneFlow::Scene scene = sceneFlow.current();
    if (scene == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        scene == AppSceneFlow::Scene::EXPLORE_MENU ||
        scene == AppSceneFlow::Scene::BATTLE ||
        scene == AppSceneFlow::Scene::COMMUNICATION) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：当前场景不可用 scene=%u area=%u",
            static_cast<unsigned>(scene), static_cast<unsigned>(area));
        return false;
    }
    // All Agent-triggered departures use the same visible home-room entry
    // point. This also handles a request that arrives while a utility page is
    // open: the page is closed first, then the pet walks to the doorway.
    return queueExploreDeparture(area, true);
}

bool AmoledApp::brainReturnHome() {
    if (gameResetPending || !gameState.oobeDone) return false;
    AppSceneFlow::Scene scene = sceneFlow.current();
    bool exploring = scene == AppSceneFlow::Scene::EXPLORE_ROUTE ||
                     scene == AppSceneFlow::Scene::EXPLORE_MENU ||
                     scene == AppSceneFlow::Scene::BATTLE;
    if (!exploring) return false;
    if (scene == AppSceneFlow::Scene::BATTLE) {
        closeBattle(Platform::clock().millis());
    }
    autonomousExpedition = false;
    leaveExploreRoute();
    return true;
}

bool AmoledApp::brainInviteFriend() {
    if (gameResetPending || !gameState.oobeDone) return false;
    if (gameState.teamCount == 0 ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
        visitSession.busy()) {
        return false;
    }
    communicationReturnToComputer = false;
    visitSession.attach(&gameState);
    startVisitHost();
    if (visitSession.state() !=
        Communication::VisitSessionService::State::HOSTING) {
        return false;
    }
    sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
    requestFullRender();
    return true;
}

bool AmoledApp::brainEat() {
    if (gameResetPending || !gameState.oobeDone) return false;
    if (gameState.teamCount == 0 ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::SHOWER ||
        sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION ||
        visitSession.busy()) {
        return false;
    }
    Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0 || monster.satiety >= 100) {
        return false;
    }
    if (gameState.room.bowlCount == 0 &&
        Game::HomeCare::placeSelectedFoodInBowl(gameState) !=
            FoodPlacementResult::ADDED) {
        return false;
    }

    bool consumed = false;
    for (uint8_t bite = 0;
         bite < Game::ROOM_NORMAL_FOOD_BITES &&
         gameState.room.bowlCount > 0 && monster.satiety < 100;
         ++bite) {
        const auto result = Game::HomeCare::consumeBowlFood(gameState, 0);
        if (result.consumed) {
            consumed = true;
            applyHomeCareExperience(0, result.careExp);
        }
    }
    if (!consumed) return false;
    homeMainActor.mind.onAte(Platform::clock().millis());
    saveState();
    requestFullRender();
    return true;
}

bool AmoledApp::brainBuyFood(uint8_t foodIndex) {
    if (gameResetPending || !gameState.oobeDone) return false;
    if (foodIndex >= Game::ROOM_FOOD_COUNT ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::SHOWER ||
        sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION ||
        visitSession.busy()) {
        return false;
    }
    Game::ItemId item = Game::itemIdForFoodIndex(foodIndex);
    if (item == Game::ItemId::COUNT ||
        Game::ShopService::buy(gameState, item) !=
            Game::ShopService::BuyResult::BOUGHT) {
        return false;
    }
    saveState();
    requestFullRender();
    return true;
}

bool AmoledApp::brainSay(const char* text) {
    if (gameResetPending || !gameState.oobeDone) return false;
    if (!text || !text[0]) return false;
    std::snprintf(brainMessage, sizeof(brainMessage), "%s", text);
    setToast(brainMessage, Platform::clock().millis(), 5000);
    requestFullRender();
    return true;
}
#endif

void AmoledApp::cancelTouch() {
    // Losing pointer capture must not turn a cancelled gesture into a tap.
    pointerDown = dragging = false;
    pressedMenuItem = pressedExploreArea = -1;
    exploreDragStartArea = pressedExploreMenuItem = -1;
    inventoryController_.cancelTouch();
    teamController_.cancelTouch();
    pressedRoomItem = pressedCommunicationItem = -1;
    contactActionPressedItem = -1;
    computerPressedItem = 0xFF;
    battleSession_.press(0xFF);
    hatchController_.cancelTouch();
    settingsController.cancelTouch();
    showerController_.finishTouch();
    progressionFlow_.cancelTouch();
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugPressedItem = -1;
#endif
    requestFullRender();
}

void AmoledApp::handleTouch(const TouchEvent& physicalEvent) {
    if (gameResetPending) return;
    if (inventoryController_.state().teamItemFeedbackActive) {
        pointerDown = false;
        dragging = false;
        teamController_.pressMember(-1);
        return;
    }
    // TouchInput has already normalized the controller sample at the
    // hardware boundary. The application consumes one stable page-space
    // event and performs no second coordinate conversion.
    const TouchEvent& event = physicalEvent;
    if (event.type == TouchEventType::DOWN) {
        // A real interaction supersedes an idle request that the display task
        // has not consumed yet. Explicit lock taps set a fresh request on UP.
        lockRequested = false;
        lastInteractionMs = event.timestampMs;
        if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
            event.y >= HOME_STATUS_TOP && toast &&
            static_cast<int32_t>(event.timestampMs - toastUntil) < 0) {
            pointerDown = dragging = false;
            return;
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::HATCH) {
        handleHatchTouch(event);
        return;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
        debugCategory == DebugViewModel::Category::TOUCH_TEST) {
        lastInteractionMs = event.timestampMs;
        if (event.type != TouchEventType::MOVE) {
            Platform::logf("[TouchRaw] event=%s raw=%d,%d mapped=%d,%d clipped=%d\n",
                event.type == TouchEventType::DOWN ? "DOWN" : "UP",
                event.rawX, event.rawY, event.x, event.y,
                (event.rawX != event.x || event.rawY != event.y) ? 1 : 0);
        }
        handleDebugTouchTest(event);
        return;
    }
    if (event.type == TouchEventType::DOWN && debugTouchDisplayEnabled) {
        debugTouchX = std::clamp<int16_t>(event.x, 0, AmoledUi::WIDTH - 1);
        debugTouchY = std::clamp<int16_t>(event.y, 0, AmoledUi::HEIGHT - 1);
        debugTouchPointValid = true;
        Platform::logf("[TouchDisplay] x=%d y=%d\n", debugTouchX,
                       debugTouchY);
        requestFullRender();
    }
#endif
    // Temporary shop diagnostics also run in release builds. Skip MOVE to
    // avoid serial traffic affecting touch responsiveness.
    const bool shopTouch = sceneFlow.current() == AppSceneFlow::Scene::SHOP;
    if (shopTouch && event.type != TouchEventType::MOVE) {
        Platform::logf(
            "[ShopTouch] t=%lu event=%s raw=%d,%d ui=%d,%d hit=%d "
            "detail=%d item=%d pointer=%d dragging=%d fade=%d\n",
            static_cast<unsigned long>(event.timestampMs),
            event.type == TouchEventType::DOWN ? "DOWN" : "UP",
            physicalEvent.x, physicalEvent.y, event.x, event.y,
            itemConfirmChoiceAt(event.x, event.y), inventoryController_.state().itemConfirmOpen,
            static_cast<int>(inventoryController_.state().pendingItem), pointerDown, dragging,
            static_cast<int>(expeditionDeparturePhase));
    }
    if (expeditionFade.active() ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_WALK_IN ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_CLEAR_MAIN ||
        expeditionDeparturePhase ==
            ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN) {
        if (shopTouch && event.type != TouchEventType::MOVE) {
            Platform::logLine("[ShopTouch] rejected=departure_fade");
        }
        return;
    }
    switch (event.type) {
    case TouchEventType::DOWN:
        if (pendingExpedition ||
            expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
            cancelExploreDeparture();
        }
#if STICKMON_HAS_CLAW
        // A physical touch immediately returns control to the player. This
        // also prevents an Agent-started route from resuming after a menu
        // visit or a touch on the route screen.
        autonomousExpedition = false;
        exploreSession_.setAutoWalk(false);
#endif
        // A touch cancels the current one-shot player walk. A route tap below
        // may start a new walk-to-interaction command.
        exploreSession_.cancelPlayerWalk();
        pointerDown = true;
        lastInteractionMs = event.timestampMs;
        dragging = false;
        touchStartX = event.x;
        touchStartY = event.y;
        touchLastY = event.y;
        menuVelocity = 0.0f;
        if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU) {
            pressedMenuItem = mainMenuItemAt(event.x, event.y, menuScroll);
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
            debugVelocity = 0.0f;
            if (debugPopup == DebugViewModel::Popup::NONE) {
                debugPressedItem = debugItemAt(
                    event.x, event.y, debugCategory, debugScroll);
            }
            requestRenderRows(0, 448);
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
            pressedExploreArea = exploreAreaAt(
                event.x, event.y, selectedExploreArea,
                ExploreItemProgression::visibleAreaCount(gameState));
            exploreDragStartArea = selectedExploreArea;
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
            pressedExploreMenuItem = exploreRouteMenuItemAt(event.x, event.y);
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamStatusOpen) {
            teamController_.interruptStatusSlide();
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamMovesOpen &&
                   event.y >= TEAM_MOVES_HEADER_HEIGHT && event.y < 448) {
            teamController_.endMovesGesture();
            requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamController_.state().teamConfirmOpen && !teamController_.state().teamActionPopupOpen && event.y >= MENU_HEADER_HEIGHT) {
            teamController_.pressMember(teamMemberAt(
                event.x, event.y,
                Game::TeamRoster::memberCount(gameState)));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
            pressedRoomItem = event.y >= MENU_HEADER_HEIGHT
                ? roomMenuItemAt(event.x, event.y) : -1;
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedRoomItem = roomFoodItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() ==
                       AppSceneFlow::Scene::COMMUNICATION &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedCommunicationItem = communicationItemAt(
                event.x, event.y, visitSession.viewModel());
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   event.y >= MENU_HEADER_HEIGHT) {
            battleSession_.press(battleSession_.logBusy()
                ? 0xFF : static_cast<uint8_t>(std::max(
                    -1, battleItemAt(event.x, event.y, battleSession_.state().battlePhase))));
            requestRenderRows(352, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   event.y >= MENU_HEADER_HEIGHT) {
            if (computerPage == ComputerViewModel::Page::STORAGE &&
                (contactActionOpen || contactConfirmOpen)) {
                computerPressedItem = 0xFF;
                contactActionPressedItem = contactActionOpen
                    ? computerContactActionItemAt(
                          event.x, event.y, contactActionCount(contactActionSlot),
                          contactActionSlot, computerScroll)
                    : -1;
                requestRenderRows(MENU_HEADER_HEIGHT, AmoledUi::HEIGHT);
                break;
            }
            if (computerPage == ComputerViewModel::Page::STATUS) {
                teamController_.endStatusGesture();
                requestRenderRows(0, 448);
                break;
            }
            if (computerPage == ComputerViewModel::Page::STORAGE) {
                computerVelocity = 0.0f;
            }
#if STICKMON_HAS_CLAW
            if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
                clawLogVelocity = 0.0f;
            }
#endif
            bool aiClawEnabled = false;
#if STICKMON_HAS_CLAW
            aiClawEnabled = Stickmon::ClawRuntime::instance().enabled();
#endif
            computerPressedItem = static_cast<uint8_t>(std::max(
                -1, computerItemAt(event.x, event.y, computerPage,
                                   computerScroll, gameState.storageCount,
                                   aiClawEnabled)));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS &&
                   event.y >= MENU_HEADER_HEIGHT) {
            applySettingsChange(settingsController.beginTouch(
                event.x, event.y, systemUpdateHandler != nullptr, gameState.settings));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
            progressionFlow_.beginTouch(event.x, event.y, event.timestampMs);
            applyProgressionEffects(event.timestampMs);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
            if (showerController_.beginTouch(event.x, event.y)) dragging = true;
            applyShowerEffects(event.timestampMs);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   event.y >= MENU_HEADER_HEIGHT) {
            inventoryController_.stopScroll();
            if (inventoryController_.state().itemConfirmOpen) {
                inventoryController_.beginDetailTouch(itemConfirmChoiceAt(event.x, event.y), gameState);
            } else {
                ShopViewModel::Mode mode =
                    inventoryController_.state().shopCategory == Game::ShopService::Category::SELL
                        ? ShopViewModel::Mode::SELL
                        : ShopViewModel::Mode::BUY;
                inventoryController_.beginListTouch(shopGridItemAt(
                    event.x, event.y, inventoryController_.state().itemScroll, mode,
                    shopDailyItemCount(), shopExploreItemCount(), currentItemCount()),
                    shopMenuItemAt(event.x, event.y));
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BAG &&
                   !inventoryController_.state().itemConfirmOpen && event.y >= MENU_HEADER_HEIGHT) {
            inventoryController_.stopScroll();
            const uint8_t dailyCount =
                Game::ItemInventory::homeBagDailyItemCount(gameState);
            inventoryController_.beginListTouch(itemListItemAt(
                event.x, event.y, inventoryController_.state().itemScroll, dailyCount,
                Game::ItemInventory::homeBagExploreItemCount(gameState),
                currentItemCount(), false));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
        break;

    case TouchEventType::MOVE:
        if (!pointerDown) break;
        if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
            if (showerController_.state().showerToolDragging) {
                updateShowerToolDrag(event.x, event.y, event.timestampMs);
            } else if (std::max(std::abs(event.x - touchStartX),
                                std::abs(event.y - touchStartY)) > TAP_SLOP) {
                showerController_.clearPress();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS &&
                   settingsController.sliderDragging()) {
            applySettingsChange(settingsController.dragSlider(event.x, gameState.settings));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedMenuItem = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                menuScroll -= static_cast<float>(deltaY);
                menuVelocity = static_cast<float>(-deltaY);
                clampMenuScroll();
                requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
            }
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
                   debugPopup == DebugViewModel::Popup::NONE) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                debugPressedItem = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                debugScroll -= static_cast<float>(deltaY);
                debugVelocity = static_cast<float>(-deltaY);
                clampDebugScroll();
                requestRenderRows(0, 448);
            }
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   touchStartY < EXPLORE_SELECTOR_TOP_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                // Scene selection is a horizontal carousel. A vertical move
                // still cancels the press, but does not select an area.
                if (std::abs(event.x - touchStartX) <= DRAG_START_SLOP) {
                    pressedExploreArea = -1;
                    dragging = true;
                    requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
                    break;
                }
            }
            if (std::abs(event.x - touchStartX) > DRAG_START_SLOP) {
                dragging = true;
                pressedExploreArea = -1;
            }
            if (dragging) {
                int count = std::min<int>(
                    ExploreItemProgression::visibleAreaCount(gameState),
                    Game::EXPLORE_AREA_COUNT);
                int startArea = exploreDragStartArea >= 0
                    ? exploreDragStartArea : selectedExploreArea;
                int candidate = startArea + static_cast<int>(std::lround(
                    (event.x - touchStartX) /
                    static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
                candidate = std::clamp(candidate, 0, std::max(0, count - 1));
                if (candidate != selectedExploreArea) {
                    selectExploreArea(static_cast<uint8_t>(candidate),
                                      event.timestampMs);
                }
                requestRenderRows(0, EXPLORE_PREVIEW_RENDER_BOTTOM);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamMovesOpen &&
                   touchStartY >= TEAM_MOVES_HEADER_HEIGHT &&
                   touchStartY < 448) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                teamController_.beginMovesDrag();
                dragging = true;
            }
            if (teamController_.state().teamMovesDragging) {
                teamController_.dragMoves(event.y - touchLastY);
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
                   progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE &&
                   touchStartY >= TEAM_MOVES_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                progressionFlow_.clearPress();
            }
            if (dragging) {
                progressionFlow_.dragReplacement(event.y - touchLastY);
                applyProgressionEffects(event.timestampMs);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedExploreMenuItem >= 0) {
            pressedExploreMenuItem = -1;
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamStatusOpen) {
            const int deltaX = event.x - touchStartX;
            const int deltaY = event.y - touchStartY;
            if (!teamController_.state().teamStatusDragging &&
                std::abs(deltaX) > DRAG_START_SLOP &&
                std::abs(deltaX) > std::abs(deltaY)) {
                teamController_.beginStatusDrag();
                dragging = true;
            }
            if (teamController_.state().teamStatusDragging) {
                const int previous = teamController_.state().teamStatusSlideX;
                teamController_.dragStatus(deltaX);
                if (previous != teamController_.state().teamStatusSlideX) {
                    requestRenderRows(MENU_HEADER_HEIGHT, 448);
                }
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamController_.state().teamConfirmOpen && !teamController_.state().teamActionPopupOpen &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   teamController_.state().pressedTeamSlot >= 0) {
            teamController_.pressMember(-1);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STATUS) {
            const int deltaX = event.x - touchStartX;
            const int deltaY = event.y - touchStartY;
            if (!teamController_.state().teamStatusDragging &&
                std::abs(deltaX) > DRAG_START_SLOP &&
                std::abs(deltaX) > std::abs(deltaY)) {
                teamController_.beginStatusDrag();
                dragging = true;
            }
            if (teamController_.state().teamStatusDragging) {
                teamController_.dragStatus(deltaX);
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STORAGE &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (contactActionOpen || contactConfirmOpen) {
                computerPressedItem = 0xFF;
                break;
            }
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                computerPressedItem = 0xFF;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                computerScroll -= static_cast<float>(deltaY);
                computerVelocity = static_cast<float>(-deltaY);
                clampComputerScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        }
#if STICKMON_HAS_CLAW
        else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                 computerPage == ComputerViewModel::Page::CLAW_SETUP &&
                 clawLogView && touchStartY >= CLAW_LOG_TOP) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                clawLogScroll -= static_cast<float>(deltaY);
                clawLogVelocity = static_cast<float>(-deltaY);
                clawLogPinned = false;
                clampClawLogScroll();
                requestRenderRows(CLAW_LOG_TOP, 448);
            }
        }
#endif
        else if ((sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD ||
                    sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
                    sceneFlow.current() == AppSceneFlow::Scene::COMPUTER ||
                    sceneFlow.current() == AppSceneFlow::Scene::SETTINGS ||
                    sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP) {
            if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
                pressedRoomItem = -1;
            } else if (sceneFlow.current() ==
                       AppSceneFlow::Scene::COMMUNICATION) {
                pressedCommunicationItem = -1;
            } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
                computerPressedItem = 0xFF;
            } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
                settingsController.invalidatePress();
            } else {
                progressionFlow_.clearPress();
            }
            // Growth animation also changes above the shared page header.
            requestRenderRows(sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION
                                  ? 0 : MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP) {
            battleSession_.press(0xFF);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                    sceneFlow.current() == AppSceneFlow::Scene::SHOP) &&
                   !inventoryController_.state().itemConfirmOpen &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                           touchStartX >= SHOP_LEFT_PANEL_WIDTH;
                inventoryController_.cancelTouch();
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                inventoryController_.dragList(deltaY, itemMaxScroll());
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        }
        touchLastY = event.y;
        break;

    case TouchEventType::UP: {
        const bool releasedShowerTool = sceneFlow.current() == AppSceneFlow::Scene::SHOWER && showerController_.state().showerToolDragging;
        if (!pointerDown) {
            if (shopTouch) Platform::logLine("[ShopTouch] rejected=no_down");
            break;
        }
        int distance = std::max(std::abs(event.x - touchStartX),
                                std::abs(event.y - touchStartY));
        const auto progressionRelease = sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION
            ? progressionFlow_.prepareRelease(event.timestampMs) : ProgressionFlow::Release{};
        applyProgressionEffects(event.timestampMs);
        const bool evolutionTouchWasConsumed = progressionRelease.consumed;
        bool settingsSliderWasDragging = settingsController.sliderDragging();
        if (settingsSliderWasDragging) {
            if (shopTouch) {
                Platform::logLine("[ShopTouch] intercepted=settings_slider");
            }
            applySettingsChange(settingsController.dragSlider(event.x, gameState.settings));
            if (settingsController.sliderChanged()) {
                saveState();
                if (settingsController.pressedItem() == 0) {
                    setToast(Ui::Settings::BRIGHTNESS_CHANGED,
                             event.timestampMs);
                } else if (settingsController.pressedItem() == 1) {
                    setToast(Ui::Settings::VOLUME_CHANGED,
                             event.timestampMs);
                }
            }
        } else if (evolutionTouchWasConsumed) {
            // A touch that began while evolution was running cannot cross the
            // completion boundary and acknowledge the completed evolution.
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
                   progressionFlow_.state().progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
                   !progressionFlow_.state().progressionLevelUpTouchAllowed) {
            // A gesture begun during the entrance animation stays consumed.
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !inventoryController_.state().selectingItemTarget &&
                   teamController_.state().pressedTeamSlot >= 0 && !dragging && distance <= TAP_SLOP) {
            teamController_.showActions(static_cast<uint8_t>(teamController_.state().pressedTeamSlot));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   inventoryController_.state().itemConfirmOpen) {
            // Detail buttons are not scrollable. A release inside the same
            // button remains a tap even if the finger drifted beyond TAP_SLOP.
            const int startChoice = itemConfirmChoiceAt(touchStartX, touchStartY);
            const int endChoice = itemConfirmChoiceAt(event.x, event.y);
            const char* decision = dragging ? "rejected=dragging"
                : startChoice < 0 ? "rejected=start_outside"
                : endChoice < 0 ? "rejected=end_outside"
                : startChoice != endChoice ? "rejected=different_button"
                : "accepted";
            Platform::logf(
                "[ShopTouch] %s start=%d,%d start_hit=%d end_hit=%d "
                "distance=%d\n",
                decision, touchStartX, touchStartY, startChoice, endChoice,
                distance);
            if (!dragging && startChoice >= 0 &&
                startChoice == itemConfirmChoiceAt(event.x, event.y)) {
                handleTap(event.x, event.y, event.timestampMs);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamStatusDragging) {
            teamController_.releaseStatus(event.timestampMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STATUS &&
                   teamController_.state().teamStatusDragging) {
            teamController_.releaseStatus(event.timestampMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamController_.state().teamMovesDragging) {
            teamController_.endMovesGesture();
        } else if (!dragging && distance <= TAP_SLOP) {
            if (shopTouch) Platform::logLine("[ShopTouch] list_tap=accepted");
            handleTap(event.x, event.y, event.timestampMs);
        } else if (shopTouch) {
            Platform::logf("[ShopTouch] list_tap=rejected dragging=%d distance=%d\n",
                           dragging, distance);
        }
        pointerDown = false;
        dragging = false;
        pressedMenuItem = -1;
#if STICKMON_ENABLE_DEBUG_FEATURES
        debugPressedItem = -1;
#endif
        pressedExploreArea = -1;
        exploreDragStartArea = -1;
        pressedExploreMenuItem = -1;
        inventoryController_.cancelTouch();
        teamController_.pressMember(-1);
        pressedRoomItem = -1;
        pressedCommunicationItem = -1;
        showerController_.clearPress();
        computerPressedItem = 0xFF;
        contactActionPressedItem = -1;
        settingsController.cancelTouch();
        progressionFlow_.finishTouch(event.timestampMs, progressionRelease.resume);
        battleSession_.press(0xFF);
        showerController_.finishTouch();
        if (releasedShowerTool) {
            requestRenderRect(showerController_.state().showerToolX - 64, showerController_.state().showerToolY - 64,
                              showerController_.state().showerToolX + 65, showerController_.state().showerToolY + 65);
            showerSaveAfterMs = event.timestampMs;
            flushShowerSave(event.timestampMs);
            break;
        }
        // Home taps invalidate their actual feedback in handleTap().
        if (sceneFlow.current() == AppSceneFlow::Scene::HOME) break;
        requestRenderRows(
                          sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                              ? MAIN_MENU_CONTENT_TOP
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE
                              ? HOME_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU
                            || sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION
                              ? 0
                          : sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                            sceneFlow.current() == AppSceneFlow::Scene::SHOP ||
                            sceneFlow.current() == AppSceneFlow::Scene::TEAM ||
                            sceneFlow.current() == AppSceneFlow::Scene::ROOM ||
                            sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD ||
                            sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
                            sceneFlow.current() == AppSceneFlow::Scene::COMPUTER ||
                            sceneFlow.current() == AppSceneFlow::Scene::SETTINGS ||
                            sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
                            sceneFlow.current() == AppSceneFlow::Scene::SHOWER
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::HOME
                              ? (touchStartY >= HOME_STATUS_TOP
                                     ? HOME_STATUS_TOP : HOME_ROOM_TOP)
                                                 : HOME_ROOM_TOP,
                          448);
        break;
    }
    }
}

void AmoledApp::handleTap(int x, int y, uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    if (inventoryController_.state().teamItemFeedbackActive) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        if (visitAwayFlow_.state().phase == VisitDeparture::WALK_TO_DOOR ||
            visitAwayFlow_.state().phase == VisitDeparture::CROSS_DOOR) return;
        if (visitRecallConfirm) {
            const int choice = recallConfirmChoiceAt(x, y);
            if (choice == 0 && visitSession.active() &&
                !visitSession.viewModel().localIsHost) {
                visitRecallConfirm = false;
                requestVisitEnd(nowMs);
            } else if (choice == 1) {
                visitRecallConfirm = false;
            }
            requestFullRender();
            return;
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugContactPending) {
            int choice = debugContactChoiceAt(x, y);
            if (choice == 0) {
                debugContactSelectedChoice = 0;
                debugContactChoiceConfirmUntilMs = nowMs + 100;
                acceptDebugContact(nowMs, false);
            } else if (choice == 1) {
                debugContactSelectedChoice = 1;
                debugContactChoiceConfirmUntilMs = nowMs + 100;
                debugContactPending = false;
                debugContactEventPending = false;
                debugContactEventWaitingForEntry = false;
                debugContactStorageSlot = 0xFF;
                debugContactKind = 0;
                requestFullRender();
            }
            // Modal while the prompt is up: swallow any tap outside the bubbles
            // since the HUD is faded out and its hit areas must stay inactive.
            return;
        }
        if (debugContactEventPending) {
            int choice = debugContactChoiceAt(x, y);
            if (choice < 0) return;
            debugContactSelectedChoice = static_cast<int8_t>(choice);
            debugContactChoiceConfirmUntilMs = nowMs + 100;
            debugContactEventPending = false;
            if (choice == 0 && debugContactKind == 3) {
                uint8_t unlocked = ExploreItemProgression::unlockedArea(gameState);
                selectedExploreArea = std::min<uint8_t>(selectedExploreArea, unlocked);
                if (!queueExploreDeparture(selectedExploreArea, false)) {
                    completeDebugContact(nowMs);
                    setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
                }
            } else if (choice == 1) {
                completeDebugContact(nowMs);
            }
            requestFullRender();
            return;
        }
        // The strip message fades the HUD out as well; keep its hidden hit
        // areas inert while the message is up.
        if (debugContactMessageUntilMs != 0 &&
            static_cast<int32_t>(nowMs - debugContactMessageUntilMs) < 0) {
            return;
        }
#endif
        // A HUD message replaces the buttons, but room interactions stay live.
        if (toast && toastScene == AppSceneFlow::Scene::HOME &&
            static_cast<int32_t>(nowMs - toastUntil) < 0 &&
            y >= HOME_STATUS_TOP) return;
        RoomResource& room = RoomResource::ins();
        int bowlX = room.available()
            ? worldToScreenX(room.foodX())
            : 290;
        int bowlY = room.available()
            ? worldToScreenY(room.foodY())
            : 286;
        switch (homeHitTargetAt(x, y, worldToScreenX(homeMainActor.x),
                               worldToScreenY(homeMainActor.y), bowlX, bowlY)) {
        case HomeHitTarget::MENU:
            if (visitAwayFlow_.state().phase == VisitDeparture::AWAY) {
                if (visitSession.active() &&
                    !visitSession.viewModel().localIsHost) {
                    visitRecallConfirm = true;
                    requestFullRender();
                }
                break;
            }
            sceneFlow.openMenu();
            menuScroll = 0.0f;
            menuVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            break;
        case HomeHitTarget::LOCK:
            lockRequested = true;
            break;
        case HomeHitTarget::PET: {
            const auto result = Game::HomeCare::petMonster(
                gameState, 0, gameState.gameMinutesTotal * 60UL);
            switch (result.outcome) {
            case PetOutcome::REWARDED:
                heartsUntil = nowMs + 1000;
                requestRenderRows(HOME_ROOM_TOP, AmoledUi::HEIGHT);
                applyHomeCareExperience(0, result.careExp);
                saveState();
                break;
            case PetOutcome::DAILY_LIMIT:
                setToast(Ui::Menu::PET_LIMIT, nowMs);
                saveState();
                break;
            case PetOutcome::NEEDS_REST:
                setToast(Ui::Menu::PET_REST, nowMs);
                break;
            }
            break;
        }
        case HomeHitTarget::BOWL: {
            FoodPlacementResult result =
                Game::HomeCare::placeSelectedFoodInBowl(gameState);
            Platform::logf(
                "[HomeFood] event=place result=%u bowl=%u bites=%u "
                "satiety=%u/%u\n",
                static_cast<unsigned>(result),
                static_cast<unsigned>(gameState.room.bowlCount),
                static_cast<unsigned>(gameState.room.bowlBitesRemaining),
                gameState.teamCount > 0
                    ? static_cast<unsigned>(gameState.team[0].satiety) : 0U,
                gameState.teamCount > 1
                    ? static_cast<unsigned>(gameState.team[1].satiety) : 0U);
            switch (result) {
            case FoodPlacementResult::ADDED:
                setToast(Ui::Menu::FOOD_ADDED, nowMs);
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
                wakeHomeFoodArbitration(nowMs);
                saveState();
                break;
            case FoodPlacementResult::NO_STOCK:
                setToast(Ui::Menu::NO_FOOD, nowMs);
                break;
            case FoodPlacementResult::BOWL_FULL:
                setToast(Ui::Menu::FOOD_FULL, nowMs);
                break;
            case FoodPlacementResult::DIFFERENT_FOOD:
                setToast(Ui::Menu::FOOD_MIXED, nowMs);
                break;
            }
            break;
        }
        case HomeHitTarget::NONE:
            break;
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        CommunicationViewModel model = visitSession.viewModel();
        if (communicationBackAt(x, y)) {
            using CommState = Communication::VisitSessionService::State;
            if (model.state == CommState::ACTIVE ||
                model.state == CommState::ENDING) {
                requestVisitEnd(nowMs);
                requestFullRender();
                return;
            }
            visitSession.stop();
            releaseVisitRadioSession();
            if (communicationReturnToComputer) {
                communicationReturnToComputer = false;
                sceneFlow.enter(AppSceneFlow::Scene::COMPUTER);
                computerPage = ComputerViewModel::Page::MENU;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                computerPressedItem = 0xFF;
            } else {
                sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            }
            requestFullRender();
            return;
        }
        int item = communicationItemAt(x, y, model);
        using CommState = Communication::VisitSessionService::State;
        if (model.state == CommState::IDLE) {
            if (item == 0) startVisitHost();
            else if (item == 1) startVisitSearch();
        } else if (model.state == CommState::ROOM_LIST && item >= 0) {
            visitSession.selectRoom(static_cast<uint8_t>(item));
        } else if (model.state == CommState::WAITING_HOST_DECISION) {
            if (item == 0) visitSession.acceptIncoming(true);
            else if (item == 1) visitSession.acceptIncoming(false);
        } else if (model.state == CommState::ACTIVE && item == 0) {
            requestVisitEnd(nowMs);
        } else if ((model.state == CommState::FAILED ||
                    model.state == CommState::ENDED) && item == 0) {
            visitSession.stop();
            releaseVisitRadioSession();
        }
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        if (route.exploreRouteIceSliding) return;
        if (route.exploreRouteComplete) {
            leaveExploreRoute();
            return;
        }
        if (route.exploreRouteExitConfirm) {
            int choice = exploreRouteExitChoiceAt(x, y);
            if (choice == 0) {
                exploreSession_.confirmExit(false);
                resumeExploreRoute(nowMs);
                requestRenderRows(HOME_HEADER_HEIGHT, 448);
            } else if (choice == 1) {
                leaveExploreRoute();
            }
            return;
        }
        if (exploreRouteBackAt(x, y)) {
            exploreSession_.confirmExit(true);
            pauseExploreRoute(nowMs);
            requestRenderRows(HOME_HEADER_HEIGHT, 448);
            return;
        }
        if (exploreRouteBagAt(x, y)) {
            pauseExploreRoute(nowMs);
            openItemScene(AppSceneFlow::Scene::BAG);
            return;
        }
        if (exploreRouteMenuAt(x, y)) {
            pauseExploreRoute(nowMs);
            sceneFlow.openExploreMenu();
            exploreMenuCursor = 0;
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreRouteMapAt(x, y)) {
            // Match Stick's beginAutoWalk(): one player tap walks through
            // route points until the next interaction. It is not Agent
            // mode and must not leave automatic exploration enabled.
            exploreSession_.walkByPlayer();
            if (!route.exploreRouteMoving) {
                beginExploreRouteStep(nowMs);
            }
            requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        if (battleSession_.inputBusy()) return;
        if (battleBackAt(x, y)) {
            if (battleSession_.state().battlePhase == BattleViewModel::Phase::BAG_SELECT ||
                battleSession_.state().battlePhase == BattleViewModel::Phase::SWITCH_SELECT) {
                battleSession_.showAction();
                battleSession_.press(0xFF);
                requestFullRender();
            }
            return;
        }
        int item = battleItemAt(x, y, battleSession_.state().battlePhase);
        if (item < 0) return;
        if (battleSession_.state().battlePhase == BattleViewModel::Phase::FRIENDSHIP) {
            resolveBattleFriendship(static_cast<uint8_t>(item), nowMs);
        } else if (battleSession_.state().battlePhase == BattleViewModel::Phase::VICTORY) {
            return;
        } else if (battleSession_.state().battlePhase == BattleViewModel::Phase::DEFEAT) {
            finishBattleDefeat(nowMs);
        } else if (battleSession_.state().battlePhase == BattleViewModel::Phase::BAG_SELECT) {
            performBattleBag(nowMs);
        } else if (battleSession_.state().battlePhase == BattleViewModel::Phase::SWITCH_SELECT) {
            battleSession_.press(static_cast<uint8_t>(item));
            performBattleSwitch(static_cast<uint8_t>(item), true, nowMs);
        } else if (item == 0) {
            battleSession_.press(0xFF);
            performBattleAttack(nowMs);
        } else if (item == 1) {
            performBattleBag(nowMs);
        } else if (item == 2) {
            const int8_t switchSlot = availableBattleSwitchSlot();
            if (switchSlot >= 0) {
                performBattleSwitch(static_cast<uint8_t>(switchSlot), true,
                                    nowMs);
            } else {
                setToast(Ui::Amoled::CANNOT_SWITCH, nowMs);
            }
        } else if (item == 3) {
            performBattleFlee(nowMs);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        int itemIndex = exploreRouteMenuItemAt(x, y);
        if (exploreRouteMenuBackAt(x, y) || itemIndex < 0) {
            sceneFlow.closeExploreMenu();
            resumeExploreRoute(nowMs);
            toast = nullptr;
            requestFullRender();
            return;
        }
        exploreMenuCursor = static_cast<uint8_t>(itemIndex);
        AppSceneFlow::ExploreMenuEntry entry =
            AppSceneFlow::exploreMenuEntry(static_cast<uint8_t>(itemIndex));
        switch (entry.item) {
        case AppSceneFlow::ExploreMenuItem::TEAM:
            openTeamScene();
            break;
        case AppSceneFlow::ExploreMenuItem::BAG:
            openItemScene(AppSceneFlow::Scene::BAG);
            break;
        case AppSceneFlow::ExploreMenuItem::END:
            leaveExploreRoute();
            break;
        case AppSceneFlow::ExploreMenuItem::BACK:
            sceneFlow.closeExploreMenu();
            resumeExploreRoute(nowMs);
            toast = nullptr;
            requestFullRender();
            break;
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        if (teamController_.state().teamStatusOpen) {
            if (teamStatusBackAt(x, y)) {
                teamController_.closeStatus();
                requestFullRender();
            }
            return;
        }
        if (teamController_.state().teamActionPopupOpen) {
            const int choice = teamActionPopupItemAt(x, y, teamController_.state().teamActionPopupSlot);
            if (choice >= 0) {
                teamController_.dismissActions();
                const bool leader = teamController_.state().teamActionPopupSlot == 0;
                if (!leader && choice == 0) {
                    teamController_.selectLeaderCandidate(teamController_.state().teamActionPopupSlot);
                    switchTeamLeader(nowMs);
                }
                else if (leader ? choice == 0 : choice == 1) {
                    teamController_.openStatus(teamController_.state().teamActionPopupSlot);
                    requestFullRender();
                }
                else if ((leader ? choice == 1 : choice == 2)) openTeamMoves(teamController_.state().teamActionPopupSlot, nowMs);
                else if (!leader && choice == 3) leaveTeamMember(teamController_.state().teamActionPopupSlot, nowMs);
                else requestRenderRows(MENU_HEADER_HEIGHT, 448);
            } else {
                teamController_.dismissActions();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (teamController_.state().teamMovesOpen) {
            if (teamMovesBackAt(x, y)) {
                teamController_.closeMoves();
                requestFullRender();
                return;
            }
            int item = teamMovesItemAt(
                x, y, teamController_.state().teamMovesMode, teamController_.state().teamMovesRecallCount,
                teamController_.state().teamMovesScroll, teamController_.state().teamMovesSelectedItem,
                teamController_.state().teamMovesDetailProgress);
            if (item < 0) return;
            const Species* species = findSpecies(gameState.team[teamController_.state().teamMovesSlot].speciesId);
            const bool learned = species && item < Game::MOVE_SLOT_COUNT &&
                Game::MoveManagementService::learnedMove(
                    *species, gameState.team[teamController_.state().teamMovesSlot], static_cast<uint8_t>(item));
            const auto selection = teamController_.selectMove(item, learned, nowMs);
            switch (selection.action) {
            case TeamController::MoveAction::NONE:
                break;
            case TeamController::MoveAction::EMPTY:
                setToast(Ui::EMPTY, nowMs);
                break;
            case TeamController::MoveAction::REDRAW:
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
                break;
            case TeamController::MoveAction::RECALL:
                if (Game::MoveManagementService::recallMove(
                        gameState, teamController_.state().teamMovesSlot, selection.moveId, selection.replacementSlot)) {
                    saveState();
                    teamController_.completeRecall(gameState);
                    setToast(Ui::Amoled::MOVE_RECALLED, nowMs);
                } else {
                    setToast(Ui::Team::MOVE_RECALL_FAILED, nowMs);
                }
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
                break;
            }
            return;
        }
        if (teamController_.state().teamConfirmOpen) {
            int choice = teamConfirmChoiceAt(x, y);
            if (choice == 0) {
                switchTeamLeader(nowMs);
            } else if (choice == 1) {
                teamController_.dismissConfirmation();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (teamBackAt(x, y)) {
            if (inventoryController_.state().selectingItemTarget) {
                inventoryController_.cancelTargetSelection();
                teamController_.dismissActions();
                sceneFlow.enter(AppSceneFlow::Scene::BAG);
                requestFullRender();
            } else closeItemScene();
            return;
        }
        int slot = teamMemberAt(
            x, y, Game::TeamRoster::memberCount(gameState));
        if (slot < 0) return;
        if (inventoryController_.state().selectingItemTarget) {
            uint8_t target = static_cast<uint8_t>(slot);
            if (Game::ItemInventory::count(gameState, inventoryController_.state().pendingItem) == 0) {
                setToast(Ui::Amoled::NO_STOCK, nowMs);
            } else {
                const Species* source = findSpecies(
                    gameState.team[target].speciesId);
                const Species* evolutionTarget =
                    source && isEvolutionStone(inventoryController_.state().pendingItem)
                        ? stoneEvolutionTarget(*source, inventoryController_.state().pendingItem) : nullptr;
                if (isEvolutionStone(inventoryController_.state().pendingItem)) {
                    if (!evolutionTarget) {
                        setToast(Ui::Amoled::NO_EFFECT, nowMs);
                        requestFullRender();
                        return;
                    }
                    const Game::ItemId stone = inventoryController_.state().pendingItem;
                    const uint16_t fromSpeciesId =
                        gameState.team[target].speciesId;
                    inventoryController_.cancelTargetSelection();
                    openEvolutionProgression(
                        inventoryController_.state().itemTargetReturnScene, target, fromSpeciesId,
                        evolutionTarget->id, nowMs, stone);
                    return;
                }
                const uint8_t oldLevel = gameState.team[target].level;
                const uint16_t hpBefore = gameState.team[target].hpCur;
                Game::ItemInventory::UseResult used =
                    Game::ItemInventory::useOnTeam(gameState, inventoryController_.state().pendingItem, target);
                if (used == Game::ItemInventory::UseResult::USED) {
                    saveState();
                    if (gameState.team[target].level != oldLevel) {
                        inventoryController_.cancelTargetSelection();
                        openProgressionScene(
                            inventoryController_.state().itemTargetReturnScene, target, oldLevel, nowMs);
                        return;
                    }
                    teamController_.pressMember(-1);
                    pointerDown = false;
                    dragging = false;
                    setToast(Ui::Amoled::ITEM_USED, Platform::clock().millis());
                    AudioManager::ins().playSfx(SfxCue::UI_CONFIRM);
                    inventoryController_.beginFeedback(target, hpBefore, gameState.team[target].hpCur,
                                                       Platform::clock().millis());
                } else setToast(Ui::Amoled::NO_EFFECT, nowMs);
            }
            requestFullRender();
            return;
        }
        if (slot == 0) {
            setToast(Ui::Amoled::CURRENT_LEADER, nowMs);
        } else if (!Game::TeamRoster::canMoveToFront(
                       gameState, static_cast<uint8_t>(slot))) {
            setToast(Ui::Amoled::VISITOR_LOCKED, nowMs);
        } else {
            teamController_.requestLeaderConfirmation(static_cast<uint8_t>(slot));
            toast = nullptr;
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
        if (roomMenuBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int item = roomMenuItemAt(x, y);
        if (item == 0) {
            openRoomFoodScene();
        } else if (item == 1) {
            openShowerScene(nowMs);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
        if (roomFoodBackAt(x, y)) {
            closeUtilityScene();
            return;
        }
        int item = roomFoodItemAt(x, y);
        if (item < 0 || item >= Game::ROOM_FOOD_COUNT) return;
        gameState.room.selectedFood = static_cast<uint8_t>(item);
        saveState();
        setToast(Ui::Room::FOOD_SELECTED, nowMs);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
        progressionFlow_.tap(gameState, x, y, nowMs);
        applyProgressionEffects(nowMs);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
        if (computerPage == ComputerViewModel::Page::STORAGE &&
            contactConfirmOpen) {
            handleContactConfirmTap(x, y, nowMs);
            return;
        }
        if (computerPage == ComputerViewModel::Page::STORAGE &&
            contactActionOpen) {
            handleContactActionTap(x, y, nowMs);
            return;
        }
        if (computerPage == ComputerViewModel::Page::STATUS &&
            computerBackAt(x, y, computerPage)) {
            computerPage = ComputerViewModel::Page::STORAGE;
            teamController_.resetStatus();
            computerPressedItem = 0xFF;
            requestFullRender();
            return;
        }
        if (computerBackAt(x, y, computerPage)) {
            if (computerPage != ComputerViewModel::Page::MENU) {
#if STICKMON_HAS_CLAW
                if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
                    Stickmon::ClawRuntime::instance().stopSetupPortal();
                }
#endif
                computerPage = computerPage == ComputerViewModel::Page::CLAW_SETUP
                    ? ComputerViewModel::Page::AI_HOSTING
                    : ComputerViewModel::Page::MENU;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                computerPressedItem = 0xFF;
                requestFullRender();
            } else {
                closeUtilityScene();
            }
            return;
        }
#if STICKMON_HAS_CLAW
        if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
            const int tab = clawTabAt(x, y);
            if (tab >= 0) {
                const bool wantLog = tab == 1;
                if (wantLog != clawLogView) {
                    clawLogView = wantLog;
                    clawLogScroll = 0.0f;
                    clawLogVelocity = 0.0f;
                    clawLogPinned = true;
                    requestFullRender();
                }
                return;
            }
        }
#endif
        bool aiClawEnabled = false;
#if STICKMON_HAS_CLAW
        aiClawEnabled = Stickmon::ClawRuntime::instance().enabled();
#endif
        int item = computerItemAt(x, y, computerPage, computerScroll,
                                  gameState.storageCount, aiClawEnabled);
        if (item < 0) return;
        if (computerPage == ComputerViewModel::Page::MENU) {
            if (item == 0) {
                if (!gameState.oobeDone) {
                    setToast(Ui::Menu::HATCH_FIRST, nowMs);
                    return;
                }
                visitSession.attach(&gameState);
                communicationReturnToComputer = true;
                sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
                pressedCommunicationItem = -1;
                toast = nullptr;
                requestFullRender();
            } else if (item == 1) {
                computerPage = ComputerViewModel::Page::STORAGE;
                computerPressedItem = 0xFF;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                toast = nullptr;
                requestFullRender();
            }
#if STICKMON_HAS_CLAW
            else if (item == 2) {
                computerPage = ComputerViewModel::Page::AI_HOSTING;
                computerPressedItem = 0xFF;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                toast = nullptr;
                requestFullRender();
            }
#endif
        } else if (computerPage == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
            Stickmon::ClawRuntime& claw = Stickmon::ClawRuntime::instance();
            if (item == 0) {
                claw.setWifiEnabled(!claw.wifiEnabled());
                computerPressedItem = 0xFF;
                requestFullRender();
            } else if (item == 1) {
                claw.setEnabled(!claw.enabled());
                computerPressedItem = 0xFF;
                requestFullRender();
            } else if (item == 2) {
                if (!claw.wifiEnabled()) {
                    setToast(Ui::Amoled::CLAW_WIFI_REQUIRED, nowMs);
                    return;
                }
                char ssid[33] = {};
                char password[65] = {};
                char ip[16] = {};
                if (claw.startSetupPortal() &&
                    claw.setupPortalInfo(ssid, sizeof(ssid), password,
                                         sizeof(password), ip, sizeof(ip))) {
                    computerPage = ComputerViewModel::Page::CLAW_SETUP;
                    computerPressedItem = 0xFF;
                    clawLogView = false;
                    clawLogScroll = 0.0f;
                    clawLogVelocity = 0.0f;
                    clawLogPinned = true;
                    clawLogGen = claw.logGeneration();
                    refreshClawLogSnapshot();
                    toast = nullptr;
                    requestFullRender();
                } else {
                    setToast(Ui::Amoled::CLAW_PORTAL_FAILED, nowMs);
                }
            } else {
                computerPage = ComputerViewModel::Page::MENU;
                computerPressedItem = 0xFF;
                requestFullRender();
            }
#else
            (void)item;
#endif
        } else if (computerPage == ComputerViewModel::Page::STATUS) {
            return;
        } else {
            openContactAction(static_cast<uint8_t>(item));
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
        using Action = SettingsController::Action;
        const Action action = settingsController.tap(
            x, y, systemUpdateHandler != nullptr, visitSession.busy(), gameState.settings);
        switch (action) {
        case Action::NONE: return;
        case Action::CLOSE: closeUtilityScene(); return;
        case Action::REDRAW:
            toast = nullptr;
            requestFullRender();
            return;
        case Action::BUSY:
            setToast(Ui::Amoled::UPDATE_BUSY, nowMs);
            return;
        case Action::RESET_GAME:
            if (!resetGame()) setToast(Ui::Settings::RESET_FAILED, nowMs);
            return;
        case Action::SYSTEM_UPDATE:
            if (!saveState() || !systemUpdateHandler || !systemUpdateHandler())
                setToast(Ui::Amoled::UPDATE_BUSY, nowMs);
            return;
        case Action::BRIGHTNESS:
            applySettingsChange(SettingsController::Change::BRIGHTNESS);
            setToast(Ui::Settings::BRIGHTNESS_CHANGED, nowMs);
            break;
        case Action::VOLUME:
            applySettingsChange(SettingsController::Change::VOLUME);
            setToast(Ui::Settings::VOLUME_CHANGED, nowMs);
            break;
        case Action::SPEED: setToast(Ui::Common::SPEED_CHANGED, nowMs); break;
        case Action::POWER_SAVE: setToast(Ui::Settings::POWER_SAVE, nowMs); break;
        }
        saveState();
        requestRenderRows(MENU_HEADER_HEIGHT, 448);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
        const auto result = showerController_.tap(gameState, x, y, nowMs);
        applyShowerEffects(nowMs);
        switch (result.action) {
        case ShowerController::Action::NONE: break;
        case ShowerController::Action::CLOSE: closeShowerScene(); break;
        case ShowerController::Action::SELECT_SOAP: startShowerSoap(result.soapIndex, nowMs); break;
        case ShowerController::Action::ALREADY_SOAPED: setToast(Ui::Shower::ALREADY_SOAPED, nowMs); break;
        case ShowerController::Action::NO_SOAP: setToast(Ui::Shower::NO_SOAP, nowMs); break;
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        if (inventoryController_.state().itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            if (choice == 0) {
                if (inventoryController_.state().battleBagMode) {
                    performBattleBagItem(inventoryController_.state().pendingItem, nowMs);
                } else if (Game::ItemInventory::usableFromHomeBag(inventoryController_.state().pendingItem) &&
                    inventoryController_.state().pendingItem != Game::ItemId::MAX_REPEL &&
                    inventoryController_.state().pendingItem != Game::ItemId::HONEY) {
                    inventoryController_.beginTargetSelection(sceneFlow.subSceneReturn());
                    openTeamScene(true);
                } else performPendingItemAction(nowMs);
                } else if (choice == 1) {
                    inventoryController_.clearAction();
                    requestFullRender();
                }
                return;
        }
        if (itemListBackAt(x, y, inventoryController_.state().itemConfirmOpen)) {
            closeItemScene();
            return;
        }
        int index = itemListItemAt(
            x, y, inventoryController_.state().itemScroll,
            Game::ItemInventory::homeBagDailyItemCount(gameState),
            Game::ItemInventory::homeBagExploreItemCount(gameState),
            currentItemCount(), false);
        if (index < 0) return;
        switch (inventoryController_.selectItem(
                gameState, AppSceneFlow::Scene::BAG, static_cast<uint8_t>(index))) {
        case InventoryController::Selection::NONE:
            return;
        case InventoryController::Selection::CANNOT_USE:
            setToast(Ui::Amoled::CANNOT_USE, nowMs);
            return;
        case InventoryController::Selection::OPEN_MOVE_MANAGER:
            openTeamScene();
            openTeamMoves(0, nowMs);
            return;
        case InventoryController::Selection::DETAIL:
            break;
        }
        toast = nullptr;
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        if (itemListBackAt(x, y, inventoryController_.state().itemConfirmOpen)) {
            closeItemScene();
            return;
        }
        if (inventoryController_.state().itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            Platform::logf("[ShopAction] t=%lu tap=%s item=%d\n",
                           static_cast<unsigned long>(nowMs),
                           choice == 0 ? "transaction" : choice == 1 ? "back" : "outside",
                           static_cast<int>(inventoryController_.state().pendingItem));
            if (choice == 0) {
                if (!inventoryController_.canConfirm(gameState)) return;
                performPendingItemAction(nowMs);
            } else if (choice == 1) {
                inventoryController_.closeDetail();
                toast = nullptr;
                requestFullRender();
                Platform::logLine("[ShopAction] detail_closed render_requested");
            }
            return;
        }
        int menuItem = shopMenuItemAt(x, y);
        if (menuItem >= 0) {
            inventoryController_.selectCategory(menuItem == 0
                ? Game::ShopService::Category::DAILY
                : Game::ShopService::Category::SELL);
            toast = nullptr;
            requestFullRender();
            return;
        }
        ShopViewModel::Mode mode =
            inventoryController_.state().shopCategory == Game::ShopService::Category::SELL
                ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
        int index = shopGridItemAt(
            x, y, inventoryController_.state().itemScroll, mode, shopDailyItemCount(),
            shopExploreItemCount(), currentItemCount());
        if (index < 0) return;
        if (inventoryController_.selectItem(gameState, AppSceneFlow::Scene::SHOP,
                static_cast<uint8_t>(index)) != InventoryController::Selection::DETAIL) return;
        Platform::logf("[ShopAction] detail_open=%d item=%d action=%s\n",
                       inventoryController_.state().itemConfirmOpen, static_cast<int>(inventoryController_.state().pendingItem),
                       inventoryController_.state().pendingItemAction == PendingItemAction::SELL ? "sell" : "buy");
        Platform::logLine(
            "[ShopTouch] detail_touch: buy x=[36,176) y=[354,426); "
            "back x=[192,332) y=[354,426)");
        toast = nullptr;
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreSelectionBackAt(x, y)) {
            clearExplorePreview();
            sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreStartAt(x, y)) {
            if (!ExploreItemProgression::isAreaUnlocked(
                    selectedExploreArea, gameState)) {
                setToast(Ui::Explore::AREA_LOCKED, nowMs);
            } else {
                queueExploreDeparture(selectedExploreArea, false);
            }
            requestFullRender();
            return;
        }
        int area = exploreAreaAt(
            x, y, selectedExploreArea,
            ExploreItemProgression::visibleAreaCount(gameState));
        if (area >= 0) {
            // Visible areas can be selected before they are unlocked. The
            // locked state is communicated by the disabled departure button.
            if (selectedExploreArea != static_cast<uint8_t>(area)) {
                selectExploreArea(static_cast<uint8_t>(area), nowMs);
                toast = nullptr;
            }
            requestRenderRows(0, EXPLORE_SELECTOR_BUTTON_TOP);
        }
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
        handleDebugTap(x, y, nowMs);
        return;
    }
#endif

    if (mainMenuBackAt(x, y)) {
        AppSceneFlow::Scene destination = sceneFlow.closeMenu();
        if (destination == AppSceneFlow::Scene::HATCH) {
            hatchController_.resume(nowMs);
            syncHatchImu(nowMs);
        }
        if (destination == AppSceneFlow::Scene::EXPLORE_ROUTE) {
            resumeExploreRoute(nowMs);
        }
        menuVelocity = 0.0f;
        toast = nullptr;
        requestFullRender();
        return;
    }

    int itemIndex = mainMenuItemAt(x, y, menuScroll);
    if (itemIndex >= 0) {
        AppSceneFlow::MainMenuEntry entry = AppSceneFlow::mainMenuEntry(
            static_cast<uint8_t>(itemIndex),
            STICKMON_ENABLE_DEBUG_FEATURES != 0);
        if (!gameState.oobeDone && entry.target != AppSceneFlow::Scene::SETTINGS &&
            entry.target != AppSceneFlow::Scene::COMPUTER &&
            entry.target != AppSceneFlow::Scene::HOME) {
            setToast(Ui::Menu::HATCH_FIRST, nowMs);
            return;
        }
        if (entry.target == AppSceneFlow::Scene::EXPLORE_AREAS) {
            if (sceneFlow.menuReturn() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
                sceneFlow.enterExploreRoute();
                resumeExploreRoute(nowMs);
            } else {
                sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);
                selectedExploreArea = std::min<uint8_t>(
                    selectedExploreArea,
                    ExploreItemProgression::unlockedArea(gameState));
                exploreAreaAnimCursor = selectedExploreArea;
                loadExplorePreview(nowMs);
            }
            toast = nullptr;
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::TEAM) {
            openTeamScene();
        } else if (entry.target == AppSceneFlow::Scene::ROOM) {
            openRoomScene();
        } else if (entry.target == AppSceneFlow::Scene::COMPUTER) {
            openComputerScene();
        } else if (entry.target == AppSceneFlow::Scene::SETTINGS) {
            openSettingsScene();
        } else if (entry.target == AppSceneFlow::Scene::BAG ||
                   entry.target == AppSceneFlow::Scene::SHOP) {
            openItemScene(entry.target);
        } else if (entry.target == AppSceneFlow::Scene::COMMUNICATION) {
            communicationReturnToComputer = false;
            sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
            visitSession.attach(&gameState);
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::HOME) {
            if (exploreSessionActive) {
                beginExploreReturn(nowMs);
                return;
            }
            exploreSession_.stopRoute();
            if (gameState.oobeDone) sceneFlow.goHome();
            else {
                sceneFlow.closeMenu();
                hatchController_.resume(nowMs);
            }
            toast = nullptr;
            requestFullRender();
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (entry.target == AppSceneFlow::Scene::DEBUG) {
            debugCategory = DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = 0.0f;
            debugVelocity = 0.0f;
            debugPopup = DebugViewModel::Popup::NONE;
            debugPressedItem = -1;
            toast = nullptr;
            sceneFlow.enter(AppSceneFlow::Scene::DEBUG);
            requestFullRender();
#endif
        } else {
            setToast(Ui::Amoled::MIGRATION_NEXT, nowMs);
        }
    }
}

void AmoledApp::updateBattery(uint32_t nowMs) {
    constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;
    if (batterySampled && nowMs - lastBatterySampleMs < SAMPLE_INTERVAL_MS) return;
    batterySampled = true;
    lastBatterySampleMs = nowMs;
    const int raw = Platform::power().batteryLevel();
    const int8_t next = raw >= 0 && raw <= 100 ? static_cast<int8_t>(raw) : -1;
    if (next == batteryPercent) return;
    batteryPercent = next;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        requestRenderRows(0, HOME_HEADER_HEIGHT);
    }
}

void AmoledApp::update(uint32_t nowMs) {
    if (gameResetPending) return;
    if (toast && toastScene != sceneFlow.current()) toast = nullptr;
    updateBattery(nowMs);
    updateExploreEcology(nowMs);
    syncHatchImu(nowMs);
    if (!gameState.oobeDone) {
        if (sceneFlow.current() == AppSceneFlow::Scene::HATCH) {
            AudioManager::ins().update();
            CryPlayer::ins().update();
            updateClockAndCare(nowMs);
            updateHatch(nowMs);
            if (toast && static_cast<int32_t>(nowMs - toastUntil) >= 0) {
                toast = nullptr;
                requestRenderRows(UiCommon::TOAST_TOP, UiCommon::TOAST_BOTTOM);
            }
            return;
        }
        // Menus pause the egg timer; keep the anchor current while they update.
        updateHatch(nowMs);
    }
#if STICKMON_HAS_CLAW
    if (Stickmon::ClawRuntime::instance().playerActive(nowMs)) {
        // WeChat messages are player input too. Stop any route/battle
        // automation before processing another frame of the app state.
        autonomousExpedition = false;
        exploreSession_.setAutoWalk(false);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
        computerPage == ComputerViewModel::Page::CLAW_SETUP) {
        Stickmon::ClawRuntime& claw = Stickmon::ClawRuntime::instance();
        if (!clawLogView && claw.setupPhoneJoined()) {
            // A phone just joined the setup hotspot: switch to the log view
            // so Wi-Fi / WeChat login progress is visible without tapping.
            clawLogView = true;
            clawLogPinned = true;
            clawLogVelocity = 0.0f;
            requestFullRender();
        }
        const uint32_t generation = claw.logGeneration();
        if (generation != clawLogGen) {
            clawLogGen = generation;
            refreshClawLogSnapshot();
            if (clawLogView) {
                // Pinned follows the tail; new lines arrive at the bottom.
                requestRenderRows(68, 448);
            }
        }
    }
#endif
    AudioManager::ins().update();
    CryPlayer::ins().update();
    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
        progressionFlow_.update(nowMs);
        applyProgressionEffects(nowMs);
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind != 3 &&
        !debugContactEventPending && !debugContactEventWaitingForEntry &&
        nowMs - debugContactStartedMs >= 30000UL) {
        completeDebugContact(nowMs);
    }
    if (debugContactSelectedChoice >= 0 &&
        static_cast<int32_t>(nowMs - debugContactChoiceConfirmUntilMs) >= 0) {
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
        debugContactChoiceVisible = false;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    // Ease the visitor prompt in/out: the bottom HUD fades away while the
    // message and the yes/no bubbles appear, and comes back after a choice.
    const bool stripMessageActive = debugContactMessageUntilMs != 0 &&
        static_cast<int32_t>(nowMs - debugContactMessageUntilMs) < 0;
    const uint8_t promptFadeTarget =
        (debugContactPending || debugContactEventPending ||
         stripMessageActive) ? 255 : 0;
    if (debugContactPromptFade != promptFadeTarget &&
        sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        // Keep the bottom HUD/message swap brisk. Choice bubbles are hidden
        // independently as soon as the tap is accepted.
        constexpr uint8_t PROMPT_FADE_STEP = 40;
        debugContactPromptFade = debugContactPromptFade < promptFadeTarget
            ? static_cast<uint8_t>(std::min<int>(
                  255, debugContactPromptFade + PROMPT_FADE_STEP))
            : static_cast<uint8_t>(std::max<int>(
                  0, debugContactPromptFade - PROMPT_FADE_STEP));
        if ((debugContactPending || debugContactEventPending) &&
            debugContactPromptFade == 255) {
            Platform::logf("[FriendDiag] prompt ready kind=%u\n",
                           static_cast<unsigned>(debugContactKind));
        }
        requestRenderRows(HOME_ROOM_TOP, 448);
    }
    if (debugBattleRequested) {
        debugBattleRequested = false;
        debugBattleActive = true;
        if (!beginExploreEncounter(nowMs)) debugBattleActive = false;
    }
#endif
    const auto battleAudio = battleSession_.takeAudio();
    if (battleAudio.sfx != 0xFF) AudioManager::ins().playSfx(static_cast<SfxCue>(battleAudio.sfx));
    if (battleAudio.crySpecies != 0) CryPlayer::ins().replay(battleAudio.crySpecies);
    CommunicationViewModel communicationBefore = visitSession.viewModel();
    Game::MonsterRuntime detachedVisitor{};
    const bool hadAttachedVisitor = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    if (hadAttachedVisitor) detachedVisitor = gameState.team[1];
    visitSession.update(nowMs);
    CommunicationViewModel communicationAfter = visitSession.viewModel();
    if (hadAttachedVisitor && gameState.teamCount < 2 &&
        homeCompanionActor.hidden) {
        cancelPairInteraction(nowMs);
        homeRuntime.releaseAll(0);
        homeRuntime.stop(0, nowMs);
        visitorFlow_.resetMotion();
        syncHomeActors(nowMs);
        sceneFlow.goHome();
        requestFullRender();
    } else if (hadAttachedVisitor && gameState.teamCount < 2 &&
               visitorFlow_.state().motion != VisitorMotion::EXITING) {
        gameState.team[1] = detachedVisitor;
        gameState.teamCount = 2;
        gameState.activeSlot = 0;
        syncHomeActors(nowMs);
        beginVisitorExit(nowMs, false);
        sceneFlow.goHome();
        requestFullRender();
    }
    using VisitState = Communication::VisitSessionService::State;
    if (visitRadioExclusive &&
        (communicationAfter.state == VisitState::IDLE ||
         communicationAfter.state == VisitState::FAILED ||
         communicationAfter.state == VisitState::ENDED)) {
        releaseVisitRadioSession();
    }
    if (communicationBefore.state !=
            Communication::VisitSessionService::State::ACTIVE &&
        communicationAfter.state ==
            Communication::VisitSessionService::State::ACTIVE &&
        sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        sceneFlow.goHome();
        if (!communicationAfter.localIsHost) beginVisitDeparture(nowMs);
    }
    if (communicationBefore.state != communicationAfter.state ||
        communicationBefore.roomCount != communicationAfter.roomCount ||
        communicationBefore.remote.speciesId != communicationAfter.remote.speciesId ||
        communicationBefore.remote.mood != communicationAfter.remote.mood ||
        communicationBefore.remote.satiety != communicationAfter.remote.satiety ||
        communicationBefore.remote.hpCur != communicationAfter.remote.hpCur ||
        communicationBefore.remote.hpMax != communicationAfter.remote.hpMax ||
        communicationBefore.remainSec != communicationAfter.remainSec) {
        requestFullRender();
    }
    updateClockAndCare(nowMs);
    updateMoodHearts(nowMs);
    if (!updateExploreDeparture(nowMs)) {
        // Preserve movement time when a 100ms lock tick lands slightly late.
        // Bound catch-up after long blocking operations just like live motion.
        if (displayLocked && lockedHomePowerAllowed() && lastPetUpdateMs != 0 &&
            nowMs - lastPetUpdateMs <= 1000) {
            uint32_t stepMs = lastPetUpdateMs;
            while (nowMs - stepMs > 100 && lockedHomePowerAllowed()) {
                stepMs += 100;
                updatePet(stepMs);
            }
        }
        updatePet(nowMs);
        startPendingCareProgression(nowMs);
        updateExploreRoute(nowMs);
    }
    bool exploreAreaAnimating = false;
    bool explorePreviewMoving = false;
    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        uint8_t visibleAreaCount = std::min<uint8_t>(
            ExploreItemProgression::visibleAreaCount(gameState),
            Game::EXPLORE_AREA_COUNT);
        float target = selectedExploreArea < visibleAreaCount
            ? static_cast<float>(selectedExploreArea)
            : static_cast<float>(std::max<int>(0, visibleAreaCount - 1));
        float difference = target - exploreAreaAnimCursor;
        if (std::fabs(difference) < 0.05f) {
            if (exploreAreaAnimCursor != target) {
                exploreAreaAnimCursor = target;
                requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
            }
        } else {
            exploreAreaAnimCursor += difference * EXPLORE_AREA_CURSOR_LERP;
            if (std::fabs(target - exploreAreaAnimCursor) < 0.05f) {
                exploreAreaAnimCursor = target;
            }
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        }
        exploreAreaAnimating = std::fabs(target - exploreAreaAnimCursor) >= 0.05f;

        bool previewLoadWasDue = explorePreviewLoadPending &&
            static_cast<int32_t>(nowMs - explorePreviewNextLoadAt) >= 0;
        updateExplorePreviewLoading(nowMs);
        if (previewLoadWasDue) {
            requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                              EXPLORE_PREVIEW_RENDER_BOTTOM);
        }
        if (explorePreviewPool.count > 0) {
            uint32_t elapsed = nowMs - explorePreviewStartedAt;
            uint32_t visualCycle = elapsed / EXPLORE_PREVIEW_CYCLE_MS;
            if (visualCycle != explorePreviewVisualCycle) {
                explorePreviewVisualCycle = visualCycle;
                requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                                  EXPLORE_PREVIEW_RENDER_BOTTOM);
            }
            explorePreviewMoving =
                elapsed % EXPLORE_PREVIEW_CYCLE_MS >
                EXPLORE_PREVIEW_HOLD_MS;
        }
    }

    const auto teamUpdate = teamController_.tick(nowMs);
    if (teamUpdate.statusCompleted) requestFullRender();
    else if (teamUpdate.statusChanged) requestRenderRows(MENU_HEADER_HEIGHT, 448);
    if (teamUpdate.movesChanged) requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
        inventoryController_.state().teamItemFeedbackActive) {
        const uint32_t itemNowMs = Platform::clock().millis();
        if (inventoryController_.feedbackFrameDue(itemNowMs, BATTLE_GAUGE_FRAME_MS)) {
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
        // Keep the completed HP value visible briefly before leaving the team.
        if (itemNowMs - inventoryController_.state().teamItemStartedMs >= BATTLE_HP_ANIMATION_MS + 200) {
            finishTeamItemUse(itemNowMs);
        }
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        battleSession_.updateGauges(Platform::clock().millis());
        applyBattleEffects(nowMs);
        battleSession_.updateAttack(Platform::clock().millis());
        applyBattleEffects(nowMs);
        battleSession_.updateFlow(gameState, nowMs);
        applyBattleEffects(nowMs);
    }

#if STICKMON_HAS_CLAW
    if (autonomousExpedition &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE && !battleSession_.inputBusy()) {
        // Reuse the game's deterministic battle AI for autonomous routes.
        // Existing animation and log gates keep each action interruptible.
        switch (battleSession_.state().battlePhase) {
        case BattleViewModel::Phase::ACTION:
            performBattleAttack(nowMs);
            break;
        case BattleViewModel::Phase::VICTORY:
            break;
        case BattleViewModel::Phase::FRIENDSHIP:
            // Do not silently add a wild monster to the player's team. The
            // player can still take over and make that choice on screen.
            resolveBattleFriendship(1, nowMs);
            break;
        case BattleViewModel::Phase::DEFEAT:
            finishBattleDefeat(nowMs);
            break;
        case BattleViewModel::Phase::SWITCH_SELECT:
            for (uint8_t slot = 0; slot < gameState.teamCount; ++slot) {
                if (slot != battleSession_.state().battlePlayerSlot &&
                    !gameState.team[slot].fainted &&
                    gameState.team[slot].hpCur > 0) {
                    performBattleSwitch(slot, false, nowMs);
                    break;
                }
            }
            break;
        case BattleViewModel::Phase::BAG_SELECT:
            battleSession_.showAction();
            break;
        }
    }
#endif

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (showerDiagnosticStartedMs != 0) {
        const uint32_t elapsed = nowMs - showerDiagnosticStartedMs;
        if (elapsed >= 12000 || sceneFlow.current() != AppSceneFlow::Scene::SHOWER) {
            showerDiagnosticStartedMs = 0;
            TouchEvent event{};
            event.type = TouchEventType::UP;
            event.x = showerController_.state().showerToolX; event.y = showerController_.state().showerToolY; event.timestampMs = nowMs;
            handleTouch(event);
            closeShowerScene();
        } else {
            const float phase = elapsed * 0.006f;
            TouchEvent event{};
            event.type = TouchEventType::MOVE;
            event.x = static_cast<int16_t>(184 + std::sin(phase) * 70);
            event.y = static_cast<int16_t>(212 + std::cos(phase * 0.7f) * 64);
            event.timestampMs = nowMs;
            handleTouch(event);
        }
    }
#endif

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) updateShower(nowMs);
    flushShowerSave(nowMs);

    if (toast && static_cast<int32_t>(nowMs - toastUntil) >= 0) {
        toast = nullptr;
        requestRenderRows(
            sceneFlow.current() == AppSceneFlow::Scene::HOME
                ? HOME_STATUS_TOP
                : sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                    ? MAIN_MENU_CONTENT_TOP : MENU_HEADER_HEIGHT,
            AmoledUi::HEIGHT);
    }
    if (heartsUntil && static_cast<int32_t>(nowMs - heartsUntil) >= 0) {
        heartsUntil = 0;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU && !pointerDown &&
        std::fabs(menuVelocity) > 0.24f) {
        float previous = menuScroll;
        menuScroll += menuVelocity;
        menuVelocity *= 0.86f;
        clampMenuScroll();
        if (std::fabs(menuScroll - previous) > 0.02f) {
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
        } else {
            menuVelocity = 0.0f;
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
    } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
               debugPopup == DebugViewModel::Popup::NONE && !pointerDown &&
               std::fabs(debugVelocity) > 0.24f) {
        float previous = debugScroll;
        debugScroll += debugVelocity;
        debugVelocity *= 0.86f;
        clampDebugScroll();
        if (std::fabs(debugScroll - previous) > 0.02f) {
            requestRenderRows(0, 448);
        } else {
            debugVelocity = 0.0f;
        }
#endif
    } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
               computerPage == ComputerViewModel::Page::STORAGE &&
               !pointerDown && std::fabs(computerVelocity) > 0.24f) {
        float previous = computerScroll;
        computerScroll += computerVelocity;
        computerVelocity *= 0.86f;
        clampComputerScroll();
        if (std::fabs(computerScroll - previous) > 0.02f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else {
            computerVelocity = 0.0f;
        }
    }
#if STICKMON_HAS_CLAW
    else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
             computerPage == ComputerViewModel::Page::CLAW_SETUP &&
             clawLogView && !pointerDown &&
             std::fabs(clawLogVelocity) > 0.24f) {
        float previous = clawLogScroll;
        clawLogScroll += clawLogVelocity;
        clawLogVelocity *= 0.86f;
        clampClawLogScroll();
        if (std::fabs(clawLogScroll - previous) > 0.02f) {
            requestRenderRows(CLAW_LOG_TOP, 448);
        } else {
            clawLogVelocity = 0.0f;
        }
    }
#endif
    else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
              sceneFlow.current() == AppSceneFlow::Scene::SHOP) &&
               !inventoryController_.state().itemConfirmOpen && !pointerDown &&
               std::fabs(inventoryController_.state().itemVelocity) > 0.24f) {
        if (inventoryController_.tickScroll(itemMaxScroll())) {
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreAreaAnimating) {
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        } else if (explorePreviewMoving &&
                   nowMs - explorePreviewLastRenderRequestMs >= 33) {
            explorePreviewLastRenderRequestMs = nowMs;
            requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                              EXPLORE_PREVIEW_RENDER_BOTTOM);
        }
    }
    uint32_t idleTimeoutMs = 0;
    switch (gameState.settings.idleTimeoutIndex) {
    case 0: idleTimeoutMs = 30UL * 1000UL; break;
    case 1: idleTimeoutMs = 2UL * 60UL * 1000UL; break;
    case 2: idleTimeoutMs = 5UL * 60UL * 1000UL; break;
    case 3: idleTimeoutMs = 10UL * 60UL * 1000UL; break;
    default: break;
    }
    bool sleepSafeScene = displayLockAllowed() &&
                          sceneFlow.current() != AppSceneFlow::Scene::BATTLE &&
                          sceneFlow.current() != AppSceneFlow::Scene::SHOWER &&
                          sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION &&
                          !visitSession.busy();
#if STICKMON_ENABLE_DEBUG_FEATURES
    sleepSafeScene = sleepSafeScene && !debugContactPending &&
                     !debugContactEventPending &&
                     !debugContactEventWaitingForEntry &&
                     !debugContactActive &&
                     (debugContactMessageUntilMs == 0 ||
                      static_cast<int32_t>(nowMs - debugContactMessageUntilMs) >= 0);
#endif
    if (!lockRequested && !pointerDown && idleTimeoutMs > 0 &&
        sleepSafeScene && nowMs - lastInteractionMs >= idleTimeoutMs) {
        lockRequested = true;
        lastInteractionMs = nowMs;
    }
}

void AmoledApp::loadExplorePreview(uint32_t nowMs) {
    std::memset(explorePreviewSpeciesIds, 0,
                sizeof(explorePreviewSpeciesIds));
    std::memset(explorePreloadSpeciesIds, 0,
                sizeof(explorePreloadSpeciesIds));
    std::memset(explorePreviewFrames, 0, sizeof(explorePreviewFrames));
    std::memset(explorePreviewHidden, 0, sizeof(explorePreviewHidden));
    explorePreviewPool = ExplorePool::Pool{};
    explorePreloadSpeciesCount = 0;
    explorePreviewLoadPending = false;
    explorePreviewVisualCycle = UINT32_MAX;
    explorePreviewStartedAt = nowMs;
    explorePreviewNextLoadAt = nowMs + EXPLORE_PREVIEW_LOAD_DELAY_MS;

    uint8_t visibleAreaCount = std::min<uint8_t>(
        ExploreItemProgression::visibleAreaCount(gameState),
        Game::EXPLORE_AREA_COUNT);
    if (visibleAreaCount == 0) {
        clearExplorePreview();
        return;
    }
    if (selectedExploreArea >= visibleAreaCount) {
        selectedExploreArea = static_cast<uint8_t>(visibleAreaCount - 1);
    }

    if (ExploreItemProgression::isAreaUnlocked(
            selectedExploreArea, gameState)) {
        explorePreviewPool = buildExplorePreviewPool(
            gameState, selectedExploreArea);
        for (uint8_t index = 0; index < explorePreviewPool.count; ++index) {
            explorePreviewSpeciesIds[index] =
                explorePreviewPool.entries[index].speciesId;
        }
        PokemonSprites::setDynamicSceneSpecies(
            explorePreviewSpeciesIds, explorePreviewPool.count);
    } else {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    }

    // Keep the visible unlocked pools resident so changing the left rail does
    // not trigger a synchronous decode on the first frame after a tap.
    explorePreloadSpeciesCount = collectExplorePreviewSpecies(
        gameState, explorePreloadSpeciesIds, EXPLORE_PRELOAD_CAP,
        selectedExploreArea);
    PokemonSprites::setPinnedDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount);
    bool ready = PokemonSprites::preloadDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount, 0);
    explorePreviewLoadPending = !ready;
    refreshExplorePreviewFrames();
}

void AmoledApp::updateExplorePreviewLoading(uint32_t nowMs) {
    if (!explorePreviewLoadPending ||
        static_cast<int32_t>(nowMs - explorePreviewNextLoadAt) < 0) {
        return;
    }
    bool ready = PokemonSprites::preloadDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount, 1);
    refreshExplorePreviewFrames();
    explorePreviewLoadPending = !ready;
    explorePreviewNextLoadAt = nowMs +
        (ready ? EXPLORE_PREVIEW_BACKGROUND_LOAD_MS : 1);
}

void AmoledApp::refreshExplorePreviewFrames() {
    for (uint8_t index = 0; index < explorePreviewPool.count; ++index) {
        uint16_t speciesId = explorePreviewPool.entries[index].speciesId;
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::FRONT);
        if (!frame) {
            frame = PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::ICON_0);
        }
        explorePreviewFrames[index] = frame;
        explorePreviewHidden[index] =
            ExplorePool::isRare(explorePreviewPool.entries[index].rarity) &&
            !hasEncounteredSpecies(speciesId);
    }
}

void AmoledApp::clearExplorePreview() {
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    PokemonSprites::setPinnedDynamicSpecies(nullptr, 0);
    explorePreviewPool = ExplorePool::Pool{};
    std::memset(explorePreviewSpeciesIds, 0,
                sizeof(explorePreviewSpeciesIds));
    std::memset(explorePreloadSpeciesIds, 0,
                sizeof(explorePreloadSpeciesIds));
    std::memset(explorePreviewFrames, 0, sizeof(explorePreviewFrames));
    std::memset(explorePreviewHidden, 0, sizeof(explorePreviewHidden));
    explorePreloadSpeciesCount = 0;
    explorePreviewLoadPending = false;
}

void AmoledApp::selectExploreArea(uint8_t area, uint32_t nowMs) {
    uint8_t visibleAreaCount = std::min<uint8_t>(
        ExploreItemProgression::visibleAreaCount(gameState),
        Game::EXPLORE_AREA_COUNT);
    if (area >= visibleAreaCount || area == selectedExploreArea) return;
    selectedExploreArea = area;
    loadExplorePreview(nowMs);
}

void AmoledApp::applyExploreEffects(uint32_t nowMs) {
    const auto effects = exploreSession_.takeEffects();
    if (effects.invalidateWorld) renderCaches_.exploreWorld.invalidate();
    if (effects.mapChanged) resetExploreMapPresentation(nowMs);
    if (effects.statusChanged) requestRenderRows(HOME_STATUS_TOP, AmoledUi::HEIGHT);
    if (effects.fullRender) requestFullRender();
    char message[64];
    switch (effects.notice) {
    case ExploreSession::Notice::MAP_FAILED: setToast(Ui::Amoled::MAP_FAILED, nowMs); break;
    case ExploreSession::Notice::PET_REST: setToast(Ui::Menu::PET_REST, nowMs); break;
    case ExploreSession::Notice::BAG_FULL: setToast(Ui::Amoled::BAG_FULL, nowMs); break;
    case ExploreSession::Notice::COINS:
        std::snprintf(message, sizeof(message), Ui::Explore::PICKUP_COIN_FMT,
            static_cast<unsigned long>(effects.coins));
        setToast(message, nowMs, 1300);
        break;
    case ExploreSession::Notice::ITEM:
        std::snprintf(message, sizeof(message), Ui::Explore::PICKUP_FMT,
            Game::ShopService::shortName(effects.item));
        setToast(message, nowMs, 1300);
        break;
    case ExploreSession::Notice::NONE: break;
    }
    if (effects.save) saveState();
    if (effects.startBattle) startExploreBattle(effects.encounter, nowMs);
    if (effects.completed && autonomousExpedition) leaveExploreRoute();
    if (effects.startExit) beginExploreRouteExit(nowMs);
    if (effects.startStep) beginExploreRouteStep(nowMs);
}

bool AmoledApp::startExploreRoute(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    if (!exploreSession_.begin(gameState, selectedExploreArea)) return false;
    exploreSessionActive = true;
    applyExploreEffects(nowMs);

    // Keep every possible encounter for this area resident before walking
    // starts. This moves sprite decompression to route entry instead of a
    // random encounter frame.
    ExploreEncounters::Table encounterTable =
        ExploreEncounters::tableForArea(selectedExploreArea);
    constexpr uint8_t ROUTE_SPECIES_CAP = ExplorePool::MAX_SOURCE_ENTRIES + 1 +
        ExploreEcology::groupCompanionCapacity();
    uint16_t routeSpecies[ROUTE_SPECIES_CAP] = {};
    uint8_t routeSpeciesCount = 0;
    for (uint8_t index = 0; index < encounterTable.count; ++index) {
        uint16_t speciesId = encounterTable.entries[index].speciesId;
        if (speciesId == 0) continue;
        bool duplicate = false;
        for (uint8_t previous = 0; previous < routeSpeciesCount; ++previous) {
            if (routeSpecies[previous] == speciesId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && routeSpeciesCount <
            ExplorePool::MAX_SOURCE_ENTRIES) {
            routeSpecies[routeSpeciesCount++] = speciesId;
        }
    }
    if (route.exploreRouteBossScheduled && route.exploreRouteBossSpeciesId != 0) {
        bool bossAlreadyIncluded = false;
        for (uint8_t index = 0; index < routeSpeciesCount; ++index) {
            if (routeSpecies[index] == route.exploreRouteBossSpeciesId) {
                bossAlreadyIncluded = true;
                break;
            }
        }
        if (!bossAlreadyIncluded &&
            routeSpeciesCount < ExplorePool::MAX_SOURCE_ENTRIES + 1) {
            routeSpecies[routeSpeciesCount++] = route.exploreRouteBossSpeciesId;
        }
    }
    // A visible pool member may bring a companion absent from the encounter
    // table. Decode/pin that artwork at entry, outside walking frame loops.
    for (uint8_t index = 0; index < route.exploreRoutePool.count; ++index) {
        const auto* family = ExploreEcology::groupFamily(route.exploreRoutePool.entries[index].speciesId);
        if (!family) continue;
        for (uint8_t member = 0; member < family->count; ++member) {
            const uint16_t species = family->species[member];
            if (std::find(routeSpecies, routeSpecies + routeSpeciesCount, species) !=
                    routeSpecies + routeSpeciesCount) continue;
            if (routeSpeciesCount < ROUTE_SPECIES_CAP) routeSpecies[routeSpeciesCount++] = species;
        }
    }
    PokemonSprites::setPinnedDynamicSpecies(routeSpecies, routeSpeciesCount);
    PokemonSprites::preloadDynamicSpecies(routeSpecies, routeSpeciesCount);
    // Route entry runs under the departure black frame. Warm battle sounds
    // here so a random encounter cannot decode them before its first frame.
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_NORMAL);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_SUPER);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_WEAK);
    AudioManager::ins().preloadSfx(SfxCue::UI_CANCEL);
    AudioManager::ins().preloadSfx(SfxCue::UI_CONFIRM);

    if (!generateExploreRouteMap(nowMs)) return false;
    explorePresenter_.startRun(nowMs);
    const uint32_t exploredAt =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (gameState.team[slot].origin != Game::Origin::VISITOR) {
            gameState.team[slot].lastExploredAt = exploredAt;
        }
    }
    saveState();
    sceneFlow.enterExploreRoute();
    setMusicContext(MusicContext::EXPLORE);
    toast = nullptr;
    requestFullRender();
    return true;
}

bool AmoledApp::generateExploreRouteMap(uint32_t nowMs) {
    const bool generated = exploreSession_.generateMap(gameState, selectedExploreArea);
    applyExploreEffects(nowMs);
    return generated;
}

void AmoledApp::resetExploreMapPresentation(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    uint8_t hiddenPoolMask = 0;
    for (uint8_t i = 0; i < route.exploreRoutePool.count; ++i) {
        const auto& entry = route.exploreRoutePool.entries[i];
        if (ExplorePool::isRare(entry.rarity) && !hasEncounteredSpecies(entry.speciesId))
            hiddenPoolMask |= 1U << i;
    }
    explorePresenter_.resetMap(route, gameState, selectedExploreArea, hiddenPoolMask, nowMs);
    requestFullRender();
}

bool AmoledApp::beginExploreRouteStep(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    if (!exploreSession_.beginStep(gameState)) {
        applyExploreEffects(nowMs);
        return false;
    }
    explorePresenter_.beginStep(route, nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES && STICKMON_ENABLE_TRACE_LOGS
    const auto& path = route.exploreRouteMap.paths[route.exploreRoutePath];
    const ExploreMapGenerator::Point& fromPoint =
        path.points[route.exploreRouteIndex - 1];
    const ExploreMapGenerator::Point& targetPoint =
        path.points[route.exploreRouteIndex];
    Platform::logf(
        "[AmoledExploreDiag] step block=%u path=%u index=%u/%u "
        "tileFrom=(%u,%u) tileTarget=(%u,%u) "
        "from=(%.2f,%.2f) target=(%.2f,%.2f) delta=(%.2f,%.2f) "
        "dir=%u followerFrom=(%.2f,%.2f) followerTarget=(%.2f,%.2f) "
        "followerDir=%u\n",
        static_cast<unsigned>(route.exploreRouteMapBlock),
        static_cast<unsigned>(route.exploreRoutePath),
        static_cast<unsigned>(route.exploreRouteIndex),
        static_cast<unsigned>(path.pointCount),
        static_cast<unsigned>(fromPoint.x), static_cast<unsigned>(fromPoint.y),
        static_cast<unsigned>(targetPoint.x),
        static_cast<unsigned>(targetPoint.y),
        static_cast<double>(explorePresenter_.state().exploreRouteFromX),
        static_cast<double>(explorePresenter_.state().exploreRouteFromY),
        static_cast<double>(explorePresenter_.state().exploreRouteTargetX),
        static_cast<double>(explorePresenter_.state().exploreRouteTargetY),
        static_cast<double>(explorePresenter_.state().exploreRouteTargetX - explorePresenter_.state().exploreRouteFromX),
        static_cast<double>(explorePresenter_.state().exploreRouteTargetY - explorePresenter_.state().exploreRouteFromY),
        static_cast<unsigned>(explorePresenter_.state().exploreRouteDirection),
        static_cast<double>(explorePresenter_.state().exploreRouteFollowerFromX),
        static_cast<double>(explorePresenter_.state().exploreRouteFollowerFromY),
        static_cast<double>(explorePresenter_.state().exploreRouteFollowerTargetX),
        static_cast<double>(explorePresenter_.state().exploreRouteFollowerTargetY),
        static_cast<unsigned>(explorePresenter_.state().exploreRouteFollowerDirection));
#endif

    return true;
}

void AmoledApp::beginExploreRouteExit(uint32_t nowMs) {
    if (!exploreSession_.beginExit()) return;
    explorePresenter_.beginExit(exploreSession_.state(), ExploreSession::hasCompanion(gameState), nowMs);
    requestFullRender();
}


void AmoledApp::updateExploreEcology(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    const bool active = sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE &&
        !route.exploreRoutePaused && !route.exploreRouteComplete && !route.exploreRouteExitConfirm &&
        !route.exploreRouteExiting && !expeditionFade.active();
    explorePresenter_.updateEcology(route, gameState, selectedExploreArea, active, nowMs);
    applyExplorePresentationDamage();
}

void AmoledApp::updateExploreRoute(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        route.exploreRoutePaused || route.exploreRouteComplete) return;
    if (route.exploreRoutePendingFrostFall) {
        exploreSession_.updatePendingFall(gameState, nowMs);
        applyExploreEffects(nowMs);
        return;
    }
    const bool exiting = route.exploreRouteExiting;
#if STICKMON_ENABLE_DEBUG_FEATURES && STICKMON_ENABLE_TRACE_LOGS
    const int previousCameraX = explorePresenter_.state().exploreRouteCameraX;
    const int previousCameraY = explorePresenter_.state().exploreRouteCameraY;
#endif
    const auto motion = explorePresenter_.updateMotion(route, gameState, selectedExploreArea, nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES && STICKMON_ENABLE_TRACE_LOGS
    const auto& presentation = explorePresenter_.state();
    if (previousCameraX != presentation.exploreRouteCameraX ||
        previousCameraY != presentation.exploreRouteCameraY) {
        Platform::logf("[AmoledExploreDiag] camera world=(%.2f,%.2f) old=(%d,%d) next=(%d,%d)\n",
            static_cast<double>(presentation.exploreRouteWorldX),
            static_cast<double>(presentation.exploreRouteWorldY),
            previousCameraX, previousCameraY,
            int(presentation.exploreRouteCameraX), int(presentation.exploreRouteCameraY));
    }
#endif
    applyExplorePresentationDamage();
    if (exiting) {
        if (motion == ExplorePresenter::Motion::EXIT_FINISHED) {
            exploreSession_.finishExit(gameState);
            applyExploreEffects(nowMs);
            requestFullRender();
        }
        return;
    }
    if (motion == ExplorePresenter::Motion::STEP_FINISHED) {
        exploreSession_.completeStep(gameState, nowMs);
        applyExploreEffects(nowMs);
        requestExploreRouteDynamicRender();
    } else if (!route.exploreRouteMoving &&
        (route.exploreRouteAutoWalk || route.exploreRoutePlayerWalkActive)) {
        beginExploreRouteStep(nowMs);
    }
}

ExploreRouteViewModel AmoledApp::exploreRouteModel() const {
    return explorePresenter_.model(exploreSession_.state(), gameState, selectedExploreArea,
        Platform::clock().millis(), externalSceneFade ? 0 : expeditionFade.alpha(), toast);
}

void AmoledApp::requestExploreRouteDynamicRender() {
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE) return;
    explorePresenter_.requestDynamicDamage(exploreRouteModel());
    applyExplorePresentationDamage();
}

void AmoledApp::applyExplorePresentationDamage() {
    const auto damage = explorePresenter_.takeDamage();
    if (!damage.valid) return;
    const auto& r = damage.bounds;
    requestRenderRect(r.x, r.y, r.x + r.width, r.y + r.height);
}



bool AmoledApp::beginExploreEncounter(uint32_t nowMs, bool boss, uint16_t speciesOverride,
    uint8_t levelOverride, uint16_t experiencePercent, ExploreSpecial::Kind specialKind) {
    const bool ready = exploreSession_.beginEncounter(gameState, selectedExploreArea,
        boss, speciesOverride, levelOverride, experiencePercent, specialKind);
    applyExploreEffects(nowMs);
    return ready;
}

void AmoledApp::startExploreBattle(const ExploreSession::Encounter& encounter, uint32_t nowMs) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t encounterStartMs = Platform::clock().millis();
#endif
    const auto& wild = encounter.wild;
    const auto leaderSlot = encounter.leaderSlot;
    const auto boss = encounter.boss;
    const auto experiencePercent = encounter.experiencePercent;
    const auto specialKind = encounter.special;
    pauseExploreRoute(nowMs);
    recordEncounteredSpecies(wild.speciesId);
    uint16_t dynamicSpecies[] = {
        gameState.team[leaderSlot].speciesId, wild.speciesId,
    };
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t spritePrepStartedMs = Platform::clock().millis();
#endif
    PokemonSprites::setDynamicSceneSpecies(dynamicSpecies, 2);
    PokemonSprites::preloadDynamicSpecies(dynamicSpecies, 2);
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t spritePrepMs =
        Platform::clock().millis() - spritePrepStartedMs;
#endif
    battleSession_.begin(gameState, wild, leaderSlot, boss, experiencePercent,
                         specialKind, selectedExploreArea, nowMs);
    inventoryController_.setBattleBagMode(false);
    // Also cover debug battles, which can start without entering a route.
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t sfxPrepStartedMs = Platform::clock().millis();
#endif
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_NORMAL);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_SUPER);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_WEAK);
    AudioManager::ins().preloadSfx(SfxCue::UI_CANCEL);
    AudioManager::ins().preloadSfx(SfxCue::UI_CONFIRM);
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t sfxPrepMs = Platform::clock().millis() - sfxPrepStartedMs;
#endif
    char message[64];
    std::snprintf(message, sizeof(message), Ui::Amoled::WILD_FMT, findSpecies(wild.speciesId)->name);
    battleSession_.pushLog(message, nowMs);
    applyBattleEffects(nowMs);
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::BATTLE);
    setMusicContext(boss ? MusicContext::BATTLE_SPECIAL
                         : MusicContext::BATTLE);
    requestFullRender();
#if STICKMON_ENABLE_DEBUG_FEATURES
    encounterStartedMs = encounterStartMs;
    encounterPreparedMs = Platform::clock().millis() - encounterStartMs;
    encounterSpritePrepMs = spritePrepMs;
    encounterSfxPrepMs = sfxPrepMs;
    encounterFirstFramePending = true;
#endif
}

void AmoledApp::performBattleAttack(uint32_t nowMs) {
    battleSession_.performBattleAttack(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::performBattlePlayerAction(
    const BattleTurnController::Action& action, uint32_t nowMs) {
    battleSession_.performBattlePlayerAction(gameState, action, nowMs);
    applyBattleEffects(nowMs);
}

bool AmoledApp::resolveBattleFaint(uint32_t nowMs) {
    const bool result = battleSession_.resolveBattleFaint(gameState, nowMs);
    applyBattleEffects(nowMs);
    return result;
}

void AmoledApp::performBattleSwitch(uint8_t teamSlot, bool consumesTurn,
                                    uint32_t nowMs) {
    battleSession_.performBattleSwitch(gameState, teamSlot, consumesTurn, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::performBattleWildTurn(uint32_t nowMs) {
    battleSession_.performBattleWildTurn(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::performBattleWildAction(
    const BattleTurnController::Action& action, uint32_t nowMs) {
    battleSession_.performBattleWildAction(gameState, action, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::performBattlePlannedAction(uint32_t nowMs) {
    battleSession_.performBattlePlannedAction(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::advanceBattleTurn(uint32_t nowMs) {
    battleSession_.advanceBattleTurn(gameState, nowMs);
    applyBattleEffects(nowMs);
}

int8_t AmoledApp::availableBattleSwitchSlot() const {
    return battleSession_.availableBattleSwitchSlot(gameState);
}

uint8_t AmoledApp::battleHpPercentForRender(
    bool wildSide, uint16_t currentHp, uint16_t maxHp,
    uint32_t nowMs) const {
    return battleSession_.timeline().hpPercent(wildSide, currentHp, maxHp, nowMs);
}

uint32_t AmoledApp::battleExperienceForRender(uint32_t nowMs) const {
    return battleSession_.timeline().experience(nowMs);
}

void AmoledApp::performBattleBag(uint32_t nowMs) {
    if (Game::ItemInventory::homeBagItemCount(gameState) == 0) {
        setToast(Ui::Amoled::NOTHING, nowMs);
        return;
    }
    battleSession_.showAction();
    battleSession_.press(0xFF);
    openItemScene(AppSceneFlow::Scene::BAG);
    inventoryController_.setBattleBagMode(true);
}

void AmoledApp::performBattleBagItem(Game::ItemId item, uint32_t nowMs) {
    if (!inventoryController_.state().battleBagMode || sceneFlow.current() != AppSceneFlow::Scene::BAG) {
        return;
    }
    const int8_t foodIndex = Game::foodIndexForItemId(item);
    if (foodIndex < 0 && !Game::ItemInventory::usableInBattle(item)) {
        setToast(Ui::Amoled::CANNOT_USE, nowMs);
        return;
    }
    if (foodIndex >= 0) {
        if (!battleSession_.throwFood(gameState, item, nowMs)) {
            setToast(Ui::Room::FOOD_NO_STOCK, nowMs);
            return;
        }
        saveState();
        inventoryController_.stopScroll();
        inventoryController_.clearAction();
        inventoryController_.cancelTouch();
        inventoryController_.setBattleBagMode(false);
        sceneFlow.closeSubScene();
        AudioManager::ins().playSfx(SfxCue::THROW);
        battleSession_.throwAudioPlayed(Platform::clock().millis());
        applyBattleEffects(nowMs);
        requestFullRender();
        return;
    }
    if (Game::ItemInventory::count(gameState, item) == 0) {
        setToast(Ui::Amoled::NO_STOCK, nowMs);
        return;
    }
    inventoryController_.beginItemTarget(item, AppSceneFlow::Scene::BATTLE);
    openTeamScene(true);
}

uint16_t AmoledApp::teamItemHpForRender(uint32_t nowMs) const {
    const uint32_t elapsed = std::min<uint32_t>(
        BATTLE_HP_ANIMATION_MS, nowMs - inventoryController_.state().teamItemStartedMs);
    const int32_t distance = static_cast<int32_t>(inventoryController_.state().teamItemHpTo) - inventoryController_.state().teamItemHpFrom;
    return static_cast<uint16_t>(static_cast<int32_t>(inventoryController_.state().teamItemHpFrom) +
        distance * static_cast<int32_t>(elapsed) / BATTLE_HP_ANIMATION_MS);
}

void AmoledApp::finishTeamItemUse(uint32_t nowMs) {
    inventoryController_.finishTargetSelection();
    teamController_.pressMember(-1);
    pointerDown = false;
    dragging = false;
    if (inventoryController_.state().battleBagMode) {
        inventoryController_.setBattleBagMode(false);
        sceneFlow.closeSubScene();
        toast = nullptr;
        battleSession_.itemUsed(nowMs);
        applyBattleEffects(nowMs);
    } else {
        sceneFlow.enter(AppSceneFlow::Scene::BAG);
    }
    requestFullRender();
}

void AmoledApp::performBattleFlee(uint32_t nowMs) {
    battleSession_.performBattleFlee(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::finishBattleVictory(uint32_t nowMs) {
    battleSession_.finishBattleVictory(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::resolveBattleFriendship(uint8_t choice, uint32_t nowMs) {
    battleSession_.resolveBattleFriendship(gameState, choice, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::finishBattleAfterFriendship(uint32_t nowMs) {
    battleSession_.finishBattleAfterFriendship(gameState, nowMs);
    applyBattleEffects(nowMs);
}

void AmoledApp::queueBattleProgression(uint8_t teamSlot, uint8_t oldLevel,
                                       bool leveledUp) {
    battleSession_.queueBattleProgression(gameState, teamSlot, oldLevel, leveledUp);
}

bool AmoledApp::startNextBattleProgression(uint32_t nowMs) {
    const bool result = battleSession_.startNextBattleProgression(gameState, nowMs);
    applyBattleEffects(nowMs);
    return result;
}

void AmoledApp::finishBattleDefeat(uint32_t nowMs) {
    if (!battleSession_.finishDefeat(gameState)) return;
    saveState();
    exploreSessionActive = false;
    autonomousExpedition = false;
    exploreSession_.stopRoute();
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    PokemonSprites::setPinnedDynamicSpecies(nullptr, 0);
    // Defeat returns directly from battle to the room instead of using the
    // normal expedition return transition. Clear departure-only visibility
    // state so fainted team members can be rendered resting at home.
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    expeditionCompanionDeparting = false;
    placeFaintedHomeActors(nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleActive) {
        debugBattleActive = false;
        sceneFlow.enter(AppSceneFlow::Scene::DEBUG);
    } else {
        sceneFlow.goHome();
    }
#else
    sceneFlow.goHome();
#endif
    setMusicContext(MusicContext::HOME);
    battleSession_.reset();
    setToast(Ui::Amoled::REST_AT_HOME, nowMs, 1500);
    requestFullRender();
}

void AmoledApp::closeBattle(uint32_t nowMs) {
    // Persist the complete result before resuming the route, including flee
    // damage, rewards, contacts and any completed progression choices.
    saveState();
    bool returnToDebug = false;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleActive) {
        returnToDebug = true;
        debugBattleActive = false;
    }
#endif
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    battleSession_.reset();
    inventoryController_.setBattleBagMode(false);
    // Battle and growth can keep the route hidden for minutes. Rebase its
    // presentation deadlines after storage/cache work, before walking resumes.
    nowMs = Platform::clock().millis();
    resumeExploreRoute(nowMs);
    exploreSession_.battleClosed();
    sceneFlow.enter(returnToDebug ? AppSceneFlow::Scene::DEBUG
                                  : AppSceneFlow::Scene::EXPLORE_ROUTE);
    setMusicContext(returnToDebug ? MusicContext::HOME
                                 : MusicContext::EXPLORE);
    requestFullRender();
}

void AmoledApp::applyBattleEffects(uint32_t nowMs) {
    const auto effects = battleSession_.takeEffects();
    requestRenderRows(effects.rowBegin, effects.rowEnd);
    requestRenderRows(effects.timeline.rowBegin, effects.timeline.rowEnd);
    if (effects.timeline.sfx != 0xFF)
        AudioManager::ins().playSfx(static_cast<SfxCue>(effects.timeline.sfx));
    if (effects.syncTeam) {
        uint16_t speciesIds[Game::TEAM_CAP] = {};
        const uint8_t count = Game::TeamRoster::memberCount(gameState);
        for (uint8_t slot = 0; slot < count; ++slot) speciesIds[slot] = gameState.team[slot].speciesId;
        PokemonSprites::syncTeamCache(speciesIds, count);
    }
    if (effects.preloadSwitch) {
        const auto& battle = battleSession_.state();
        const uint16_t species[] = {gameState.team[battle.battlePlayerSlot].speciesId,
                                    battle.battleWild.speciesId};
        PokemonSprites::setDynamicSceneSpecies(species, 2);
        PokemonSprites::preloadDynamicSpecies(species, 2);
        battleSession_.switchPreloaded(Platform::clock().millis());
        const auto acknowledged = battleSession_.takeEffects();
        requestRenderRows(acknowledged.timeline.rowBegin, acknowledged.timeline.rowEnd);
    }
    if (effects.stopAutonomous) autonomousExpedition = false;
    if (effects.openProgression)
        openProgressionScene(AppSceneFlow::Scene::EXPLORE_ROUTE,
            effects.progressionSlot, effects.progressionOldLevel, nowMs);
    if (effects.save) saveState();
    if (effects.close) {
        closeBattle(nowMs);
        if (effects.resumeAutonomous && autonomousExpedition) exploreSession_.setAutoWalk(true);
    }
    if (effects.toast) setToast(effects.toast, nowMs);
}


void AmoledApp::pauseExploreRoute(uint32_t nowMs) {
    exploreSession_.pause(nowMs);
}

void AmoledApp::resumeExploreRoute(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    if (!route.exploreRoutePaused) return;
    const uint32_t paused = exploreSession_.resume(nowMs);
    explorePresenter_.resume(route, paused, nowMs);
}

void AmoledApp::leaveExploreRoute() {
    if (expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        beginExploreReturn(Platform::clock().millis());
    }
}

void AmoledApp::completeExploreReturn() {
    autonomousExpedition = false;
    PokemonSprites::setPinnedDynamicSpecies(nullptr, 0);
    exploreSession_.finishReturn(gameState);
    applyExploreEffects(Platform::clock().millis());
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind == 3) {
        Platform::logf("[FriendDiag] home-return kind=3 team=%u phase=%u\n",
                       static_cast<unsigned>(gameState.teamCount),
                       static_cast<unsigned>(expeditionDeparturePhase));
        completeDebugContact(Platform::clock().millis());
    }
#endif
    sceneFlow.goHome();
    setMusicContext(MusicContext::HOME);
    requestFullRender();
}

void AmoledApp::placeFaintedHomeActors(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    if (gameState.teamCount > 0 &&
        (gameState.team[0].fainted || gameState.team[0].hpCur == 0)) {
        homeMainActor.x = homeMainActor.targetX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        homeMainActor.y = homeMainActor.targetY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        homeMainActor.faintRestActive = true;
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        petDirection = PokemonSprites::WalkDirection::DOWN;
        petFrame = 0;
        startSleepAnimation(nowMs, 700);
        homeMainActor.route.clear();
        homeMainActor.velocityX = homeMainActor.velocityY = 0.0f;
        homeRuntime.transition(0, Home::Task::FAINTED, nowMs, 0, true);
    }
    if (gameState.teamCount > 1 && homeCompanionActor.active &&
        (gameState.team[1].fainted || gameState.team[1].hpCur == 0)) {
        float sleepX = homeCompanionActor.x;
        float sleepY = homeCompanionActor.y;
        if (chooseCompanionSleepSpot(sleepX, sleepY)) {
            homeCompanionActor.x = sleepX;
            homeCompanionActor.y = sleepY;
        }
        homeCompanionActor.targetX = homeCompanionActor.x;
        homeCompanionActor.targetY = homeCompanionActor.y;
        homeCompanionActor.route.clear();
        homeCompanionActor.velocityX = homeCompanionActor.velocityY = 0.0f;
        homeCompanionActor.faintRestActive = true;
        homeRuntime.transition(1, Home::Task::FAINTED, nowMs, 0, true);
        companionFrame = 0;
        nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
    }
}

void AmoledApp::updateClockAndCare(uint32_t nowMs) {
    const auto& route = exploreSession_.state();
    Game::CareExperience::prepareDay(gameState);
    uint16_t previousMinuteOfDay = static_cast<uint16_t>(
        gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
    bool previousNight = previousMinuteOfDay < 6U * 60U ||
                         previousMinuteOfDay >= 18U * 60U;
    bool clockChanged =
        gameClock.sync(nowMs, gameSpeed(), gameState.gameMinutesTotal);
    if (clockChanged) {
        Game::resetDailyCareCounters(gameState);
        requestRenderRows(0, HOME_HEADER_HEIGHT);
        uint16_t minuteOfDay = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        bool night = minuteOfDay < 6U * 60U || minuteOfDay >= 18U * 60U;
        if (night != previousNight) {
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE &&
        !route.exploreRoutePaused && !route.exploreRouteComplete && !route.exploreRouteExitConfirm &&
        !route.exploreRouteExiting && !expeditionFade.active()) {
        explorePresenter_.updateDaylight(gameState.gameMinutesTotal, selectedExploreArea);
        applyExplorePresentationDamage();
    }

    uint32_t elapsedMs = nowMs - lastCareMs;
    if (elapsedMs >= CARE_TICK_MS) {
        uint32_t elapsedMinutes = elapsedMs / CARE_TICK_MS;
        lastCareMs += elapsedMinutes * CARE_TICK_MS;
        const bool homeRecoveryActive =
            !exploreSessionActive &&
            sceneFlow.current() != AppSceneFlow::Scene::BATTLE;
        Game::applyCareMinutes(gameState, careAcc, elapsedMinutes,
                               elapsedMinutes * gameSpeed(), homeRecoveryActive);
        if (gameState.teamCount > 0) requestRenderRows(HOME_STATUS_TOP, 448);
    }

    if (!pointerDown && !saveWritesBlocked && !saveCoordinator_.dirty() &&
        !exploreSessionActive && sceneFlow.current() != AppSceneFlow::Scene::BATTLE &&
        (clockChanged || elapsedMs >= CARE_TICK_MS) &&
        nowMs - lastPersistMs >= PERIODIC_SAVE_MS) {
        saveState();
    }
    // Failed event saves remain pending across scenes. Avoid storage writes
    // during an active gesture; the coordinator bounds the retry frequency.
    if (!pointerDown && saveCoordinator_.due(nowMs)) saveState();
}

void AmoledApp::updateMoodHearts(uint32_t nowMs) {
    const uint8_t nextHeartCount = gameState.teamCount > 0
        ? moodHeartCountFor(gameState.team[0].mood) : 0;
    if (nextHeartCount < moodHeartCount &&
        sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        // Animate the first heart that disappeared. A single burst also
        // handles a larger drop caused by a delayed care tick.
        moodBurstHeart = nextHeartCount;
        moodBurstStartedMs = nowMs;
        moodBurstUntilMs = nowMs + MOOD_BURST_DURATION_MS;
        nextMoodBurstFrameMs = nowMs;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
    }
    moodHeartCount = nextHeartCount;

    if (moodBurstUntilMs == 0) return;
    if (static_cast<int32_t>(nowMs - moodBurstUntilMs) >= 0) {
        moodBurstUntilMs = 0;
        moodBurstHeart = 0xFF;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
        return;
    }
    if (static_cast<int32_t>(nowMs - nextMoodBurstFrameMs) >= 0) {
        nextMoodBurstFrameMs = nowMs + MOOD_BURST_FRAME_MS;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
    }
}

void AmoledApp::startSleepAnimation(uint32_t nowMs, uint16_t frameMs) {
    PokemonSprites::PetAnimationProfile profile{};
    PokemonSprites::petAnimationProfile(gameState.team[0].speciesId, profile);
    uint8_t expanded = 0;
    PokemonSprites::FrameBounds largest{};
    for (uint8_t i = 0; i < profile.sleepingFrames; ++i) {
        const auto* frame = PokemonSprites::findSpeciesSprite(
            gameState.team[0].speciesId, static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(profile.sleepingBase) + i));
        const auto bounds = PokemonSprites::frameVisibleBounds(frame);
        if (bounds.height > largest.height ||
            (bounds.height == largest.height && bounds.width > largest.width)) {
            expanded = i;
            largest = bounds;
        }
    }
    sleepBreath.start(nowMs, frameMs, profile.sleepingFrames, expanded);
    petFrame = sleepBreath.frame(nowMs);
}

void AmoledApp::updateSleepAnimation(uint32_t nowMs) {
    const uint8_t frame = sleepBreath.frame(nowMs);
    if (frame == petFrame) return;
    petFrame = frame;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::updatePet(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        lastPetUpdateMs = nowMs;
        return;
    }
    if (gameState.teamCount == 0) return;
    syncHomeActors(nowMs);
    // Actor presentation state and the shared coordinator can be touched by
    // scene transitions, save restore, and visitor teardown. Repair stale
    // leases/routes before the next decision is allowed to claim a resource.
    homeRuntime.beginTick(nowMs);
    if (visitAwayFlow_.state().phase != VisitDeparture::NONE &&
        updateVisitDeparture(nowMs)) {
        lastPetUpdateMs = nowMs;
        return;
    }
    if (visitSession.visitorArrivalReady() &&
        homeCompanionActor.active && homeCompanionActor.hidden &&
        visitorFlow_.state().motion == VisitorMotion::NONE &&
        gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR) {
        Platform::logf("[VisitArrival] departed species=%u\n",
                       static_cast<unsigned>(homeCompanionActor.speciesId));
        beginVisitorEntry(nowMs);
    }
#if STICKMON_HAS_CLAW
    // External ownership must be visible to the shared policy before food
    // arbitration. An already committed feeding transaction may still finish.
    homeRuntime.setControlOwner(
        0, Stickmon::ClawRuntime::instance().autonomyActive()
               ? Home::ControlOwner::CLAW
               : Home::ControlOwner::AUTONOMOUS);
#endif
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (static_cast<int32_t>(nowMs - nextHomeStateDiagnosticMs) >= 0) {
        debugReportHomeState();
        nextHomeStateDiagnosticMs = nowMs + 5000;
    }
#endif
    // Keep an unowned serving actionable. A route failure releases the bowl,
    // and food restored from a save has no lease; either case must return to
    // arbitration so an actor blocking the approach can be asked to yield.
    if (!homeFoodArbitrationPending &&
        gameState.room.bowlCount > 0 &&
        homeRuntime.owner(Home::Resource::BOWL) < 0 &&
        preferredBowlEater(nowMs) >= 0) {
        homeFoodArbitrationPending = true;
        nextHomeFoodArbitrationMs = nowMs;
    }
    serviceHomeFoodArbitration(nowMs);
    updateCompanion(nowMs);
    if (visitorFlow_.state().motion == VisitorMotion::HOST_APPROACH ||
        visitorFlow_.state().motion == VisitorMotion::ENTERING ||
        visitorFlow_.state().motion == VisitorMotion::EXITING) {
        lastPetUpdateMs = nowMs;
        return;
    }
    float elapsedSeconds = static_cast<float>(nowMs - lastPetUpdateMs) / 1000.0f;
    lastPetUpdateMs = nowMs;
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);
    if (updatePairInteraction(nowMs, elapsedSeconds)) return;
    // Clearing a shared resource is part of the committed food transaction,
    // not an autonomous choice. It must finish even if Claw takes control.
    if (homeFlow_.state().action == RoomAction::FEED_FINISH &&
        updateRoomAction(nowMs, elapsedSeconds)) {
        return;
    }
#if STICKMON_HAS_CLAW
    if (!homeRuntime.autonomousAllowed(0)) {
        // Keep care/clock updates running, but do not let MonsterMind start a
        // competing walk or feed while the Agent owns the idle turn.
        cancelRoomAction(nowMs);
        lastPetUpdateMs = nowMs;
        return;
    }
#endif
    Game::MonsterRuntime& monster = gameState.team[0];
    const Home::ActorIntent initialMainIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(0, nowMs),
            gameState.room.bowlCount > 0);
    if (initialMainIntent == Home::ActorIntent::FAINT_REST) {
        cancelRoomAction(nowMs);
        if (homeRuntime.pairActive()) cancelPairInteraction(nowMs);
        homeRuntime.transition(0, Home::Task::FAINTED, nowMs, 0, true);
        RoomResource& room = RoomResource::ins();
        const float restX = room.available() ? static_cast<float>(room.bedX())
                                              : 76.0f;
        const float restY = room.available() ? static_cast<float>(room.bedY())
                                              : 99.0f;
        if (!petIsResting() || std::fabs(homeMainActor.x - restX) > 0.01f ||
            std::fabs(homeMainActor.y - restY) > 0.01f) {
            homeMainActor.faintRestActive = true;
            homeMainActor.x = homeMainActor.targetX = restX;
            homeMainActor.y = homeMainActor.targetY = restY;
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            petDirection = PokemonSprites::WalkDirection::DOWN;
            petFrame = 0;
            startSleepAnimation(nowMs, 700);
            updateCamera();
            Platform::logf("[AmoledPet] faint rest started bed=%.1f,%.1f\n",
                           restX, restY);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else {
            updateSleepAnimation(nowMs);
        }
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        return;
    }

    const Game::SpeciesCareProfile mainCare =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool mainSleepTime = mainCare.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    if (homeMainActor.task == Home::Task::SLEEPING) {
        const Home::ActorIntent sleepIntent =
            Home::ActorController::survivalIntent(
                homeActorObservation(0, nowMs),
                gameState.room.bowlCount > 0);
        const bool wakeForFood =
            sleepIntent == Home::ActorIntent::WAKE_FOR_FOOD;
        if (sleepIntent == Home::ActorIntent::WAKE || wakeForFood) {
            homeMainActor.faintRestActive = false;
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            petFrame = 0;
            homeRuntime.release(Home::Resource::BED, 0);
            homeRuntime.stop(0, nowMs, wakeForFood ? 0 : 700);
            homeMainActor.mind.onRested(nowMs);
            homeMainActor.nextDecisionMs = wakeForFood ? nowMs : nowMs + 700;
            homeMainActor.nextMindUpdateMs = nowMs;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else {
            updateSleepAnimation(nowMs);
            return;
        }
    }

    if (homeMainActor.faintRestActive) {
        homeMainActor.faintRestActive = false;
        homeRuntime.stop(0, nowMs);
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        petFrame = 0;
        nextPetFrameMs = nowMs + 520;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        homeMainActor.mind.reset(nowMs);
        schedulePetDecision(nowMs);
        Platform::logLine("[AmoledPet] faint rest complete; waking at bed");
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }

    if (static_cast<int32_t>(nowMs - homeMainActor.nextMindUpdateMs) >= 0) {
        homeMainActor.mind.update(monster, mainSleepTime,
                           mainCare.needsFood && gameState.room.bowlCount > 0,
                           nowMs);
        homeMainActor.nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
    }

    // A sleep pose is persisted at the bed center. When the sleep window has
    // ended, the actor must leave that pose before normal idle decisions can
    // run; otherwise STARE/turning can keep it visually stranded on the bed.
    const bool atBedPose = std::fabs(homeMainActor.x -
        (RoomResource::ins().available()
             ? static_cast<float>(RoomResource::ins().bedX()) : 76.0f)) <= 2.5f &&
        std::fabs(homeMainActor.y -
        (RoomResource::ins().available()
             ? static_cast<float>(RoomResource::ins().bedY()) : 99.0f)) <= 6.0f;
    if (!mainSleepTime && petMotion() == PetMotion::IDLE && atBedPose &&
        homeMainActor.task == Home::Task::IDLE) {
        float wakeX = homeMainActor.x;
        float wakeY = homeMainActor.y;
        if (chooseWakeTarget(wakeX, wakeY) &&
            beginPetMove(PetMotion::WANDERING, wakeX, wakeY, nowMs)) {
            Platform::logf(
                "[HomeSleep] event=leave_bed from=%.1f,%.1f target=%.1f,%.1f\n",
                homeMainActor.x, homeMainActor.y, wakeX, wakeY);
            return;
        }
        homeMainActor.nextDecisionMs = nowMs + 1000;
        if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeSleep] event=leave_bed_failed pos=%.1f,%.1f\n",
                homeMainActor.x, homeMainActor.y);
            nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
        }
        return;
    }

    const bool urgentNeed = monster.hpMax == 0 ||
        static_cast<uint32_t>(monster.hpCur) * 100UL <=
            static_cast<uint32_t>(monster.hpMax) * 25UL ||
        monster.satiety <= 25 || mainSleepTime ||
        (gameState.room.bowlCount > 0 &&
         monster.satiety < MONSTER_FEED_TARGET_SATIETY &&
         homeMainActor.mind.topDesire() == MonsterDesire::EAT);
    if (homeFlow_.state().action != RoomAction::NONE && urgentNeed) {
        cancelRoomAction(nowMs);
        homeMainActor.nextDecisionMs = nowMs;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    updateDebugPairChase(nowMs, elapsedSeconds);
    if (debugTiltControl) {
        cancelRoomAction(nowMs);
        homeRuntime.stop(0, nowMs);
        petStopping = false;
        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        if (Platform::imu().readAcceleration(ax, ay, az)) {
            auto applyDeadzone = [](float value) {
                if (std::fabs(value) < DEBUG_TILT_DEADZONE) return 0.0f;
                return std::clamp(value, -DEBUG_TILT_MAX, DEBUG_TILT_MAX) /
                       DEBUG_TILT_MAX;
            };
            float inputX = applyDeadzone(-ax);
            float inputY = applyDeadzone(ay);
            float length = std::sqrt(inputX * inputX + inputY * inputY);
            if (length > 1.0f) {
                inputX /= length;
                inputY /= length;
            }
            float nextX = homeMainActor.x + inputX * DEBUG_TILT_SPEED * elapsedSeconds;
            float nextY = homeMainActor.y + inputY * DEBUG_TILT_SPEED * 0.75f *
                                  elapsedSeconds;
            if (petFootprintInsideWalkArea(nextX, nextY)) {
                homeMainActor.x = nextX;
                homeMainActor.y = nextY;
                if (std::fabs(inputX) > 0.01f ||
                    std::fabs(inputY) > 0.01f) {
                    petDirection = petDirectionForDelta(inputX, inputY);
                    homeRuntime.transition(0, Home::Task::WANDER, nowMs);
                    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                        petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
                        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
                    }
                } else {
                    homeMainActor.motion = Home::MotionPhase::STILL;
                    petStopping = false;
                    petFrame = 0;
                }
                homeMainActor.targetX = homeMainActor.x;
                homeMainActor.targetY = homeMainActor.y;
                updateCamera();
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            }
        }
        return;
    }
#endif

    if (homeFlow_.state().action != RoomAction::NONE &&
        updateRoomAction(nowMs, elapsedSeconds)) {
        return;
    }
    if (ambientActionAllowed()) {
        const bool started = homeFlow_.tryAmbient(
            homeActionLayout(homeFlow_.needsWindowAnchor()), petDirection, nowMs);
        applyHomeFlowEffects(nowMs);
        if (started) return;
    }

    if (petMotion() == PetMotion::TURNING) {
        if (static_cast<int32_t>(nowMs - homeMainActor.taskUntilMs) < 0) return;
        homeRuntime.finishTurn(0, nowMs);
        petStopping = false;
        petFrame = 0;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (petMotion() == PetMotion::EATING) {
        bool canContinue = homeRuntime.owner(Home::Resource::BOWL) == 0 &&
                           gameState.room.bowlCount > 0 &&
                           monster.satiety < MONSTER_FEED_TARGET_SATIETY;
        if (canContinue &&
            static_cast<int32_t>(nowMs - nextFeedBiteMs) >= 0) {
            FoodConsumeResult result =
                Game::HomeCare::consumeBowlFood(gameState, 0);
            nextFeedBiteMs = nowMs + GameRandom::range(1000, 1601);
            if (result.consumed) {
                applyHomeCareExperience(0, result.careExp);
                Platform::logf(
                    "[AmoledApp] auto-feed satiety=%u->%u bites=%u\n",
                    static_cast<unsigned>(result.satietyBefore),
                    static_cast<unsigned>(result.satietyAfter),
                    static_cast<unsigned>(gameState.room.bowlBitesRemaining));
                if (result.reaction == FoodReaction::LIKED ||
                    result.foodIndex == Game::ROOM_TASTY_FOOD_INDEX) {
                    heartsUntil = nowMs + 900;
                }
                requestRenderRows(HOME_ROOM_TOP, 448);
                saveState();
            } else {
                homeMainActor.taskUntilMs = nowMs;
            }
        }
        if (!canContinue ||
            static_cast<int32_t>(nowMs - homeMainActor.taskUntilMs) >= 0) {
            homeRuntime.beginBowlClearance(0, nowMs);
            // Release the feeding task before the bowl-clearance movement.
            homeRuntime.stop(0, nowMs);
            Platform::logf(
                "[HomeFood] event=release actor=0 stage=feed_complete "
                "satiety=%u bowl=%u companion=%u/%u\n",
                static_cast<unsigned>(monster.satiety),
                static_cast<unsigned>(gameState.room.bowlCount),
                gameState.teamCount > 1
                    ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
                static_cast<unsigned>(homeCompanionActor.task));
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            petFrame = 0;
            PokemonSprites::PetAnimationProfile profile{};
            if (PokemonSprites::petAnimationProfile(monster.speciesId,
                                                     profile)) {
                nextPetFrameMs = nowMs + profile.idleFrameMs;
            } else {
                nextPetFrameMs = nowMs + 520;
            }
            homeMainActor.mind.onAte(nowMs);
            startFeedFinish(nowMs);
            if (gameState.room.bowlCount > 0 &&
                preferredBowlEater(nowMs) >= 0) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs;
                homeCompanionActor.nextMindUpdateMs = nowMs;
                homeCompanionActor.nextDecisionMs = nowMs;
            }
            requestRenderRows(HOME_ROOM_TOP, 448);
        }
        return;
    }

    if (petMotion() == PetMotion::STOPPING) {
        PokemonSprites::PetAnimationProfile profile{};
        uint8_t stopFrames = 1;
        if (PokemonSprites::petAnimationProfile(monster.speciesId, profile) &&
            profile.motionMode == PokemonSprites::PetMotionMode::PINGPONG &&
            profile.walkingFrames >= 2) {
            stopFrames = 2;
        }
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) < 0) return;
        if (petFrame + 1 < stopFrames) {
            ++petFrame;
            nextPetFrameMs += MOTION_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return;
        }

        petFrame = 0;
        if (homeMainActor.task == Home::Task::SEEK_FOOD) {
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            petDirection = foodFacingDirection(homeMainActor.x, homeMainActor.y);
            homeRuntime.transition(
                0, Home::Task::FEEDING, nowMs, 0, true);
            nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
            homeMainActor.taskUntilMs = nowMs + GameRandom::range(3200, 5601);
        } else {
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            homeRuntime.stop(0, nowMs);
            homeMainActor.mind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            nextPetFrameMs = nowMs + profile.idleFrameMs;
        }
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (petMotion() == PetMotion::WANDERING ||
        petMotion() == PetMotion::SEEKING_FOOD ||
        petMotion() == PetMotion::SEEKING_SLEEP) {
        float waypointX = homeMainActor.targetX;
        float waypointY = homeMainActor.targetY;
        homeMainActor.route.current(waypointX, waypointY);
        const float dx = waypointX - homeMainActor.x;
        const float dy = waypointY - homeMainActor.y;
        petDirection = petDirectionForDelta(dx, dy);
        const Home::Task speedTask = petMotion() == PetMotion::WANDERING
            ? Home::Task::WANDER : homeMainActor.task;
        const float speed = Home::ActorController::routeSpeed(
            speedTask, homeMainActor.behavior, monster);
        const float previousY = homeMainActor.y;
        const int previousCameraX = static_cast<int>(std::lround(cameraX));
        const int previousCameraY = static_cast<int>(std::lround(cameraY));
        Home::RouteStep routeStep = homeRuntime.advanceRoute(
            0, nowMs, speed, elapsedSeconds, 0.8f,
            homeCompanionActor.active);
        if (routeStep == Home::RouteStep::ARRIVED) {
            finishPetMove(nowMs);
        } else if (routeStep == Home::RouteStep::BLOCKED) {
            const bool seekingFood = petMotion() == PetMotion::SEEKING_FOOD;
            const bool seekingSleep = petMotion() == PetMotion::SEEKING_SLEEP;
            if (seekingFood) {
                Platform::logf("[HomeFood] event=blocked actor=0 stage=route "
                               "pos=%.1f,%.1f target=%.1f,%.1f\n",
                               homeMainActor.x, homeMainActor.y, homeMainActor.targetX, homeMainActor.targetY);
            }
            if (seekingFood || seekingSleep) {
                tryYieldHomeBlocker(0, homeMainActor.targetX, homeMainActor.targetY, nowMs);
            }
            homeRuntime.stop(0, nowMs, 1000);
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            homeMainActor.targetX = homeMainActor.x;
            homeMainActor.targetY = homeMainActor.y;
            petFrame = 0;
            nextPetFrameMs = nowMs + 520;
            homeMainActor.nextDecisionMs = nowMs + 1000;
            if (seekingFood) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs + 1000;
            }
        } else if (routeStep == Home::RouteStep::NO_ROUTE) {
            if (petMotion() == PetMotion::SEEKING_FOOD) {
                Platform::logf(
                    "[HomeFood] event=blocked actor=0 stage=no_route "
                    "owner=%d pos=%.1f,%.1f target=%.1f,%.1f\n",
                    static_cast<int>(homeRuntime.owner(
                        Home::Resource::BOWL)),
                    homeMainActor.x, homeMainActor.y, homeMainActor.targetX, homeMainActor.targetY);
            }
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            homeMainActor.targetX = homeMainActor.x;
            homeMainActor.targetY = homeMainActor.y;
            petFrame = 0;
            homeRuntime.stop(0, nowMs, 700);
            schedulePetDecision(nowMs);
        }
        updateCamera();
        const bool cameraMoved =
            static_cast<int>(std::lround(cameraX)) != previousCameraX ||
            static_cast<int>(std::lround(cameraY)) != previousCameraY;
        if (cameraMoved) {
            // The room background is sampled in camera-space. Present it as
            // soon as the camera changes, even when the sprite animation
            // clock has not reached its next frame yet.
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        {
            static uint32_t s_petMoveSampleMs = 0;
            static float s_petSampleX = -1.0f;
            static float s_petSampleY = -1.0f;
            if (static_cast<int32_t>(nowMs - s_petMoveSampleMs) >= 0) {
                s_petMoveSampleMs = nowMs + 500;
                const bool moved = s_petSampleX >= 0.0f &&
                    (std::fabs(homeMainActor.x - s_petSampleX) > 0.5f ||
                     std::fabs(homeMainActor.y - s_petSampleY) > 0.5f);
                Platform::logf(
                    "[HomeDiag] pet-moving motion=%d pos=%.1f,%.1f frame=%u "
                    "frameIn=%ld moved=%d route=%d\n",
                    static_cast<int>(petMotion()), homeMainActor.x, homeMainActor.y,
                    static_cast<unsigned>(petFrame),
                    static_cast<long>(static_cast<int32_t>(
                        nextPetFrameMs - nowMs)),
                    moved ? 1 : 0, static_cast<int>(routeStep));
                s_petSampleX = homeMainActor.x;
                s_petSampleY = homeMainActor.y;
            }
        }
#endif
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            PokemonSprites::PetAnimationProfile profile{};
            uint8_t frameCount = 3;
            if (PokemonSprites::petAnimationProfile(monster.speciesId, profile)) {
                frameCount = profile.walkingFrames > 0 ? profile.walkingFrames : 1;
                if (profile.motionMode ==
                        PokemonSprites::PetMotionMode::START_HOLD_END &&
                    frameCount >= 3) {
                    frameCount = 2;
                } else if (profile.motionMode ==
                               PokemonSprites::PetMotionMode::PINGPONG &&
                           frameCount == 3 && !petLongMove) {
                    frameCount = 2;
                }
                if (profile.motionMode ==
                        PokemonSprites::PetMotionMode::START_HOLD_END &&
                    profile.walkingFrames >= 3) {
                    petFrame = std::min<uint8_t>(1, petFrame + 1);
                } else {
                    petFrame = static_cast<uint8_t>(
                        (petFrame + 1) % frameCount);
                }
            } else {
                petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
            }
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            if (!cameraMoved) {
                requestHomeActorRows(previousY, homeMainActor.y);
            }
        }
        return;
    }

    if (petMotion() == PetMotion::IDLE) {
        PokemonSprites::PetAnimationProfile profile{};
        if (PokemonSprites::petAnimationProfile(monster.speciesId, profile) &&
            static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            const uint8_t frameCount = std::max<uint8_t>(1, profile.idleFrames);
            do {
                petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
                nextPetFrameMs += profile.idleFrameMs;
            } while (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
    }

    if (homeFoodArbitrationPending &&
        preferredBowlEater(nowMs) == 0) return;
    if (static_cast<int32_t>(nowMs - homeMainActor.nextDecisionMs) < 0) return;
    const Home::ActorObservation mainObservation =
        homeActorObservation(0, nowMs);
    const Home::ActorIntent mainIntent =
        Home::ActorController::survivalIntent(
            mainObservation, gameState.room.bowlCount > 0);
    if (mainObservation.sleepTime &&
        static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
        RoomResource& room = RoomResource::ins();
        const float bedX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        const float bedY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        Platform::logf(
            "[HomeSleep] event=decision intent=%u desire=%u "
            "sleepTime=%u usesBed=%u canMove=%u control=%u "
            "bedOwner=%d pos=%.1f,%.1f bed=%.1f,%.1f "
            "task=%u motion=%u\n",
            static_cast<unsigned>(mainIntent),
            static_cast<unsigned>(mainObservation.desire),
            mainObservation.sleepTime ? 1U : 0U,
            mainObservation.usesBed ? 1U : 0U,
            mainObservation.canMove ? 1U : 0U,
            mainObservation.controlAvailable ? 1U : 0U,
            static_cast<int>(homeRuntime.owner(Home::Resource::BED)),
            homeMainActor.x, homeMainActor.y, bedX, bedY,
            static_cast<unsigned>(homeMainActor.task),
            static_cast<unsigned>(petMotion()));
        nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
    }
    MonsterDesire desire = homeMainActor.mind.topDesire();
    const Home::IdleGoal goal = Home::ActorController::idleGoal(
        desire, mainIntent);
    if (goal == Home::IdleGoal::SEEK_FOOD &&
        preferredBowlEater(nowMs) == 0) {
        if (startMainFoodSeek(nowMs)) {
            return;
        }
    }

    if (goal == Home::IdleGoal::SEEK_SLEEP) {
        const int8_t bedOwnerBefore =
            homeRuntime.owner(Home::Resource::BED);
        if (!homeRuntime.acquire(Home::Resource::BED, 0,
                                 Home::Task::SEEK_SLEEP, nowMs)) {
            homeMainActor.nextDecisionMs = nowMs + 1000;
            if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
                Platform::logf(
                    "[AmoledPet] sleep blocked bedOwner=%d pos=%.1f,%.1f\n",
                    static_cast<int>(bedOwnerBefore), homeMainActor.x, homeMainActor.y);
                nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
            }
            return;
        }
        RoomResource& room = RoomResource::ins();
        const float sleepX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        const float sleepY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        const float companionDx = homeCompanionActor.x - sleepX;
        const float companionDy = homeCompanionActor.y - sleepY;
        if (homeCompanionActor.active &&
            companionDx * companionDx + companionDy * companionDy < 900.0f) {
            float yieldX = homeCompanionActor.x;
            float yieldY = homeCompanionActor.y;
            if (chooseCompanionSleepSpot(yieldX, yieldY, true)) {
                homeRuntime.transition(
                    1, Home::Task::YIELDING, nowMs, 0, true);
                beginCompanionMove(Home::Task::YIELDING, yieldX, yieldY,
                                   nowMs, false, true);
            }
        }
        // The bed pose is also used as the persisted resting position. When
        // a save restore or a previous interrupted transition already leaves
        // the actor there, asking the navigator to route to the same point
        // can produce an empty route and immediately cancel the sleep task.
        const bool atSleepPose =
            std::fabs(homeMainActor.x - sleepX) <= 2.5f &&
            std::fabs(homeMainActor.y - sleepY) <= 6.0f;
        if (atSleepPose) {
            homeMainActor.route.clear();
            homeMainActor.velocityX = 0.0f;
            homeMainActor.velocityY = 0.0f;
            homeMainActor.targetX = homeMainActor.x;
            homeMainActor.targetY = homeMainActor.y;
            homeMainActor.motion = Home::MotionPhase::STILL;
            petStopping = false;
            petFrame = 0;
            homeRuntime.transition(
                0, Home::Task::SLEEPING, nowMs, 0, true);
            startSleepAnimation(nowMs, HOME_SLEEP_FRAME_MS);
            Platform::logf(
                "[AmoledPet] sleep enter direct bed=%.1f,%.1f owner=%d\n",
                sleepX, sleepY,
                static_cast<int>(homeRuntime.owner(Home::Resource::BED)));
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return;
        }
        if (beginPetMove(PetMotion::SEEKING_SLEEP,
                         sleepX, sleepY, nowMs)) {
            return;
        }
        homeRuntime.release(Home::Resource::BED, 0);
        Platform::logf(
            "[AmoledPet] sleep enter rejected pos=%.1f,%.1f target=%.1f,%.1f "
            "bedOwnerBefore=%d atSleepPose=%u\n",
            homeMainActor.x, homeMainActor.y, sleepX, sleepY,
            static_cast<int>(bedOwnerBefore),
            atSleepPose ? 1U : 0U);
    }
    if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
        int32_t decisionInMs = static_cast<int32_t>(
            homeMainActor.nextDecisionMs - nowMs);
        const int8_t bowlOwner = homeRuntime.owner(Home::Resource::BOWL);
        const int8_t preferredEater = preferredBowlEater(nowMs);
        Platform::logf(
            "[AmoledPet] decision desire=%u motion=%u pos=%.1f,%.1f "
            "next=%ld walk=%u bowl=%u/%u owner=%d preferred=%d "
            "satiety=%u/%u canEat=%u/%u task=%u/%u desire2=%u "
            "near=%u/%u block=%u/%u pending=%u\n",
            static_cast<unsigned>(desire),
            static_cast<unsigned>(petMotion()), homeMainActor.x, homeMainActor.y,
            static_cast<long>(decisionInMs),
            petFootprintInsideWalkArea(homeMainActor.x, homeMainActor.y) ? 1U : 0U,
            static_cast<unsigned>(gameState.room.bowlCount),
            static_cast<unsigned>(gameState.room.bowlBitesRemaining),
            static_cast<int>(bowlOwner), static_cast<int>(preferredEater),
            static_cast<unsigned>(monster.satiety),
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
            homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
            homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
            static_cast<unsigned>(homeMainActor.task),
            static_cast<unsigned>(homeCompanionActor.task),
            static_cast<unsigned>(homeCompanionActor.mind.topDesire()),
            homeActorNearFood(0) ? 1U : 0U,
            homeActorNearFood(1) ? 1U : 0U,
            homeActorBlocksFoodApproach(0) ? 1U : 0U,
            homeActorBlocksFoodApproach(1) ? 1U : 0U,
            homeFoodArbitrationPending ? 1U : 0U);
        nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
    }

    // Stick uses part of its STARE decisions for a small idle turn. Most
    // species have only one idle frame, so this direction change is the
    // visible waiting animation on those species.
    if (goal == Home::IdleGoal::STARE &&
        GameRandom::random(100) < 32) {
        int direction = static_cast<int>(petDirection) +
            (GameRandom::random(2) == 0 ? -1 : 1);
        if (direction < 0) direction += 4;
        if (direction >= 4) direction -= 4;
        petDirection = static_cast<PokemonSprites::WalkDirection>(direction);
        homeRuntime.beginTurn(0, Home::Task::IDLE, nowMs,
                              homeMainActor.behavior.turnPauseMs);
        petStopping = false;
        petFrame = 0;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        // Turning in place is still an idle decision. Do not reset the
        // inactivity timer, otherwise repeated STARE turns can starve WANDER.
        schedulePetDecision(nowMs);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (goal == Home::IdleGoal::WANDER &&
        homeMainActor.behavior.movementMode == MonsterMovementMode::NORMAL) {
        float x = homeMainActor.x;
        float y = homeMainActor.y;
        if (chooseWanderTarget(x, y) &&
            beginPetMove(PetMotion::WANDERING, x, y, nowMs)) {
            return;
        }
        // A rejected target is a routing failure, not activity. Retry soon
        // while preserving boredom so the pet cannot silently stall for a
        // full idle interval after every failed sample.
        homeMainActor.nextDecisionMs = nowMs + GameRandom::range(
            PET_WANDER_RETRY_MIN_MS, PET_WANDER_RETRY_MAX_MS + 1);
        if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
            Platform::logf(
                "[AmoledPet] wander target rejected desire=%u pos=%.1f,%.1f "
                "walk=%u\n",
                static_cast<unsigned>(homeMainActor.mind.topDesire()), homeMainActor.x, homeMainActor.y,
                petFootprintInsideWalkArea(homeMainActor.x, homeMainActor.y) ? 1U : 0U);
            nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
        }
        return;
    }
    // A decision that elects to stare is not activity. Keep the boredom
    // timer running so WANDER can eventually outrank STARE, as on Stick.
    schedulePetDecision(nowMs);
}

bool AmoledApp::beginPetMove(PetMotion motion, float x, float y,
                             uint32_t nowMs) {
    syncHomeActors(nowMs);
    Home::Task task = Home::Task::WANDER;
    if (motion == PetMotion::SEEKING_FOOD) task = Home::Task::SEEK_FOOD;
    else if (motion == PetMotion::SEEKING_SLEEP) task = Home::Task::SEEK_SLEEP;
    if (!homeRuntime.transition(0, task, nowMs)) return false;
    if (!petFootprintInsideWalkArea(x, y) ||
        !homeRuntime.planRoute(
            0, x, y, false, homeCompanionActor.active)) {
        homeRuntime.stop(0, nowMs, 700);
        return false;
    }
    homeMainActor.targetX = x;
    homeMainActor.targetY = y;
    float firstX = x;
    float firstY = y;
    homeMainActor.route.current(firstX, firstY);
    PokemonSprites::WalkDirection previousDirection = petDirection;
    petDirection = petDirectionForDelta(firstX - homeMainActor.x, firstY - homeMainActor.y);
    float dx = homeMainActor.targetX - homeMainActor.x;
    float dy = homeMainActor.targetY - homeMainActor.y;
    petLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
    if (petDirection != previousDirection) {
        homeRuntime.beginTurn(0, task, nowMs, TURN_PAUSE_MS);
        petStopping = false;
    } else {
        homeMainActor.motion = Home::MotionPhase::ROUTE;
        petStopping = false;
    }
    petFrame = 0;
    nextPetFrameMs = nowMs;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::finishPetMove(uint32_t nowMs) {
    homeMainActor.route.clear();
    if (petMotion() == PetMotion::SEEKING_SLEEP) {
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        petFrame = 0;
        homeRuntime.transition(0, Home::Task::SLEEPING, nowMs, 0, true);
        startSleepAnimation(nowMs, HOME_SLEEP_FRAME_MS);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }
    petStopping = true;
    homeMainActor.motion = Home::MotionPhase::STILL;
    petFrame = 0;
    nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::schedulePetDecision(uint32_t nowMs) {
    uint32_t minimum = homeMainActor.behavior.idleMinMs;
    uint32_t maximum = homeMainActor.behavior.idleMaxMs;
    if (maximum < minimum) maximum = minimum;
    homeMainActor.nextDecisionMs = nowMs + GameRandom::range(minimum, maximum + 1);
}

bool AmoledApp::ambientActionAllowed() const {
    if (homeFlow_.state().action != RoomAction::NONE || petMotion() != PetMotion::IDLE ||
        homeMainActor.task != Home::Task::IDLE || homeRuntime.pairActive() ||
        visitorFlow_.state().motion == VisitorMotion::HOST_APPROACH ||
        visitorFlow_.state().motion == VisitorMotion::ENTERING ||
        visitorFlow_.state().motion == VisitorMotion::EXITING || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
        return false;
    }
    const Game::MonsterRuntime& monster = gameState.team[0];
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool sleepTime = care.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    return Home::ActorController::ambientEligible(
        monster, homeMainActor.mind.topDesire(), sleepTime,
        gameState.room.bowlCount > 0);
}

HomeFlow::Layout AmoledApp::homeActionLayout(bool includeWindow) const {
    RoomResource& room = RoomResource::ins();
    HomeFlow::Layout layout;
    layout.polygon = room.available() ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    layout.polygonCount = room.available() ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) / sizeof(FALLBACK_WALK_POLYGON[0]));
    layout.bounds = {
        room.available() ? static_cast<float>(room.walkMinX()) : FALLBACK_ROOM_MIN_X,
        room.available() ? static_cast<float>(room.walkMinY()) : FALLBACK_ROOM_MIN_Y,
        room.available() ? static_cast<float>(room.walkMaxX()) : FALLBACK_ROOM_MAX_X,
        room.available() ? static_cast<float>(room.walkMaxY()) : FALLBACK_ROOM_MAX_Y,
    };
    layout.foodX = room.available() ? room.foodX() : FALLBACK_FOOD_APPROACH_X;
    layout.foodY = room.available() ? room.foodY() : FALLBACK_FOOD_APPROACH_Y;
    RoomResource::BehaviorAnchor anchor{};
    layout.hasWindow = includeWindow && room.available() && room.findBehaviorAnchor(
        RoomResource::BehaviorAnchorType::WINDOW_GAZE, anchor);
    layout.windowX = anchor.footX;
    layout.windowY = anchor.footY;
    layout.windowDirection = mainDirectionFromView(anchor.facing);
    return layout;
}

void AmoledApp::applyHomeFlowEffects(uint32_t nowMs) {
    const HomeFlow::Effects effects = homeFlow_.takeEffects();
    if (effects.stopAnimation) petStopping = false;
    if (effects.resetFrame) petFrame = 0;
    if (effects.startWalk) nextPetFrameMs = nowMs;
    if (effects.directionChanged) petDirection = effects.direction;
    if (effects.heart == HomeFlow::Heart::SHOW) heartsUntil = effects.heartsUntilMs;
    else if (effects.heart == HomeFlow::Heart::CLEAR) heartsUntil = 0;
    if (effects.scheduleDecision) schedulePetDecision(nowMs);
    if (effects.save) saveState();
    if (effects.roomChanged) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::startFeedFinish(uint32_t nowMs) {
    homeFlow_.startFeedFinish(nowMs);
    applyHomeFlowEffects(nowMs);
}

bool AmoledApp::updateRoomAction(uint32_t nowMs, float elapsedSeconds) {
    const HomeFlow::Layout layout = homeActionLayout();
    const HomeFlow::Update result = homeFlow_.update(
        layout, nowMs, elapsedSeconds, homeActorBlocksFoodApproach(0));
    applyHomeFlowEffects(nowMs);
    if (!result.moved) return result.active;
    const int previousCameraX = static_cast<int>(std::lround(cameraX));
    const int previousCameraY = static_cast<int>(std::lround(cameraY));
    updateCamera();
    const bool cameraMoved =
        static_cast<int>(std::lround(cameraX)) != previousCameraX ||
        static_cast<int>(std::lround(cameraY)) != previousCameraY;
    if (cameraMoved) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        PokemonSprites::PetAnimationProfile animation{};
        const uint8_t frameCount = PokemonSprites::petAnimationProfile(
            gameState.team[0].speciesId, animation)
                ? std::max<uint8_t>(1, animation.walkingFrames) : 3;
        petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        if (!cameraMoved) requestHomeActorRows(result.previousY, homeMainActor.y);
    }
    homeFlow_.completeMovement(layout, result.step, homeActorBlocksFoodApproach(0), nowMs);
    applyHomeFlowEffects(nowMs);
    return true;
}

void AmoledApp::cancelRoomAction(uint32_t nowMs) {
    homeFlow_.cancel(homeActorBlocksFoodApproach(0), nowMs);
    applyHomeFlowEffects(nowMs);
}

void AmoledApp::updatePetFootprint() {
    PokemonSprites::WalkingAnimation animation{};
    if (!PokemonSprites::walkingAnimation(
            gameState.team[0].speciesId,
            PokemonSprites::WalkDirection::DOWN, animation) ||
        animation.frameCount == 0) {
        return;
    }
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            gameState.team[0].speciesId, animation.base);
    if (!frame) return;
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    Home::GroundFootprintProfile footprint =
        Home::groundFootprintForSpecies(
            gameState.team[0].speciesId,
            static_cast<uint8_t>(width), static_cast<uint8_t>(height));
    homeMainActor.geometry.footprint.radiusX = footprint.radiusX;
    homeMainActor.geometry.footprint.radiusY = footprint.radiusY;
}

void AmoledApp::configureHomeRuntime() {
    RoomResource& room = RoomResource::ins();
    Home::NavigationWorld world;
    world.polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    world.polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    world.footBounds = {
        room.available() ? static_cast<float>(room.walkMinX())
                         : FALLBACK_ROOM_MIN_X,
        room.available() ? static_cast<float>(room.walkMinY())
                         : FALLBACK_ROOM_MIN_Y,
        room.available() ? static_cast<float>(room.walkMaxX())
                         : FALLBACK_ROOM_MAX_X,
        room.available() ? static_cast<float>(room.walkMaxY())
                         : FALLBACK_ROOM_MAX_Y,
    };
    const float companionRadius = homeCompanionActor.active
        ? homeCompanionActor.geometry.footprint.radiusX : 0.0f;
    world.actorMinSeparation = std::max(
        20.0f, homeMainActor.geometry.footprint.radiusX +
                   companionRadius + 2.0f);
    // Room actors share the same walkable area, but one actor must not turn
    // the other into an A* wall. Soft steering keeps sprites readable while
    // allowing narrow-room routes and food/bed transitions to complete.
    world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
    world.actorOverlapPadding = 1.0f;
    world.scratch = {
        gHomeNavParent, gHomeNavQueue, RoomNavigator::MAX_NODES};
    homeRuntime.setNavigation(world);
}

void AmoledApp::initializeCompanionActor(uint32_t nowMs) {
    const bool guest = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    homeCompanionActor.reset(
        guest ? Home::ActorRole::GUEST : Home::ActorRole::TEAMMATE,
        1, nowMs);
    homeCompanionActor.active = gameState.teamCount > 1 &&
        gameState.team[1].speciesId != 0;
    companionFrame = 0;
    companionDirection = PokemonSprites::WalkDirection::DOWN;
    companionLongMove = true;
    nextCompanionFrameMs = nowMs + 520;
    lastCompanionUpdateMs = nowMs;
    if (!homeCompanionActor.active) return;

    const Game::MonsterRuntime& monster = gameState.team[1];
    homeCompanionActor.speciesId = monster.speciesId;
    const Species* species = findSpecies(monster.speciesId);
    homeCompanionActor.behavior = behaviorProfileFor(
        species ? *species : starterSpecies(), monster);

    PokemonSprites::PetAnimationProfile animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    if (PokemonSprites::petAnimationProfile(monster.speciesId, animation)) {
        frame = PokemonSprites::findSpeciesSprite(
            monster.speciesId, animation.idleBase);
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            monster.speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    const uint8_t width = frame ? FlashStorage::readByte(&frame->width) : 38;
    const uint8_t height = frame ? FlashStorage::readByte(&frame->height) : 42;
    const Home::GroundFootprintProfile footprint =
        Home::groundFootprintForSpecies(monster.speciesId, width, height);
    homeCompanionActor.geometry = {
        0.0f, {footprint.radiusX, footprint.radiusY}};

    homeCompanionActor.x = homeMainActor.x;
    homeCompanionActor.y = homeMainActor.y;
    homeCompanionActor.targetX = homeCompanionActor.x;
    homeCompanionActor.targetY = homeCompanionActor.y;
    homeRuntime.attach(homeMainActor, &homeCompanionActor);
    configureHomeRuntime();

    if (guest) {
        if (visitSession.active() && visitSession.viewModel().localIsHost &&
            !pendingExpedition &&
            expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
            expeditionCompanionHidden = false;
        }
        if (visitSession.active() && visitSession.viewModel().localIsHost &&
            !visitSession.visitorArrivalReady()) {
            homeCompanionActor.hidden = true;
            homeCompanionActor.stop(nowMs, 0);
            Platform::logf("[VisitArrival] waiting species=%u\n",
                           static_cast<unsigned>(monster.speciesId));
            return;
        }
        if (pendingExpedition ||
            expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
            // A departure is already queued: skip the door-entry animation
            // and spawn inside the room so the companion can join the
            // departure door walk.
            float spawnX = homeCompanionActor.x;
            float spawnY = homeCompanionActor.y;
            if (chooseCompanionTarget(spawnX, spawnY)) {
                homeCompanionActor.x = spawnX;
                homeCompanionActor.y = spawnY;
                homeCompanionActor.targetX = spawnX;
                homeCompanionActor.targetY = spawnY;
                homeCompanionActor.route.clear();
            }
            homeCompanionActor.stop(nowMs, 0);
            return;
        }
        beginVisitorEntry(nowMs);
        return;
    }

    const SecondarySceneViewState& saved = mainViewState.secondary;
    const bool sameMonster = saved.valid &&
        saved.speciesId == monster.speciesId &&
        saved.ivPacked == monster.ivPacked &&
        saved.metAt == monster.metAt &&
        saved.nature == monster.nature &&
        saved.metArea == monster.metArea &&
        saved.origin == static_cast<uint8_t>(monster.origin);
    if (sameMonster) {
        const float groundOffset = speciesGroundOffset(monster.speciesId);
        const float savedX = saved.x;
        const float savedY = saved.y + groundOffset;
        RoomResource& room = RoomResource::ins();
        const RoomResource::Point* polygon = room.available()
            ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
        const uint8_t polygonCount = room.available()
            ? room.walkPolygonCount()
            : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                                   sizeof(FALLBACK_WALK_POLYGON[0]));
        if (RoomMovementArea::containsFootprint(
                polygon, polygonCount, savedX, savedY,
                homeCompanionActor.geometry.footprint)) {
            homeCompanionActor.x = savedX;
            homeCompanionActor.y = savedY;
            homeCompanionActor.targetX = saved.targetX;
            homeCompanionActor.targetY = saved.targetY + groundOffset;
            companionDirection = saved.direction <= 3
                ? static_cast<PokemonSprites::WalkDirection>(saved.direction)
                : PokemonSprites::WalkDirection::DOWN;
            companionFrame = saved.frameIndex;
            homeCompanionActor.nextDecisionMs =
                nowMs + saved.stateRemainingMs;
            homeCompanionActor.foodWakeRetryAfterMs =
                saved.foodRetryRemainingMs == 0
                    ? 0 : nowMs + saved.foodRetryRemainingMs;
            if (saved.state == 1) {
                homeRuntime.transition(
                    1, Home::Task::WANDER, nowMs, 0, true);
                if (homeRuntime.planRoute(
                        1, homeCompanionActor.targetX,
                        homeCompanionActor.targetY)) {
                    return;
                }
                homeRuntime.stop(1, nowMs, 700);
            }
            const Game::SpeciesCareProfile care =
                Game::speciesCareProfileFor(monster.speciesId);
            const bool sleepTime = care.usesBed && Game::isSleepCareTime(
                gameState.gameMinutesTotal, monster.nature);
            if (saved.state == 6 && sleepTime) {
                homeRuntime.transition(
                    1, Home::Task::SLEEPING, nowMs, 0, true);
                homeCompanionActor.sleepX = saved.sleepX;
                homeCompanionActor.sleepY = saved.sleepY + groundOffset;
                homeCompanionActor.sleepSpotValid = saved.sleepSpotValid;
                return;
            }
            homeCompanionActor.stop(
                nowMs, saved.stateRemainingMs);
            return;
        }
    }

    float spawnX = homeCompanionActor.x;
    float spawnY = homeCompanionActor.y;
    if (chooseCompanionTarget(spawnX, spawnY)) {
        homeCompanionActor.x = spawnX;
        homeCompanionActor.y = spawnY;
        homeCompanionActor.targetX = spawnX;
        homeCompanionActor.targetY = spawnY;
        homeCompanionActor.route.clear();
    }
    homeCompanionActor.stop(
        nowMs, static_cast<uint32_t>(GameRandom::range(
            homeCompanionActor.behavior.idleMinMs,
            homeCompanionActor.behavior.idleMaxMs + 1)));
}

void AmoledApp::syncHomeActors(uint32_t nowMs) {
    const bool firstSetup = !homeRuntimeReady;
    const uint16_t nextMainSpecies = gameState.teamCount > 0
        ? gameState.team[0].speciesId : 0;
    const bool mainChanged = !firstSetup &&
        homeMainActor.speciesId != nextMainSpecies;
    if (firstSetup) {
        homeRuntimeReady = true;
    } else if (mainChanged) {
        homeMainActor.route.clear();
        homeMainActor.velocityX = 0.0f;
        homeMainActor.velocityY = 0.0f;
    }
    homeMainActor.active = gameState.teamCount > 0;
    homeMainActor.speciesId = nextMainSpecies;
    homeMainActor.geometry.groundOffsetY = 0.0f;

    const bool wantsCompanion = gameState.teamCount > 1 &&
        gameState.team[1].speciesId != 0;
    const bool companionChanged = wantsCompanion &&
        (!homeCompanionActor.active ||
         homeCompanionActor.speciesId != gameState.team[1].speciesId);
    if (companionChanged) {
        initializeCompanionActor(nowMs);
    } else if (!wantsCompanion && homeCompanionActor.active) {
        cancelPairInteraction(nowMs);
        homeRuntime.releaseAll(1);
        homeCompanionActor = Home::Actor{};
        companionFrame = 0;
        visitorFlow_.resetMotion();
        homeRuntime.attach(homeMainActor);
    } else {
        homeRuntime.attach(
            homeMainActor,
            homeCompanionActor.active ? &homeCompanionActor : nullptr);
    }
    configureHomeRuntime();
}

Home::ActorObservation AmoledApp::homeActorObservation(
    uint8_t teamSlot, uint32_t nowMs) const {
    Home::ActorObservation observation;
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        return observation;
    }

    const Home::Actor* actor = homeRuntime.actor(teamSlot);
    const Game::MonsterRuntime& monster = gameState.team[teamSlot];
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    observation.active = actor && actor->active && !actor->hidden;
    observation.controlAvailable = homeRuntime.autonomousAllowed(teamSlot);
    observation.visitor = monster.origin == Game::Origin::VISITOR;
    observation.canMove = care.canMove;
    observation.needsFood = care.needsFood;
    observation.usesBed = care.usesBed;
    observation.fainted = monster.fainted || monster.hpCur == 0;
    observation.statusSleeping =
        monster.majorStatus == Game::MajorStatus::SLEEP;
    observation.sleepTime = observation.statusSleeping ||
        (care.usesBed && Game::isSleepCareTime(
             gameState.gameMinutesTotal, monster.nature));
    observation.sleeping = actor && actor->task == Home::Task::SLEEPING;
    if (teamSlot == 0) observation.sleeping = (homeMainActor.task == Home::Task::SLEEPING);
    observation.wakeForFood = care.needsFood &&
        monsterShouldWakeForFood(monster.satiety);
    observation.foodRetryReady = teamSlot == 0 || !actor ||
        static_cast<int32_t>(nowMs - actor->foodWakeRetryAfterMs) >= 0;
    observation.satiety = monster.satiety;
    observation.feedTarget = MONSTER_FEED_TARGET_SATIETY;
    if (actor) {
        observation.task = actor->task;
        observation.resumeTask = actor->resumeTask;
        const bool foodCommitted =
            Home::ActorController::committedToFood(observation);
        observation.foodActionAvailable = foodCommitted ||
            actor->task == Home::Task::IDLE ||
            actor->task == Home::Task::SLEEPING ||
            actor->task == Home::Task::WAKING ||
            actor->task == Home::Task::LEAVING_SLEEP;
        observation.desire = teamSlot == 0
            ? homeMainActor.mind.topDesire() : actor->mind.topDesire();
    } else {
        observation.foodActionAvailable = false;
    }
    return observation;
}

Home::HouseholdObservation AmoledApp::homeHouseholdObservation(
    uint32_t nowMs) const {
    Home::HouseholdObservation observation;
    observation.actorCount = std::min<uint8_t>(
        gameState.teamCount, Home::ACTOR_CAP);
    for (uint8_t actorId = 0; actorId < observation.actorCount; ++actorId) {
        observation.actors[actorId] = homeActorObservation(actorId, nowMs);
    }
    observation.bowlHasFood = gameState.room.bowlCount > 0;
    observation.bowlClearing = homeRuntime.bowlSession().clearing();
    observation.bowlOwner = homeRuntime.owner(Home::Resource::BOWL);
    return observation;
}

bool AmoledApp::homeActorCanEatFromBowl(uint8_t teamSlot,
                                        uint32_t nowMs) const {
    return Home::ActorController::foodEligible(
        homeActorObservation(teamSlot, nowMs),
        gameState.room.bowlCount > 0);
}

int8_t AmoledApp::preferredBowlEater(uint32_t nowMs) const {
    return Home::ActorController::selectBowlActor(
        homeHouseholdObservation(nowMs)).actorId;
}

bool AmoledApp::claimBowl(uint8_t teamSlot, uint32_t nowMs) {
    if (!homeRuntime.bowlClaimAllowed(teamSlot)) return false;
    // Do not hand the bowl to the leader while the previous eater is still
    // occupying its approach. A released lease alone does not clear space.
    if (teamSlot == 0 && homeActorBlocksFoodApproach(1)) {
        return false;
    }
    int8_t owner = homeRuntime.owner(Home::Resource::BOWL);
    if (owner >= 0 &&
        !homeActorCanEatFromBowl(static_cast<uint8_t>(owner), nowMs)) {
        homeRuntime.release(Home::Resource::BOWL,
                            static_cast<uint8_t>(owner));
        owner = -1;
    }
    if (owner >= 0) return owner == static_cast<int8_t>(teamSlot);
    if (preferredBowlEater(nowMs) != static_cast<int8_t>(teamSlot)) {
        return false;
    }
    return homeRuntime.acquire(Home::Resource::BOWL, teamSlot,
                               Home::Task::SEEK_FOOD, nowMs);
}

bool AmoledApp::homeActorNearFood(uint8_t actorId) const {
    const Home::Actor* actor = homeRuntime.actor(actorId);
    if (!actor || !actor->active || actor->hidden) return false;
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float toleranceX = std::max(
        9.0f, actor->geometry.footprint.radiusX + 8.0f);
    const float toleranceY = std::max(
        7.0f, actor->geometry.footprint.radiusY + 6.0f);
    return std::fabs(actor->x - foodX) <= toleranceX &&
           std::fabs(actor->y + actor->geometry.groundOffsetY - foodY) <=
               toleranceY;
}

bool AmoledApp::homeActorBlocksFoodApproach(uint8_t actorId) const {
    const Home::Actor* actor = homeRuntime.actor(actorId);
    if (!actor || !actor->active || actor->hidden) return false;
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float otherRadius = actorId == 0
        ? homeCompanionActor.geometry.footprint.radiusX
        : homeMainActor.geometry.footprint.radiusX;
    const float clearance = std::max(
        26.0f, actor->geometry.footprint.radiusX + otherRadius + 6.0f);
    const float dx = actor->x - foodX;
    const float dy = actor->y - foodY;
    return dx * dx + dy * dy < clearance * clearance;
}

bool AmoledApp::tryYieldHomeBlocker(uint8_t waitingActorId,
                                    float goalX, float goalY, uint32_t nowMs) {
    const uint8_t blockerId = waitingActorId == 0 ? 1 : 0;
    const Home::Actor* waiting = homeRuntime.actor(waitingActorId);
    Home::Actor* blocker = homeRuntime.actor(blockerId);
    if (!waiting || !blocker || !blocker->active || blocker->hidden ||
        blocker->task != Home::Task::IDLE ||
        !homeRuntime.autonomousAllowed(blockerId) ||
        blocker->behavior.movementMode == MonsterMovementMode::STATIONARY ||
        blockerId >= gameState.teamCount || gameState.team[blockerId].fainted ||
        gameState.team[blockerId].hpCur == 0) return false;
    if (blockerId == 0 && (homeFlow_.state().action != RoomAction::NONE ||
                           petMotion() != PetMotion::IDLE)) return false;
    const float clearance = waiting->geometry.footprint.radiusX +
                            blocker->geometry.footprint.radiusX + 2.0f;
    RoomNavigator::Obstacle obstacle;
    obstacle.active = true;
    obstacle.x = blocker->x;
    obstacle.footY = blocker->y + blocker->geometry.groundOffsetY;
    obstacle.minSeparation = clearance;
    if (RoomNavigator::segmentKeepsSpacing(
            waiting->x, waiting->y + waiting->geometry.groundOffsetY,
            goalX, goalY + waiting->geometry.groundOffsetY, obstacle)) return false;

    constexpr float directions[][2] = {
        {0, -1}, {0, 1}, {1, 0}, {-1, 0},
        {0.707f, -0.707f}, {0.707f, 0.707f},
        {-0.707f, -0.707f}, {-0.707f, 0.707f},
    };
    const float originX = blocker->x;
    const float originY = blocker->y;
    for (float radius : {24.0f, 40.0f, 56.0f}) {
        for (const auto& direction : directions) {
            const float x = originX + direction[0] * radius;
            const float y = originY + direction[1] * radius;
            if (!homeRuntime.targetAvailable(blockerId, x, y)) continue;
            obstacle.x = x;
            obstacle.footY = y + blocker->geometry.groundOffsetY;
            if (!RoomNavigator::segmentKeepsSpacing(
                    waiting->x, waiting->y + waiting->geometry.groundOffsetY,
                    goalX, goalY + waiting->geometry.groundOffsetY, obstacle)) continue;
            const bool started = blockerId == 1
                ? beginCompanionMove(Home::Task::YIELDING, x, y, nowMs)
                : beginPetMove(PetMotion::WANDERING, x, y, nowMs);
            if (!started) continue;
            if (blockerId == 0) {
                if (homeMainActor.task == Home::Task::TURNING) {
                    homeMainActor.resumeTask = Home::Task::YIELDING;
                } else {
                    homeRuntime.transitionPreparedRoute(
                        0, Home::Task::YIELDING, nowMs, 0, true);
                }
            }
            Platform::logf("[HomeAvoid] yield actor=%u for=%u target=%.1f,%.1f\n",
                           blockerId, waitingActorId, x, y);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return true;
        }
    }
    return false;
}

bool AmoledApp::startMainFoodSeek(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    float approachX = foodX;
    float approachY = foodY;
    if (!claimBowl(0, nowMs)) {
        if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeFood] event=reject actor=0 stage=claim owner=%d "
                "preferred=%d canEat=%u/%u task=%u/%u\n",
                static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
                static_cast<int>(preferredBowlEater(nowMs)),
                homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
                homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
                static_cast<unsigned>(homeMainActor.task),
                static_cast<unsigned>(homeCompanionActor.task));
            nextHomeFoodDebugLogMs = nowMs + 2000;
        }
        return false;
    }
    if (homeActorNearFood(0)) {
        homeRuntime.transition(0, Home::Task::FEEDING, nowMs, 0, true);
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        petDirection = foodFacingDirection(homeMainActor.x, homeMainActor.y);
        petFrame = 0;
        nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
        homeMainActor.taskUntilMs = nowMs + GameRandom::range(3200, 5601);
        Platform::logf(
            "[HomeFood] event=direct actor=0 pos=%.1f,%.1f target=%.1f,%.1f\n",
            homeMainActor.x, homeMainActor.y, foodX, foodY);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }
    if (!chooseFoodApproachTarget(foodX, foodY, approachX, approachY)) {
        Platform::logf(
            "[HomeFood] event=reject actor=0 stage=approach "
            "pos=%.1f,%.1f target=%.1f,%.1f\n",
            homeMainActor.x, homeMainActor.y, foodX, foodY);
        homeRuntime.release(Home::Resource::BOWL, 0);
        return false;
    }
    if (!beginPetMove(PetMotion::SEEKING_FOOD, approachX, approachY,
                      nowMs)) {
        Platform::logf(
            "[HomeFood] event=reject actor=0 stage=route "
            "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
            homeMainActor.x, homeMainActor.y, approachX, approachY,
            homeCompanionActor.x, homeCompanionActor.y);
        homeRuntime.release(Home::Resource::BOWL, 0);
        return false;
    }
    Platform::logf(
        "[HomeFood] event=seek actor=0 pos=%.1f,%.1f target=%.1f,%.1f\n",
        homeMainActor.x, homeMainActor.y, approachX, approachY);
    return true;
}

bool AmoledApp::startCompanionFoodSeek(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    if (!claimBowl(1, nowMs)) {
        if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeFood] event=reject actor=1 stage=claim owner=%d "
                "preferred=%d canEat=%u/%u task=%u/%u\n",
                static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
                static_cast<int>(preferredBowlEater(nowMs)),
                homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
                homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
                static_cast<unsigned>(homeMainActor.task),
                static_cast<unsigned>(homeCompanionActor.task));
            nextHomeFoodDebugLogMs = nowMs + 2000;
        }
        return false;
    }
    if (homeActorNearFood(1)) {
        homeRuntime.transition(1, Home::Task::FEEDING, nowMs,
                               static_cast<uint32_t>(GameRandom::range(
                                   3200, 5601)), true);
        homeCompanionActor.targetX = homeCompanionActor.x;
        homeCompanionActor.targetY = homeCompanionActor.y;
        companionDirection = foodFacingDirection(
            homeCompanionActor.x, homeCompanionActor.y);
        companionFrame = 0;
        companionFoodBiteMs = nowMs + GameRandom::range(700, 1301);
        Platform::logf(
            "[HomeFood] event=direct actor=1 pos=%.1f,%.1f target=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y, foodX, foodY);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }
    float approachX = foodX;
    float approachY = foodY;
    bool routeFound = false;
    // The main actor already searches for a reachable point around the bowl.
    // Use the same idea for the companion, but test the real companion route
    // because its footprint can be wider than the leader's.
    for (int radius = 0; radius <= 36 && !routeFound; radius += 2) {
        const int yRadius = std::min(radius, 24);
        for (int dy = -yRadius; dy <= yRadius && !routeFound; dy += 2) {
            const float candidates[] = {
                foodX + static_cast<float>(radius),
                foodX - static_cast<float>(radius),
            };
            for (float candidateX : candidates) {
                const float candidateY = foodY + static_cast<float>(dy);
                if (homeRuntime.planRoute(1, candidateX, candidateY,
                                          false, true)) {
                    approachX = candidateX;
                    approachY = candidateY;
                    routeFound = true;
                    break;
                }
            }
        }
        for (int dx = -radius + 2;
             dx <= radius - 2 && !routeFound; dx += 2) {
            const float candidates[] = {
                foodY + static_cast<float>(yRadius),
                foodY - static_cast<float>(yRadius),
            };
            for (float candidateY : candidates) {
                const float candidateX = foodX + static_cast<float>(dx);
                if (homeRuntime.planRoute(1, candidateX, candidateY,
                                          false, true)) {
                    approachX = candidateX;
                    approachY = candidateY;
                    routeFound = true;
                    break;
                }
            }
        }
    }
    if (!routeFound || !beginCompanionMove(Home::Task::SEEK_FOOD,
                                           approachX, approachY, nowMs,
                                           false, true)) {
        Platform::logf(
            "[HomeFood] event=reject actor=1 stage=route "
            "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y, foodX, foodY,
            homeMainActor.x, homeMainActor.y);
        homeRuntime.release(Home::Resource::BOWL, 1);
        return false;
    }
    Platform::logf(
        "[HomeFood] event=seek actor=1 pos=%.1f,%.1f target=%.1f,%.1f "
        "route=%u avoidMain=%u mainTask=%u\n",
        homeCompanionActor.x, homeCompanionActor.y, approachX, approachY,
        static_cast<unsigned>(homeCompanionActor.route.count),
        1U,
        static_cast<unsigned>(homeMainActor.task));
    return true;
}

bool AmoledApp::startCompanionFoodExit(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float clearance = std::max(
        26.0f, homeCompanionActor.geometry.footprint.radiusX +
                   homeMainActor.geometry.footprint.radiusX + 6.0f);
    // Try points on the room side of the bowl first. The outgoing eater has
    // priority over the incoming one, so it must be able to leave even when
    // the leader is already waiting at the approach.
    constexpr float DIRECTIONS[][2] = {
        {1.0f, 0.0f}, {0.8f, -0.6f}, {0.8f, 0.6f},
        {0.0f, -1.0f}, {0.0f, 1.0f}, {-0.8f, -0.6f},
        {-0.8f, 0.6f}, {-1.0f, 0.0f},
    };
    // Clear only the space the next eater needs before trying wider exits.
    // Fixed distant rings can all land outside the room or on the waiting
    // actor even when a nearby point just beyond the clearance is reachable.
    const float RADII[] = {
        clearance + 2.0f, clearance + 8.0f, 40.0f, 52.0f, 64.0f,
    };
    for (float radius : RADII) {
        for (const auto& direction : DIRECTIONS) {
            const float x = foodX + radius * direction[0];
            const float y = foodY + radius * direction[1];
            const float dx = x - foodX;
            const float dy = y - foodY;
            if (dx * dx + dy * dy <= clearance * clearance) continue;
            if (beginCompanionMove(Home::Task::YIELDING, x, y,
                                   nowMs, false, true)) {
                Platform::logf(
                    "[HomeFood] event=exit actor=1 pos=%.1f,%.1f "
                    "target=%.1f,%.1f\n",
                    homeCompanionActor.x, homeCompanionActor.y, x, y);
                return true;
            }
        }
    }
    if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
        Platform::logf(
            "[HomeFood] event=exit_failed actor=1 pos=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y);
        nextHomeFoodDebugLogMs = nowMs + 2000;
    }
    return false;
}

void AmoledApp::wakeHomeFoodArbitration(uint32_t nowMs) {
    cancelRoomAction(nowMs);
    if (homeRuntime.pairActive()) cancelPairInteraction(nowMs);
    // ADDED means the previous serving was empty. End any actor animation
    // that was only lingering until its old feeding-session timer expired so
    // ownership of the new serving can be decided from current hunger.
    const int8_t previousOwner = homeRuntime.owner(Home::Resource::BOWL);
    if (previousOwner >= 0) {
        homeRuntime.release(Home::Resource::BOWL,
                            static_cast<uint8_t>(previousOwner));
    }
    if (petMotion() == PetMotion::SEEKING_FOOD ||
        petMotion() == PetMotion::EATING ||
        (petMotion() == PetMotion::STOPPING &&
         homeMainActor.task == Home::Task::SEEK_FOOD)) {
        homeRuntime.stop(0, nowMs);
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        petFrame = 0;
    }
    if (homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::FEEDING) {
        stopCompanion(nowMs);
    }
    homeRuntime.notify(Home::WorldEvent::BOWL_CHANGED);
    homeFoodArbitrationPending = true;
    nextHomeFoodArbitrationMs = nowMs;
    homeMainActor.nextMindUpdateMs = nowMs;
    homeMainActor.nextDecisionMs = nowMs;
    if (homeCompanionActor.active) {
        homeCompanionActor.nextMindUpdateMs = nowMs;
        homeCompanionActor.nextDecisionMs = nowMs;
        homeCompanionActor.foodWakeRetryAfterMs = 0;
    }
    Platform::logf(
        "[HomeFood] event=added satiety=%u/%u pos=%.1f,%.1f companion=%.1f,%.1f\n",
        gameState.teamCount > 0
            ? static_cast<unsigned>(gameState.team[0].satiety) : 0U,
        gameState.teamCount > 1
            ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
        homeMainActor.x, homeMainActor.y, homeCompanionActor.x, homeCompanionActor.y);
}

void AmoledApp::serviceHomeFoodArbitration(uint32_t nowMs) {
    if (!homeFoodArbitrationPending ||
        static_cast<int32_t>(nowMs - nextHomeFoodArbitrationMs) < 0) {
        return;
    }
    if (gameState.room.bowlCount == 0) {
        homeFoodArbitrationPending = false;
        return;
    }

    const int8_t preferred = preferredBowlEater(nowMs);
    if (preferred < 0) {
        homeFoodArbitrationPending = false;
        return;
    }
    const Home::ActorIntent preferredIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(static_cast<uint8_t>(preferred), nowMs),
            true);
    if (preferredIntent == Home::ActorIntent::WAKE_FOR_FOOD) {
        nextHomeFoodArbitrationMs = nowMs + 80;
        return;
    }

    const bool started = preferred == 0
        ? startMainFoodSeek(nowMs)
        : startCompanionFoodSeek(nowMs);
    if (started) {
        homeFoodArbitrationPending = false;
        return;
    }
    if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
        Platform::logf(
            "[HomeFood] event=retry preferred=%d owner=%d "
            "main=%.1f,%.1f companion=%.1f,%.1f\n",
            static_cast<int>(preferred),
            static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
            homeMainActor.x, homeMainActor.y, homeCompanionActor.x, homeCompanionActor.y);
        nextHomeFoodDebugLogMs = nowMs + 2000;
    }
    RoomResource& room = RoomResource::ins();
    tryYieldHomeBlocker(static_cast<uint8_t>(preferred),
        room.available() ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X,
        room.available() ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y,
        nowMs);
    nextHomeFoodArbitrationMs = nowMs + 1000;
}

bool AmoledApp::chooseCompanionTarget(float& x, float& y) {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    const uint8_t polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    const int minimumX = room.available() ? room.walkMinX()
                                          : FALLBACK_ROOM_MIN_X;
    const int maximumX = room.available() ? room.walkMaxX()
                                          : FALLBACK_ROOM_MAX_X;
    const int minimumY = room.available() ? room.walkMinY()
                                          : FALLBACK_ROOM_MIN_Y;
    const int maximumY = room.available() ? room.walkMaxY()
                                          : FALLBACK_ROOM_MAX_Y;
    const int radiusX = std::max<int>(
        1, homeCompanionActor.behavior.wanderRadiusX);
    const int radiusY = std::max<int>(
        1, homeCompanionActor.behavior.wanderRadiusY);
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        const int candidateX = attempt < 32
            ? std::clamp(
                  static_cast<int>(std::lround(homeCompanionActor.x)) +
                      static_cast<int>(GameRandom::random(
                          -radiusX, radiusX + 1)),
                  minimumX, maximumX)
            : static_cast<int>(GameRandom::random(
                  minimumX, maximumX + 1));
        const int candidateY = attempt < 32
            ? std::clamp(
                  static_cast<int>(std::lround(homeCompanionActor.y)) +
                      static_cast<int>(GameRandom::random(
                          -radiusY, radiusY + 1)),
                  minimumY, maximumY)
            : static_cast<int>(GameRandom::random(
                  minimumY, maximumY + 1));
        if (!homeRuntime.targetAvailable(1, candidateX, candidateY)) continue;
        if (std::fabs(candidateX - homeCompanionActor.x) < 8.0f &&
            std::fabs(candidateY - homeCompanionActor.y) < 4.0f) {
            continue;
        }
        if (!RoomMovementArea::containsFootprint(
                polygon, polygonCount, static_cast<float>(candidateX),
                static_cast<float>(candidateY),
                homeCompanionActor.geometry.footprint)) {
            continue;
        }
        x = static_cast<float>(candidateX);
        y = static_cast<float>(candidateY);
        return true;
    }
    return false;
}

float AmoledApp::companionSleepMinDistance() const {
    const float mainRadius = std::clamp(
        homeMainActor.geometry.footprint.radiusX, 10.0f, 20.0f);
    const float companionRadius = std::clamp(
        homeCompanionActor.geometry.footprint.radiusX, 10.0f, 20.0f);
    return std::clamp(mainRadius + companionRadius + 8.0f, 30.0f, 44.0f);
}

bool AmoledApp::companionSleepSpotUsableWithDistance(
    float x, float y, float minDistance) const {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    const uint8_t polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    if (!RoomMovementArea::containsFootprint(
            polygon, polygonCount, x, y,
            homeCompanionActor.geometry.footprint)) {
        return false;
    }

    if (room.available() &&
        x >= static_cast<float>(room.bedMinX()) &&
        x <= static_cast<float>(room.bedMaxX()) &&
        y >= static_cast<float>(room.bedMinY()) &&
        y <= static_cast<float>(room.bedMaxY())) {
        return false;
    }

    const float dx = x - homeMainActor.x;
    const float dy = y - homeMainActor.y;
    const float visualOverlapX = homeMainActor.geometry.footprint.radiusX +
        homeCompanionActor.geometry.footprint.radiusX + 1.0f;
    const float visualOverlapY = homeMainActor.geometry.footprint.radiusY +
        homeCompanionActor.geometry.footprint.radiusY + 1.0f;
    if (std::fabs(dx) < visualOverlapX && std::fabs(dy) < visualOverlapY) {
        return false;
    }

    // Keep the companion's sleeping pose away from the leader's bed pose,
    // matching Stick's visitor sleep rule while using AMOLED ground coords.
    const float bedX = room.available()
        ? static_cast<float>(room.bedX()) : 76.0f;
    const float bedY = room.available()
        ? static_cast<float>(room.bedY()) : 99.0f;
    const float bedDx = x - bedX;
    const float bedDy = y - bedY;
    return bedDx * bedDx + bedDy * bedDy >= minDistance * minDistance;
}

bool AmoledApp::companionSleepSpotUsable(float x, float y) const {
    return companionSleepSpotUsableWithDistance(
        x, y, companionSleepMinDistance());
}

bool AmoledApp::companionPointBlocksBedRoute(float x, float y) const {
    const float clearance = std::max(
        20.0f, homeMainActor.geometry.footprint.radiusX +
                   homeCompanionActor.geometry.footprint.radiusX + 2.0f);
    const float clearanceSq = clearance * clearance;
    auto segmentDistanceSq = [x, y](float fromX, float fromY,
                                    float toX, float toY) {
        const float dx = toX - fromX;
        const float dy = toY - fromY;
        const float lengthSq = dx * dx + dy * dy;
        float t = lengthSq <= 0.001f
            ? 0.0f
            : ((x - fromX) * dx + (y - fromY) * dy) / lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
        const float nearestX = fromX + dx * t;
        const float nearestY = fromY + dy * t;
        const float distanceX = x - nearestX;
        const float distanceY = y - nearestY;
        return distanceX * distanceX + distanceY * distanceY;
    };

    float fromX = homeMainActor.x;
    float fromY = homeMainActor.y;
    if (homeMainActor.route.index >= homeMainActor.route.count) {
        return segmentDistanceSq(
            fromX, fromY, homeMainActor.targetX,
            homeMainActor.targetY) < clearanceSq;
    }
    for (uint8_t index = homeMainActor.route.index;
         index < homeMainActor.route.count; ++index) {
        const float toX = homeMainActor.route.x[index];
        const float toY = homeMainActor.route.y[index];
        if (segmentDistanceSq(fromX, fromY, toX, toY) < clearanceSq) {
            return true;
        }
        fromX = toX;
        fromY = toY;
    }
    return false;
}

bool AmoledApp::chooseCompanionSleepSpot(float& x, float& y,
                                         bool avoidBedRoute) const {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const float minDistance = companionSleepMinDistance();
    const bool mainSeekingBed = avoidBedRoute ||
        homeMainActor.task == Home::Task::SEEK_SLEEP ||
        (homeMainActor.task == Home::Task::TURNING &&
         homeMainActor.resumeTask == Home::Task::SEEK_SLEEP);

    RoomResource::BehaviorAnchor anchor{};
    const bool hasAnchor = room.available() && room.findBehaviorAnchor(
        RoomResource::BehaviorAnchorType::VISITOR_SLEEP, anchor);
    const float preferredX = hasAnchor
        ? static_cast<float>(anchor.footX)
        : (room.available() ? (room.walkMinX() + room.walkMaxX()) * 0.5f
                            : (FALLBACK_ROOM_MIN_X + FALLBACK_ROOM_MAX_X) * 0.5f);
    const float preferredY = hasAnchor
        ? static_cast<float>(anchor.footY)
        : (room.available() ? (room.walkMinY() + room.walkMaxY()) * 0.5f
                            : (FALLBACK_ROOM_MIN_Y + FALLBACK_ROOM_MAX_Y) * 0.5f);

    if (hasAnchor && companionSleepSpotUsableWithDistance(
            preferredX, preferredY, minDistance) &&
        (!mainSeekingBed || !companionPointBlocksBedRoute(
            preferredX, preferredY))) {
        x = preferredX;
        y = preferredY;
        return true;
    }

    const int minimumX = room.available() ? room.walkMinX()
                                          : FALLBACK_ROOM_MIN_X;
    const int maximumX = room.available() ? room.walkMaxX()
                                          : FALLBACK_ROOM_MAX_X;
    const int minimumY = room.available() ? room.walkMinY()
                                          : FALLBACK_ROOM_MIN_Y;
    const int maximumY = room.available() ? room.walkMaxY()
                                          : FALLBACK_ROOM_MAX_Y;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int candidateY = minimumY; candidateY <= maximumY; candidateY += 3) {
        for (int candidateX = minimumX; candidateX <= maximumX; candidateX += 3) {
            const float pointX = static_cast<float>(candidateX);
            const float pointY = static_cast<float>(candidateY);
            if (!companionSleepSpotUsableWithDistance(
                    pointX, pointY, minDistance)) continue;
            if (mainSeekingBed && companionPointBlocksBedRoute(pointX, pointY)) {
                continue;
            }
            const float dx = pointX - preferredX;
            const float dy = pointY - preferredY;
            const float score = dx * dx + dy * dy;
            if (score >= bestScore) continue;
            bestScore = score;
            x = pointX;
            y = pointY;
            found = true;
        }
    }
    return found;
}

bool AmoledApp::beginCompanionMove(Home::Task task, float x, float y,
                                   uint32_t nowMs, bool allowOutsideStart,
                                   bool avoidOther) {
    if (!homeCompanionActor.active ||
        !homeRuntime.transition(1, task, nowMs)) {
        return false;
    }
    // The visitor entry ends on the doorway point, which sits outside the
    // walk polygon. Routing out of it needs the ingress allowance, otherwise
    // every wander/seek plan fails and the visitor stays frozen at the door.
    if (!allowOutsideStart &&
        !companionFootprintInsideWalkArea(
            homeCompanionActor.x, homeCompanionActor.y)) {
        allowOutsideStart = true;
    }
    if (!homeRuntime.planRoute(
            1, x, y, allowOutsideStart, avoidOther)) {
        homeRuntime.stop(1, nowMs, 700);
        return false;
    }
    if (task == Home::Task::SEEK_SLEEP) {
        homeCompanionActor.sleepX = x;
        homeCompanionActor.sleepY = y;
        homeCompanionActor.sleepSpotValid = true;
    }
    const float dx = x - homeCompanionActor.x;
    const float dy = y - homeCompanionActor.y;
    companionLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
    companionDirection = petDirectionForDelta(dx, dy);
    companionFrame = 0;
    nextCompanionFrameMs = nowMs;
    return true;
}

void AmoledApp::stopCompanion(uint32_t nowMs, uint32_t idleDelayMs) {
    homeRuntime.stop(1, nowMs, idleDelayMs);
    companionFrame = 0;
    nextCompanionFrameMs = nowMs + 520;
}

void AmoledApp::requestHomeActorRows(float previousY, float currentY) {
    const int previousScreenY = worldToScreenY(previousY);
    const int currentScreenY = worldToScreenY(currentY);
    // Room artwork is expanded from 1x source pixels into 2x2 framebuffer
    // blocks. Keep the dirty band on that same grid and include the complete
    // sprite/shadow envelope from both positions; otherwise one row from the
    // previous frame can survive at the edge of a moving actor.
    constexpr int ACTOR_TOP_MARGIN = 192;
    constexpr int ACTOR_BOTTOM_MARGIN = 48;
    const int rawTop = std::max(
        HOME_ROOM_TOP,
        std::min(previousScreenY, currentScreenY) - ACTOR_TOP_MARGIN);
    const int rawBottom = std::min(
        HOME_STATUS_TOP,
        std::max(previousScreenY, currentScreenY) + ACTOR_BOTTOM_MARGIN);
    const int scale = AmoledUi::RESOURCE_SCALE;
    const int top = std::max(
        HOME_ROOM_TOP, rawTop - rawTop % scale);
    const int bottom = std::min(
        HOME_STATUS_TOP,
        ((rawBottom + scale - 1) / scale) * scale);
    requestRenderRows(static_cast<uint16_t>(top),
                      static_cast<uint16_t>(bottom));
}

void AmoledApp::updateCompanion(uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2) {
        lastCompanionUpdateMs = nowMs;
        return;
    }
    float elapsedSeconds = static_cast<float>(
        nowMs - lastCompanionUpdateMs) / 1000.0f;
    lastCompanionUpdateMs = nowMs;
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);

    updateVisitorMotion(nowMs, elapsedSeconds);
    if (homeCompanionActor.hidden && visitSession.active() &&
        visitSession.viewModel().localIsHost &&
        !visitSession.visitorArrivalReady()) return;
    if (visitorFlow_.state().motion == VisitorMotion::HOST_APPROACH ||
        visitorFlow_.state().motion == VisitorMotion::ENTERING ||
        visitorFlow_.state().motion == VisitorMotion::EXITING) {
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    {
        static uint32_t s_companionSampleMs = 0;
        if (static_cast<int32_t>(nowMs - s_companionSampleMs) >= 0) {
            s_companionSampleMs = nowMs + 1000;
            Platform::logf(
                "[HomeDiag] companion task=%d motion=%d pos=%.1f,%.1f "
                "target=%.1f,%.1f decisionIn=%ld moveMode=%d pair=%d "
                "visitor=%d\n",
                static_cast<int>(homeCompanionActor.task),
                static_cast<int>(homeCompanionActor.motion),
                homeCompanionActor.x, homeCompanionActor.y,
                homeCompanionActor.targetX, homeCompanionActor.targetY,
                static_cast<long>(static_cast<int32_t>(
                    homeCompanionActor.nextDecisionMs - nowMs)),
                static_cast<int>(homeCompanionActor.behavior.movementMode),
                static_cast<int>(pairFlow_.state().phase),
                static_cast<int>(visitorFlow_.state().motion));
        }
    }
#endif

    Game::MonsterRuntime& monster = gameState.team[1];
    const Home::ActorIntent initialCompanionIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(1, nowMs),
            gameState.room.bowlCount > 0);
    if (initialCompanionIntent == Home::ActorIntent::FAINT_REST) {
        cancelPairInteraction(nowMs);
        homeRuntime.transition(1, Home::Task::FAINTED, nowMs, 0, true);
        if (!homeCompanionActor.faintRestActive) {
            float sleepX = homeCompanionActor.x;
            float sleepY = homeCompanionActor.y;
            if (chooseCompanionSleepSpot(sleepX, sleepY)) {
                homeCompanionActor.x = sleepX;
                homeCompanionActor.y = sleepY;
            }
            homeCompanionActor.targetX = homeCompanionActor.x;
            homeCompanionActor.targetY = homeCompanionActor.y;
            homeCompanionActor.faintRestActive = true;
            companionFrame = 0;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestHomeActorRows(
                homeCompanionActor.y, homeCompanionActor.y);
        }
        return;
    }
    if (homeCompanionActor.faintRestActive) {
        homeCompanionActor.faintRestActive = false;
        stopCompanion(nowMs, 700);
        homeCompanionActor.mind.reset(nowMs);
    }

    PokemonSprites::PetAnimationProfile animation{};
    PokemonSprites::petAnimationProfile(monster.speciesId, animation);
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool sleepTime = care.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    if (static_cast<int32_t>(nowMs -
                             homeCompanionActor.nextMindUpdateMs) >= 0) {
        homeCompanionActor.mind.update(
            monster, sleepTime,
            care.needsFood && monster.origin != Game::Origin::VISITOR &&
                gameState.room.bowlCount > 0,
            nowMs);
        homeCompanionActor.nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
    }

    if (homeCompanionActor.task == Home::Task::PAIR_ACTION) return;

    if (homeCompanionActor.task == Home::Task::FEEDING) {
        const bool canEat =
            homeRuntime.owner(Home::Resource::BOWL) == 1 &&
            monster.origin != Game::Origin::VISITOR &&
            gameState.room.bowlCount > 0 &&
            monster.satiety < MONSTER_FEED_TARGET_SATIETY;
        if (canEat && static_cast<int32_t>(nowMs - companionFoodBiteMs) >= 0) {
            FoodConsumeResult result =
                Game::HomeCare::consumeBowlFood(gameState, 1);
            companionFoodBiteMs = nowMs + GameRandom::range(1000, 1601);
            if (result.consumed) {
                applyHomeCareExperience(1, result.careExp);
                if (result.reaction == FoodReaction::LIKED ||
                    result.foodIndex == Game::ROOM_TASTY_FOOD_INDEX) {
                    heartsUntil = nowMs + 900;
                }
                requestRenderRows(HOME_ROOM_TOP, 448);
                saveState();
            }
        }
        if (!canEat ||
            static_cast<int32_t>(nowMs - homeCompanionActor.taskUntilMs) >= 0) {
            homeCompanionActor.mind.onAte(nowMs);
            homeRuntime.beginBowlClearance(1, nowMs);
            stopCompanion(nowMs);
            if (homeActorBlocksFoodApproach(1)) {
                startCompanionFoodExit(nowMs);
            }
            if (gameState.room.bowlCount > 0 &&
                preferredBowlEater(nowMs) >= 0) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs;
                homeMainActor.nextMindUpdateMs = nowMs;
                homeMainActor.nextDecisionMs = nowMs;
            }
            requestRenderRows(HOME_ROOM_TOP, 448);
        }
        return;
    }

    if (homeCompanionActor.task == Home::Task::SLEEPING) {
        const Home::ActorIntent sleepIntent =
            Home::ActorController::survivalIntent(
                homeActorObservation(1, nowMs),
                gameState.room.bowlCount > 0);
        const bool wakeForFood =
            sleepIntent == Home::ActorIntent::WAKE_FOR_FOOD;
        if (sleepIntent == Home::ActorIntent::WAKE || wakeForFood) {
            homeRuntime.release(Home::Resource::BED, 1);
            homeCompanionActor.sleepSpotValid = false;
            stopCompanion(nowMs, wakeForFood ? 0 : 700);
            homeCompanionActor.foodWakeRetryAfterMs =
                wakeForFood ? nowMs + COMPANION_NIGHT_FOOD_RETRY_MS : 0;
            homeCompanionActor.mind.onRested(nowMs);
        } else if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestHomeActorRows(
                homeCompanionActor.y, homeCompanionActor.y);
        }
        return;
    }

    const bool companionClearingBowl =
        homeRuntime.bowlSession().clearing() &&
        homeRuntime.bowlSession().actorId == 1;
    if (homeCompanionActor.task == Home::Task::IDLE &&
        homeActorBlocksFoodApproach(1) &&
        (companionClearingBowl ||
         (gameState.room.bowlCount > 0 &&
          preferredBowlEater(nowMs) == 0))) {
        if (static_cast<int32_t>(nowMs -
                                 homeCompanionActor.nextDecisionMs) >= 0) {
            startCompanionFoodExit(nowMs);
        }
        return;
    }

    const bool routedTask = homeCompanionActor.task == Home::Task::WANDER ||
        homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::SEEK_SLEEP ||
        homeCompanionActor.task == Home::Task::YIELDING;
    if (routedTask) {
        float waypointX = homeCompanionActor.targetX;
        float waypointY = homeCompanionActor.targetY;
        homeCompanionActor.route.current(waypointX, waypointY);
        companionDirection = petDirectionForDelta(
            waypointX - homeCompanionActor.x,
            waypointY - homeCompanionActor.y);
        const float previousY = homeCompanionActor.y;
        const float speed = Home::ActorController::routeSpeed(
            homeCompanionActor.task, homeCompanionActor.behavior, monster, 3.0f);
        const Home::RouteStep result = homeRuntime.advanceRoute(
            1, nowMs, speed, elapsedSeconds, 0.8f,
            true);
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
            requestHomeActorRows(previousY, homeCompanionActor.y);
        }
        if (result == Home::RouteStep::ARRIVED) {
            const Home::Task arrivedTask = homeCompanionActor.task;
            if (arrivedTask == Home::Task::SEEK_FOOD) {
                companionDirection = foodFacingDirection(
                    homeCompanionActor.x, homeCompanionActor.y);
                companionFrame = 0;
                homeRuntime.transition(
                    1, Home::Task::FEEDING, nowMs,
                    static_cast<uint32_t>(GameRandom::range(3200, 5601)),
                    true);
                companionFoodBiteMs = nowMs + GameRandom::range(700, 1301);
            } else if (arrivedTask == Home::Task::SEEK_SLEEP) {
                homeRuntime.transition(
                    1, Home::Task::SLEEPING, nowMs, 0, true);
                nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            } else {
                if (arrivedTask == Home::Task::YIELDING &&
                    homeRuntime.bowlSession().clearing() &&
                    homeRuntime.bowlSession().actorId == 1 &&
                    !homeActorBlocksFoodApproach(1)) {
                    homeRuntime.finishBowlClearance(1, nowMs);
                }
                stopCompanion(nowMs, static_cast<uint32_t>(GameRandom::range(
                    homeCompanionActor.behavior.idleMinMs,
                    homeCompanionActor.behavior.idleMaxMs + 1)));
            }
            requestHomeActorRows(previousY, homeCompanionActor.y);
        } else if (result == Home::RouteStep::BLOCKED ||
                   result == Home::RouteStep::NO_ROUTE) {
            const bool seekingFood = homeCompanionActor.task == Home::Task::SEEK_FOOD;
            if (seekingFood || homeCompanionActor.task == Home::Task::SEEK_SLEEP) {
                tryYieldHomeBlocker(1, homeCompanionActor.targetX,
                                    homeCompanionActor.targetY, nowMs);
            }
            if (seekingFood) {
                Platform::logf(
                    "[HomeFood] event=blocked actor=1 stage=%s owner=%d "
                    "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
                    result == Home::RouteStep::BLOCKED
                        ? "advance" : "no_route",
                    static_cast<int>(homeRuntime.owner(
                        Home::Resource::BOWL)),
                    homeCompanionActor.x, homeCompanionActor.y,
                    homeCompanionActor.targetX,
                    homeCompanionActor.targetY, homeMainActor.x, homeMainActor.y);
            }
            stopCompanion(nowMs, 1000);
            if (seekingFood) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs + 1000;
            }
        }
        return;
    }

    if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
        const uint8_t frameCount = std::max<uint8_t>(
            1, animation.idleFrames);
        companionFrame = static_cast<uint8_t>(
            (companionFrame + 1) % frameCount);
        nextCompanionFrameMs = nowMs +
            (animation.idleFrameMs ? animation.idleFrameMs : 520);
        requestHomeActorRows(
            homeCompanionActor.y, homeCompanionActor.y);
    }
    if (homeFoodArbitrationPending &&
        preferredBowlEater(nowMs) == 1) return;
    if (homeCompanionActor.behavior.movementMode ==
            MonsterMovementMode::STATIONARY ||
        static_cast<int32_t>(nowMs -
                             homeCompanionActor.nextDecisionMs) < 0) {
        return;
    }

    const Home::ActorIntent companionIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(1, nowMs),
            gameState.room.bowlCount > 0);
    if (companionIntent == Home::ActorIntent::SEEK_FOOD &&
        preferredBowlEater(nowMs) == 1) {
        if (startCompanionFoodSeek(nowMs)) {
            return;
        }
    }

    if (companionIntent == Home::ActorIntent::SEEK_SLEEP) {
        float sleepX = homeCompanionActor.x;
        float sleepY = homeCompanionActor.y;
        if (chooseCompanionSleepSpot(sleepX, sleepY) &&
            beginCompanionMove(Home::Task::SEEK_SLEEP,
                               sleepX, sleepY, nowMs)) {
            return;
        }
    }

    float targetX = homeCompanionActor.x;
    float targetY = homeCompanionActor.y;
    if (chooseCompanionTarget(targetX, targetY) &&
        beginCompanionMove(Home::Task::WANDER, targetX, targetY, nowMs)) {
        const float dx = targetX - homeCompanionActor.x;
        const float dy = targetY - homeCompanionActor.y;
        companionLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
        companionDirection = petDirectionForDelta(dx, dy);
        homeCompanionActor.targetX = targetX;
        homeCompanionActor.targetY = targetY;
        homeCompanionActor.motion = Home::MotionPhase::ROUTE;
        companionFrame = 0;
        nextCompanionFrameMs = nowMs;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[HomeDiag] companion wander from=%.1f,%.1f to=%.1f,%.1f\n",
                       homeCompanionActor.x, homeCompanionActor.y,
                       targetX, targetY);
#endif
    } else {
        homeCompanionActor.nextDecisionMs = nowMs + 700;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[HomeDiag] companion decision failed pos=%.1f,%.1f "
                       "target=%.1f,%.1f intent=%d\n",
                       homeCompanionActor.x, homeCompanionActor.y,
                       targetX, targetY,
                       static_cast<int>(companionIntent));
#endif
    }
}

PairFlow::Environment AmoledApp::pairEnvironment() const {
    PairFlow::Environment environment;
    const bool home = sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        homeFlow_.state().action == RoomAction::NONE;
    environment.ordinaryStartAllowed = home && petMotion() == PetMotion::IDLE &&
        visitorFlow_.state().motion != VisitorMotion::HOST_APPROACH &&
        visitorFlow_.state().motion != VisitorMotion::ENTERING && visitorFlow_.state().motion != VisitorMotion::EXITING;
#if STICKMON_HAS_CLAW
    if (Stickmon::ClawRuntime::instance().autonomyActive()) environment.ordinaryStartAllowed = false;
#endif
    environment.forcedTalkAllowed = home && visitorFlow_.state().motion == VisitorMotion::ACTIVE;
    environment.visitorExiting = visitorFlow_.state().motion == VisitorMotion::EXITING;
    return environment;
}

PairFlow::Layout AmoledApp::pairLayout(bool includeSpacing) const {
    const RoomResource& room = RoomResource::ins();
    PairFlow::Layout layout;
    layout.bounds = {
        static_cast<float>(room.available() ? room.walkMinX() : FALLBACK_ROOM_MIN_X),
        static_cast<float>(room.available() ? room.walkMinY() : FALLBACK_ROOM_MIN_Y),
        static_cast<float>(room.available() ? room.walkMaxX() : FALLBACK_ROOM_MAX_X),
        static_cast<float>(room.available() ? room.walkMaxY() : FALLBACK_ROOM_MAX_Y)};
    if (includeSpacing) {
        layout.talkDistance = (pairIdleWidthPx(gameState.team[0].speciesId) +
            pairIdleWidthPx(gameState.team[1].speciesId)) / (2.0f * AmoledUi::RESOURCE_SCALE) +
            PAIR_TALK_MIN_GAP_PX / static_cast<float>(AmoledUi::RESOURCE_SCALE);
    }
    return layout;
}

bool AmoledApp::pairInteractionAllowed() const {
    return pairFlow_.eligible(pairEnvironment());
}

void AmoledApp::schedulePairInteraction(uint32_t nowMs, bool immediate) {
    pairFlow_.schedule(nowMs, immediate);
}

bool AmoledApp::startPairInteraction(uint32_t nowMs, bool forceChase,
                                    bool forceTalk, bool arrivalTalk) {
    const auto environment = pairEnvironment();
    const bool allowed = forceTalk ? pairFlow_.forcedTalkAllowed(environment) : pairFlow_.eligible(environment);
    const bool started = pairFlow_.start(pairLayout(allowed), environment, nowMs,
                                        forceChase, forceTalk, arrivalTalk);
    applyPairFlowEffects(nowMs);
    return started;
}

bool AmoledApp::updatePairInteraction(uint32_t nowMs, float elapsedSeconds) {
    const auto environment = pairEnvironment();
    const auto layout = pairLayout(pairFlow_.needsStart(nowMs) && pairFlow_.eligible(environment));
    pairFlow_.update(layout, environment, nowMs, elapsedSeconds);
    applyPairFlowEffects(nowMs);
#if STICKMON_ENABLE_DEBUG_FEATURES
    const auto& pair = pairFlow_.state();
    if (pair.phase == PairPhase::APPROACH && pair.parallelApproach &&
        static_cast<int32_t>(nowMs - visitorDiagNextLogMs) >= 0) {
        visitorDiagNextLogMs = nowMs + 300;
        Platform::logf("[FriendDiag] talk approach-parallel kind=%u main=%.1f,%.1f "
                       "companion=%.1f,%.1f arrived=%u,%u\n",
                       static_cast<unsigned>(debugContactKind), homeMainActor.x, homeMainActor.y,
                       homeCompanionActor.x, homeCompanionActor.y,
                       pair.mainArrived ? 1U : 0U, pair.companionArrived ? 1U : 0U);
    }
#endif
    const bool active = pairFlow_.completeMovement(layout, nowMs);
    applyPairFlowEffects(nowMs);
    return active;
}

void AmoledApp::finishPairInteraction(uint32_t nowMs, bool reward) {
    pairFlow_.finish(nowMs, reward);
    applyPairFlowEffects(nowMs);
}

void AmoledApp::cancelPairInteraction(uint32_t nowMs) {
    pairFlow_.cancel(nowMs);
    applyPairFlowEffects(nowMs);
}

void AmoledApp::applyPairFlowEffects(uint32_t nowMs) {
    const auto effects = pairFlow_.takeEffects();
    if (effects.directionMask & 1) petDirection = effects.direction[0];
    if (effects.directionMask & 2) companionDirection = effects.direction[1];
    if (effects.camera) updateCamera();
    if ((effects.frameMask & 1) && static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        ++petFrame; nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    if ((effects.frameMask & 2) && static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
        ++companionFrame; nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
    }
    if (effects.stopAnimation) petStopping = false;
    if (effects.heart == PairFlow::Heart::SHOW) heartsUntil = effects.heartsUntilMs;
    else if (effects.heart == PairFlow::Heart::CLEAR) heartsUntil = 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
    for (uint8_t i = 0; i < effects.traceCount; ++i) {
        const auto& trace = effects.traces[i];
        const auto& pair = pairFlow_.state();
        switch (trace.kind) {
        case PairFlow::Trace::START:
            Platform::logf("[HomeDiag] pair start activity=%d leaderMain=%d main=%.1f,%.1f companion=%.1f,%.1f\n",
                static_cast<int>(pair.activity), pair.leaderMain ? 1 : 0,
                homeMainActor.x, homeMainActor.y, homeCompanionActor.x, homeCompanionActor.y);
            break;
        case PairFlow::Trace::FIXED_STAGE:
            Platform::logf("[FriendDiag] talk fixed-stage companion=%.1f,%.1f main=%.1f,%.1f speeds=%.2f,%.2f\n",
                PairFlow::TALK_COMPANION_X, PairFlow::TALK_STAGE_Y, PairFlow::TALK_MAIN_X, PairFlow::TALK_STAGE_Y,
                pair.companionApproachSpeed, pair.mainApproachSpeed);
            break;
        case PairFlow::Trace::POSITIONED: {
            const int dx = std::abs(worldToScreenX(homeMainActor.x) - worldToScreenX(homeCompanionActor.x));
            const int dy = std::abs(worldToScreenY(homeMainActor.y) - worldToScreenY(homeCompanionActor.y));
            const int firstWidth = pairIdleWidthPx(gameState.team[0].speciesId);
            const int secondWidth = pairIdleWidthPx(gameState.team[1].speciesId);
            const int gap = dx - (firstWidth + secondWidth) / 2;
            if (pair.parallelApproach && gap < PAIR_TALK_MIN_GAP_PX) {
                Platform::logf("[FriendDiag] talk configured-gap kind=%u gapXpx=%d minimum=%d (fixed points kept)\n",
                    static_cast<unsigned>(debugContactKind), gap, PAIR_TALK_MIN_GAP_PX);
            }
            Platform::logf("[FriendDiag] talk positioned kind=%u parallel=%u dxPx=%d dyPx=%d widths=%d,%d gapXpx=%d main=%.1f,%.1f companion=%.1f,%.1f\n",
                static_cast<unsigned>(debugContactKind), pair.parallelApproach ? 1U : 0U,
                dx, dy, firstWidth, secondWidth, gap,
                homeMainActor.x, homeMainActor.y, homeCompanionActor.x, homeCompanionActor.y);
            Platform::logf("[FriendDiag] arrival talk-ready kind=%u mainDir=%u guestDir=%u\n",
                static_cast<unsigned>(debugContactKind), static_cast<unsigned>(petDirection),
                static_cast<unsigned>(companionDirection));
            break;
        }
        case PairFlow::Trace::ACTIVE:
            Platform::logf("[FriendDiag] talk active kind=%u durationMs=%u\n",
                static_cast<unsigned>(debugContactKind),
                static_cast<unsigned>(pair.phaseUntilMs - pair.phaseStartedMs));
            break;
        case PairFlow::Trace::FINISH:
            Platform::logf("[HomeDiag] pair finish phase=%d reward=%d\n", static_cast<int>(trace.phase), effects.reward ? 1 : 0);
            break;
        case PairFlow::Trace::ROUTE_FAILED:
            Platform::logf("[FriendDiag] talk route-failed kind=%u main=%.1f,%.1f companion=%.1f,%.1f\n",
                static_cast<unsigned>(debugContactKind), homeMainActor.x, homeMainActor.y,
                homeCompanionActor.x, homeCompanionActor.y);
            break;
        case PairFlow::Trace::NEAR_GOAL:
        case PairFlow::Trace::APPROACH_FAILED:
            Platform::logf("[FriendDiag] talk %s kind=%u role=%s step=%d distance=%.2f timeout=%u\n",
                trace.kind == PairFlow::Trace::NEAR_GOAL ? "approach-near-goal" : "approach-failed",
                static_cast<unsigned>(debugContactKind), trace.role == 0 ? "main" : "companion",
                static_cast<int>(trace.step), static_cast<double>(trace.distance), trace.timedOut ? 1U : 0U);
            break;
        }
    }
#endif
    if (effects.finished) {
        // Keep PRNG and progression order: decision, pair interval, each care grant/XP, save.
        schedulePetDecision(nowMs);
        pairFlow_.schedule(nowMs);
        if (effects.reward) {
            const bool moodReward = gameState.pairMoodRewardsToday < 3;
            for (uint8_t slot = 0; slot < 2; ++slot) {
                auto& monster = gameState.team[slot];
                const uint32_t experience = Game::CareExperience::grant(gameState, slot,
                    Game::CareExperience::Source::PLAY);
                applyHomeCareExperience(slot, experience);
                if (moodReward && !monster.fainted && monster.hpCur > 0 && monster.mood < 100) ++monster.mood;
            }
            if (moodReward) ++gameState.pairMoodRewardsToday;
            saveState();
        }
    }
    if (effects.roomChanged) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}


VisitorFlow::Layout AmoledApp::visitorLayout() const {
    const RoomResource& room = RoomResource::ins();
    VisitorFlow::Layout layout;
    layout.available = room.available();
    layout.polygon = room.walkPolygon();
    layout.polygonCount = room.walkPolygonCount();
    layout.walkPolygon = room.available() ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    layout.walkCount = room.available() ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) / sizeof(FALLBACK_WALK_POLYGON[0]));
    layout.bounds = {room.available() ? static_cast<float>(room.walkMinX()) : FALLBACK_ROOM_MIN_X,
        room.available() ? static_cast<float>(room.walkMinY()) : FALLBACK_ROOM_MIN_Y,
        room.available() ? static_cast<float>(room.walkMaxX()) : FALLBACK_ROOM_MAX_X,
        room.available() ? static_cast<float>(room.walkMaxY()) : FALLBACK_ROOM_MAX_Y};
    layout.insideX = room.available() ? room.doorwayInsideX() : FALLBACK_ROOM_MAX_X - 8.0f;
    layout.insideY = room.available() ? room.doorwayInsideY() : FALLBACK_ROOM_MAX_Y;
    layout.outsideX = room.available() ? room.doorwayOutsideX() : layout.insideX + 12.0f;
    layout.outsideY = room.available() ? room.doorwayOutsideY() : layout.insideY + 10.0f;
    return layout;
}

void AmoledApp::beginVisitDeparture(uint32_t nowMs) {
    visitRecallConfirm = false;
    cancelPairInteraction(nowMs);
    cancelRoomAction(nowMs);
    const auto layout = visitorLayout();
    visitAwayFlow_.begin({layout.available, layout.insideX, layout.insideY,
                         layout.outsideX, layout.outsideY}, nowMs);
    applyVisitAwayEffects(nowMs);
}

bool AmoledApp::updateVisitDeparture(uint32_t nowMs) {
    const float elapsedSeconds = lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs
        ? 0.0f : std::min<uint32_t>(nowMs - lastPetUpdateMs, 120U) / 1000.0f;
    const bool active = visitAwayFlow_.update(visitSession.active(), nowMs, elapsedSeconds);
    applyVisitAwayEffects(nowMs);
    visitAwayFlow_.completeMovement(nowMs);
    applyVisitAwayEffects(nowMs);
    return active;
}

void AmoledApp::applyVisitAwayEffects(uint32_t nowMs) {
    const auto effects = visitAwayFlow_.takeEffects();
    if (effects.clearRecall) visitRecallConfirm = false;
    if (effects.stopAnimation || effects.resetAnimation) petStopping = false;
    if (effects.resetAnimation) nextPetFrameMs = nowMs;
    if (effects.directionChanged) petDirection = effects.direction;
    if (effects.startWalk && static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        ++petFrame; nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    if (effects.camera) updateCamera();
    if (effects.actorChanged) requestHomeActorRows(effects.previousY, homeMainActor.y);
    if (effects.departed) visitSession.markVisitorDeparted();
    if (effects.fullRender) requestFullRender();
}

void AmoledApp::beginVisitorEntry(uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR) return;
    cancelRoomAction(nowMs);
    const auto layout = visitorLayout();
    cancelPairInteraction(nowMs);
    visitorFlow_.beginVisitorEntry(layout, nowMs);
    applyVisitorEffects(nowMs);
}

void AmoledApp::beginVisitorExit(uint32_t nowMs, bool debugVisitor) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR ||
        visitorFlow_.state().motion == VisitorMotion::EXITING) return;
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    visitorFlow_.beginVisitorExit(visitorLayout(), nowMs, debugVisitor);
    applyVisitorEffects(nowMs);
}

void AmoledApp::updateVisitorMotion(uint32_t nowMs, float elapsedSeconds) {
    const auto layout = visitorLayout();
    visitorFlow_.update(layout, nowMs, elapsedSeconds);
    applyVisitorEffects(nowMs);
    visitorFlow_.completeMovement(layout, nowMs);
    applyVisitorEffects(nowMs);
}

void AmoledApp::applyVisitorEffects(uint32_t nowMs) {
    // A landing request hands off only the existing autonomous target selection.
    // Its acknowledgement produces the entry pose/frame effects in the next pass.
    for (uint8_t pass = 0; pass < 2; ++pass) {
        const auto effects = visitorFlow_.takeEffects();
        if (effects.stopAnimation) petStopping = false;
        if (effects.guestIdle) { companionFrame = 0; nextCompanionFrameMs = nowMs + 520; }
        if (effects.directionMask & 1) petDirection = effects.direction[0];
        if (effects.directionMask & 2) companionDirection = effects.direction[1];
        if (effects.resetFrameMask & 1) { petFrame = 0; nextPetFrameMs = nowMs; }
        if (effects.resetFrameMask & 2) { companionFrame = 0; nextCompanionFrameMs = nowMs; }
        if ((effects.frameMask & 1) && static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            ++petFrame; nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if ((effects.frameMask & 2) && static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame; nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (effects.camera) updateCamera();
        for (uint8_t i = 0; i < effects.traceCount; ++i) {
            const auto& trace = effects.traces[i];
            const char* label = nullptr;
            using Trace = VisitorFlow::Trace;
            switch (trace.kind) {
            case Trace::HOST_CLEAR: label = "host_clear_begin"; break;
            case Trace::CLEAR_UNAVAILABLE: label = "host_clear_unavailable"; break;
            case Trace::WELCOME_UNAVAILABLE: label = "welcome_unavailable"; break;
            case Trace::RECOVER: label = "host_recover"; break;
            case Trace::HOST_FALLBACK: label = "host_to_door_fallback"; break;
            case Trace::HOST_APPROACH: label = "host_approach_begin"; break;
            case Trace::GUEST_VISIBLE: label = "guest_enter_begin"; break;
            case Trace::EXIT_BEGIN: label = "guest_exit_begin"; break;
            case Trace::ENTERED: label = "guest_enter_arrived"; break;
            case Trace::EXITED: label = "guest_exit_end"; break;
            case Trace::DOOR_OPEN: label = "host_open_done"; break;
            case Trace::CLEAR_END: label = "host_clear_end"; break;
            case Trace::HOST_READY: label = "host_approach_end"; break;
            }
            Platform::logf("[VisitArrival] %s host=%.1f,%.1f guest=%.1f,%.1f goal=%.1f,%.1f\n",
                label, trace.mainX, trace.mainY, trace.guestX, trace.guestY, trace.targetX, trace.targetY);
#if STICKMON_ENABLE_DEBUG_FEATURES
            const char* debugLabel = nullptr;
            switch (trace.kind) {
            case Trace::HOST_APPROACH: debugLabel = "arrival host-approach"; break;
            case Trace::HOST_FALLBACK: debugLabel = "arrival host-route-failed"; break;
            case Trace::HOST_READY: debugLabel = "arrival host-ready"; break;
            case Trace::GUEST_VISIBLE: debugLabel = "arrival visitor-visible"; break;
            case Trace::ENTERED: debugLabel = "arrival visitor-entered"; break;
            case Trace::EXIT_BEGIN: debugLabel = "visitor-exit begin"; break;
            case Trace::EXITED: debugLabel = "visitor-exit finish"; break;
            default: break;
            }
            if (debugLabel) Platform::logf("[FriendDiag] %s kind=%u host=%.1f,%.1f guest=%.1f,%.1f\n",
                debugLabel, static_cast<unsigned>(debugContactKind), trace.mainX, trace.mainY,
                trace.guestX, trace.guestY);
#endif
        }
        if (effects.entryFinished) finishVisitorEntry(nowMs, effects.timedOut);
        if (effects.exitFinished) finishVisitorExit(nowMs, effects.debugExit);
        if (effects.actorChanged) requestHomeActorRows(effects.previousY,
            effects.actorId == 0 ? homeMainActor.y : homeCompanionActor.y);
        if (effects.roomChanged) requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        if (effects.fullRender) requestFullRender();
        if (!effects.needLanding) break;
        float x = visitorFlow_.state().landingX, y = visitorFlow_.state().landingY;
        const bool valid = visitorFlow_.state().landingValid || chooseCompanionTarget(x, y);
        visitorFlow_.completeLanding(valid, x, y);
    }
}

void AmoledApp::finishVisitorEntry(uint32_t nowMs, bool timedOut) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    showDebugContactEventPrompt(nowMs);
#endif
    const bool talkStarted = startPairInteraction(nowMs, false, true, true);
    Platform::logf("[VisitArrival] guest_enter_end host=%.1f,%.1f guest=%.1f,%.1f "
                   "timeout=%u talkStarted=%u\n", homeMainActor.x, homeMainActor.y,
                   homeCompanionActor.x, homeCompanionActor.y, timedOut ? 1U : 0U,
                   talkStarted ? 1U : 0U);
    if (!talkStarted) {
        visitArrival.advance(Home::VisitArrivalEvent::TALK_SKIPPED, nowMs);
        schedulePairInteraction(nowMs, true);
    }
}

void AmoledApp::finishVisitorExit(uint32_t nowMs, bool debugVisitor) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugVisitor) { finalizeDebugContact(nowMs); return; }
#else
    (void)debugVisitor;
#endif
    if (gameState.teamCount > 1 && gameState.team[1].origin == Game::Origin::VISITOR) {
        gameState.team[1] = Game::MonsterRuntime{};
        gameState.teamCount = 1;
        gameState.activeSlot = 0;
    }
    homeCompanionActor = Home::Actor{};
    homeRuntime.attach(homeMainActor);
    configureHomeRuntime();
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    speciesIds[0] = gameState.team[0].speciesId;
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    schedulePairInteraction(nowMs);
    requestFullRender();
}


void AmoledApp::requestVisitEnd(uint32_t nowMs) {
    Game::MonsterRuntime guest{};
    const bool hasGuest = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    if (hasGuest) guest = gameState.team[1];
    visitSession.endVisit();
    if (hasGuest && gameState.teamCount < 2) {
        gameState.team[1] = guest;
        gameState.teamCount = 2;
        gameState.activeSlot = 0;
    }
    if (hasGuest) beginVisitorExit(nowMs, false);
    sceneFlow.goHome();
    requestFullRender();
}

bool AmoledApp::petFootprintInsideWalkArea(float x, float y) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    RoomMovementArea::Footprint footprint = {
        homeMainActor.geometry.footprint.radiusX, homeMainActor.geometry.footprint.radiusY};
    return RoomMovementArea::containsFootprint(
        polygon, count, x, y, footprint);
}

bool AmoledApp::companionFootprintInsideWalkArea(float x, float y) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    return RoomMovementArea::containsFootprint(
        polygon, count, x, y, homeCompanionActor.geometry.footprint);
}

bool AmoledApp::petPathInsideWalkArea(float fromX, float fromY,
                                      float toX, float toY) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    RoomMovementArea::Footprint footprint = {
        homeMainActor.geometry.footprint.radiusX, homeMainActor.geometry.footprint.radiusY};
    return RoomMovementArea::segmentInsideFootprint(
        polygon, count, fromX, fromY, toX, toY, footprint);
}

bool AmoledApp::chooseWanderTarget(float& x, float& y,
                                   bool requirePath) const {
    RoomResource& room = RoomResource::ins();
    int minimumX = room.available() ? room.walkMinX()
                                    : static_cast<int>(FALLBACK_ROOM_MIN_X);
    int maximumX = room.available() ? room.walkMaxX()
                                    : static_cast<int>(FALLBACK_ROOM_MAX_X);
    int minimumY = room.available() ? room.walkMinY()
                                    : static_cast<int>(FALLBACK_ROOM_MIN_Y);
    int maximumY = room.available() ? room.walkMaxY()
                                    : static_cast<int>(FALLBACK_ROOM_MAX_Y);
    for (uint8_t attempt = 0; attempt < 40; ++attempt) {
        int candidateX;
        int candidateY;
        if (attempt < 24) {
            int radiusX = std::max<int>(1, homeMainActor.behavior.wanderRadiusX);
            int radiusY = std::max<int>(1, homeMainActor.behavior.wanderRadiusY);
            int rawX = static_cast<int>(std::lround(homeMainActor.x)) +
                       static_cast<int>(GameRandom::random(
                           -radiusX, radiusX + 1));
            int rawY = static_cast<int>(std::lround(homeMainActor.y)) +
                       static_cast<int>(GameRandom::random(
                           -radiusY, radiusY + 1));
            candidateX = std::clamp(rawX, minimumX, maximumX);
            candidateY = std::clamp(rawY, minimumY, maximumY);
        } else {
            candidateX = GameRandom::random(minimumX, maximumX + 1);
            candidateY = GameRandom::random(minimumY, maximumY + 1);
        }
        if (!petFootprintInsideWalkArea(candidateX, candidateY) ||
            (homeMainActor.active &&
             !homeRuntime.targetAvailable(0, candidateX, candidateY))) continue;
        if (requirePath &&
            !petPathInsideWalkArea(homeMainActor.x, homeMainActor.y, candidateX, candidateY)) {
            continue;
        }
        if (std::fabs(candidateX - homeMainActor.x) < 8.0f &&
            std::fabs(candidateY - homeMainActor.y) < 4.0f) {
            continue;
        }
        x = static_cast<float>(candidateX);
        y = static_cast<float>(candidateY);
        return true;
    }
    return false;
}

bool AmoledApp::chooseWakeTarget(float& x, float& y) const {
    RoomResource& room = RoomResource::ins();
    const float bedX = room.available() ? static_cast<float>(room.bedX()) : 76.0f;
    const float bedY = room.available() ? static_cast<float>(room.bedY()) : 99.0f;
    const float originX = homeMainActor.x;
    const float originY = homeMainActor.y;

    // Prefer a nearby point so waking does not look like a teleport, while
    // requiring enough distance to visibly clear the bed footprint.
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        float candidateX = 0.0f;
        float candidateY = 0.0f;
        if (!chooseWanderTarget(candidateX, candidateY, true)) continue;
        const float bedDx = candidateX - bedX;
        const float bedDy = candidateY - bedY;
        if (bedDx * bedDx + bedDy * bedDy < 18.0f * 18.0f) continue;
        if (!petPathInsideWalkArea(originX, originY, candidateX, candidateY)) {
            continue;
        }
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool AmoledApp::chooseFoodApproachTarget(float desiredX, float desiredY,
                                         float& x, float& y) const {
    auto tryCandidate = [&](float candidateX, float candidateY) {
        if (!homeRuntime.targetAvailable(0, candidateX, candidateY) ||
            !petPathInsideWalkArea(
                homeMainActor.x, homeMainActor.y, candidateX, candidateY)) {
            return false;
        }
        x = candidateX;
        y = candidateY;
        return true;
    };

    if (tryCandidate(desiredX, desiredY)) return true;
    for (int radius = 2; radius <= 36; radius += 2) {
        int yRadius = std::min(radius, 24);
        for (int dy = -yRadius; dy <= yRadius; dy += 2) {
            if (tryCandidate(desiredX + radius, desiredY + dy) ||
                tryCandidate(desiredX - radius, desiredY + dy)) {
                return true;
            }
        }
        for (int dx = -radius + 2; dx <= radius - 2; dx += 2) {
            if (tryCandidate(desiredX + dx, desiredY + yRadius) ||
                tryCandidate(desiredX + dx, desiredY - yRadius)) {
                return true;
            }
        }
    }
    return false;
}

void AmoledApp::updateCamera() {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) {
        cameraX = 0.0f;
        cameraY = 0.0f;
        return;
    }

    float screenX = homeMainActor.x - cameraX;
    if (screenX < CAMERA_SAFE_LEFT) cameraX = homeMainActor.x - CAMERA_SAFE_LEFT;
    else if (screenX > CAMERA_SAFE_RIGHT) cameraX = homeMainActor.x - CAMERA_SAFE_RIGHT;

    float screenY = homeMainActor.y - cameraY;
    if (screenY < CAMERA_SAFE_TOP) cameraY = homeMainActor.y - CAMERA_SAFE_TOP;
    else if (screenY > CAMERA_SAFE_BOTTOM) cameraY = homeMainActor.y - CAMERA_SAFE_BOTTOM;

    float maximumCameraX = std::max<float>(0.0f,
        static_cast<float>(room.width() - HOME_ROOM_WIDTH / AmoledUi::RESOURCE_SCALE));
    float minimumCameraY = static_cast<float>(room.roomY());
    float maximumCameraY = std::max<float>(minimumCameraY,
        static_cast<float>(room.roomY() + room.height() - HOME_ROOM_HEIGHT / AmoledUi::RESOURCE_SCALE));
    // The room cache is sampled at integer logical pixels. Keep the camera
    // on that same grid so the room, sprites, bowl, and dirty-region decision
    // use one coordinate system instead of mixing float and rounded values.
    cameraX = std::clamp(std::round(cameraX), 0.0f, maximumCameraX);
    cameraY = std::clamp(std::round(cameraY), minimumCameraY, maximumCameraY);
}

int AmoledApp::worldToScreenX(float worldX) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround((worldX) * AmoledUi::RESOURCE_SCALE));
    }
    return static_cast<int>(std::lround((worldX - cameraX) * AmoledUi::RESOURCE_SCALE));
}

int AmoledApp::worldToScreenY(float worldY) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround((worldY) * AmoledUi::RESOURCE_SCALE));
    }
    return HOME_ROOM_TOP +
           static_cast<int>(std::lround((worldY - cameraY) * AmoledUi::RESOURCE_SCALE));
}

void AmoledApp::requestRenderRows(uint16_t begin, uint16_t end) {
    requestRenderRect(0, begin, AmoledUi::WIDTH, end);
}

void AmoledApp::requestRenderRect(int left, int top, int right, int bottom) {
    left = std::clamp(left, 0, AmoledUi::WIDTH);
    right = std::clamp(right, left, AmoledUi::WIDTH);
    top = std::clamp(top, 0, AmoledUi::HEIGHT);
    bottom = std::clamp(bottom, top, AmoledUi::HEIGHT);
    if (left == right || top == bottom) return;
    if (!dirtyRowsValid) {
        dirtyColumnBegin = left;
        dirtyColumnEnd = right;
        dirtyRowBegin = top;
        dirtyRowEnd = bottom;
        dirtyRowsValid = true;
        return;
    }
    dirtyColumnBegin = std::min<int>(dirtyColumnBegin, left);
    dirtyColumnEnd = std::max<int>(dirtyColumnEnd, right);
    dirtyRowBegin = std::min<int>(dirtyRowBegin, top);
    dirtyRowEnd = std::max<int>(dirtyRowEnd, bottom);
}

void AmoledApp::requestFullRender() {
    // Ordinary feedback belongs to the page that produced it.
    if (toast && toastScene != sceneFlow.current()) toast = nullptr;
    requestRenderRows(0, 448);
}

void AmoledApp::markRendered() {
    if (dirtyRowsValid && dirtyRowBegin == 0 && dirtyRowEnd == AmoledUi::HEIGHT &&
        dirtyColumnBegin == 0 && dirtyColumnEnd == AmoledUi::WIDTH) {
        expeditionFade.markPresented(Platform::clock().millis());
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE &&
        sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_MENU) explorePresenter_.invalidateDrawn();
    dirtyRowsValid = false;
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) battleSession_.markPresented();
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void AmoledApp::markEncounterFirstFramePresented(uint32_t drawUs,
                                                 uint32_t lcdUs) {
    if (!encounterFirstFramePending) return;
    encounterFirstFramePending = false;
    if (sceneFlow.current() != AppSceneFlow::Scene::BATTLE) return;
    Platform::logf(
        "[EncounterPerf] triggerToFirstBattleFrame=%lu ms prep=%lu ms "
        "sprite=%lu ms sfx=%lu ms draw=%lu us lcd=%lu us\n",
        static_cast<unsigned long>(
            Platform::clock().millis() - encounterStartedMs),
        static_cast<unsigned long>(encounterPreparedMs),
        static_cast<unsigned long>(encounterSpritePrepMs),
        static_cast<unsigned long>(encounterSfxPrepMs),
        static_cast<unsigned long>(drawUs),
        static_cast<unsigned long>(lcdUs));
}
#endif

float AmoledApp::gameSpeed() const {
    static constexpr float SPEEDS[] = {1.0f, 2.0f, 4.0f, 8.0f};
    uint8_t index = gameState.settings.speedIndex;
    return SPEEDS[index < 4 ? index : 0];
}

void AmoledApp::fillHomePetModel(HomeViewModel& model) const {
    const auto homeSnapshot = homeRuntime.renderSnapshot();
    model.speciesId = gameState.team[0].speciesId;
    model.petCenterX = static_cast<int16_t>(worldToScreenX(
        homeSnapshot.actors[0].x));
    model.petGroundY = static_cast<int16_t>(worldToScreenY(
        homeSnapshot.actors[0].y));
    model.petVisible = homeSnapshot.actors[0].active &&
        !homeSnapshot.actors[0].hidden && !expeditionMainHidden;
    model.petRenderOffsetY = static_cast<int8_t>(std::lround(
        pairFlow_.state().mainRenderOffsetY * AmoledUi::RESOURCE_SCALE));
    model.petFrame = petFrame;
    model.petDirection = petDirection;
    model.petLongMove = petLongMove;
    // The main pet stays in PetMotion::IDLE while a pair interaction
    // moves it (approach leader or chase); present that as walking so
    // the sprite does not slide in its idle pose.
    const bool mainPairMoving =
        (pairFlow_.state().phase == PairPhase::APPROACH &&
         (pairFlow_.state().parallelApproach
              ? !pairFlow_.state().mainArrived : pairFlow_.state().leaderMain)) ||
        (pairFlow_.state().phase == PairPhase::ACTIVE &&
         pairFlow_.state().activity == Home::PairActivity::CHASE);
    const bool mainWelcomingVisitor =
        visitorFlow_.state().motion == VisitorMotion::HOST_APPROACH;
    model.petAction = petMotion() == PetMotion::STOPPING
        ? HomeViewModel::PetVisualAction::STOPPING
        : (petMotion() == PetMotion::WANDERING ||
           petMotion() == PetMotion::SEEKING_FOOD ||
           petMotion() == PetMotion::SEEKING_SLEEP ||
           mainPairMoving || mainWelcomingVisitor ||
           visitAwayFlow_.state().phase == VisitDeparture::WALK_TO_DOOR ||
           visitAwayFlow_.state().phase == VisitDeparture::CROSS_DOOR)
            ? HomeViewModel::PetVisualAction::WALKING
            : HomeViewModel::PetVisualAction::IDLE;
    model.petResting = petIsResting();
}

void AmoledApp::render(Canvas565& canvas) const {
    const auto& route = exploreSession_.state();
    renderCaches_.retainForScene(sceneFlow.current());
    if (sceneFlow.current() == AppSceneFlow::Scene::HATCH) {
        const auto model = hatchController_.model(gameState.gameMinutesTotal, toast);
        renderHatchScreen(canvas, model, dirtyRowBegin, dirtyRowEnd,
                          &hatchController_.eggHitBounds());
        return;
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        const Home::RenderSnapshot homeSnapshot =
            homeRuntime.renderSnapshot();
        const Game::MonsterRuntime& monster = gameState.team[0];
        HomeViewModel model;
        model.speciesId = monster.speciesId;
        model.batteryPercent = batteryPercent;
        model.visitAway = visitAwayFlow_.state().phase == VisitDeparture::AWAY;
        model.recallConfirm = visitRecallConfirm && model.visitAway;
        uint8_t visibleSlots[Game::TEAM_CAP] = {};
        model.monsterCount = Game::HomeHud::visibleTeamSlots(
            gameState, visibleSlots, visitSession.active());
        for (uint8_t index = 0; index < model.monsterCount; ++index) {
            const Game::MonsterRuntime& hudMonster =
                gameState.team[visibleSlots[index]];
            model.monsters[index].hp =
                Game::HomeHud::hpPercent(hudMonster);
            model.monsters[index].fainted = hudMonster.fainted;
            model.monsters[index].faintRest =
                Game::HomeHud::faintRestPercent(
                    hudMonster, gameState.gameMinutesTotal);
            model.monsters[index].hunger =
                Game::HomeHud::hungerPercent(hudMonster);
            model.monsters[index].hpKnown =
                hudMonster.origin != Game::Origin::VISITOR ||
                !visitSession.active() || visitSession.visitorHealthKnown();
        }
        model.gameMinutesOfDay = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        model.cameraX = static_cast<int16_t>(std::lround(cameraX));
        model.cameraY = static_cast<int16_t>(std::lround(cameraY));
        RoomResource& room = RoomResource::ins();
        if (room.available()) {
            model.bowlCenterX = static_cast<int16_t>(
                worldToScreenX(room.foodX()));
            model.bowlCenterY = static_cast<int16_t>(
                worldToScreenY(room.foodY()));
        }
        fillHomePetModel(model);
        if (gameState.teamCount > 1 && gameState.team[1].speciesId != 0 &&
            homeSnapshot.actors[1].active &&
            !homeSnapshot.actors[1].hidden &&
            !expeditionCompanionHidden
#if STICKMON_ENABLE_DEBUG_FEATURES
            && !(debugPairChaseActive && gameState.teamCount > 1)
#endif
        ) {
            model.companionVisible = true;
            model.companionSpeciesId = gameState.team[1].speciesId;
            model.companionCenterX = static_cast<int16_t>(
                worldToScreenX(homeSnapshot.actors[1].x));
            model.companionGroundY = static_cast<int16_t>(
                worldToScreenY(homeSnapshot.actors[1].y));
            model.companionRenderOffsetY = static_cast<int8_t>(std::lround(
                pairFlow_.state().companionRenderOffsetY * AmoledUi::RESOURCE_SCALE));
            model.companionFrame = companionFrame;
            model.companionDirection = companionDirection;
            model.companionAction =
                (homeSnapshot.actors[1].task == Home::Task::WANDER ||
                 homeSnapshot.actors[1].task == Home::Task::SEEK_FOOD ||
                 homeSnapshot.actors[1].task == Home::Task::SEEK_SLEEP ||
                 homeSnapshot.actors[1].task == Home::Task::YIELDING ||
                 homeSnapshot.actors[1].task == Home::Task::DOOR_ACTION ||
                 (homeSnapshot.actors[1].task == Home::Task::PAIR_ACTION &&
                  !homeCompanionActor.route.empty()))
                    ? HomeViewModel::PetVisualAction::WALKING
                    : HomeViewModel::PetVisualAction::IDLE;
            model.companionLongMove = companionLongMove;
            const Game::MonsterRuntime& companion = gameState.team[1];
            model.companionResting = companion.fainted ||
                companion.hpCur == 0 ||
                companion.majorStatus == Game::MajorStatus::SLEEP ||
                homeCompanionActor.task == Home::Task::SLEEPING ||
                homeCompanionActor.task == Home::Task::FAINTED;
        }
        model.night = model.gameMinutesOfDay < 6U * 60U ||
                      model.gameMinutesOfDay >= 18U * 60U;
        model.moodHearts = moodHeartCount;
        if (moodBurstUntilMs != 0) {
            uint32_t nowMs = Platform::clock().millis();
            model.moodBurstHeart = moodBurstHeart;
            model.moodBurstAgeMs = static_cast<uint16_t>(std::min<uint32_t>(
                0xFFFFU, nowMs - moodBurstStartedMs));
        }
        model.showHearts = heartsUntil != 0;
        model.bowlFilled = gameState.room.bowlCount > 0;
        model.fadeAlpha = externalSceneFade ? 0 : expeditionFade.alpha();
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugContactPrompt = debugContactPending ||
            debugContactEventPending;
        model.debugContactPromptFade = debugContactPromptFade;
        model.debugContactPromptText =
            debugContactPromptFade > 0 ? debugPromptBuffer : nullptr;
        model.debugContactChoiceVisible = debugContactChoiceVisible;
        model.debugContactSelectedChoice = debugContactSelectedChoice;
        // Contact visitors now use the same independent secondary actor.
        model.debugContactActive = false;
        model.debugContactKind = debugContactKind;
        model.debugContactSpeciesId = debugContactStorageSlot < gameState.storageCount
            ? gameState.storage[debugContactStorageSlot].speciesId : 0;
        model.debugPairChaseActive = debugPairChaseActive &&
            gameState.teamCount > 1;
        model.debugPairSpeciesId = gameState.teamCount > 1
            ? gameState.team[1].speciesId : 0;
        model.debugPairCenterX = static_cast<int16_t>(
            worldToScreenX(debugPairX));
        model.debugPairGroundY = static_cast<int16_t>(
            worldToScreenY(debugPairY));
        model.debugPairFrame = debugPairFrame;
        model.debugPairDirection = debugPairDirection;
        model.debugLightSource = debugLightSource;
        model.debugBoundaryVisible = debugWalkBoundaryVisible;
        model.debugTalkPointsVisible = debugTalkPointsVisible;
        float debugTalkCompanionX = PairFlow::TALK_COMPANION_X;
        float debugTalkCompanionY = PairFlow::TALK_STAGE_Y;
        float debugTalkMainX = PairFlow::TALK_MAIN_X;
        float debugTalkMainY = PairFlow::TALK_STAGE_Y;
        if (pairFlow_.state().activity == Home::PairActivity::TALK &&
            pairFlow_.state().phase != PairPhase::NONE) {
            debugTalkMainX = homeMainActor.targetX;
            debugTalkMainY = homeMainActor.targetY;
            debugTalkCompanionX = homeCompanionActor.targetX;
            debugTalkCompanionY = homeCompanionActor.targetY;
        }
        model.debugTalkCompanionX = static_cast<int16_t>(
            std::lround(debugTalkCompanionX));
        model.debugTalkCompanionY = static_cast<int16_t>(
            std::lround(debugTalkCompanionY));
        model.debugTalkMainX = static_cast<int16_t>(
            std::lround(debugTalkMainX));
        model.debugTalkMainY = static_cast<int16_t>(
            std::lround(debugTalkMainY));
        RoomResource& debugRoom = RoomResource::ins();
        float welcomeX = debugRoom.available()
            ? static_cast<float>(debugRoom.doorwayInsideX()) +
                  VISITOR_WELCOME_OFFSET_X
            : FALLBACK_ROOM_MAX_X - 8.0f + VISITOR_WELCOME_OFFSET_X;
        float welcomeY = debugRoom.available()
            ? static_cast<float>(debugRoom.doorwayInsideY()) +
                  VISITOR_WELCOME_OFFSET_Y
            : FALLBACK_ROOM_MAX_Y + VISITOR_WELCOME_OFFSET_Y;
        if (visitorFlow_.state().welcomeValid) {
            welcomeX = visitorFlow_.state().welcomeX;
            welcomeY = visitorFlow_.state().welcomeY;
        }
        model.debugWelcomeCenterX = static_cast<int16_t>(
            worldToScreenX(welcomeX));
        model.debugWelcomeGroundY = static_cast<int16_t>(
            worldToScreenY(welcomeY));
#endif
        model.toast = toast;
        renderHomeScreenRegion(canvas, model, dirtyRowBegin, dirtyRowEnd,
                               dirtyColumnBegin, dirtyColumnEnd);
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
        DebugViewModel model;
        model.category = debugCategory;
        model.cursor = debugCursor;
        model.scroll = debugScroll;
        model.pressedItem = debugPressedItem;
        model.popup = debugPopup;
        model.bedStyle = debugBedStyle;
        model.rugStyle = debugRugStyle;
        model.focus = debugFocus;
        for (uint8_t index = 0; index < 4; ++index) {
            model.digits[index] = debugDigits[index];
        }
        model.state = &gameState;
        model.toast = toast;
        char debugTime[24] = {};
        uint16_t debugMinutes = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        std::snprintf(debugTime, sizeof(debugTime), Ui::Debug::CURRENT_TIME_FMT,
                      debugMinutes / 60, debugMinutes % 60);
        model.currentTime = debugTime;
        model.lightSource = Ui::Debug::LIGHT_SOURCE_ITEMS[debugLightSource];
        model.tiltEnabled = debugTiltControl;
        model.boundaryVisible = debugWalkBoundaryVisible;
        model.talkPointsVisible = debugTalkPointsVisible;
        model.battleBoundsVisible = debugBattleDrawBoundsVisible;
            model.touchDisplayEnabled = debugTouchDisplayEnabled;
        model.touchTest = &debugTouchTest;
        renderDebugScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }
#endif

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        ExploreViewModel model;
        model.visibleAreaCount =
            ExploreItemProgression::visibleAreaCount(gameState);
        model.unlockedArea = ExploreItemProgression::unlockedArea(gameState);
        model.selectedArea = selectedExploreArea;
        model.currentLevel = gameState.team[0].level;
        model.areaAnimCursor = exploreAreaAnimCursor;
        model.previewStartedAt = explorePreviewStartedAt;
        model.previewPool = explorePreviewPool;
        for (uint8_t index = 0; index < ExplorePool::POOL_CAP; ++index) {
            model.previewFrames[index] = explorePreviewFrames[index];
            model.previewHidden[index] = explorePreviewHidden[index];
        }
        model.pressedArea = pressedExploreArea;
        model.toast = toast;
        renderExploreScreen(canvas, model, renderCaches_.exploreBackground,
                            dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        ExploreRouteViewModel model;
        model = exploreRouteModel();
        model.renderColumnBegin = dirtyColumnBegin;
        model.renderColumnEnd = dirtyColumnEnd;
        if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE &&
            route.exploreRouteSteps == 0 && dirtyRowBegin == 0 &&
            dirtyRowEnd == AmoledUi::HEIGHT) {
            // The departure fade is black here. Build the static battle layer
            // before the route redraws the same full framebuffer.
            prepareBattleBackground(canvas, renderCaches_.battleBackground,
                                    battleBackgroundForArea(selectedExploreArea));
        }
        renderExploreRouteScreen(
            canvas, model, renderCaches_.exploreWorld, dirtyRowBegin, dirtyRowEnd);
        explorePresenter_.recordDrawn(model);
        if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
            ExploreMenuViewModel menuModel;
            menuModel.cursor = exploreMenuCursor;
            menuModel.pressedItem = pressedExploreMenuItem;
            menuModel.toast = toast;
            renderExploreMenuScreen(
                canvas, menuModel, dirtyRowBegin, dirtyRowEnd);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        BattleViewModel model = battleSession_.model(gameState, Platform::clock().millis());
        model.battleBackground = battleBackgroundForArea(selectedExploreArea);
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugDrawBounds = debugBattleDrawBoundsVisible;
#endif
        renderBattleScreen(canvas, model, renderCaches_.battleBackground,
                           dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        renderCommunicationScreen(canvas, visitSession.viewModel(),
                                  dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        if (teamController_.state().teamStatusOpen) {
            auto model = teamController_.statusModel(gameState);
            renderTeamStatusScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        if (teamController_.state().teamMovesOpen) {
            auto model = teamController_.movesModel(gameState, toast);
            renderTeamMovesScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        auto model = teamController_.teamModel(gameState, toast);
        if (inventoryController_.state().teamItemFeedbackActive) {
            model.hpOverrideSlot = inventoryController_.state().teamItemTargetSlot;
            model.hpOverride = teamItemHpForRender(Platform::clock().millis());
        }
        renderTeamScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
        RoomMenuViewModel model;
        model.state = &gameState;
        model.pressedItem = pressedRoomItem;
        model.toast = toast;
        renderRoomMenuScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
        RoomFoodViewModel model;
        model.state = &gameState;
        model.selectedFood = gameState.room.selectedFood;
        model.pressedItem = pressedRoomItem;
        model.toast = toast;
        renderRoomFoodScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
        if (computerPage == ComputerViewModel::Page::STATUS) {
            auto model = teamController_.statusModel(
                gameState, teamController_.state().teamStatusFromContact ? contactMonster(teamController_.state().teamStatusContactSlot) : nullptr);
            renderTeamStatusScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        ComputerViewModel model;
        model.state = &gameState;
        model.page = computerPage;
        model.storageScroll = computerScroll;
        model.pressedItem = computerPressedItem == 0xFF
            ? -1 : computerPressedItem;
        model.contactActionOpen = contactActionOpen;
        model.contactActionSlot = contactActionSlot;
        for (uint8_t slot = 0;
             slot < gameState.storageCount && slot < Game::STORAGE_CAP;
             ++slot) {
            if (contactIsVisiting(slot)) {
                model.contactVisitingSlot = slot;
                break;
            }
        }
        model.contactActionPressedItem = contactActionPressedItem;
        model.contactConfirmOpen = contactConfirmOpen;
        model.contactConfirmYes = contactConfirmYes;
        model.contactConfirmDelete = contactConfirmDelete;
        model.contactInTeam = contactIsInTeam(contactActionSlot);
        model.contactVisiting = contactIsVisiting(contactActionSlot);
        model.contactInviteLocked = contactInviteLocked(contactActionSlot);
        model.contactCanDelete = contactCanDelete(contactActionSlot);
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime& clawRuntime =
            Stickmon::ClawRuntime::instance();
        model.clawEnabled = clawRuntime.enabled();
        model.wifiEnabled = clawRuntime.wifiEnabled();
        char clawSsid[33] = {};
        char clawPassword[65] = {};
        char clawIp[16] = {};
        if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
            clawRuntime.setupPortalInfo(
                clawSsid, sizeof(clawSsid), clawPassword,
                sizeof(clawPassword), clawIp, sizeof(clawIp));
            model.clawSsid = clawSsid;
            model.clawPassword = clawPassword;
            model.clawIp = clawIp;
            model.clawLogView = clawLogView;
            Stickmon::ClawStatus status;
            clawRuntime.statusSnapshot(status);
            model.clawStaConnected = status.staConnected;
            std::memcpy(model.clawStaIp, status.staIp,
                        sizeof(model.clawStaIp));
            model.clawPhoneJoined = status.phoneJoined;
            model.clawStarted = status.clawStarted;
            std::memcpy(model.clawWechatPhase, status.wechatPhase,
                        sizeof(model.clawWechatPhase));
            model.clawWechatPersisted = status.wechatPersisted;
            // Entries were copied by refreshClawLogSnapshot() (update path);
            // rendering stays read-only.
            model.clawLog = s_clawLogEntries;
            model.clawLogCount = clawLogCount;
            model.clawLogScroll = clawLogScroll;
            model.clawLogPinned = clawLogPinned;
        }
#endif
        model.toast = toast;
        renderComputerScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
        const auto model = settingsController.model(
            gameState, systemUpdateHandler != nullptr, toast);
        renderSettingsScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
        const auto model = progressionFlow_.model(gameState, toast, Platform::clock().millis());
        renderProgressionScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
        const auto model = showerController_.model(gameState, toast,
            Platform::clock().millis(), dirtyColumnBegin, dirtyColumnEnd);
        renderShowerScreenCached(canvas, model, renderCaches_.showerBackground,
                                dirtyRowBegin, dirtyRowEnd, &renderCaches_.showerPet);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        auto model = inventoryController_.shopModel(gameState, toast);
        renderShopScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        auto model = inventoryController_.bagModel(gameState, toast);
        renderItemListScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    MenuViewModel model;
    model.scroll = menuScroll;
    model.pressedItem = pressedMenuItem;
    model.toast = toast;
    renderMainMenu(canvas, model, dirtyRowBegin, dirtyRowEnd);
}

bool AmoledApp::consumeLockRequest() {
    bool requested = lockRequested;
    lockRequested = false;
    return requested;
}

bool AmoledApp::lockedHomePowerAllowed() const {
    return sceneFlow.current() == AppSceneFlow::Scene::HOME &&
           gameState.teamCount > 0 && !gameResetPending &&
           visitAwayFlow_.state().phase == VisitDeparture::NONE &&
           expeditionDeparturePhase == ExpeditionDeparturePhase::NONE &&
           !expeditionFade.active();
}

bool AmoledApp::displayLockAllowed() const {
    return gameState.oobeDone && gameState.teamCount > 0 &&
           !gameResetPending && !pendingExpedition &&
           sceneFlow.current() != AppSceneFlow::Scene::BATTLE &&
           sceneFlow.current() != AppSceneFlow::Scene::SHOWER &&
           sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION &&
           visitAwayFlow_.state().phase == VisitDeparture::NONE &&
           expeditionDeparturePhase == ExpeditionDeparturePhase::NONE &&
           !expeditionFade.active();
}

bool AmoledApp::prepareDisplayLock(uint32_t nowMs) {
    if (!displayLockAllowed()) return false;
    if (exploreSessionActive) {
        // Settle the expedition through its normal return, then retry locking.
        beginExploreReturn(nowMs);
        return false;
    }
    sceneFlow.goHome();
    pointerDown = false;
    inventoryController_.suspendDetail();
    teamController_.dismissConfirmation();
    contactConfirmOpen = false;
    settingsController.reset();
    visitRecallConfirm = false;
    toast = nullptr;
    setMusicContext(MusicContext::HOME);
    syncHomeActors(nowMs);
    updateCamera();
    // Both manual and idle locks arrive here, after any expedition has settled.
    saveState();
    requestFullRender();
    int16_t focusX = 0;
    int16_t focusY = 0;
    return lockFocusPoint(focusX, focusY);
}

void AmoledApp::onWake(uint32_t nowMs) {
    batterySampled = false;
    updateBattery(nowMs);
    lockRequested = false;
    pointerDown = false;
    lastInteractionMs = nowMs;
    requestFullRender();
}

bool AmoledApp::lockFocusPoint(int16_t& x, int16_t& y) const {
    x = AmoledUi::WIDTH / 2;
    y = AmoledUi::HEIGHT / 2;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        gameState.teamCount > 0 && homeMainActor.active &&
        !homeMainActor.hidden && !expeditionMainHidden) {
        HomeViewModel model;
        fillHomePetModel(model);
        auto bounds = homePetVisibleBounds(model);
        if (petIsResting()) {
            PokemonSprites::PetAnimationProfile profile{};
            PokemonSprites::petAnimationProfile(gameState.team[0].speciesId, profile);
            for (uint8_t i = 0; i < profile.sleepingFrames; ++i) {
                model.petFrame = i;
                const auto pose = homePetVisibleBounds(model);
                const int right = std::max(bounds.x + bounds.width, pose.x + pose.width);
                const int bottom = std::max(bounds.y + bounds.height, pose.y + pose.height);
                bounds.x = std::min(bounds.x, pose.x);
                bounds.y = std::min(bounds.y, pose.y);
                bounds.width = right - bounds.x;
                bounds.height = bottom - bounds.y;
            }
        }
        if (bounds.width <= 0 || bounds.height <= 0) return false;
        x = static_cast<int16_t>(bounds.x + bounds.width / 2);
        y = static_cast<int16_t>(bounds.y + bounds.height / 2);
        return true;
    }
    return false;
}

bool AmoledApp::lockTapHitsPet(int16_t x, int16_t y) const {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        gameState.teamCount == 0) return false;
    HomeViewModel model;
    fillHomePetModel(model);
    const auto bounds = homePetVisibleBounds(model);
    constexpr int TAP_PADDING = 12;
    return bounds.width > 0 && bounds.height > 0 &&
           x >= bounds.x - TAP_PADDING &&
           x < bounds.x + bounds.width + TAP_PADDING &&
           y >= bounds.y - TAP_PADDING &&
           y < bounds.y + bounds.height + TAP_PADDING;
}

AmoledApp::PetMotion AmoledApp::petMotion() const {
    if (petStopping) return PetMotion::STOPPING;
    switch (homeMainActor.task) {
    case Home::Task::TURNING: return PetMotion::TURNING;
    case Home::Task::SLEEPING: return PetMotion::SLEEPING;
    case Home::Task::FEEDING: return PetMotion::EATING;
    case Home::Task::SEEK_FOOD: return PetMotion::SEEKING_FOOD;
    case Home::Task::SEEK_SLEEP: return PetMotion::SEEKING_SLEEP;
    case Home::Task::WANDER:
    case Home::Task::YIELDING:
    case Home::Task::LEAVING_SLEEP:
    case Home::Task::ROOM_ACTION:
    case Home::Task::DOOR_ACTION:
        return homeMainActor.motion == Home::MotionPhase::ROUTE
            ? PetMotion::WANDERING : PetMotion::IDLE;
    default: return PetMotion::IDLE;
    }
}

bool AmoledApp::petIsResting() const {
    return homeMainActor.faintRestActive ||
           homeMainActor.task == Home::Task::SLEEPING;
}

bool AmoledApp::petIsSleeping() const {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        gameState.teamCount == 0) {
        return false;
    }

    const Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0) return petIsResting();

    return homeMainActor.task == Home::Task::SLEEPING;
}

void AmoledApp::applySettingsChange(SettingsController::Change change) {
    if (change == SettingsController::Change::BRIGHTNESS)
        Platform::display().setBrightness(gameState.settings.brightness);
    else if (change == SettingsController::Change::VOLUME)
        Platform::audio().setVolume(gameState.settings.volume);
}

void AmoledApp::setToast(const char* value, uint32_t nowMs,
                         uint32_t durationMs) {
    // Rendering happens after callers' temporary formatting buffers expire.
    if (value) {
        const size_t length = std::min(std::strlen(value), sizeof(toastText) - 1);
        std::memmove(toastText, value, length);
        toastText[length] = '\0';
    }
    toast = value ? toastText : nullptr;
    toastScene = sceneFlow.current();
    if (toastScene == AppSceneFlow::Scene::HOME) {
        const bool needsAttention = value && (
            std::strcmp(value, Ui::Menu::PET_LIMIT) == 0 ||
            std::strcmp(value, Ui::Menu::PET_REST) == 0 ||
            std::strcmp(value, Ui::Menu::NO_FOOD) == 0 ||
            std::strcmp(value, Ui::Menu::FOOD_FULL) == 0 ||
            std::strcmp(value, Ui::Menu::FOOD_MIXED) == 0 ||
            std::strcmp(value, Ui::Amoled::REST_AT_HOME) == 0);
        durationMs = std::max<uint32_t>(durationMs, needsAttention ? 2000 : 1500);
    }
    toastUntil = nowMs + durationMs;
    requestRenderRows(
        sceneFlow.current() == AppSceneFlow::Scene::HOME
            ? HOME_STATUS_TOP
            : sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                ? MAIN_MENU_CONTENT_TOP : MENU_HEADER_HEIGHT,
        AmoledUi::HEIGHT);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void AmoledApp::clampDebugScroll() {
    float maximum = debugMaxScroll(debugCategory);
    if (debugScroll <= 0.0f) {
        debugScroll = 0.0f;
        if (debugVelocity < 0.0f) debugVelocity = 0.0f;
    } else if (debugScroll >= maximum) {
        debugScroll = maximum;
        if (debugVelocity > 0.0f) debugVelocity = 0.0f;
    }
}

void AmoledApp::acceptDebugContact(uint32_t nowMs, bool autoResolve) {
    if (!debugContactPending ||
        debugContactStorageSlot >= gameState.storageCount ||
        gameState.teamCount != 1) {
        debugContactPending = false;
        debugContactEventPending = false;
        debugContactEventWaitingForEntry = false;
        debugContactChoiceVisible = false;
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
        debugContactStorageSlot = 0xFF;
        debugContactKind = 0;
        return;
    }

    const uint8_t storageSlot = debugContactStorageSlot;
    Game::MonsterRuntime guest = gameState.storage[storageSlot];
    guest.origin = Game::Origin::VISITOR;
    guest.petCountToday = 0;
    gameState.team[1] = guest;
    gameState.teamCount = 2;
    gameState.activeSlot = 0;
    debugContactPending = false;
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = !autoResolve;
    debugContactChoiceVisible = debugContactSelectedChoice >= 0;
    debugContactActive = true;
    debugContactStartedMs = nowMs;

    if (debugContactKind == 1) {
        Game::MonsterRuntime& original = gameState.storage[storageSlot];
        original.bond = Game::Bond::increase(original.bond, 2);
        gameState.team[1].bond = original.bond;
    } else if (debugContactKind == 2) {
        uint8_t& food = gameState.room.food[Game::ROOM_NORMAL_FOOD_INDEX];
        if (food < Game::ITEM_STACK_CAP) ++food;
    }

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    saveState();

    const Species* species = findSpecies(gameState.team[1].speciesId);
    const char* name = species && species->name ? species->name : "";
    const char* format = debugContactKind == 1
        ? Ui::ContactVisit::PLAY_FMT
        : debugContactKind == 2
            ? Ui::ContactVisit::GIFT_FMT : Ui::ContactVisit::EXPLORE_FMT;
    std::snprintf(debugToastBuffer, sizeof(debugToastBuffer), format, name);
    Platform::logf("[FriendDiag] accepted kind=%u slot=%u species=%u team=%u\n",
                   static_cast<unsigned>(debugContactKind),
                   static_cast<unsigned>(storageSlot),
                   static_cast<unsigned>(guest.speciesId),
                   static_cast<unsigned>(gameState.teamCount));
    if (autoResolve && debugContactKind == 3) {
        uint8_t unlocked = ExploreItemProgression::unlockedArea(gameState);
        selectedExploreArea = std::min<uint8_t>(selectedExploreArea, unlocked);
        if (!queueExploreDeparture(selectedExploreArea, false)) {
            Platform::logLine("[FriendDiag] departure rejected kind=3");
            completeDebugContact(nowMs);
            setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            return;
        }
        Platform::logf("[FriendDiag] departure queued kind=3 area=%u\n",
                       static_cast<unsigned>(selectedExploreArea));
    }
    if (autoResolve) showDebugContactStripMessage(debugToastBuffer, nowMs, 2000);
    requestFullRender();
}

void AmoledApp::showDebugContactEventPrompt(uint32_t nowMs) {
    if (!debugContactActive || !debugContactEventWaitingForEntry) return;
    debugContactEventWaitingForEntry = false;
    debugContactStartedMs = nowMs;
    if (debugContactKind == 3) {
        debugContactEventPending = true;
        debugContactChoiceVisible = true;
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
        debugContactMessageUntilMs = 0;
        const Species* species = findSpecies(gameState.team[1].speciesId);
        const char* name = species && species->name ? species->name : "";
        // The strip prompt wraps after the visitor name (EXPLORE_FMT = "%s" +
        // EXPLORE_SUFFIX).
        std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s\n%s",
                      name, Ui::ContactVisit::EXPLORE_SUFFIX);
    } else {
        debugContactEventPending = false;
        debugContactChoiceVisible = false;
        showDebugContactStripMessage(debugToastBuffer, nowMs, 2000);
    }
    requestFullRender();
}

void AmoledApp::showDebugContactStripMessage(const char* text,
                                             uint32_t nowMs,
                                             uint32_t durationMs) {
    if (!text) return;
    debugContactChoiceVisible = false;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
    std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s", text);
    debugContactMessageUntilMs = nowMs + durationMs;
    requestFullRender();
}

void AmoledApp::completeDebugContact(uint32_t nowMs) {
    if (!debugContactActive) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        homeCompanionActor.active) {
        if (visitorFlow_.state().motion != VisitorMotion::EXITING) {
            beginVisitorExit(nowMs, true);
        }
        return;
    }
    finalizeDebugContact(nowMs);
}

void AmoledApp::finalizeDebugContact(uint32_t nowMs) {
    if (!debugContactActive) return;

    uint8_t visitorSlot = 0xFF;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (gameState.team[slot].origin == Game::Origin::VISITOR) {
            visitorSlot = slot;
            break;
        }
    }
    if (debugContactStorageSlot < gameState.storageCount &&
        visitorSlot < gameState.teamCount) {
        Game::Origin originalOrigin =
            gameState.storage[debugContactStorageSlot].origin;
        uint8_t inviteMarker =
            gameState.storage[debugContactStorageSlot].petCountToday;
        gameState.storage[debugContactStorageSlot] = gameState.team[visitorSlot];
        gameState.storage[debugContactStorageSlot].origin = originalOrigin;
        gameState.storage[debugContactStorageSlot].petCountToday = inviteMarker;
        gameState.storage[debugContactStorageSlot].lastSeenAt =
            static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    }
    if (visitorSlot < gameState.teamCount) {
        for (uint8_t slot = visitorSlot; slot + 1 < gameState.teamCount &&
             slot + 1 < Game::TEAM_CAP; ++slot) {
            gameState.team[slot] = gameState.team[slot + 1];
        }
        --gameState.teamCount;
        gameState.team[gameState.teamCount] = Game::MonsterRuntime{};
        gameState.activeSlot = 0;
    }

    uint8_t kind = debugContactKind;
    debugContactActive = false;
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = false;
    debugContactChoiceVisible = false;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
    debugContactStorageSlot = 0xFF;
    debugContactKind = 0;
    debugContactStartedMs = 0;
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    saveState();
    Platform::logf("[FriendDiag] complete kind=%u team=%u slot=%u scene=%u phase=%u\n",
                   static_cast<unsigned>(kind),
                   static_cast<unsigned>(gameState.teamCount),
                   static_cast<unsigned>(visitorSlot),
                   static_cast<unsigned>(sceneFlow.current()),
                   static_cast<unsigned>(expeditionDeparturePhase));
    showDebugContactStripMessage(kind == 3 ? Ui::ContactVisit::HAPPY_RETURN
                                           : Ui::ContactVisit::HAPPY_VISIT,
                                 nowMs, 2000);
    requestFullRender();
}

void AmoledApp::startDebugPairChase(uint32_t nowMs) {
    if (gameState.teamCount < 2 || gameState.team[0].fainted ||
        gameState.team[1].fainted || gameState.team[0].hpCur == 0 ||
        gameState.team[1].hpCur == 0) {
        setToast(Ui::Debug::PAIR_NEEDS_TWO, nowMs, 1400);
        return;
    }
    sceneFlow.goHome();
    syncHomeActors(nowMs);
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    if (!startPairInteraction(nowMs, true)) {
        setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
        return;
    }
    debugPairChaseActive = false;
    setToast(Ui::Debug::PAIR_INTERACTION, nowMs, 750);
    requestFullRender();
}

void AmoledApp::updateDebugPairChase(uint32_t nowMs,
                                     float elapsedSeconds) {
    (void)nowMs;
    (void)elapsedSeconds;
}

void AmoledApp::stopDebugPairChase(uint32_t nowMs, bool reward) {
    debugPairChaseActive = false;
    debugPairChaseUntilMs = 0;
    finishPairInteraction(nowMs, reward);
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::openDebugSwitchPopup() {
    uint16_t speciesId = gameState.teamCount > 0
        ? gameState.team[0].speciesId : 1;
    speciesId = std::min<uint16_t>(speciesId, 999);
    debugDigits[0] = static_cast<uint8_t>((speciesId / 100) % 10);
    debugDigits[1] = static_cast<uint8_t>((speciesId / 10) % 10);
    debugDigits[2] = static_cast<uint8_t>(speciesId % 10);
    debugFocus = 0;
    debugPopup = DebugViewModel::Popup::SWITCH_MONSTER;
    toast = nullptr;
    requestRenderRows(0, 448);
}

void AmoledApp::openDebugTimePopup() {
    uint16_t minutes = static_cast<uint16_t>(
        gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
    uint8_t hour = static_cast<uint8_t>(minutes / 60);
    uint8_t minute = static_cast<uint8_t>(minutes % 60);
    debugDigits[0] = hour / 10;
    debugDigits[1] = hour % 10;
    debugDigits[2] = minute / 10;
    debugDigits[3] = minute % 10;
    debugFocus = 0;
    debugPopup = DebugViewModel::Popup::SET_TIME;
    toast = nullptr;
    requestRenderRows(0, 448);
}

void AmoledApp::persistDebugMotionFlags() {
    uint8_t flags = 0;
    if (debugTiltControl) flags |= Game::DEBUG_MOTION_TILT;
    if (debugWalkBoundaryVisible) flags |= Game::DEBUG_MOTION_WALK_BOUNDARY;
    if (debugTalkPointsVisible) flags |= Game::DEBUG_MOTION_TALK_POINTS;
    if (gameState.debugMotionFlags == flags) return;
    gameState.debugMotionFlags = flags;
    saveState();
}

void AmoledApp::handleDebugPopupTap(int x, int y, uint32_t nowMs) {
    if (debugPopup == DebugViewModel::Popup::BED_STYLE ||
        debugPopup == DebugViewModel::Popup::RUG_STYLE) {
        const uint8_t style = static_cast<uint8_t>(debugFurnitureStyleAt(x, y));
        if (style != 0) {
            const bool bed = debugPopup == DebugViewModel::Popup::BED_STYLE;
            const uint8_t nextBed = bed ? style : debugBedStyle;
            const uint8_t nextRug = bed ? debugRugStyle : style;
            static constexpr const char* STYLES[] = {nullptr, "forest_leaf", "starry_night", "cloud"};
            char assetId[32];
            std::snprintf(assetId, sizeof(assetId), "%s_%s", bed ? "bed" : "rug", STYLES[style]);
            if (RoomResource::ins().setFurniture(bed ? "bed" : "rug", assetId)) {
                debugBedStyle = nextBed;
                debugRugStyle = nextRug;
                debugPopup = DebugViewModel::Popup::NONE;
                Platform::logf("[RoomPreview] bed=%u rug=%u\n", nextBed, nextRug);
            } else {
                setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            }
        } else if (debugFurnitureCancelAt(x, y)) {
            debugPopup = DebugViewModel::Popup::NONE;
        }
        requestFullRender();
        return;
    }
    uint8_t digitCount = debugPopup == DebugViewModel::Popup::SET_TIME ? 4 : 3;
    int digit = debugPopupDigitAt(x, y, digitCount);
    if (digit >= 0) {
        debugFocus = static_cast<uint8_t>(digit);
        debugDigits[debugFocus] = static_cast<uint8_t>(
            (debugDigits[debugFocus] + 1) % 10);
        requestRenderRows(0, 448);
        return;
    }
    int choice = debugPopupChoiceAt(x, y);
    if (choice < 0) return;
    if (choice == 1) {
        debugPopup = DebugViewModel::Popup::NONE;
        requestRenderRows(0, 448);
        return;
    }

    if (debugPopup == DebugViewModel::Popup::SWITCH_MONSTER) {
        uint16_t speciesId = static_cast<uint16_t>(
            debugDigits[0] * 100 + debugDigits[1] * 10 + debugDigits[2]);
        if (const Species* species = findSpecies(speciesId)) {
            uint8_t level = gameState.teamCount > 0 ? gameState.team[0].level : 5;
            Game::MonsterRuntime monster = Game::MonsterFactory::create(
                species->id, level);
            if (gameState.teamCount == 0) gameState.teamCount = 1;
            gameState.team[0] = monster;
            gameState.activeSlot = 0;
            debugPopup = DebugViewModel::Popup::NONE;
            petFrame = 0;
            homeMainActor.nextDecisionMs = nowMs;
            PokemonSprites::syncTeamCache(&speciesId, 1);
            saveState();
            setToast(Ui::Debug::SWITCHED, nowMs);
        } else {
            setToast(Ui::Debug::INVALID_ID, nowMs);
        }
    } else {
        uint8_t hour = static_cast<uint8_t>(debugDigits[0] * 10 + debugDigits[1]);
        uint8_t minute = static_cast<uint8_t>(debugDigits[2] * 10 + debugDigits[3]);
        if (hour > 23) hour = 23;
        if (minute > 59) minute = 59;
        uint16_t current = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        uint16_t target = static_cast<uint16_t>(hour * 60 + minute);
        uint32_t delta = target >= current
            ? target - current
            : Game::GAME_MINUTES_PER_DAY - current + target;
        gameState.gameMinutesTotal += delta;
        gameClock.set(nowMs, gameState.gameMinutesTotal);
        saveState();
        debugPopup = DebugViewModel::Popup::NONE;
        std::snprintf(debugToastBuffer, sizeof(debugToastBuffer), "%s +%lum",
                      Ui::Debug::TIME_SET,
                      static_cast<unsigned long>(delta));
        setToast(debugToastBuffer, nowMs);
    }
    requestRenderRows(0, 448);
}

void AmoledApp::executeDebugAction(uint32_t nowMs) {
    if (debugCategory == DebugViewModel::Category::ROOT) {
        switch (debugCursor) {
        case 0: debugCategory = DebugViewModel::Category::MONSTER; break;
        case 1: debugCategory = DebugViewModel::Category::RESOURCE; break;
        case 2: debugCategory = DebugViewModel::Category::ENV; break;
        case 3: debugCategory = DebugViewModel::Category::MOTION; break;
        case 4: debugCategory = DebugViewModel::Category::BATTLE; break;
        case 5: debugCategory = DebugViewModel::Category::CONTACT_EVENT; break;
        case 6:
            debugTouchDisplayEnabled = !debugTouchDisplayEnabled;
            if (!debugTouchDisplayEnabled) debugTouchPointValid = false;
            requestFullRender();
            return;
        case 7:
            debugCategory = DebugViewModel::Category::TOUCH_TEST;
            debugTouchTest = {};
            debugVelocity = 0;
            pointerDown = false;
            Platform::logLine("[TouchTest] begin native=368x448 diagnostic_only=1");
            requestFullRender();
            return;
        default:
            // The Debug page is entered as a standalone scene, so there is
            // no menu stack entry for closeMenu() to pop here.
            sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
            requestFullRender();
            return;
        }
        debugCursor = 0;
        debugScroll = 0.0f;
        requestFullRender();
        return;
    }

    const uint8_t backIndex = debugCategory == DebugViewModel::Category::TOUCH_TEST ? 0
        : debugCategory == DebugViewModel::Category::MONSTER ? 3
        : debugCategory == DebugViewModel::Category::RESOURCE ? 1
        : debugCategory == DebugViewModel::Category::ENV ? 3
        : debugCategory == DebugViewModel::Category::ROOM ? 2
        : debugCategory == DebugViewModel::Category::MOTION ? 4
        : debugCategory == DebugViewModel::Category::BATTLE ? 2 : 3;
    if (debugCursor == backIndex) {
        debugCategory = debugCategory == DebugViewModel::Category::ROOM
            ? DebugViewModel::Category::ENV : DebugViewModel::Category::ROOT;
        debugCursor = 0;
        debugScroll = 0.0f;
        debugVelocity = 0.0f;
        requestFullRender();
        return;
    }

    switch (debugCategory) {
    case DebugViewModel::Category::TOUCH_TEST:
        return;
    case DebugViewModel::Category::MONSTER:
        if (debugCursor == 0) {
            if (gameState.teamCount == 0) setToast(Ui::Menu::HATCH_FIRST, nowMs);
            else {
                for (uint8_t slot = 0; slot < gameState.teamCount &&
                     slot < Game::TEAM_CAP; ++slot) {
                    Game::MonsterRuntime& monster = gameState.team[slot];
                    monster.fainted = false;
                    monster.majorStatus = Game::MajorStatus::NONE;
                    monster.majorStatusTurns = 0;
                    monster.hpCur = monster.hpMax;
                    monster.satiety = 100;
                    monster.mood = 100;
                }
                saveState();
                setToast(Ui::Debug::RECOVERED, nowMs);
            }
        } else if (debugCursor == 1) {
            if (gameState.teamCount == 0) {
                setToast(Ui::Menu::HATCH_FIRST, nowMs);
            } else if (gameState.team[0].level < Game::LEVEL_MAX) {
                const Species* species = findSpecies(gameState.team[0].speciesId);
                const uint8_t oldLevel = gameState.team[0].level;
                bool leveledUp = false;
                if (species) {
                    uint32_t targetExp = minimumExpForLevel(
                        species->growthRate,
                        static_cast<uint8_t>(oldLevel + 1));
                    uint32_t required = targetExp > gameState.team[0].exp
                        ? targetExp - gameState.team[0].exp : 1;
                    Game::ExperienceService::Result experience =
                        Game::ExperienceService::add(
                            gameState.team[0], *species, required);
                    leveledUp = experience.leveledUp;
                    if (leveledUp) {
                        gameState.pendingLevelUp = true;
                        gameState.pendingLevelUpLevel =
                            gameState.team[0].level;
                    }
                }
                saveState();
                std::snprintf(debugToastBuffer, sizeof(debugToastBuffer),
                              Ui::Debug::LEVEL_UP_FMT,
                              gameState.team[0].level);
                setToast(debugToastBuffer, nowMs);
                if (leveledUp) {
                    openProgressionScene(AppSceneFlow::Scene::DEBUG, 0,
                                         oldLevel, nowMs);
                    return;
                }
            } else {
                setToast(Ui::Debug::LEVEL_MAX, nowMs);
            }
        } else if (debugCursor == 2) {
            openDebugSwitchPopup();
        }
        break;
    case DebugViewModel::Category::RESOURCE:
        gameState.coins += 1000;
        saveState();
        setToast(Ui::Debug::COINS_ADDED, nowMs);
        break;
    case DebugViewModel::Category::ENV:
        if (debugCursor == 0) openDebugTimePopup();
        else if (debugCursor == 1) debugLightSource = static_cast<uint8_t>((debugLightSource + 1) % 6);
        else if (debugCursor == 2) {
            debugCategory = DebugViewModel::Category::ROOM;
            debugCursor = 0;
            debugScroll = debugVelocity = 0;
        }
        break;
    case DebugViewModel::Category::ROOM:
        debugPopup = debugCursor == 0 ? DebugViewModel::Popup::BED_STYLE
                                     : DebugViewModel::Popup::RUG_STYLE;
        toast = nullptr;
        break;
    case DebugViewModel::Category::MOTION:
        if (debugCursor == 0) {
            if (debugTiltControl) {
                debugTiltControl = false;
                persistDebugMotionFlags();
            } else {
                float ax = 0.0f;
                float ay = 0.0f;
                float az = 0.0f;
                if (Platform::imu().readAcceleration(ax, ay, az)) {
                    debugTiltControl = true;
                    persistDebugMotionFlags();
                    stopDebugPairChase(nowMs, false);
                    sceneFlow.goHome();
                    requestFullRender();
                    return;
                }
                setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            }
        }
        else if (debugCursor == 1) {
            debugWalkBoundaryVisible = !debugWalkBoundaryVisible;
            persistDebugMotionFlags();
        }
        else if (debugCursor == 2) {
            debugTalkPointsVisible = !debugTalkPointsVisible;
            persistDebugMotionFlags();
        } else if (debugCursor == 3) {
            if (!gameState.oobeDone || gameState.teamCount < 2) {
                setToast(Ui::Debug::PAIR_NEEDS_TWO, nowMs, 1400);
            } else {
                startDebugPairChase(nowMs);
                return;
            }
        }
        break;
    case DebugViewModel::Category::BATTLE:
        if (debugCursor == 0) {
            if (!gameState.oobeDone || gameState.teamCount == 0) {
                setToast(Ui::Menu::HATCH_FIRST, nowMs);
            } else if (gameState.team[0].fainted || gameState.team[0].hpCur == 0) {
                setToast(Ui::Menu::FAINTED_TOAST, nowMs);
            } else {
                debugBattleRequested = true;
            }
        } else if (debugCursor == 1) {
            debugBattleDrawBoundsVisible = !debugBattleDrawBoundsVisible;
        }
        break;
    case DebugViewModel::Category::CONTACT_EVENT:
        beginDebugContactEvent(static_cast<uint8_t>(debugCursor + 1), nowMs);
        return;
    case DebugViewModel::Category::ROOT:
    default: break;
    }
    requestRenderRows(0, 448);
}

bool AmoledApp::beginDebugContactEvent(uint8_t kind, uint32_t nowMs) {
    if (kind < 1 || kind > 3) return false;
    if (gameState.teamCount != 1) {
        setToast(Ui::Social::HOST_TEAM_REQUIRED, nowMs, 1400);
        Platform::logf("[FriendDiag] rejected kind=%u reason=team team=%u\n",
                       static_cast<unsigned>(kind),
                       static_cast<unsigned>(gameState.teamCount));
        return false;
    }
    if (debugContactPending || debugContactEventPending || debugContactActive) {
        setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
        Platform::logf("[FriendDiag] rejected kind=%u reason=busy\n",
                       static_cast<unsigned>(kind));
        return false;
    }
    uint8_t selected = 0xFF;
    for (uint8_t slot = 0; slot < gameState.storageCount &&
         slot < Game::STORAGE_CAP; ++slot) {
        const Game::MonsterRuntime& contact = gameState.storage[slot];
        bool representedByTeam = false;
        for (uint8_t teamSlot = 0; teamSlot < gameState.teamCount &&
             teamSlot < Game::TEAM_CAP; ++teamSlot) {
            if (ContactRoster::sameMonster(
                    contact, gameState.team[teamSlot])) {
                representedByTeam = true;
                break;
            }
        }
        if (representedByTeam || contact.fainted || contact.hpCur == 0 ||
            contact.origin == Game::Origin::VISITOR) {
            continue;
        }
        if (selected == 0xFF ||
            contact.bond > gameState.storage[selected].bond) {
            selected = slot;
        }
    }
    if (selected == 0xFF) {
        setToast(Ui::Debug::NO_CONTACT, nowMs, 1400);
        Platform::logf("[FriendDiag] rejected kind=%u reason=no-contact\n",
                       static_cast<unsigned>(kind));
        return false;
    }
    debugContactStorageSlot = selected;
    debugContactKind = kind;
    debugContactPending = true;
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = false;
    debugContactChoiceVisible = true;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
    debugContactMessageUntilMs = 0;
    gameState.storage[selected].lastSeenAt =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    saveState();
    sceneFlow.enter(AppSceneFlow::Scene::HOME);
    std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s",
                  Ui::ContactVisit::KNOCK_LINES);
    Platform::logf("[FriendDiag] begin kind=%u slot=%u species=%u bond=%u\n",
                   static_cast<unsigned>(kind), static_cast<unsigned>(selected),
                   static_cast<unsigned>(gameState.storage[selected].speciesId),
                   static_cast<unsigned>(gameState.storage[selected].bond));
    requestFullRender();
    return true;
}

void AmoledApp::renderDebugTouchOverlay(Canvas565& canvas) const {
    if (expeditionFade.active()) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
        debugCategory == DebugViewModel::Category::TOUCH_TEST) return;
    if (!debugTouchDisplayEnabled || !debugTouchPointValid) return;

    canvas.clearClipRect();
    constexpr int markerRadius = 10;
    const uint16_t shadow = PixelRenderer::rgb(12, 18, 25);
    const uint16_t accent = PixelRenderer::rgb(248, 210, 105);
    const uint16_t ink = PixelRenderer::rgb(255, 255, 255);

    canvas.fillCircle(debugTouchX, debugTouchY, 4, shadow);
    canvas.drawCircle(debugTouchX, debugTouchY, markerRadius, accent);
    canvas.drawFastHLine(debugTouchX - markerRadius - 4, debugTouchY,
                         markerRadius * 2 + 9, ink);
    canvas.drawFastVLine(debugTouchX, debugTouchY - markerRadius - 4,
                         markerRadius * 2 + 9, ink);

    char coordinates[20] = {};
    std::snprintf(coordinates, sizeof(coordinates), "%d,%d",
                  debugTouchX, debugTouchY);
    constexpr int labelWidth = 120;
    constexpr int labelHeight = 36;
    int labelX = debugTouchX + 14;
    if (labelX + labelWidth > AmoledUi::WIDTH) {
        labelX = debugTouchX - labelWidth - 14;
    }
    labelX = std::clamp(labelX, 0, AmoledUi::WIDTH - labelWidth);
    int labelY = debugTouchY + 14;
    if (labelY + labelHeight > AmoledUi::HEIGHT) {
        labelY = debugTouchY - labelHeight - 14;
    }
    labelY = std::clamp(labelY, 0, AmoledUi::HEIGHT - labelHeight);
    canvas.fillRoundRect(labelX, labelY, labelWidth, labelHeight, 5, shadow);
    canvas.drawRoundRect(labelX, labelY, labelWidth, labelHeight, 5, accent);
    PixelRenderer::text(canvas, labelX + 4, labelY + 2, coordinates, ink);
}

void AmoledApp::handleDebugTap(int x, int y, uint32_t nowMs) {
    if (debugPopup != DebugViewModel::Popup::NONE) {
        handleDebugPopupTap(x, y, nowMs);
        return;
    }
    if (debugBackAt(x, y)) {
        if (debugCategory == DebugViewModel::Category::ROOT) {
            // DEBUG is opened from the main menu with enter(), so closeMenu()
            // cannot pop it. Return explicitly to the menu scene.
            sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
        } else {
            debugCategory = debugCategory == DebugViewModel::Category::ROOM
                ? DebugViewModel::Category::ENV : DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = debugVelocity = 0.0f;
        }
        requestFullRender();
        return;
    }
    int item = debugItemAt(x, y, debugCategory, debugScroll);
    if (item < 0) return;
    debugCursor = static_cast<uint8_t>(item);
    executeDebugAction(nowMs);
}

void AmoledApp::handleDebugTouchTest(const TouchEvent& event) {
    const TouchTest::Point point{event.x, event.y};
    if (event.type == TouchEventType::DOWN) {
        debugTouchTest.down(point);
    } else if (event.type == TouchEventType::MOVE) {
        debugTouchTest.move(point);
    } else {
        const auto result = debugTouchTest.up(point);
        if (result == TouchTest::Result::BACK) {
            debugCategory = DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = debugVelocity = 0;
            pointerDown = dragging = false;
            Platform::logLine("[TouchTest] exit");
        } else if (result == TouchTest::Result::CLEAR) {
            Platform::logLine("[TouchTest] cleared");
        } else if (result == TouchTest::Result::SAMPLE) {
            const int index = debugTouchTest.count - 1;
            const auto target = TouchTest::TARGETS[index];
            const auto& sample = debugTouchTest.samples[index];
            for (int phase = 0; phase < 2; ++phase) {
                const auto actual = phase ? sample.up : sample.down;
                const int dx = actual.x - target.x, dy = actual.y - target.y;
                Platform::logf("[TouchTest] t=%lu target=%d event=%s expected=%d,%d actual=%d,%d dx=%d dy=%d error=%.1f\n",
                    static_cast<unsigned long>(event.timestampMs), index + 1,
                    phase ? "UP" : "DOWN", target.x, target.y, actual.x, actual.y,
                    dx, dy, std::sqrt(float(dx * dx + dy * dy)));
            }
            Platform::logf("[TouchTest] target=%d range_x=%d..%d range_y=%d..%d\n", index + 1,
                sample.minimum.x, sample.maximum.x, sample.minimum.y, sample.maximum.y);
            float dx, dy, maximum;
            debugTouchTest.statistics(dx, dy, maximum);
            Platform::logf("[TouchTest] completed=%d/9 down_mean_dx=%.1f down_mean_dy=%.1f down_max_error=%.1f\n",
                          debugTouchTest.count, dx, dy, maximum);
        }
    }
    requestFullRender();
}
#endif

void AmoledApp::clampMenuScroll() {
    float maximum = mainMenuMaxScroll();
    if (menuScroll <= 0.0f) {
        menuScroll = 0.0f;
        if (menuVelocity < 0.0f) menuVelocity = 0.0f;
    } else if (menuScroll >= maximum) {
        menuScroll = maximum;
        if (menuVelocity > 0.0f) menuVelocity = 0.0f;
    }
}

float AmoledApp::itemMaxScroll() const {
    const uint8_t dailyCount =
        Game::ItemInventory::homeBagDailyItemCount(gameState);
    float maximum = itemListMaxScroll(
        dailyCount,
        Game::ItemInventory::homeBagExploreItemCount(gameState),
        currentItemCount(), false);
    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        ShopViewModel::Mode mode =
            inventoryController_.state().shopCategory == Game::ShopService::Category::SELL
                ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
        maximum = shopGridMaxScroll(
            mode, shopDailyItemCount(), shopExploreItemCount(),
            currentItemCount());
    }
    return maximum;
}

void AmoledApp::clampItemScroll() {
    inventoryController_.clampScroll(itemMaxScroll());
}

uint8_t AmoledApp::shopDailyItemCount() const {
    return inventoryController_.dailyCount(gameState);
}

uint8_t AmoledApp::shopExploreItemCount() const {
    return inventoryController_.exploreCount(gameState);
}

uint8_t AmoledApp::currentItemCount() const {
    return inventoryController_.itemCount(gameState, sceneFlow.current());
}

void AmoledApp::openItemScene(AppSceneFlow::Scene target) {
    cancelTouch();
    sceneFlow.openSubScene(target);
    inventoryController_.open();
    teamController_.dismissConfirmation();
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::closeItemScene() {
    cancelTouch();
    inventoryController_.close();
    teamController_.dismissConfirmation();
    teamController_.pressMember(-1);
    toast = nullptr;
    AppSceneFlow::Scene destination = sceneFlow.closeSubScene();
    if (destination == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        resumeExploreRoute(Platform::clock().millis());
    }
    requestFullRender();
}

void AmoledApp::openRoomScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::ROOM);
    pressedRoomItem = -1;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openRoomFoodScene() {
    // Food is a room detail; preserve the room's outer return destination.
    sceneFlow.enter(AppSceneFlow::Scene::ROOM_FOOD);
    pressedRoomItem = -1;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openComputerScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::COMPUTER);
    computerPage = ComputerViewModel::Page::MENU;
    computerScroll = 0.0f;
    computerVelocity = 0.0f;
    computerPressedItem = 0xFF;
    contactActionOpen = false;
    contactActionSlot = 0xFF;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    teamController_.resetStatus();
    clawLogView = false;
    clawLogScroll = 0.0f;
    clawLogVelocity = 0.0f;
    clawLogPinned = true;
    clawLogCount = 0;
    toast = nullptr;
    requestFullRender();
}

const Game::MonsterRuntime* AmoledApp::contactMonster(uint8_t slot) const {
    return ContactRoster::liveMonsterForContact(gameState, slot);
}

bool AmoledApp::contactIsInTeam(uint8_t slot) const {
    return ContactRoster::teamSlotForContact(gameState, slot) >= 0;
}

bool AmoledApp::contactIsVisiting(uint8_t slot) const {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if ((debugContactActive || debugContactPending || debugContactEventPending) &&
        debugContactStorageSlot == slot) return true;
#endif
    const int8_t teamSlot = ContactRoster::teamSlotForContact(gameState, slot);
    return teamSlot >= 0 &&
           gameState.team[teamSlot].origin == Game::Origin::VISITOR;
}

bool AmoledApp::contactInviteLocked(uint8_t slot) const {
    const Game::MonsterRuntime* monster = contactMonster(slot);
    return monster && Game::Bond::inviteLockedToday(
        monster->petCountToday,
        Game::Bond::invitationDay(gameState.gameMinutesTotal));
}

bool AmoledApp::contactCanDelete(uint8_t slot) const {
    return ContactRoster::canDelete(gameState, slot, contactIsVisiting(slot));
}

uint8_t AmoledApp::contactActionCount(uint8_t slot) const {
    if (!contactMonster(slot)) return 0;
    return static_cast<uint8_t>(1 + (contactIsInTeam(slot) ? 0 : 1) +
                               (contactCanDelete(slot) ? 1 : 0));
}

void AmoledApp::openContactAction(uint8_t slot) {
    if (!contactMonster(slot)) return;
    contactActionOpen = true;
    contactActionSlot = slot;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    computerPressedItem = 0xFF;
    requestFullRender();
}

void AmoledApp::openContactStatus(uint8_t slot) {
    if (!contactMonster(slot)) return;
    contactActionOpen = false;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    teamController_.openContactStatus(slot);
    computerPage = ComputerViewModel::Page::STATUS;
    computerPressedItem = 0xFF;
    requestFullRender();
}

void AmoledApp::closeContactOverlay() {
    contactActionOpen = false;
    contactConfirmOpen = false;
    contactActionPressedItem = -1;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    requestFullRender();
}

void AmoledApp::handleContactActionTap(int x, int y, uint32_t nowMs) {
    const uint8_t actionCount = contactActionCount(contactActionSlot);
    const int action = computerContactActionItemAt(
        x, y, actionCount, contactActionSlot, computerScroll);
    if (action < 0) {
        if (!computerContactMenuAt(
                x, y, actionCount, contactActionSlot, computerScroll)) {
            closeContactOverlay();
        }
        return;
    }
    contactActionPressedItem = action;
    int cursor = 0;
    if (action == cursor) {
        openContactStatus(contactActionSlot);
        return;
    }
    ++cursor;
    if (!contactIsInTeam(contactActionSlot) && action == cursor) {
        if (contactInviteLocked(contactActionSlot)) {
            setToast(Ui::Storage::INVITE_LOCKED, nowMs);
            closeContactOverlay();
        } else if (contactIsVisiting(contactActionSlot)) {
            setToast(Ui::Storage::VISITING, nowMs);
            closeContactOverlay();
        } else if (gameState.teamCount >= Game::TEAM_CAP) {
            setToast(Ui::Storage::TEAM_FULL_TOAST, nowMs);
            closeContactOverlay();
        } else {
            contactConfirmOpen = true;
            contactConfirmYes = true;
            contactConfirmDelete = false;
            requestFullRender();
        }
        return;
    }
    if (!contactIsInTeam(contactActionSlot)) ++cursor;
    if (contactCanDelete(contactActionSlot) && action == cursor) {
        contactConfirmOpen = true;
        contactConfirmYes = true;
        contactConfirmDelete = true;
        requestFullRender();
        return;
    }
    closeContactOverlay();
}

void AmoledApp::handleContactConfirmTap(int x, int y, uint32_t nowMs) {
    const int choice = computerContactConfirmChoiceAt(x, y);
    if (choice < 0) {
        closeContactOverlay();
        return;
    }
    if (choice == 1) {
        closeContactOverlay();
        return;
    }
    if (contactConfirmDelete) {
        if (ContactRoster::deleteContact(gameState, contactActionSlot,
                                         contactIsVisiting(contactActionSlot))) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            if ((debugContactActive || debugContactPending || debugContactEventPending) &&
                contactActionSlot < debugContactStorageSlot) {
                --debugContactStorageSlot;
            }
#endif
            if (computerScroll > 0.0f) clampComputerScroll();
            setToast(Ui::Storage::DELETE_TOAST, nowMs);
            saveState();
            contactActionOpen = false;
            contactConfirmOpen = false;
            contactActionSlot = 0xFF;
            requestFullRender();
        } else {
            setToast(Ui::Storage::VISITING, nowMs);
            closeContactOverlay();
        }
        return;
    }

    const Game::FriendshipService::InviteResult result =
        Game::FriendshipService::inviteContact(
            gameState, contactActionSlot, gameState.gameMinutesTotal);
    switch (result) {
    case Game::FriendshipService::InviteResult::JOINED: {
        uint16_t speciesIds[Game::TEAM_CAP] = {};
        for (uint8_t slot = 0; slot < gameState.teamCount &&
             slot < Game::TEAM_CAP; ++slot) {
            speciesIds[slot] = gameState.team[slot].speciesId;
        }
        PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
        syncHomeActors(nowMs);
        saveState();
        setToast(Ui::Storage::INVITE_ACCEPTED, nowMs);
        closeContactOverlay();
        break;
    }
    case Game::FriendshipService::InviteResult::REFUSED:
        saveState();
        setToast(Ui::Storage::INVITE_REFUSED, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::LOCKED:
        setToast(Ui::Storage::INVITE_LOCKED, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::ALREADY_IN_TEAM:
        setToast(Ui::Storage::IN_TEAM, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::TEAM_FULL:
        setToast(Ui::Storage::TEAM_FULL_TOAST, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::INVALID:
    default:
        closeContactOverlay();
        break;
    }
}

void AmoledApp::openSettingsScene() {
    cancelTouch();
    sceneFlow.openSubScene(AppSceneFlow::Scene::SETTINGS);
    settingsController.reset();
    Platform::display().setBrightness(gameState.settings.brightness);
    Platform::audio().setVolume(gameState.settings.volume);
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::closeUtilityScene() {
    cancelTouch();
    computerScroll = 0.0f;
    computerVelocity = 0.0f;
    computerPressedItem = 0xFF;
    clawLogView = false;
    clawLogScroll = 0.0f;
    clawLogVelocity = 0.0f;
    clawLogPinned = true;
    clawLogCount = 0;
    settingsController.reset();
    pressedRoomItem = -1;
    toast = nullptr;
    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD)
        sceneFlow.enter(AppSceneFlow::Scene::ROOM);
    else sceneFlow.closeSubScene();
    requestFullRender();
}

void AmoledApp::clampComputerScroll() {
    const float maximum = static_cast<float>(
        computerMaxStorageScroll(gameState.storageCount));
    if (computerScroll <= 0.0f) {
        computerScroll = 0.0f;
        if (computerVelocity < 0.0f) computerVelocity = 0.0f;
    } else if (computerScroll >= maximum) {
        computerScroll = maximum;
        if (computerVelocity > 0.0f) computerVelocity = 0.0f;
    }
}

void AmoledApp::clampClawLogScroll() {
    const float maximum = std::max(
        0.0f, static_cast<float>(clawLogCount) * CLAW_LOG_ROW_HEIGHT -
                  CLAW_LOG_VIEWPORT);
    if (clawLogPinned) {
        // Pinned follows the tail: always show the newest entries.
        clawLogScroll = maximum;
        clawLogVelocity = 0.0f;
        return;
    }
    if (clawLogScroll <= 0.0f) {
        clawLogScroll = 0.0f;
        if (clawLogVelocity < 0.0f) clawLogVelocity = 0.0f;
    } else if (clawLogScroll >= maximum) {
        // Scrolling back to the bottom re-engages tail-follow.
        clawLogScroll = maximum;
        clawLogPinned = true;
        if (clawLogVelocity > 0.0f) clawLogVelocity = 0.0f;
    }
}

void AmoledApp::clampTeamMovesScroll() {
    teamController_.clampMovesScroll();
}

#if STICKMON_HAS_CLAW
void AmoledApp::refreshClawLogSnapshot() {
    clawLogCount = Stickmon::ClawRuntime::instance().copyLog(
        s_clawLogEntries, Stickmon::ClawStatusLog::CAPACITY);
    clampClawLogScroll();
}
#endif

void AmoledApp::openProgressionScene(AppSceneFlow::Scene returnScene,
                                     uint8_t teamSlot, uint8_t oldLevel,
                                     uint32_t nowMs, bool fromCare) {
    if (!progressionFlow_.openLevel(gameState, returnScene, teamSlot, oldLevel, nowMs, fromCare)) return;
    cancelTouch();
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    setMusicContext(returnScene == AppSceneFlow::Scene::EXPLORE_ROUTE
                        ? MusicContext::EXPLORE : MusicContext::HOME);
    applyProgressionEffects(nowMs);
}

void AmoledApp::openEvolutionProgression(AppSceneFlow::Scene returnScene,
                                          uint8_t teamSlot, uint16_t fromSpeciesId,
                                          uint16_t toSpeciesId, uint32_t nowMs,
                                          Game::ItemId consumedItem) {
    if (!progressionFlow_.openEvolution(gameState, returnScene, teamSlot, fromSpeciesId,
                                         toSpeciesId, nowMs, consumedItem)) return;
    cancelTouch();
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    setMusicContext(returnScene == AppSceneFlow::Scene::EXPLORE_ROUTE
                        ? MusicContext::EXPLORE : MusicContext::HOME);
    applyProgressionEffects(nowMs);
}

void AmoledApp::advanceProgression(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION) return;
    progressionFlow_.advance(gameState, nowMs);
    applyProgressionEffects(nowMs);
}

void AmoledApp::applyProgressionEffects(uint32_t nowMs) {
    const auto effects = progressionFlow_.takeEffects();
    if (effects.stopCry) CryPlayer::ins().stop();
    if (effects.crySpecies) CryPlayer::ins().replay(effects.crySpecies);
    if (effects.syncTeam) {
        uint16_t speciesIds[Game::TEAM_CAP] = {};
        const uint8_t count = Game::TeamRoster::memberCount(gameState);
        for (uint8_t slot = 0; slot < count; ++slot) speciesIds[slot] = gameState.team[slot].speciesId;
        PokemonSprites::syncTeamCache(speciesIds, count);
        if (progressionFlow_.state().progressionTeamSlot == 0) {
            if (const Species* leader = findSpecies(gameState.team[0].speciesId))
                homeMainActor.behavior = behaviorProfileFor(*leader, gameState.team[0]);
        }
        syncHomeActors(nowMs);
    }
    if (effects.preloadTo) {
        const uint16_t species[] = {effects.preloadFrom, effects.preloadTo};
        PokemonSprites::preloadDynamicSpecies(species, 2, 2);
        Platform::logf("[AmoledEvolution] begin slot=%u from=%u to=%u item=%u\n",
            progressionFlow_.state().progressionTeamSlot, effects.preloadFrom, effects.preloadTo,
            static_cast<unsigned>(progressionFlow_.state().progressionEvolutionItem));
    }
    if (effects.noStock) setToast(Ui::Amoled::NO_STOCK, nowMs);
    if (effects.save) saveState();
    requestRenderRows(effects.rowBegin, effects.rowEnd);
    if (!effects.completed) return;
    if (battleSession_.state().battleProgressionSequenceActive) {
        if (startNextBattleProgression(nowMs)) return;
        closeBattle(nowMs);
        return;
    }
    const auto returnScene = progressionFlow_.state().progressionReturnScene;
    sceneFlow.enter(returnScene);
    if (returnScene == AppSceneFlow::Scene::EXPLORE_ROUTE) resumeExploreRoute(nowMs);
    requestFullRender();
}

uint32_t AmoledApp::applyHomeCareExperience(uint8_t slot, uint32_t amount) {
    if (slot >= gameState.teamCount || slot >= Game::TEAM_CAP || amount == 0) return 0;
    auto& monster = gameState.team[slot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species || monster.origin == Game::Origin::VISITOR) return 0;
    const auto result = Game::ExperienceService::add(monster, *species, amount);
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[CareExp] slot=%u amount=%lu exp=%lu level=%u->%u\n", slot,
        static_cast<unsigned long>(result.awarded), static_cast<unsigned long>(monster.exp),
        result.oldLevel, result.newLevel);
#endif
    if (result.leveledUp) {
        if (auto* record = Game::CareExperience::recordFor(gameState, monster)) {
            if (record->pendingOldLevel == 0) record->pendingOldLevel = result.oldLevel;
        }
        gameState.pendingLevelUp = true;
        gameState.pendingLevelUpLevel = monster.level;
    }
    return result.awarded;
}

bool AmoledApp::startPendingCareProgression(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME || visitSession.busy()) return false;
    for (uint8_t slot = 0; slot < std::min(gameState.teamCount, Game::TEAM_CAP); ++slot) {
        auto* record = Game::CareExperience::recordFor(gameState, gameState.team[slot]);
        if (!record || !record->pendingOldLevel) continue;
        if (gameState.team[slot].level <= record->pendingOldLevel) {
            record->pendingOldLevel = 0;
            continue;
        }
        cancelPairInteraction(nowMs);
        cancelRoomAction(nowMs);
        homeRuntime.releaseAll(0);
        homeRuntime.stop(0, nowMs);
        homeMainActor.motion = Home::MotionPhase::STILL;
        petStopping = false;
        stopCompanion(nowMs);
        saveState();
        openProgressionScene(AppSceneFlow::Scene::HOME, slot, record->pendingOldLevel, nowMs, true);
        return true;
    }
    return false;
}

void AmoledApp::applyShowerEffects(uint32_t nowMs) {
    const auto effects = showerController_.takeEffects();
    if (effects.clearToast) toast = nullptr;
    if (effects.sessionReset) {
        showerController_.alignPetGround(showerPetGroundY(gameState.team[0].speciesId));
        showerToast[0] = '\0';
    }
    for (uint8_t stage = 0; stage < 3; ++stage) {
        if (effects.rewardStageMask & (1U << stage))
            grantShowerStage(static_cast<Game::BathService::Stage>(stage), nowMs);
    }
    if (effects.completionScore) {
        Game::BathService::applyCompletionRecovery(gameState, effects.completionScore, 0);
        showerSavePending = true;
        showerSaveAfterMs = nowMs;
    }
    if (effects.cry) CryPlayer::ins().play(gameState.team[0].speciesId);
    if (effects.fullRender) requestFullRender();
    else if (effects.redraw)
        requestRenderRect(effects.left, effects.top, effects.right, effects.bottom);
}

void AmoledApp::flushShowerSave(uint32_t nowMs) {
    if (!showerSavePending || pointerDown ||
        static_cast<int32_t>(nowMs - showerSaveAfterMs) < 0) return;
    // Initial reward debounce stays local; failed attempts share the global
    // retry interval, including when one delayed update reaches both paths.
    if (saveCoordinator_.dirty() && !saveCoordinator_.due(nowMs)) return;
    if (saveState()) showerSavePending = false;
}

void AmoledApp::updateShower(uint32_t nowMs) {
    showerController_.update(nowMs, dirtyRowsValid);
    applyShowerEffects(nowMs);
}

void AmoledApp::openShowerScene(uint32_t nowMs) {
    cancelTouch();
    sceneFlow.enter(AppSceneFlow::Scene::SHOWER);
    resetShowerSession(nowMs);
    requestFullRender();
}

void AmoledApp::closeShowerScene() {
    cancelTouch();
    showerSaveAfterMs = 0;
    flushShowerSave(Platform::clock().millis());
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::ROOM);
    requestFullRender();
}

void AmoledApp::startShowerSoap(uint8_t soapIndex, uint32_t nowMs) {
    if (soapIndex >= Game::SOAP_VARIANT_COUNT ||
        !Game::BathService::consumeSoap(gameState, soapIndex)) {
        setToast(Ui::Shower::NO_SOAP, nowMs);
        return;
    }
    saveState();
    showerController_.soapConsumed(soapIndex, nowMs);
    applyShowerEffects(nowMs);
}

void AmoledApp::startShowerTool(ShowerMode mode, uint32_t nowMs) {
    showerController_.startTool(mode, nowMs);
    applyShowerEffects(nowMs);
}

void AmoledApp::updateShowerToolDrag(int x, int y, uint32_t nowMs) {
    showerController_.dragTool(x, y, nowMs);
    applyShowerEffects(nowMs);
}

bool AmoledApp::tryMergeShowerFoam(ShowerFoamState& source, uint32_t nowMs) {
    const bool merged = showerController_.tryMergeFoam(source, nowMs);
    applyShowerEffects(nowMs);
    return merged;
}

void AmoledApp::rubShowerFoamAt(int x, int y, uint32_t nowMs) {
    showerController_.rubFoamAt(x, y, nowMs);
    applyShowerEffects(nowMs);
}

void AmoledApp::checkShowerAtmosphereThreshold(uint32_t nowMs) {
    showerController_.checkAtmosphereThreshold(nowMs);
    applyShowerEffects(nowMs);
}

bool AmoledApp::anyShowerFoam() const {
    return showerController_.anyFoam();
}

Game::BathInteraction::Layout AmoledApp::showerBathLayout() const {
    return showerController_.bathLayout();
}

void AmoledApp::grantShowerStage(Game::BathService::Stage stage,
                                 uint32_t nowMs) {
    Game::BathService::RewardResult reward =
        Game::BathService::applyStageReward(gameState, stage);
    Game::MonsterRuntime& monster = gameState.team[0];
    const uint8_t oldLevel = monster.level;
    Game::ExperienceService::Result experience;
    if (reward.experience > 0) {
        if (const Species* species = findSpecies(monster.speciesId)) {
            experience = Game::ExperienceService::add(
                monster, *species, reward.experience);
            reward.experience = experience.awarded;
            if (experience.leveledUp) {
                gameState.pendingLevelUp = true;
                gameState.pendingLevelUpLevel = monster.level;
            }
        }
    }

    if (reward.experience > 0) {
        std::snprintf(showerToast, sizeof(showerToast),
                      Ui::Amoled::SHOWER_EXP_FMT,
                      static_cast<unsigned>(reward.experience));
        setToast(showerToast, nowMs, 1300);
    } else if (reward.moodGain > 0) {
        toast = nullptr;
    } else {
        std::snprintf(showerToast, sizeof(showerToast), "%s",
                      Ui::Amoled::CARE_LIMIT);
        setToast(showerToast, nowMs, 1300);
    }
    showerSavePending = true;
    showerSaveAfterMs = nowMs + 300;
    showerController_.rewardFeedback(stage, reward.experience, reward.moodGain, nowMs);
    if (experience.leveledUp) {
        if (saveState()) showerSavePending = false;
        openProgressionScene(AppSceneFlow::Scene::SHOWER, 0, oldLevel, nowMs);
    }
}

void AmoledApp::startShowerRinse(uint32_t nowMs) {
    showerController_.startRinse(nowMs);
    applyShowerEffects(nowMs);
}

void AmoledApp::resetShowerSession(uint32_t nowMs) {
    showerController_.reset(nowMs, showerPetGroundY(gameState.team[0].speciesId));
    showerToast[0] = '\0';
    applyShowerEffects(nowMs);
}

void AmoledApp::openTeamScene(bool preserveSubSceneReturn) {
    cancelTouch();
    if (preserveSubSceneReturn) {
        sceneFlow.enter(AppSceneFlow::Scene::TEAM);
    } else {
        sceneFlow.openSubScene(AppSceneFlow::Scene::TEAM);
    }
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    uint8_t count = Game::TeamRoster::memberCount(gameState);
    for (uint8_t slot = 0; slot < count; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, count);
    teamController_.openTeam();
    inventoryController_.suspendDetail();
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openTeamMoves(uint8_t teamSlot, uint32_t nowMs) {
    if (!teamController_.openMoves(gameState, teamSlot)) {
        setToast(Ui::Amoled::NO_TEAM_MEMBER, nowMs);
        return;
    }
    cancelTouch();
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::switchTeamLeader(uint32_t nowMs) {
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    bool changed = teamController_.state().pendingTeamSlot > 0 &&
        Game::TeamRoster::moveToFront(gameState, teamController_.state().pendingTeamSlot);
    teamController_.completeLeaderChange();
    if (!changed) {
        setToast(Ui::Amoled::CANNOT_SWITCH, nowMs);
        return;
    }

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    uint8_t count = Game::TeamRoster::memberCount(gameState);
    for (uint8_t slot = 0; slot < count; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, count);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        homeMainActor.behavior = behaviorProfileFor(*species, gameState.team[0]);
    }
    homeRuntime.stop(0, nowMs);
    homeMainActor.motion = Home::MotionPhase::STILL;
    petStopping = false;
    homeMainActor.faintRestActive = false;
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    homeMainActor.mind.reset(nowMs);
    homeMainActor.nextMindUpdateMs = nowMs;
    schedulePetDecision(nowMs);
    homeFlow_.scheduleAttention(nowMs, true);
    homeFlow_.scheduleSpecialAction(nowMs);
    updatePetFootprint();
    updateCamera();
    saveState();
    setToast(Ui::Amoled::LEADER_CHANGED, nowMs);
}

void AmoledApp::leaveTeamMember(uint8_t teamSlot, uint32_t nowMs) {
    if (gameState.teamCount <= 1) {
        setToast(Ui::Team::LEAVE_LAST_TOAST, nowMs);
        return;
    }
    if (teamSlot == 0 || teamSlot >= gameState.teamCount ||
        teamSlot >= Game::TEAM_CAP ||
        gameState.team[teamSlot].origin == Game::Origin::VISITOR) {
        return;
    }
    if (gameState.storageCount >= Game::STORAGE_CAP) {
        setToast(Ui::Team::CONTACTS_FULL_TOAST, nowMs);
        return;
    }

    int8_t contactSlot = -1;
    for (uint8_t index = 0;
         index < gameState.storageCount && index < Game::STORAGE_CAP; ++index) {
        if (ContactRoster::sameMonster(gameState.storage[index],
                                       gameState.team[teamSlot])) {
            contactSlot = static_cast<int8_t>(index);
            break;
        }
    }
    if (contactSlot >= 0) {
        gameState.storage[contactSlot] = gameState.team[teamSlot];
    } else {
        gameState.storage[gameState.storageCount++] = gameState.team[teamSlot];
    }
    for (uint8_t index = teamSlot;
         index + 1 < gameState.teamCount && index + 1 < Game::TEAM_CAP;
         ++index) {
        gameState.team[index] = gameState.team[index + 1];
    }
    --gameState.teamCount;
    gameState.team[gameState.teamCount] = Game::MonsterRuntime{};
    gameState.activeSlot = 0;

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t index = 0; index < gameState.teamCount &&
         index < Game::TEAM_CAP; ++index) {
        speciesIds[index] = gameState.team[index].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    syncHomeActors(nowMs);
    saveState();
    teamController_.dismissActions();
    setToast(Ui::Team::LEAVE_TOAST, nowMs);
}

void AmoledApp::performPendingItemAction(uint32_t nowMs) {
    const uint32_t actionStartMs = Platform::clock().millis();
    const uint32_t coinsBefore = gameState.coins;
    const unsigned stockBefore = Game::ItemInventory::count(gameState, inventoryController_.state().pendingItem);
    const char* resultText = Ui::Amoled::NO_EFFECT;
    bool changed = false;
    if (inventoryController_.state().pendingItemAction == PendingItemAction::BUY) {
        switch (Game::ShopService::buy(gameState, inventoryController_.state().pendingItem)) {
        case Game::ShopService::BuyResult::BOUGHT:
            resultText = Ui::Amoled::ITEM_BOUGHT;
            changed = true;
            break;
        case Game::ShopService::BuyResult::LOCKED:
            resultText = Ui::Amoled::ITEM_LOCKED;
            break;
        case Game::ShopService::BuyResult::NOT_ENOUGH_COINS:
            resultText = Ui::Amoled::NOT_ENOUGH_COINS;
            break;
        case Game::ShopService::BuyResult::BAG_FULL:
            resultText = Ui::Amoled::BAG_FULL;
            break;
        case Game::ShopService::BuyResult::DAILY_LIMIT:
            resultText = Ui::Amoled::DAILY_LIMIT;
            break;
        case Game::ShopService::BuyResult::INVALID_ITEM:
            resultText = Ui::Amoled::INVALID_ITEM;
            break;
        }
    } else if (inventoryController_.state().pendingItemAction == PendingItemAction::SELL) {
        switch (Game::ShopService::sell(gameState, inventoryController_.state().pendingItem)) {
        case Game::ShopService::SellResult::SOLD:
            resultText = Ui::Amoled::ITEM_SOLD;
            changed = true;
            break;
        case Game::ShopService::SellResult::NO_STOCK:
            resultText = Ui::Amoled::NO_STOCK;
            break;
        case Game::ShopService::SellResult::INVALID_ITEM:
            resultText = Ui::Amoled::INVALID_ITEM;
            break;
        }
    } else if (inventoryController_.state().pendingItemAction == PendingItemAction::USE) {
        if (sceneFlow.current() == AppSceneFlow::Scene::BAG &&
            !Game::ItemInventory::usableFromHomeBag(inventoryController_.state().pendingItem)) {
            inventoryController_.clearAction();
            setToast(Ui::Amoled::NOT_READY, nowMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
            return;
        }
        bool explorationItem = inventoryController_.state().pendingItem == Game::ItemId::MAX_REPEL ||
                               inventoryController_.state().pendingItem == Game::ItemId::HONEY;
        AppSceneFlow::Scene itemReturn = sceneFlow.subSceneReturn();
        bool exploring = itemReturn == AppSceneFlow::Scene::EXPLORE_MENU ||
                         itemReturn == AppSceneFlow::Scene::EXPLORE_ROUTE;
        if (explorationItem && exploring) {
            const bool activated = exploreSession_.useItem(gameState,
                inventoryController_.state().pendingItem);
            if (activated) {
                resultText = inventoryController_.state().pendingItem == Game::ItemId::MAX_REPEL
                    ? Ui::Bag::MAX_REPEL_ACTIVE : Ui::Bag::HONEY_ACTIVE;
                changed = true;
            } else {
                resultText = Ui::Amoled::ITEM_NOT_READY;
            }
        } else if (explorationItem) {
            resultText = Ui::Amoled::EXPLORE_ONLY;
        } else {
            uint8_t target = Game::ItemInventory::preferredTarget(
                gameState, inventoryController_.state().pendingItem);
            uint8_t oldLevel = target < gameState.teamCount
                ? gameState.team[target].level : 0;
            uint16_t oldSpeciesId = target < gameState.teamCount
                ? gameState.team[target].speciesId : 0;
            switch (Game::ItemInventory::useOnTeam(
                        gameState, inventoryController_.state().pendingItem, target)) {
        case Game::ItemInventory::UseResult::USED:
            resultText = Ui::Amoled::ITEM_USED;
            changed = true;
            break;
        case Game::ItemInventory::UseResult::NO_STOCK:
            resultText = Ui::Amoled::NO_STOCK;
            break;
        case Game::ItemInventory::UseResult::INVALID_TARGET:
            resultText = Ui::Amoled::NO_TARGET;
            break;
        case Game::ItemInventory::UseResult::FAINTED:
            resultText = Ui::Amoled::MON_FAINTED;
            break;
        case Game::ItemInventory::UseResult::HP_FULL:
            resultText = Ui::Amoled::HP_FULL;
            break;
        case Game::ItemInventory::UseResult::STATUS_NORMAL:
            resultText = Ui::Amoled::STATUS_NORMAL;
            break;
        case Game::ItemInventory::UseResult::NO_FAINTED_TARGET:
            resultText = Ui::Amoled::NO_FAINTED_MON;
            break;
        case Game::ItemInventory::UseResult::NOT_USABLE:
            resultText = Ui::Amoled::NOT_READY;
            break;
            }
            if (changed && target == 0 &&
                (oldLevel != gameState.team[0].level ||
                 oldSpeciesId != gameState.team[0].speciesId)) {
                uint16_t speciesIds[Game::TEAM_CAP] = {};
                uint8_t count = Game::TeamRoster::memberCount(gameState);
                for (uint8_t slot = 0; slot < count; ++slot) {
                    speciesIds[slot] = gameState.team[slot].speciesId;
                }
                PokemonSprites::syncTeamCache(speciesIds, count);
                if (oldLevel != gameState.team[0].level) {
                    saveState();
                    openProgressionScene(
                        exploring ? itemReturn : AppSceneFlow::Scene::HOME,
                        0, oldLevel, nowMs);
                    return;
                }
                if (oldSpeciesId != gameState.team[0].speciesId) {
                    saveState();
                    openEvolutionProgression(
                        exploring ? itemReturn : AppSceneFlow::Scene::HOME,
                        0, oldSpeciesId, gameState.team[0].speciesId, nowMs);
                    return;
                }
            }
        }
    }

    bool shopTransaction = sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
        (inventoryController_.state().pendingItemAction == PendingItemAction::BUY ||
         inventoryController_.state().pendingItemAction == PendingItemAction::SELL);
    if (shopTransaction) {
        Platform::logf(
            "[ShopAction] action=%s item=%d changed=%d result=%s "
            "coins=%lu->%lu stock=%u->%u elapsed_ms=%lu\n",
            inventoryController_.state().pendingItemAction == PendingItemAction::BUY ? "buy" : "sell",
            static_cast<int>(inventoryController_.state().pendingItem), changed, resultText,
            static_cast<unsigned long>(coinsBefore),
            static_cast<unsigned long>(gameState.coins), stockBefore,
            static_cast<unsigned>(Game::ItemInventory::count(gameState, inventoryController_.state().pendingItem)),
            static_cast<unsigned long>(Platform::clock().millis() - actionStartMs));
        if (changed) {
            const uint32_t saveStartMs = Platform::clock().millis();
            const bool saved = saveState();
            Platform::logf("[ShopAction] saved=%d save_ms=%lu\n", saved,
                           static_cast<unsigned long>(Platform::clock().millis() - saveStartMs));
            toast = nullptr;
            requestFullRender();
        } else {
            setToast(resultText, nowMs);
        }
        Platform::logLine("[ShopAction] feedback_render_requested");
        return;
    }

    inventoryController_.clearAction();
    if (changed) saveState();
    clampItemScroll();
    setToast(resultText, nowMs);
}

void AmoledApp::persistHomeViewState(uint32_t nowMs) {
    if (!homeRuntimeReady || gameState.teamCount == 0) return;
    const Game::MonsterRuntime& mainMonster = gameState.team[0];
    const Game::SpeciesCareProfile mainCare =
        Game::speciesCareProfileFor(mainMonster.speciesId);
    const bool mainSleepTime = mainCare.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, mainMonster.nature);
    const bool transientMain = homeFlow_.state().action != RoomAction::NONE ||
        homeRuntime.pairActive() ||
        visitorFlow_.state().motion == VisitorMotion::HOST_APPROACH ||
        visitorFlow_.state().motion == VisitorMotion::ENTERING ||
        visitorFlow_.state().motion == VisitorMotion::EXITING || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        homeMainActor.task == Home::Task::ROOM_ACTION ||
        homeMainActor.task == Home::Task::PAIR_ACTION ||
        homeMainActor.task == Home::Task::DOOR_ACTION;
    const float mainGroundOffset = speciesGroundOffset(
        gameState.team[0].speciesId);
    mainViewState.valid = true;
    mainViewState.speciesId = gameState.team[0].speciesId;
    float savedMainX = homeMainActor.x;
    float savedMainY = homeMainActor.y;
    if (!petFootprintInsideWalkArea(savedMainX, savedMainY)) {
        RoomResource& room = RoomResource::ins();
        savedMainX = room.available()
            ? static_cast<float>(room.doorwayInsideX()) : 92.0f;
        savedMainY = room.available()
            ? static_cast<float>(room.doorwayInsideY()) : 151.0f;
    }
    mainViewState.monsterX = savedMainX;
    mainViewState.monsterY = savedMainY - mainGroundOffset;
    PetMotion storedMotion = petMotion();
    if (storedMotion == PetMotion::TURNING) {
        switch (homeMainActor.resumeTask) {
        case Home::Task::WANDER:
        case Home::Task::YIELDING:
        case Home::Task::LEAVING_SLEEP:
            storedMotion = PetMotion::WANDERING;
            break;
        case Home::Task::SEEK_FOOD: storedMotion = PetMotion::SEEKING_FOOD; break;
        case Home::Task::SEEK_SLEEP: storedMotion = PetMotion::SEEKING_SLEEP; break;
        default: storedMotion = PetMotion::IDLE; break;
        }
    }
    if (transientMain || storedMotion == PetMotion::EATING ||
        storedMotion == PetMotion::STOPPING ||
        (storedMotion == PetMotion::SLEEPING && !mainSleepTime)) {
        storedMotion = PetMotion::IDLE;
    }
    const bool stableRoute = storedMotion == PetMotion::WANDERING ||
        storedMotion == PetMotion::SEEKING_FOOD ||
        storedMotion == PetMotion::SEEKING_SLEEP;
    mainViewState.targetX = stableRoute ? homeMainActor.targetX : savedMainX;
    mainViewState.targetY =
        (stableRoute ? homeMainActor.targetY : savedMainY) - mainGroundOffset;
    switch (storedMotion) {
    case PetMotion::WANDERING: mainViewState.aiMode = 1; break;
    case PetMotion::SEEKING_FOOD: mainViewState.aiMode = 3; break;
    case PetMotion::SEEKING_SLEEP: mainViewState.aiMode = 4; break;
    case PetMotion::SLEEPING: mainViewState.aiMode = 8; break;
    case PetMotion::TURNING:
    case PetMotion::EATING:
    case PetMotion::IDLE:
    case PetMotion::STOPPING:
    default: mainViewState.aiMode = petIsResting() ? 8 : 0; break;
    }
    if (mainViewState.aiMode == 8 && !mainSleepTime) {
        mainViewState.aiMode = 0;
    }
    mainViewState.pmdAction = 0;
    mainViewState.pmdDirection = mainDirectionForView(petDirection);
    mainViewState.pmdFrame = transientMain ? 0 : petFrame;
    mainViewState.facingRight =
        petDirection == PokemonSprites::WalkDirection::RIGHT;
    mainViewState.faintRestActive =
        mainMonster.fainted || mainMonster.hpCur == 0;
    const bool resetMainDecision = transientMain ||
        petMotion() == PetMotion::EATING || petMotion() == PetMotion::STOPPING;
    mainViewState.nextDecisionRemainingMs =
        !resetMainDecision &&
        static_cast<int32_t>(homeMainActor.nextDecisionMs - nowMs) > 0
            ? homeMainActor.nextDecisionMs - nowMs : 0;
    mainViewState.postFeedAwakeRemainingMs = 0;

    SecondarySceneViewState& secondary = mainViewState.secondary;
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        homeCompanionActor.speciesId != gameState.team[1].speciesId ||
        gameState.team[1].origin == Game::Origin::VISITOR) {
        secondary = SecondarySceneViewState{};
        return;
    }
    const Game::MonsterRuntime& monster = gameState.team[1];
    const Game::SpeciesCareProfile companionCare =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool companionSleepTime = companionCare.usesBed &&
        Game::isSleepCareTime(gameState.gameMinutesTotal, monster.nature);
    const bool transientCompanion = homeRuntime.pairActive() ||
        visitorFlow_.state().motion != VisitorMotion::NONE ||
        homeCompanionActor.task == Home::Task::ROOM_ACTION ||
        homeCompanionActor.task == Home::Task::PAIR_ACTION ||
        homeCompanionActor.task == Home::Task::DOOR_ACTION ||
        homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::FEEDING ||
        homeCompanionActor.task == Home::Task::SEEK_SLEEP ||
        homeCompanionActor.task == Home::Task::YIELDING;
    const float companionGroundOffset = speciesGroundOffset(
        monster.speciesId);
    secondary = SecondarySceneViewState{};
    secondary.valid = true;
    secondary.speciesId = monster.speciesId;
    secondary.ivPacked = monster.ivPacked;
    secondary.metAt = monster.metAt;
    secondary.nature = monster.nature;
    secondary.metArea = monster.metArea;
    secondary.origin = static_cast<uint8_t>(monster.origin);
    secondary.x = homeCompanionActor.x;
    secondary.y = homeCompanionActor.y - companionGroundOffset;
    const bool stableCompanionRoute = !transientCompanion &&
        homeCompanionActor.task == Home::Task::WANDER;
    secondary.targetX = stableCompanionRoute
        ? homeCompanionActor.targetX : homeCompanionActor.x;
    secondary.targetY = (stableCompanionRoute
        ? homeCompanionActor.targetY : homeCompanionActor.y) -
        companionGroundOffset;
    secondary.sleepX = homeCompanionActor.sleepX;
    secondary.sleepY = homeCompanionActor.sleepY - companionGroundOffset;
    secondary.state = stableCompanionRoute
        ? 1
        : (!transientCompanion && companionSleepTime &&
           homeCompanionActor.task == Home::Task::SLEEPING)
            ? 6 : 0;
    secondary.direction = static_cast<uint8_t>(companionDirection);
    secondary.frameIndex = transientCompanion ? 0 : companionFrame;
    secondary.facingRight =
        companionDirection == PokemonSprites::WalkDirection::RIGHT;
    secondary.sleepSpotValid = homeCompanionActor.sleepSpotValid;
    secondary.stateRemainingMs =
        !transientCompanion &&
        static_cast<int32_t>(homeCompanionActor.nextDecisionMs - nowMs) > 0
            ? homeCompanionActor.nextDecisionMs - nowMs : 0;
    secondary.foodRetryRemainingMs =
        static_cast<int32_t>(
            homeCompanionActor.foodWakeRetryAfterMs - nowMs) > 0
            ? homeCompanionActor.foodWakeRetryAfterMs - nowMs : 0;
}

bool AmoledApp::resetGame() {
    if (gameResetPending || !storageReady || visitSession.busy() ||
        !saveManager.clearAll()) {
        Platform::logLine("[AmoledApp] failed to clear game data");
        return false;
    }
    // Restart through the normal new-game path. Never write the old in-memory
    // state back if a platform's restart request returns before rebooting.
    gameResetPending = true;
    syncHatchImu(Platform::clock().millis());
    storageReady = false;
    lockRequested = false;
    visitSession.stop();
    releaseVisitRadioSession();
    Platform::power().restart();
    return true;
}

bool AmoledApp::saveState() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleScenarioActive) return false;
#endif
    if (gameResetPending || !storageReady || saveWritesBlocked) return false;
    if (!persistHatchProgress(true)) {
        saveCoordinator_.recordAttempt(Platform::clock().millis(), false);
        return false;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    const bool profileExploreSave =
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE;
    const uint32_t saveStartedUs = Platform::clock().micros();
#endif
    if (gameState.oobeDone) persistHomeViewState(Platform::clock().millis());
    else mainViewState = MainSceneViewState{};
    Game::GameState persistentState = gameState;
    for (uint8_t slot = 0; slot < persistentState.teamCount;) {
        if (persistentState.team[slot].origin != Game::Origin::VISITOR) {
            ++slot;
            continue;
        }
        for (uint8_t next = slot + 1; next < persistentState.teamCount;
             ++next) {
            persistentState.team[next - 1] = persistentState.team[next];
        }
        --persistentState.teamCount;
        persistentState.team[persistentState.teamCount] =
            Game::MonsterRuntime{};
    }
    persistentState.activeSlot = 0;
    bool snapshotSaved = saveManager.saveSnapshot(
        persistentState, mainViewState);
    bool historySaved = !encounterHistoryDirty ||
        saveManager.saveEncounterHistory(encounterHistory);
    if (historySaved) encounterHistoryDirty = false;
    if (!snapshotSaved || !historySaved) {
        Platform::logLine("[AmoledApp] save failed");
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (snapshotSaved && historySaved) ++diagnosticSaveCount;
    Platform::logf("[SaveDiag] event=save scene=%u ok=%u saves=%lu "
                   "steps=%u coins=%lu elapsedUs=%lu\n",
        static_cast<unsigned>(sceneFlow.current()),
        snapshotSaved && historySaved ? 1U : 0U,
        static_cast<unsigned long>(diagnosticSaveCount), gameState.stepsToday,
        static_cast<unsigned long>(gameState.coins),
        static_cast<unsigned long>(Platform::clock().micros() - saveStartedUs));
    if (profileExploreSave) {
        static uint32_t lastReportMs = 0, saves = 0, maxUs = 0;
        static uint64_t totalUs = 0;
        const uint32_t saveUs = Platform::clock().micros() - saveStartedUs;
        ++saves;
        totalUs += saveUs;
        maxUs = std::max(maxUs, saveUs);
        const uint32_t nowMs = Platform::clock().millis();
        if (nowMs - lastReportMs >= 1000) {
            Platform::logf("[ExploreSavePerf] saves=%lu avg=%lu us max=%lu us\n",
                static_cast<unsigned long>(saves),
                static_cast<unsigned long>(totalUs / saves),
                static_cast<unsigned long>(maxUs));
            lastReportMs = nowMs;
            saves = maxUs = 0;
            totalUs = 0;
        }
    }
#endif
    const bool saved = snapshotSaved && historySaved;
    saveCoordinator_.recordAttempt(Platform::clock().millis(), saved);
    if (saved) {
        lastPersistMs = Platform::clock().millis();
        showerSavePending = false;
    }
    return saved;
}

bool AmoledApp::hasEncounteredSpecies(uint16_t speciesId) const {
    if (encounterHistory.contains(speciesId)) return true;
    for (uint8_t i = 0;
         i < gameState.teamCount && i < Game::TEAM_CAP; ++i) {
        if (gameState.team[i].speciesId == speciesId) return true;
    }
    for (uint8_t i = 0;
         i < gameState.storageCount && i < Game::STORAGE_CAP; ++i) {
        if (gameState.storage[i].speciesId == speciesId) return true;
    }
    return false;
}

bool AmoledApp::recordEncounteredSpecies(uint16_t speciesId) {
    if (hasEncounteredSpecies(speciesId)) return false;
    if (!encounterHistory.add(speciesId)) return false;
    explorePresenter_.revealSpecies(speciesId);
    encounterHistoryDirty = true;
    Platform::logf("[AmoledApp] encountered species=%u total=%u\n",
                   speciesId, encounterHistory.count);
    return true;
}

bool AmoledApp::syncOwnedSpeciesToEncounterHistory() {
    bool changed = false;
    for (uint8_t i = 0;
         i < gameState.teamCount && i < Game::TEAM_CAP; ++i) {
        changed |= encounterHistory.add(gameState.team[i].speciesId);
    }
    for (uint8_t i = 0;
         i < gameState.storageCount && i < Game::STORAGE_CAP; ++i) {
        changed |= encounterHistory.add(gameState.storage[i].speciesId);
    }
    return changed;
}

}  // namespace AmoledV1

#if STICKMON_ENABLE_DEBUG_FEATURES
bool AmoledV1::AmoledApp::debugStartShowerDrag(uint8_t tool, uint32_t nowMs) {
    if (!gameState.oobeDone || tool > 1 || visitSession.active()) return false;
    openShowerScene(nowMs);
    // Benchmark visual interaction without consuming items or granting rewards.
    showerController_.prepareVisualBenchmark(tool == 1, nowMs);
    startShowerTool(tool == 0 ? ShowerMode::SOAPING : ShowerMode::BRUSHING, nowMs);
    TouchEvent event{};
    event.type = TouchEventType::DOWN;
    event.x = showerController_.state().showerToolX; event.y = showerController_.state().showerToolY; event.timestampMs = nowMs;
    handleTouch(event);
    showerDiagnosticStartedMs = nowMs;
    return true;
}
#endif
