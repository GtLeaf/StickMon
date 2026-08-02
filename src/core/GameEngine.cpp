#include "core/GameEngine.h"
#include "core/CryPlayer.h"
#include "core/VoiceCallService.h"
#include <algorithm>
#include <cstdio>
#include <new>
#include "assets/PokemonSprites.h"
#include "core/ButtonDispatcher.h"
#include "core/FontResource.h"
#include "core/MathUtil.h"
#include "core/RoomResource.h"
#include "core/TraceLog.h"
#include "core/UiStrings.h"
#include "game/BondSystem.h"
#include "game/ExploreItemProgression.h"
#include "game/FoodTuning.h"
#include "game/GameRandom.h"
#include "game/SpeciesBehavior.h"
#include "hardware/EspNowLink.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "platform/api/PlatformServices.h"
#include "scenes/MainScene.h"
#include "scenes/MenuScene.h"
#include "scenes/SocialScene.h"
#include "scenes/ShopScene.h"
#include "scenes/ExploreScene.h"
#include "scenes/SettingsScene.h"
#include "scenes/HatchScene.h"
#include "scenes/ShowerScene.h"

namespace {
// 照护相关常量与睡眠时段辅助函数已移至 game/CareTicker.h，
// 供 GameEngine（清醒 tick）与深度睡眠定时静默唤醒路径共用。
static constexpr uint8_t DEBUG_LIGHT_SOURCE_COUNT = 6;
static constexpr uint16_t SCENE_FADE_HOLD_MS = 500;
static constexpr uint32_t VISIT_PING_INTERVAL_MS = 5000UL;
static constexpr uint32_t VISIT_STATUS_INTERVAL_MS = 3000UL;
static constexpr uint32_t VISIT_PEER_TIMEOUT_MS = 12000UL;
static constexpr uint32_t VISIT_DURATION_SEC = 1800UL;
static constexpr uint8_t CONTACT_VISIT_DAILY_CHANCE = 20;
static constexpr uint32_t CONTACT_VISIT_COOLDOWN_SEC = 3UL * 24UL * 60UL * 60UL;
static constexpr uint32_t CONTACT_VISIT_PLAY_MS = 30000UL;
// 深度睡眠期间的定时静默唤醒周期：到点静默跑一遍照护逻辑再自动深睡。
static constexpr uint32_t SILENT_CARE_WAKE_MINUTES = 10;
static constexpr uint64_t SILENT_CARE_WAKE_INTERVAL_US =
    (uint64_t)SILENT_CARE_WAKE_MINUTES * 60ULL * 1000000ULL;
static constexpr uint32_t FULL_FRAME_BYTES =
    Hal::DISPLAY_W * Hal::DISPLAY_H * 2UL;

using Game::gameSecondsForMinutes;
using Game::isScheduledSleepMinute;

const char* sceneLogName(SceneID scene) {
    switch (scene) {
    case SceneID::MAIN: return "main";
    case SceneID::MENU: return "menu";
    case SceneID::SOCIAL: return "social";
    case SceneID::SHOP: return "shop";
    case SceneID::EXPLORE: return "explore";
    case SceneID::SETTINGS: return "settings";
    case SceneID::HATCH: return "hatch";
    case SceneID::SHOWER: return "shower";
    default: return "unknown";
    }
}

constexpr bool supportsLongPressHome(SceneID scene) {
    return scene == SceneID::MENU ||
           scene == SceneID::SOCIAL ||
           scene == SceneID::SHOP ||
           scene == SceneID::SETTINGS;
}

static_assert(supportsLongPressHome(SceneID::MENU),
              "menu scenes must support the long-press home shortcut");
static_assert(!supportsLongPressHome(SceneID::MAIN) &&
              !supportsLongPressHome(SceneID::EXPLORE) &&
              !supportsLongPressHome(SceneID::HATCH),
              "home, explore, and hatch scenes must retain their own long-press behavior");

uint16_t careDailyCapForLevel(uint8_t level) {
    if (level <= 10) return 60;
    if (level <= 20) return 35;
    return 15;
}

uint8_t careExpMultiplierForLevel(uint8_t level) {
    if (level <= 5) return 3;
    if (level <= 10) return 2;
    return 1;
}

constexpr uint8_t fullBathExperienceForLevel(uint8_t level) {
    return 9 + (static_cast<uint16_t>(level) + 4) / 5 < 15
        ? static_cast<uint8_t>(
            9 + (static_cast<uint16_t>(level) + 4) / 5)
        : 15;
}

static_assert(fullBathExperienceForLevel(1) == 10 &&
              fullBathExperienceForLevel(5) == 10 &&
              fullBathExperienceForLevel(6) == 11 &&
              fullBathExperienceForLevel(11) == 12 &&
              fullBathExperienceForLevel(16) == 13 &&
              fullBathExperienceForLevel(21) == 14 &&
              fullBathExperienceForLevel(26) == 15 &&
              fullBathExperienceForLevel(Game::LEVEL_MAX) == 15,
              "bath experience must rise slowly and remain capped at 15");

uint8_t* effortField(Game::StatLine& ev, uint8_t statIndex) {
    switch (statIndex) {
    case 0: return &ev.hp;
    case 1: return &ev.atk;
    case 2: return &ev.def;
    case 3: return &ev.spa;
    case 4: return &ev.spd;
    case 5: return &ev.spe;
    default: return nullptr;
    }
}

uint8_t clampFoodIndex(uint8_t foodIndex) {
    return foodIndex < Game::ROOM_FOOD_COUNT ? foodIndex : 0;
}

void selectFirstAvailableFood(Game::RoomState& room) {
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        if (room.food[i] > 0) {
            room.selectedFood = i;
            return;
        }
    }
    room.selectedFood = 0;
}

Scene* allocateScene(SceneID id) {
    switch (id) {
    case SceneID::HATCH: return new (std::nothrow) HatchScene();
    case SceneID::SHOWER: return new (std::nothrow) ShowerScene();
    case SceneID::SETTINGS: return new (std::nothrow) SettingsScene();
    case SceneID::EXPLORE: return new (std::nothrow) ExploreScene();
    case SceneID::SHOP: return new (std::nothrow) ShopScene();
    case SceneID::SOCIAL: return new (std::nothrow) SocialScene();
    case SceneID::MENU: return new (std::nothrow) MenuScene();
    case SceneID::MAIN:
    default:
        return new (std::nothrow) MainScene();
    }
}
}

GameEngine& GameEngine::ins() {
    static GameEngine instance;
    return instance;
}

bool GameEngine::begin() {
    bootStartedMs = Platform::clock().millis();
    if (!Hal::ins().begin()) {
        Platform::logLine("[GameEngine] Hal init failed");
        return false;
    }
    PixelRenderer::bind(Hal::ins().frameBuffer());
    if (!resourceService.begin()) {
        Platform::logLine("[GameEngine] external resource FS unavailable");
    }
    saveManager.begin();
    bool normalizedState = false;
    bool loadedState = saveManager.load(state, mainViewState, &normalizedState);
    bool normalizedEncounterHistory = false;
    saveManager.loadEncounterHistory(
        encounterHistory, &normalizedEncounterHistory);
    encounterHistoryDirty = normalizedEncounterHistory;
    if (!loadedState) {
        initDefaultState();
        clearMainSceneViewState();
    }
    uint32_t loadedAt = Hal::ins().millis();
    gameClock.start(loadedAt, state.gameMinutesTotal);
    saveCoordinator.reset(loadedAt);
    if (normalizedState) markDirty(SaveUrgency::DEFERRED);
    if (!loadedState) saveNow();
    if (state.settings.idleTimeoutIndex >= 5) {
        state.settings.idleTimeoutIndex = 0;
        markDirty(SaveUrgency::DEFERRED);
    }
    if (state.room.selectedFood >= Game::ROOM_FOOD_COUNT ||
        (state.room.food[state.room.selectedFood] == 0 && foodCount() > 0)) {
        selectFirstAvailableFood(state.room);
        markDirty(SaveUrgency::DEFERRED);
    }
    sanitizeMonsterMoves();
    if (syncOwnedSpeciesToEncounterHistory()) {
        encounterHistoryDirty = true;
    }
    if (encounterHistoryDirty) markDirty(SaveUrgency::SOON);
    resetDailyCountersIfNeeded();
    Hal::ins().setBrightness(state.settings.brightness);
    Hal::ins().setAudioVolume(state.settings.volume);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    EspNowLink::ins().beginStub();
    startupFirstFrameRendered = false;
    mainSceneFirstFrameRendered = false;
    nextExplorePoolPreloadMs = 0;
    PokemonSprites::setDynamicLoadingEnabled(false);
    startupSpriteCacheReady = syncSpriteCache(0);
    switchScene(state.oobeDone ? SceneID::MAIN : SceneID::HATCH);
    uint32_t now = Hal::ins().millis();
    gameClock.start(now, state.gameMinutesTotal);
    lastInputMs = lastFrameMs = lastUpdateMs = lastSceneUpdateMs =
        lastCareMs = lastActivityMs = now;
    nextSceneUpdateMs = now;
    sceneUpdateScheduled = true;
    sceneDirty = true;
    resourceAlertWasVisible = resourceAlertVisible();
#if STICKMON_ENABLE_RENDER_STATS
    renderStatsStartedMs = now;
#endif
    Platform::logf("[BootTiming] init_ms=%u sprites_ready=%u\n",
                  now - bootStartedMs, startupSpriteCacheReady ? 1 : 0);
    return true;
}

void GameEngine::run() {
    Platform::input().update();
    uint32_t now = Hal::ins().millis();
    bool didWork = false;

    if (now - lastInputMs >= INPUT_SAMPLE_MS) {
        lastInputMs = now;
        processInput(now);
        now = Hal::ins().millis();
        didWork = true;
    }

    updateIdle(now);
    uint32_t frameMs = idleActive ? IDLE_FRAME_MS : FRAME_MS;
    if (now - lastUpdateMs >= frameMs) {
        update(now);
        now = Hal::ins().millis();
        didWork = true;
    }

    bool immediateSceneUpdatePending =
        sceneUpdateScheduled &&
        static_cast<int32_t>(now - nextSceneUpdateMs) >= 0;
    if (sceneDirty && !immediateSceneUpdatePending &&
        now - lastFrameMs >= frameMs) {
        sceneDirty = false;
        render(now);
        // Anchor cadence to frame start so drawing and SPI transfer time do not
        // get added to every frame interval.
        lastFrameMs = now;
        didWork = true;
    }

#if STICKMON_ENABLE_RENDER_STATS
    emitRenderStats(Hal::ins().millis());
#endif
    if (!didWork) Platform::clock().sleepMs(1);
}

void GameEngine::requestScene(SceneID id, bool saveBeforeSwitch) {
    if (id == currentId) return;
    switchScene(id, saveBeforeSwitch);
}

bool GameEngine::fadeToScene(SceneID id, uint16_t durationMs) {
    if (id == currentId || sceneFade != SceneFadePhase::NONE) return false;
    sceneFadeTarget = id;
    sceneFadeDurationMs = MathUtil::max<uint16_t>(1, durationMs);
    sceneFadeStartedMs = Hal::ins().millis();
    sceneFadeLastStepMs = sceneFadeStartedMs;
    sceneFadeProgressMs = 0;
    sceneFade = SceneFadePhase::OUT;
    sceneDirty = true;
    scheduleSceneUpdate(sceneFadeStartedMs);
    resetIdle(sceneFadeStartedMs);
    return true;
}

bool GameEngine::sceneFadeActive() const {
    return sceneFade != SceneFadePhase::NONE;
}

bool GameEngine::sceneFadeInActive() const {
    return sceneFade == SceneFadePhase::IN;
}

bool GameEngine::beginExploreDeparture(uint8_t area) {
    if (exploreBlockedByGuest()) {
        Platform::logLine("[Explore] blocked while guest is at home");
        return false;
    }
    if (!ExploreItemProgression::isAreaUnlocked(area, state)) {
        Platform::logf("[Explore] blocked locked area=%u unlocked=%u\n",
                       area, ExploreItemProgression::unlockedArea(state));
        return false;
    }
    exploreArea = area;
    exploreTravel = ExploreTravelPhase::DEPARTING;
    requestScene(SceneID::MAIN);
    return true;
}

void GameEngine::markExploreActive() {
    if (exploreTravel == ExploreTravelPhase::DEPARTING) {
        exploreTravel = ExploreTravelPhase::ACTIVE;
        uint32_t exploredAt = gameSecondsForMinutes(gameMinutesTotal());
        for (uint8_t slot = 0;
             slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
            if (state.team[slot].origin != Game::Origin::VISITOR) {
                state.team[slot].lastExploredAt = exploredAt;
            }
        }
        markDirty(SaveUrgency::DEFERRED);
    }
}

void GameEngine::beginExploreReturn(bool fainted) {
    restoreContactHostToFront();
    exploreTravel = fainted ? ExploreTravelPhase::RETURNING_FAINTED
                            : ExploreTravelPhase::RETURNING;
    if (!fadeToScene(SceneID::MAIN)) requestScene(SceneID::MAIN);
}

void GameEngine::finishExploreReturn() {
    exploreTravel = ExploreTravelPhase::NONE;
    bool statusCleared = false;
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& mon = state.team[slot];
        if (mon.majorStatus == Game::MajorStatus::NONE && mon.majorStatusTurns == 0) continue;
        mon.majorStatus = Game::MajorStatus::NONE;
        mon.majorStatusTurns = 0;
        statusCleared = true;
    }
    if (contactVisit.active && contactVisit.exploring) {
        contactVisit.farewellPending = true;
    }
    if (statusCleared) markDirty(SaveUrgency::SOON);
}

void GameEngine::beginDebugBattle() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugBattleRequested = true;
    debugMenuReturnRequested = false;
    requestScene(SceneID::EXPLORE);
#endif
}

bool GameEngine::consumeDebugBattleRequest() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool requested = debugBattleRequested;
    debugBattleRequested = false;
    return requested;
#else
    return false;
#endif
}

void GameEngine::endDebugBattle() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugBattleRequested = false;
    debugMenuReturnRequested = true;
    requestScene(SceneID::MENU);
#endif
}

bool GameEngine::consumeDebugMenuReturnRequest() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool requested = debugMenuReturnRequested;
    debugMenuReturnRequested = false;
    return requested;
#else
    return false;
#endif
}

float GameEngine::gameSpeed() const {
    static constexpr float SPEEDS[] = {1.0f, 2.0f, 4.0f, 8.0f};
    uint8_t idx = state.settings.speedIndex;
    if (idx >= 4) idx = 0;
    return SPEEDS[idx];
}

uint32_t GameEngine::gameMinutesTotal() const {
    return gameClock.minutesAt(Hal::ins().millis(), gameSpeed());
}

uint16_t GameEngine::gameMinutesOfDay() const {
    return (uint16_t)(gameMinutesTotal() % (24UL * 60UL));
}

bool GameEngine::isMonsterSleepTime() const {
    return isScheduledSleepMinute(gameMinutesOfDay(), activeMonster().nature);
}

void GameEngine::cycleGameSpeed() {
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);
    state.settings.speedIndex = (state.settings.speedIndex + 1) % 4;
    gameClock.set(now, state.gameMinutesTotal);
    markDirty(SaveUrgency::DEFERRED);
}

uint8_t GameEngine::idleTimeoutIndex() const {
    uint8_t idx = state.settings.idleTimeoutIndex;
    return idx < 5 ? idx : 0;
}

const char* GameEngine::idleTimeoutLabel() const {
    static constexpr const char* LABELS[] = {
        Ui::Settings::IDLE_30S,
        Ui::Settings::IDLE_2MIN,
        Ui::Settings::IDLE_5MIN,
        Ui::Settings::IDLE_10MIN,
        Ui::Settings::IDLE_NEVER,
    };
    return LABELS[idleTimeoutIndex()];
}

void GameEngine::cycleIdleTimeout() {
    state.settings.idleTimeoutIndex = (idleTimeoutIndex() + 1) % 5;
    resetIdle(Hal::ins().millis());
    markDirty(SaveUrgency::DEFERRED);
}

uint8_t GameEngine::hungerValue() const {
    if (!Game::speciesCareProfileFor(activeMonster().speciesId).needsFood) {
        return 0;
    }
    return activeMonster().satiety;
}

uint8_t GameEngine::moodValue() const {
    return activeMonster().mood;
}

const Game::MonsterRuntime& GameEngine::activeMonster() const {
    return state.team[0];
}

Game::MonsterRuntime& GameEngine::activeMonster() {
    return state.team[0];
}

const Species& GameEngine::activeSpecies() const {
    return speciesFor(activeMonster());
}

const Species& GameEngine::speciesFor(const Game::MonsterRuntime& monster) const {
    const Species* species = findSpecies(monster.speciesId);
    return species ? *species : starterSpecies();
}

Game::MonsterRuntime GameEngine::createMonster(uint16_t speciesId, uint8_t level) const {
    const Species* species = findSpecies(speciesId);
    if (!species) species = &starterSpecies();
    if (level < 1) level = 1;
    if (level > Game::LEVEL_MAX) level = Game::LEVEL_MAX;

    Game::MonsterRuntime mon;
    mon.speciesId = species->id;
    mon.level = level;
    mon.exp = minimumExpForLevel(species->growthRate, level);
    resetMovesForLevel(mon, *species);
    mon.ivPacked = randomIvPacked();
    mon.nature = GameRandom::random(0, Game::NATURE_COUNT);
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    mon.metAt = gameSecondsForMinutes(gameMinutesTotal());
    mon.lastSeenAt = mon.metAt;
    mon.lastExploredAt = mon.metAt;
    mon.lastWindowGazeAt = mon.metAt;
    return mon;
}

bool GameEngine::switchActiveMonster() {
    if (state.teamCount < 2) return false;
    if (state.team[1].origin == Game::Origin::VISITOR) return false;
    return moveTeamMemberToFront(1);
}

bool GameEngine::moveTeamMemberToFront(uint8_t slot) {
    if (slot == 0) return true;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) return false;
    if (state.team[slot].origin == Game::Origin::VISITOR &&
        !contactVisit.active) {
        return false;
    }
    Game::MonsterRuntime selected = state.team[slot];
    for (int8_t i = slot; i > 0; --i) {
        state.team[i] = state.team[i - 1];
    }
    state.team[0] = selected;
    state.activeSlot = 0;
    clearMainSceneViewState();
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

bool GameEngine::forgetTeamMemberMove(uint8_t teamSlot, uint8_t moveSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    if (moveSlot == 0 || moveSlot >= Game::MOVE_SLOT_COUNT) return false;

    Game::MonsterRuntime& mon = state.team[teamSlot];
    Game::MoveId* moveId = moveSlot == 1 ? &mon.move2Id : &mon.move3Id;
    if (*moveId == 0) return false;

    *moveId = 0;
    mon.moveProficiency[moveSlot] = 0;
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

bool GameEngine::moveTeamMemberToContacts(uint8_t slot) {
    if (state.teamCount <= 1) return false;
    if (contactVisit.active) return false;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) return false;
    if (state.team[slot].origin == Game::Origin::VISITOR) return false;
    if (state.storageCount >= Game::STORAGE_CAP) return false;

    state.storage[state.storageCount++] = state.team[slot];
    for (uint8_t i = slot; i + 1 < state.teamCount && i + 1 < Game::TEAM_CAP; ++i) {
        state.team[i] = state.team[i + 1];
    }
    state.teamCount--;
    if (state.teamCount < Game::TEAM_CAP) {
        state.team[state.teamCount] = Game::MonsterRuntime{};
    }
    state.activeSlot = 0;
    if (slot == 0) clearMainSceneViewState();
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

VisitHostResult GameEngine::beginVisitAsHost(
    uint16_t speciesId, uint8_t level, uint8_t nature,
    uint8_t satiety, uint8_t mood, uint8_t affection) {
    if (!state.oobeDone || state.teamCount == 0) {
        return VisitHostResult::NO_MONSTER;
    }
    if (!canHostVisit()) return VisitHostResult::TEAM_NOT_SOLO;

    state.team[1] = createMonster(speciesId, level);
    Game::MonsterRuntime& guest = state.team[1];
    guest.nature = nature;
    guest.satiety = satiety;
    guest.mood = mood;
    guest.affection = affection;
    guest.origin = Game::Origin::VISITOR;
    state.teamCount = 2;
    state.activeSlot = 0;
    syncSpriteCache();

    uint32_t now = Hal::ins().millis();
    visitSession.active = true;
    visitSession.asHost = true;
    visitSession.startedMs = now;
    visitSession.lastPingSentMs = now;
    visitSession.lastStatusSentMs = now;
    visitSession.lastPeerMessageMs = now;
    EspNowLink::ins().copyPeerMac(visitSession.peerMac);
    markDirty(SaveUrgency::IMMEDIATE);
    return VisitHostResult::ACCEPTED;
}

void GameEngine::beginVisitAsVisitor() {
    uint32_t now = Hal::ins().millis();
    visitSession.active = true;
    visitSession.asHost = false;
    visitSession.startedMs = now;
    visitSession.lastPingSentMs = now;
    visitSession.lastStatusSentMs = now;
    visitSession.lastPeerMessageMs = now;
    EspNowLink::ins().copyPeerMac(visitSession.peerMac);
}

void GameEngine::endVisit() {
    if (!visitSession.active) return;
    if (visitSession.asHost && state.teamCount >= 2 &&
        state.team[1].origin == Game::Origin::VISITOR) {
        state.team[1] = Game::MonsterRuntime{};
        state.teamCount = 1;
        state.activeSlot = 0;
        syncSpriteCache();
        markDirty(SaveUrgency::IMMEDIATE);
    }
    visitSession = VisitSessionState{};
}

bool GameEngine::takeVisitLinkLost() {
    bool value = visitLinkLost;
    visitLinkLost = false;
    return value;
}

void GameEngine::updateVisit(uint32_t nowMs) {
    EspNowLink& link = EspNowLink::ins();
    link.update();

    if (visitSession.asHost) {
        if (nowMs - visitSession.lastStatusSentMs >= VISIT_STATUS_INTERVAL_MS) {
            uint32_t elapsedSec = (nowMs - visitSession.startedMs) / 1000;
            VisitStatusPayload status{};
            status.active = 1;
            status.remainSec = elapsedSec >= VISIT_DURATION_SEC
                                   ? 0
                                   : (uint16_t)(VISIT_DURATION_SEC - elapsedSec);
            if (link.sendSessionMessage(LinkMessageType::VISIT_STATUS, &status, sizeof(status))) {
                visitSession.lastStatusSentMs = nowMs;
            }
        }
    } else {
        if (nowMs - visitSession.lastPingSentMs >= VISIT_PING_INTERVAL_MS) {
            VisitPingPayload ping{};
            ping.satiety = activeMonster().satiety;
            ping.mood = activeMonster().mood;
            if (link.sendSessionMessage(LinkMessageType::VISIT_PING, &ping, sizeof(ping))) {
                visitSession.lastPingSentMs = nowMs;
            }
        }
    }

    LinkMessageType type;
    uint8_t payload[24];
    uint8_t payloadLen = 0;
    if (link.takeSessionMessage(type, payload, payloadLen)) {
        visitSession.lastPeerMessageMs = nowMs;
        if (visitSession.asHost) {
            if (type == LinkMessageType::VISIT_PING && payloadLen >= sizeof(VisitPingPayload) &&
                state.teamCount >= 2 && state.team[1].origin == Game::Origin::VISITOR) {
                VisitPingPayload ping{};
                memcpy(&ping, payload, sizeof(ping));
                if (state.team[1].satiety != ping.satiety || state.team[1].mood != ping.mood) {
                    state.team[1].satiety = ping.satiety;
                    state.team[1].mood = ping.mood;
                    markDirty(SaveUrgency::DEFERRED);
                }
            } else if (type == LinkMessageType::VISIT_RECALL) {
                VisitEndPayload end{0};
                link.sendSessionMessage(LinkMessageType::VISIT_END, &end, sizeof(end));
                endVisit();
                return;
            }
        } else if (type == LinkMessageType::VISIT_END) {
            endVisit();
            return;
        }
    }

    if (nowMs - visitSession.lastPeerMessageMs > VISIT_PEER_TIMEOUT_MS) {
        visitLinkLost = true;
        endVisit();
    }
}

bool GameEngine::contactInviteLocked(uint8_t slot) const {
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return false;
    uint32_t day = Game::Bond::invitationDay(gameMinutesTotal());
    return Game::Bond::inviteLockedToday(
        state.storage[slot].petCountToday, day);
}

uint8_t GameEngine::contactInviteChance(uint8_t slot) const {
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return 0;
    const Game::MonsterRuntime& mon = state.storage[slot];
    bool firstInvitation =
        mon.bond == Game::Bond::NEW_CONTACT_VALUE &&
        mon.lastExploredAt <= mon.metAt;
    return Game::Bond::inviteChance(mon.bond, firstInvitation);
}

ContactInviteResult GameEngine::inviteContactToTeam(uint8_t slot) {
    if (state.teamCount >= Game::TEAM_CAP) {
        return ContactInviteResult::TEAM_FULL;
    }
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) {
        return ContactInviteResult::INVALID;
    }
    if (contactInviteLocked(slot)) return ContactInviteResult::LOCKED;

    uint8_t chance = contactInviteChance(slot);
    if (GameRandom::random(100) >= chance) {
        state.storage[slot].petCountToday = Game::Bond::inviteLockMarker(
            Game::Bond::invitationDay(gameMinutesTotal()));
        markDirty(SaveUrgency::IMMEDIATE);
        return ContactInviteResult::REFUSED;
    }

    state.storage[slot].petCountToday = 0;
    state.team[state.teamCount++] = state.storage[slot];
    for (uint8_t i = slot; i + 1 < state.storageCount && i + 1 < Game::STORAGE_CAP; ++i) {
        state.storage[i] = state.storage[i + 1];
    }
    state.storageCount--;
    if (state.storageCount < Game::STORAGE_CAP) {
        state.storage[state.storageCount] = Game::MonsterRuntime{};
    }
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    return ContactInviteResult::JOINED;
}

uint16_t GameEngine::contactVisitSpeciesId() const {
    uint8_t visitorSlot = contactVisitorTeamSlot();
    if (visitorSlot < state.teamCount) {
        return state.team[visitorSlot].speciesId;
    }
    if (contactVisit.storageSlot < state.storageCount) {
        return state.storage[contactVisit.storageSlot].speciesId;
    }
    return 0;
}

bool GameEngine::prepareDailyContactVisit() {
    if (contactVisit.pendingKnock || contactVisit.active) return true;
    uint32_t day = Game::Bond::invitationDay(gameMinutesTotal());
    if (state.teamCount != 1 || state.storageCount == 0 ||
        visitSession.active || exploreTravel != ExploreTravelPhase::NONE) {
        return false;
    }
    if (contactVisit.checkedDay == day) return false;

    uint8_t eligible[Game::STORAGE_CAP] = {};
    uint8_t eligibleCount = 0;
    uint32_t nowSec = gameSecondsForMinutes(gameMinutesTotal());
    uint32_t seed = day * 2654435761UL + state.team[0].speciesId * 97UL;
    for (uint8_t slot = 0;
         slot < state.storageCount && slot < Game::STORAGE_CAP; ++slot) {
        const Game::MonsterRuntime& mon = state.storage[slot];
        if (mon.bond < 75 || mon.fainted || mon.hpCur == 0 ||
            mon.origin == Game::Origin::VISITOR) {
            continue;
        }
        if (mon.lastSeenAt != 0 && nowSec >= mon.lastSeenAt &&
            nowSec - mon.lastSeenAt < CONTACT_VISIT_COOLDOWN_SEC) {
            continue;
        }
        eligible[eligibleCount++] = slot;
        seed ^= static_cast<uint32_t>(mon.speciesId) * 2246822519UL;
    }
    if (eligibleCount == 0) return false;

    contactVisit.checkedDay = day;
    seed ^= seed >> 16;
    seed *= 2246822519UL;
    seed ^= seed >> 13;
    if (seed % 100 >= CONTACT_VISIT_DAILY_CHANCE) return false;

    uint8_t selected = eligible[(seed >> 8) % eligibleCount];
    uint8_t eventRoll = static_cast<uint8_t>((seed >> 16) % 100);
    contactVisit.pendingKnock = true;
    contactVisit.storageSlot = selected;
    contactVisit.kind = eventRoll < 45
        ? ContactVisitKind::PLAY
        : (eventRoll < 80 ? ContactVisitKind::GIFT
                          : ContactVisitKind::EXPLORE);
    state.storage[selected].lastSeenAt = nowSec;
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

DebugContactEventResult GameEngine::debugTriggerContactVisit(
    ContactVisitKind kind) {
#if !STICKMON_ENABLE_DEBUG_FEATURES
    (void)kind;
    return DebugContactEventResult::INVALID;
#else
    if (kind != ContactVisitKind::PLAY && kind != ContactVisitKind::GIFT &&
        kind != ContactVisitKind::EXPLORE) {
        return DebugContactEventResult::INVALID;
    }
    if (state.teamCount != 1) {
        return DebugContactEventResult::TEAM_NOT_SOLO;
    }
    if (contactVisit.pendingKnock || contactVisit.active ||
        visitSession.active || exploreTravel != ExploreTravelPhase::NONE) {
        return DebugContactEventResult::BUSY;
    }

    uint8_t selected = 0xFF;
    for (uint8_t slot = 0;
         slot < state.storageCount && slot < Game::STORAGE_CAP; ++slot) {
        const Game::MonsterRuntime& mon = state.storage[slot];
        if (mon.fainted || mon.hpCur == 0 ||
            mon.origin == Game::Origin::VISITOR) {
            continue;
        }
        if (selected == 0xFF || mon.bond > state.storage[selected].bond) {
            selected = slot;
        }
    }
    if (selected == 0xFF) return DebugContactEventResult::NO_CONTACT;

    contactVisit.pendingKnock = true;
    contactVisit.storageSlot = selected;
    contactVisit.kind = kind;
    state.storage[selected].lastSeenAt =
        gameSecondsForMinutes(gameMinutesTotal());
    markDirty(SaveUrgency::IMMEDIATE);
    return DebugContactEventResult::STARTED;
#endif
}

bool GameEngine::acceptContactKnock() {
    if (!contactVisit.pendingKnock || contactVisit.active ||
        state.teamCount != 1 ||
        contactVisit.storageSlot >= state.storageCount) {
        contactVisit.pendingKnock = false;
        return false;
    }

    Game::MonsterRuntime guest = state.storage[contactVisit.storageSlot];
    guest.origin = Game::Origin::VISITOR;
    guest.petCountToday = 0;
    state.team[1] = guest;
    state.teamCount = 2;
    state.activeSlot = 0;
    contactVisit.pendingKnock = false;
    contactVisit.active = true;
    contactVisit.exploring = false;
    contactVisit.farewellPending = false;
    contactVisit.startedMs = Hal::ins().millis();

    if (contactVisit.kind == ContactVisitKind::PLAY) {
        Game::MonsterRuntime& original =
            state.storage[contactVisit.storageSlot];
        original.bond = Game::Bond::increase(original.bond, 2);
        state.team[1].bond = original.bond;
    } else if (contactVisit.kind == ContactVisitKind::GIFT) {
        uint8_t& food = state.room.food[Game::ROOM_NORMAL_FOOD_INDEX];
        if (food < Game::ITEM_STACK_CAP) ++food;
    }

    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

void GameEngine::declineContactKnock() {
    contactVisit.pendingKnock = false;
    contactVisit.storageSlot = 0xFF;
    contactVisit.kind = ContactVisitKind::NONE;
}

void GameEngine::acceptContactExploreInvitation() {
    if (!contactVisit.active ||
        contactVisit.kind != ContactVisitKind::EXPLORE) {
        return;
    }
    contactVisit.exploring = true;
    contactVisit.farewellPending = false;
    requestScene(SceneID::EXPLORE);
}

bool GameEngine::exploreBlockedByGuest() const {
    bool localGuestAtHome = contactVisit.active && !contactVisit.exploring;
    bool linkedGuestAtHome = visitSession.active && visitSession.asHost;
    return localGuestAtHome || linkedGuestAtHome;
}

bool GameEngine::contactVisitTimedOut(uint32_t nowMs) const {
    return contactVisit.active && !contactVisit.exploring &&
           !contactVisit.farewellPending &&
           nowMs - contactVisit.startedMs >= CONTACT_VISIT_PLAY_MS;
}

void GameEngine::requestContactVisitFarewell() {
    if (!contactVisit.active) return;
    restoreContactHostToFront();
    contactVisit.farewellPending = true;
}

uint8_t GameEngine::contactVisitorTeamSlot() const {
    if (!contactVisit.active) return 0xFF;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (state.team[slot].origin == Game::Origin::VISITOR) return slot;
    }
    return 0xFF;
}

bool GameEngine::restoreContactHostToFront() {
    uint8_t visitorSlot = contactVisitorTeamSlot();
    if (visitorSlot != 0 || state.teamCount < 2) return visitorSlot != 0xFF;
    std::swap(state.team[0], state.team[1]);
    state.activeSlot = 0;
    clearMainSceneViewState();
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

void GameEngine::completeContactVisit() {
    if (!contactVisit.active) return;
    uint8_t visitorSlot = contactVisitorTeamSlot();
    if (contactVisit.storageSlot < state.storageCount &&
        visitorSlot < state.teamCount) {
        Game::Origin originalOrigin =
            state.storage[contactVisit.storageSlot].origin;
        uint8_t inviteMarker =
            state.storage[contactVisit.storageSlot].petCountToday;
        state.storage[contactVisit.storageSlot] = state.team[visitorSlot];
        state.storage[contactVisit.storageSlot].origin = originalOrigin;
        state.storage[contactVisit.storageSlot].petCountToday = inviteMarker;
        state.storage[contactVisit.storageSlot].lastSeenAt =
            gameSecondsForMinutes(gameMinutesTotal());
    }
    if (visitorSlot < state.teamCount) {
        for (uint8_t slot = visitorSlot;
             slot + 1 < state.teamCount && slot + 1 < Game::TEAM_CAP;
             ++slot) {
            state.team[slot] = state.team[slot + 1];
        }
        --state.teamCount;
        state.team[state.teamCount] = Game::MonsterRuntime{};
        state.activeSlot = 0;
    }
    contactVisit.active = false;
    contactVisit.exploring = false;
    contactVisit.farewellPending = false;
    contactVisit.storageSlot = 0xFF;
    contactVisit.kind = ContactVisitKind::NONE;
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
}

bool GameEngine::canDeleteContact(uint8_t slot) const {
    return slot < state.storageCount && slot < Game::STORAGE_CAP &&
           state.storage[slot].origin != Game::Origin::HATCHED &&
           !contactIsVisiting(slot);
}

bool GameEngine::deleteContact(uint8_t slot) {
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return false;
    if (!canDeleteContact(slot)) return false;
    if (contactVisit.active && slot < contactVisit.storageSlot) {
        --contactVisit.storageSlot;
    }

    for (uint8_t i = slot; i + 1 < state.storageCount && i + 1 < Game::STORAGE_CAP; ++i) {
        state.storage[i] = state.storage[i + 1];
    }
    state.storageCount--;
    if (state.storageCount < Game::STORAGE_CAP) {
        state.storage[state.storageCount] = Game::MonsterRuntime{};
    }
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

uint8_t GameEngine::foodCount() const {
    uint16_t total = 0;
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        total += state.room.food[i];
    }
    return total > 255 ? 255 : (uint8_t)total;
}

uint8_t GameEngine::foodCount(uint8_t foodIndex) const {
    foodIndex = clampFoodIndex(foodIndex);
    return state.room.food[foodIndex];
}

uint8_t GameEngine::selectedFoodIndex() const {
    return clampFoodIndex(state.room.selectedFood);
}

uint8_t GameEngine::selectedFoodCount() const {
    return foodCount(selectedFoodIndex());
}

uint8_t GameEngine::bowlFoodIndex() const {
    return clampFoodIndex(state.room.bowlFood);
}

uint8_t GameEngine::bowlFoodCount() const {
    return state.room.bowlCount;
}

bool GameEngine::addFood(uint8_t amount) {
    return addFoodStock(0, amount);
}

bool GameEngine::addFoodStock(uint8_t foodIndex, uint8_t amount) {
    if (amount == 0) return true;
    foodIndex = clampFoodIndex(foodIndex);
    uint8_t& count = state.room.food[foodIndex];
    if (count >= Game::ITEM_STACK_CAP) return false;
    bool hadFood = foodCount() > 0;
    count = (uint8_t)MathUtil::min<uint16_t>(Game::ITEM_STACK_CAP, (uint16_t)count + amount);
    if (!hadFood) state.room.selectedFood = foodIndex;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::selectFood(uint8_t foodIndex) {
    foodIndex = clampFoodIndex(foodIndex);
    if (state.room.food[foodIndex] == 0) return false;
    state.room.selectedFood = foodIndex;
    markDirty(SaveUrgency::SOON);
    return true;
}

FoodPlacementResult GameEngine::placeSelectedFoodInBowl() {
    uint8_t foodIndex = selectedFoodIndex();
    if (state.room.food[foodIndex] == 0) {
        selectFirstAvailableFood(state.room);
        foodIndex = selectedFoodIndex();
        if (state.room.food[foodIndex] == 0) return FoodPlacementResult::NO_STOCK;
    }
    if (state.room.bowlCount > 0) {
        uint8_t bitesPerServing = Game::roomFoodBitesPerServing(state.room.bowlFood);
        uint8_t remaining = state.room.bowlBitesRemaining;
        if (remaining == 0 || remaining > bitesPerServing) remaining = bitesPerServing;
        uint8_t refillThreshold = bitesPerServing / 2;
        if (remaining > refillThreshold) return FoodPlacementResult::BOWL_FULL;
        if (state.room.bowlFood != foodIndex) return FoodPlacementResult::DIFFERENT_FOOD;
    }

    state.room.food[foodIndex]--;
    state.room.bowlFood = foodIndex;
    state.room.bowlCount = 1;
    state.room.bowlBitesRemaining = Game::roomFoodBitesPerServing(foodIndex);
    if (state.room.food[foodIndex] == 0) selectFirstAvailableFood(state.room);
    markDirty(SaveUrgency::SOON);
    return FoodPlacementResult::ADDED;
}

FoodConsumeResult GameEngine::consumeBowlFood(uint8_t teamSlot) {
    FoodConsumeResult result;
    if (state.room.bowlCount == 0 ||
        teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) {
        return result;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (mon.origin == Game::Origin::VISITOR ||
        mon.fainted || mon.hpCur == 0 ||
        !Game::speciesCareProfileFor(mon.speciesId).needsFood) {
        return result;
    }
    uint8_t foodIndex = bowlFoodIndex();
    result.foodIndex = foodIndex;
    uint8_t bitesPerServing = Game::roomFoodBitesPerServing(foodIndex);
    if (state.room.bowlBitesRemaining == 0 ||
        state.room.bowlBitesRemaining > bitesPerServing) {
        state.room.bowlBitesRemaining = bitesPerServing;
    }
    state.room.bowlBitesRemaining--;
    if (state.room.bowlBitesRemaining == 0) {
        state.room.bowlCount = 0;
        state.room.bowlFood = 0;
    }

    result.satietyBefore = mon.satiety;
    result.moodBefore = mon.mood;
    bool wasFull = mon.satiety >= 100;
    const FoodTuning::FoodProfile& profile = FoodTuning::PROFILES[foodIndex];
    uint16_t moodGain = profile.moodGain;
    // 口味偏好只作用于口味树果（2~6 号），基础树果不参与。
    if (foodIndex >= Game::ROOM_SWEET_FOOD_INDEX) {
        if (natureLikedFoodIndex(mon.nature) == (int8_t)foodIndex) {
            moodGain = (uint16_t)moodGain * FoodTuning::LIKED_MOOD_PERCENT / 100;
            result.reaction = FoodReaction::LIKED;
        } else if (natureDislikedFoodIndex(mon.nature) == (int8_t)foodIndex) {
            moodGain = (uint16_t)moodGain * FoodTuning::DISLIKED_MOOD_PERCENT / 100;
            result.reaction = FoodReaction::DISLIKED;
        }
    }
    mon.satiety = (uint8_t)MathUtil::min<uint16_t>(100, (uint16_t)mon.satiety + profile.satietyGain);
    mon.mood = (uint8_t)MathUtil::min<uint16_t>(100, (uint16_t)mon.mood + moodGain);
    result.consumed = true;
    result.satietyAfter = mon.satiety;
    result.moodAfter = mon.mood;
    result.lastBite = state.room.bowlCount == 0;
    result.becameFull = !wasFull && mon.satiety >= 100;
    grantCareExperience(profile.careExp, wasFull, teamSlot);
    markDirty(SaveUrgency::SOON);
    return result;
}

bool GameEngine::addCandy(uint8_t amount) {
    return addItem(Game::ItemId::CANDY, amount);
}

bool GameEngine::addPotion(uint8_t amount) {
    return addItem(Game::ItemId::POTION, amount);
}

bool GameEngine::addSuperPotion(uint8_t amount) {
    return addItem(Game::ItemId::SUPER_POTION, amount);
}

bool GameEngine::usePotion(uint8_t teamSlot) {
    if (state.bag.potion == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (mon.fainted || mon.hpCur == 0 || mon.hpCur >= mon.hpMax) return false;
    state.bag.potion--;
    mon.hpCur = MathUtil::min<uint16_t>(mon.hpMax, mon.hpCur + 20);
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useSuperPotion(uint8_t teamSlot) {
    if (state.bag.superPotion == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (mon.fainted || mon.hpCur == 0 || mon.hpCur >= mon.hpMax) return false;
    state.bag.superPotion--;
    mon.hpCur = MathUtil::min<uint16_t>(mon.hpMax, mon.hpCur + 50);
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::addAntidote(uint8_t amount) {
    return addItem(Game::ItemId::ANTIDOTE, amount);
}

namespace {
bool cureMajorStatus(uint8_t& stock, Game::MonsterRuntime& mon,
                     Game::MajorStatus curable1,
                     Game::MajorStatus curable2 = Game::MajorStatus::NONE) {
    if (stock == 0) return false;
    if (mon.majorStatus != curable1 &&
        (curable2 == Game::MajorStatus::NONE || mon.majorStatus != curable2)) {
        return false;
    }
    stock--;
    mon.majorStatus = Game::MajorStatus::NONE;
    mon.majorStatusTurns = 0;
    return true;
}
}

bool GameEngine::useAntidote(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    bool cured = cureMajorStatus(state.bag.antidote, state.team[teamSlot],
                                 Game::MajorStatus::POISON, Game::MajorStatus::TOXIC);
    if (cured) markDirty(SaveUrgency::SOON);
    return cured;
}

bool GameEngine::addParalyzeHeal(uint8_t amount) {
    return addItem(Game::ItemId::PARALYZE_HEAL, amount);
}

bool GameEngine::useParalyzeHeal(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    bool cured = cureMajorStatus(state.bag.paralyzeHeal, state.team[teamSlot],
                                 Game::MajorStatus::PARALYSIS);
    if (cured) markDirty(SaveUrgency::SOON);
    return cured;
}

bool GameEngine::addAwakening(uint8_t amount) {
    return addItem(Game::ItemId::AWAKENING, amount);
}

bool GameEngine::useAwakening(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    bool cured = cureMajorStatus(state.bag.awakening, state.team[teamSlot],
                                 Game::MajorStatus::SLEEP);
    if (cured) markDirty(SaveUrgency::SOON);
    return cured;
}

bool GameEngine::addBurnHeal(uint8_t amount) {
    return addItem(Game::ItemId::BURN_HEAL, amount);
}

bool GameEngine::useBurnHeal(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    bool cured = cureMajorStatus(state.bag.burnHeal, state.team[teamSlot],
                                 Game::MajorStatus::BURN);
    if (cured) markDirty(SaveUrgency::SOON);
    return cured;
}

bool GameEngine::addIceHeal(uint8_t amount) {
    return addItem(Game::ItemId::ICE_HEAL, amount);
}

bool GameEngine::useIceHeal(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    bool cured = cureMajorStatus(state.bag.iceHeal, state.team[teamSlot],
                                 Game::MajorStatus::FREEZE);
    if (cured) markDirty(SaveUrgency::SOON);
    return cured;
}

bool GameEngine::useMaxPotion(uint8_t teamSlot) {
    if (state.bag.maxPotion == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (mon.fainted || mon.hpCur == 0 || mon.hpCur >= mon.hpMax) return false;
    state.bag.maxPotion--;
    mon.hpCur = mon.hpMax;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useFullRestore(uint8_t teamSlot) {
    if (state.bag.fullRestore == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (mon.fainted || mon.hpCur == 0) return false;
    if (mon.hpCur >= mon.hpMax && mon.majorStatus == Game::MajorStatus::NONE) {
        return false;
    }
    state.bag.fullRestore--;
    mon.hpCur = mon.hpMax;
    mon.majorStatus = Game::MajorStatus::NONE;
    mon.majorStatusTurns = 0;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useFullHeal(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (state.bag.fullHeal == 0 || mon.majorStatus == Game::MajorStatus::NONE) {
        return false;
    }
    state.bag.fullHeal--;
    mon.majorStatus = Game::MajorStatus::NONE;
    mon.majorStatusTurns = 0;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useRevive(uint8_t teamSlot) {
    if (state.bag.revive == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    Game::MonsterRuntime& mon = state.team[teamSlot];
    if (!mon.fainted) return false;
    state.bag.revive--;
    mon.fainted = false;
    mon.hpCur = MathUtil::max<uint16_t>(1, mon.hpMax / 2);
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useMaxRepel() {
    if (state.bag.maxRepel == 0) return false;
    if (!exploreItemEffects.activateMaxRepel()) return false;
    state.bag.maxRepel--;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::useHoney() {
    if (state.bag.honey == 0) return false;
    if (!exploreItemEffects.activateHoney()) return false;
    state.bag.honey--;
    markDirty(SaveUrgency::SOON);
    return true;
}

namespace {
uint8_t* itemStockPointer(Game::GameState& s, Game::ItemId item) {
    int8_t foodIndex = Game::foodIndexForItemId(item);
    if (foodIndex >= 0) return &s.room.food[foodIndex];
    int8_t soapIndex = Game::soapIndexForItemId(item);
    if (soapIndex >= 0) return &s.bag.soap[soapIndex];
    switch (item) {
    case Game::ItemId::POTION: return &s.bag.potion;
    case Game::ItemId::SUPER_POTION: return &s.bag.superPotion;
    case Game::ItemId::ANTIDOTE: return &s.bag.antidote;
    case Game::ItemId::CANDY: return &s.bag.candy;
    case Game::ItemId::PARALYZE_HEAL: return &s.bag.paralyzeHeal;
    case Game::ItemId::AWAKENING: return &s.bag.awakening;
    case Game::ItemId::BURN_HEAL: return &s.bag.burnHeal;
    case Game::ItemId::ICE_HEAL: return &s.bag.iceHeal;
    case Game::ItemId::MAX_POTION: return &s.bag.maxPotion;
    case Game::ItemId::FULL_RESTORE: return &s.bag.fullRestore;
    case Game::ItemId::FULL_HEAL: return &s.bag.fullHeal;
    case Game::ItemId::FIRE_STONE: return &s.bag.fireStone;
    case Game::ItemId::WATER_STONE: return &s.bag.waterStone;
    case Game::ItemId::THUNDER_STONE: return &s.bag.thunderStone;
    case Game::ItemId::REVIVE: return &s.bag.revive;
    case Game::ItemId::MAX_REPEL: return &s.bag.maxRepel;
    case Game::ItemId::HONEY: return &s.bag.honey;
    case Game::ItemId::NUGGET: return &s.bag.nugget;
    case Game::ItemId::BIG_PEARL: return &s.bag.bigPearl;
    case Game::ItemId::STAR_PIECE: return &s.bag.starPiece;
    case Game::ItemId::HEART_SCALE: return &s.bag.heartScale;
    default: return nullptr;
    }
}
}

uint8_t GameEngine::itemCount(Game::ItemId item) const {
    const uint8_t* count = itemStockPointer(const_cast<Game::GameState&>(state), item);
    return count ? *count : 0;
}

bool GameEngine::addItem(Game::ItemId item, uint8_t amount, SaveUrgency urgency) {
    if (amount == 0) return true;

    uint8_t* count = itemStockPointer(state, item);
    if (!count) return false;
    if (*count >= Game::ITEM_STACK_CAP) return false;
    *count = static_cast<uint8_t>(MathUtil::min<uint16_t>(
        Game::ITEM_STACK_CAP, static_cast<uint16_t>(*count) + amount));
    if (Game::foodIndexForItemId(item) >= 0 && foodCount() == amount) {
        state.room.selectedFood = (uint8_t)Game::foodIndexForItemId(item);
    }
    markDirty(urgency);
    return true;
}

bool GameEngine::removeItem(Game::ItemId item, uint8_t amount, SaveUrgency urgency) {
    if (amount == 0) return true;

    uint8_t* count = itemStockPointer(state, item);
    if (!count) return false;
    if (*count < amount) return false;
    *count -= amount;
    uint8_t selectedFood = selectedFoodIndex();
    if (Game::foodIndexForItemId(item) >= 0 && state.room.food[selectedFood] == 0) {
        selectFirstAvailableFood(state.room);
    }
    markDirty(urgency);
    return true;
}

uint8_t GameEngine::collectRecallableMoves(uint8_t teamSlot,
                                           Game::MoveId* outMoves,
                                           uint8_t capacity) const {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return 0;
    const Game::MonsterRuntime& mon = state.team[teamSlot];
    return ::collectRecallableMoves(
        speciesFor(mon), mon, outMoves, capacity);
}

bool GameEngine::recallMove(uint8_t teamSlot, Game::MoveId moveId,
                            uint8_t replacementSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP ||
        replacementSlot == 0 || replacementSlot >= Game::MOVE_SLOT_COUNT ||
        itemCount(Game::ItemId::HEART_SCALE) == 0) {
        return false;
    }

    Game::MoveId recallable[MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t recallableCount = collectRecallableMoves(
        teamSlot, recallable, MAX_RECALLABLE_MOVE_COUNT);
    bool validMove = false;
    for (uint8_t index = 0; index < recallableCount; ++index) {
        if (recallable[index] == moveId) {
            validMove = true;
            break;
        }
    }
    if (!validMove ||
        !removeItem(Game::ItemId::HEART_SCALE, 1, SaveUrgency::DEFERRED)) {
        return false;
    }

    Game::MonsterRuntime& mon = state.team[teamSlot];
    Game::MoveId& target = replacementSlot == 1 ? mon.move2Id : mon.move3Id;
    target = moveId;
    mon.moveProficiency[replacementSlot] = 0;
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

bool GameEngine::useEvolutionStone(uint8_t teamSlot, Game::ItemId stone) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return false;
    if (itemCount(stone) == 0) return false;
    Game::MonsterRuntime& mon = state.team[teamSlot];
    const Species* target = stoneEvolutionTarget(speciesFor(mon), stone);
    if (!target) return false;
    return queueEvolutionEvent(
        teamSlot, mon.speciesId, target->id, mon.level, stone);
}

uint8_t GameEngine::grantBathReward(BathRewardStage stage) {
    if (state.teamCount == 0) return 0;
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);
    resetDailyCountersIfNeeded();

    Game::MonsterRuntime& mon = activeMonster();
    uint8_t fullBathExp = fullBathExperienceForLevel(mon.level);
    uint8_t soapExp = fullBathExp <= 13 ? 2 : 3;
    uint8_t brushExp = fullBathExp <= 11 ? 3 : 4;
    uint8_t expGain = 0;
    uint8_t moodGain = 0;
    switch (stage) {
    case BathRewardStage::SOAP:
        expGain = soapExp;
        break;
    case BathRewardStage::BRUSH:
        expGain = brushExp;
        moodGain = 2;
        break;
    case BathRewardStage::RINSE:
        expGain = fullBathExp - soapExp - brushExp;
        moodGain = 8;
        break;
    }

    uint16_t cap = careDailyCapForLevel(mon.level);
    uint16_t available = state.careExpToday < cap
        ? cap - state.careExpToday
        : 0;
    expGain = static_cast<uint8_t>(MathUtil::min<uint16_t>(expGain, available));
    if (expGain > 0) {
        state.careExpToday += expGain;
        addExperience(expGain);
    }
    if (moodGain > 0) {
        mon.mood = static_cast<uint8_t>(
            MathUtil::min<uint16_t>(100, static_cast<uint16_t>(mon.mood) + moodGain));
    }
    markDirty(SaveUrgency::SOON);
    return expGain;
}

bool GameEngine::spendCoins(uint32_t amount) {
    if (state.coins < amount) return false;
    state.coins -= amount;
    markDirty(SaveUrgency::SOON);
    return true;
}

void GameEngine::addCoins(uint32_t amount) {
    uint64_t total = static_cast<uint64_t>(state.coins) + amount;
    state.coins = total > 99999 ? 99999 : static_cast<uint32_t>(total);
    markDirty(SaveUrgency::SOON);
}

bool GameEngine::candyPurchaseAvailable() {
    syncGameClock(Hal::ins().millis());
    resetDailyCountersIfNeeded();
    return state.candyPurchasesToday < Game::DAILY_CANDY_PURCHASE_CAP;
}

bool GameEngine::recordCandyPurchase() {
    syncGameClock(Hal::ins().millis());
    resetDailyCountersIfNeeded();
    if (state.candyPurchasesToday >= Game::DAILY_CANDY_PURCHASE_CAP) {
        return false;
    }
    ++state.candyPurchasesToday;
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::recordFriendContact(const Game::MonsterRuntime& monster,
                                     uint8_t metArea,
                                     uint8_t* contactSlot) {
    const Species* species = findSpecies(monster.speciesId);
    if (!species || state.storageCount >= Game::STORAGE_CAP) return false;

    Game::MonsterRuntime mon = monster;
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    resetMovesForLevel(mon, *species);
    mon.origin = Game::Origin::BEFRIENDED;
    mon.bond = Game::Bond::NEW_CONTACT_VALUE;
    mon.metArea = metArea;
    mon.metAt = gameSecondsForMinutes(gameMinutesTotal());
    mon.lastSeenAt = mon.metAt;
    mon.lastExploredAt = mon.metAt;
    mon.lastWindowGazeAt = mon.metAt;

    uint8_t slot = state.storageCount;
    state.storage[state.storageCount++] = mon;
    if (contactSlot) *contactSlot = slot;
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

void GameEngine::grantEffortFrom(const Species& defeatedSpecies) {
    grantEffortToTeamMember(0, defeatedSpecies);
}

void GameEngine::grantEffortToTeamMember(uint8_t teamSlot,
                                         const Species& defeatedSpecies) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& mon = state.team[teamSlot];
    bool changed = false;
    for (uint8_t i = 0; i < Game::STAT_COUNT; ++i) {
        uint8_t amount = evYieldAt(defeatedSpecies, i);
        if (amount == 0) continue;

        uint8_t* value = effortField(mon.ev, i);
        if (!value || *value >= Game::EV_MAX) continue;
        uint16_t total = Game::evTotal(mon.ev);
        if (total >= Game::EV_TOTAL_MAX) break;

        uint8_t roomByStat = Game::EV_MAX - *value;
        uint16_t roomByTotal = Game::EV_TOTAL_MAX - total;
        uint8_t gain = MathUtil::min<uint16_t>(amount, MathUtil::min<uint16_t>(roomByStat, roomByTotal));
        if (gain == 0) continue;
        *value += gain;
        changed = true;
    }

    if (changed) {
        const Species& species = speciesFor(mon);
        uint16_t oldMax = mon.hpMax;
        mon.hpMax = maxHpFor(species, mon);
        if (mon.hpMax > oldMax) mon.hpCur = MathUtil::min<uint16_t>(mon.hpMax, mon.hpCur + (mon.hpMax - oldMax));
        markDirty(SaveUrgency::DEFERRED);
    }
}

bool GameEngine::rewardPairInteractionMood() {
    if (state.teamCount < 2) return false;
    syncGameClock(Hal::ins().millis());
    resetDailyCountersIfNeeded();
    if (state.pairMoodRewardsToday >= 3) return false;

    bool changed = false;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& mon = state.team[slot];
        if (mon.fainted || mon.hpCur == 0 || mon.mood >= 100) continue;
        ++mon.mood;
        changed = true;
    }
    ++state.pairMoodRewardsToday;
    markDirty(SaveUrgency::SOON);
    return changed;
}

PetResult GameEngine::petMonster() {
    PetResult result;
    uint32_t nowMs = Hal::ins().millis();
    syncGameClock(nowMs);
    resetDailyCountersIfNeeded();
    Game::MonsterRuntime& mon = activeMonster();
    if (mon.fainted || mon.hpCur == 0) {
        result.outcome = PetOutcome::NEEDS_REST;
        return result;
    }

    mon.lastPettedAt = gameSecondsForMinutes(state.gameMinutesTotal);
    if (mon.petCountToday >= 4) {
        result.outcome = PetOutcome::DAILY_LIMIT;
        markDirty(SaveUrgency::DEFERRED);
        return result;
    }

    uint8_t oldMood = mon.mood;
    uint8_t oldAffection = mon.affection;
    mon.petCountToday++;
    mon.mood = (uint8_t)MathUtil::min<uint16_t>(100, (uint16_t)mon.mood + 5);
    mon.affection = (uint8_t)MathUtil::min<uint16_t>(255, (uint16_t)mon.affection + 2);
    result.outcome = PetOutcome::REWARDED;
    result.moodGain = mon.mood - oldMood;
    result.affectionGain = mon.affection - oldAffection;
    grantCareExperience(2);
    markDirty(SaveUrgency::SOON);
    return result;
}

void GameEngine::finishHatch(uint8_t starterStyle) {
    uint16_t starterId = 1;
    if (starterStyle == 1) starterId = 4;
    else if (starterStyle == 2) starterId = 7;
    const Species* species = findSpecies(starterId);
    if (!species) species = &starterSpecies();

    state.teamCount = 1;
    state.activeSlot = 0;
    state.team[0] = createMonster(species->id, 5);
    clearMainSceneViewState();
    state.team[0].origin = Game::Origin::HATCHED;
    state.team[0].metArea = Game::MET_AREA_HATCHED;
    state.team[0].metAt = gameSecondsForMinutes(gameMinutesTotal());
    state.team[0].lastSeenAt = state.team[0].metAt;
    state.team[0].lastExploredAt = state.team[0].metAt;
    state.team[0].lastWindowGazeAt = state.team[0].metAt;
    state.bag = Game::BagState{};
    state.room = Game::RoomState{};
    state.coins = Game::INITIAL_COINS;
    state.oobeDone = true;
    markDirty(SaveUrgency::IMMEDIATE);
    requestScene(SceneID::MAIN);
}

void GameEngine::addExperience(uint32_t amount) {
    addExperienceToTeamMember(0, amount);
}

uint32_t GameEngine::addExperienceToTeamMember(uint8_t teamSlot, uint32_t amount) {
    if (amount == 0 || teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return 0;
    Game::MonsterRuntime& mon = state.team[teamSlot];
    const Species& beforeSpecies = speciesFor(mon);
    uint8_t oldLevel = mon.level;
    uint16_t oldHpMax = mon.hpMax;
    uint32_t oldExp = mon.exp;
    uint32_t maxExp = minimumExpForLevel(beforeSpecies.growthRate, Game::LEVEL_MAX);
    uint64_t totalExp = static_cast<uint64_t>(mon.exp) + amount;
    mon.exp = totalExp > maxExp ? maxExp : static_cast<uint32_t>(totalExp);
    mon.level = levelForExp(beforeSpecies.growthRate, mon.exp);
    mon.hpMax = maxHpFor(beforeSpecies, mon);
    if (mon.hpMax > oldHpMax) {
        mon.hpCur = MathUtil::min<uint16_t>(mon.hpMax, mon.hpCur + (mon.hpMax - oldHpMax));
    }
    if (mon.level < oldLevel) mon.level = oldLevel;
    bool leveledUp = mon.level > oldLevel;
    if (leveledUp) {
        bool evolutionQueued = applyLevelUpEvolutions(
            mon, teamSlot, true, oldLevel);
        state.pendingLevelUp = true;
        state.pendingLevelUpLevel = mon.level;
        if (!evolutionQueued) {
            queueMoveLearnIfReady(mon, speciesFor(mon), oldLevel, teamSlot);
        }
    }
    markDirty(leveledUp ? SaveUrgency::IMMEDIATE : SaveUrgency::DEFERRED);
    return mon.exp > oldExp ? mon.exp - oldExp : 0;
}

bool GameEngine::acknowledgePendingLevelUp() {
    if (!state.pendingLevelUp) return false;
    state.pendingLevelUp = false;
    state.pendingLevelUpLevel = 0;
    markDirty(SaveUrgency::IMMEDIATE);
    return true;
}

bool GameEngine::pendingMoveLearnNeedsReplacement() const {
    if (!state.pendingMoveLearn ||
        state.pendingMoveSlot >= state.teamCount ||
        state.pendingMoveSlot >= Game::TEAM_CAP) {
        return false;
    }
    const Game::MonsterRuntime& mon = state.team[state.pendingMoveSlot];
    return mon.move2Id != 0 && mon.move3Id != 0;
}

bool GameEngine::resolvePendingMoveLearn(bool learn) {
    return resolvePendingMoveLearnInternal(
        learn, Game::MOVE_SLOT_COUNT);
}

bool GameEngine::resolvePendingMoveLearnReplacing(
    uint8_t replacementSlot) {
    if (replacementSlot == 0 ||
        replacementSlot >= Game::MOVE_SLOT_COUNT) {
        return false;
    }
    return resolvePendingMoveLearnInternal(true, replacementSlot);
}

bool GameEngine::resolvePendingMoveLearnInternal(
    bool learn, uint8_t replacementSlot) {
    if (!state.pendingMoveLearn) return false;
    const uint8_t teamSlot = state.pendingMoveSlot;
    const uint16_t nextCursor = state.pendingMoveCursor;
    const Game::MoveId learnedMoveId = state.pendingMoveId;
    bool applied = false;
    if (teamSlot < state.teamCount && teamSlot < Game::TEAM_CAP) {
        Game::MonsterRuntime& mon = state.team[teamSlot];
        const Species* species = findSpecies(mon.speciesId);
        if (learn && species && mon.level >= moveLearnLevelForSpecies(*species, learnedMoveId) &&
            canLearnAsSpecialMove(*species, learnedMoveId) &&
            mon.move2Id != learnedMoveId && mon.move3Id != learnedMoveId) {
            Game::MoveId replacedMoveId = 0;
            if (mon.move2Id == 0) {
                mon.move2Id = learnedMoveId;
                mon.moveProficiency[1] = 0;
            } else if (mon.move3Id == 0) {
                mon.move3Id = learnedMoveId;
                mon.moveProficiency[2] = 0;
            } else if (replacementSlot == 1) {
                replacedMoveId = mon.move2Id;
                mon.move2Id = learnedMoveId;
                mon.moveProficiency[1] = 0;
            } else if (replacementSlot == 2) {
                replacedMoveId = mon.move3Id;
                mon.move3Id = learnedMoveId;
                mon.moveProficiency[2] = 0;
            } else {
                return false;
            }
            if (replacedMoveId != 0) {
                queueMoveReplacementEvent(teamSlot, replacedMoveId, learnedMoveId);
            }
            applied = true;
        }

        clearPendingMoveLearn();
        if (species) queueNextPendingMove(mon, *species, teamSlot, nextCursor);
    } else {
        clearPendingMoveLearn();
    }
    markDirty(SaveUrgency::IMMEDIATE);
    return applied;
}

uint8_t GameEngine::pendingMoveReplacementSlot() const {
    return hasPendingMoveReplacement()
        ? moveReplacementEvents[moveReplacementEventHead].teamSlot
        : 0;
}

Game::MoveId GameEngine::pendingMoveReplacementOldId() const {
    return hasPendingMoveReplacement()
        ? moveReplacementEvents[moveReplacementEventHead].oldMoveId
        : 0;
}

Game::MoveId GameEngine::pendingMoveReplacementNewId() const {
    return hasPendingMoveReplacement()
        ? moveReplacementEvents[moveReplacementEventHead].newMoveId
        : 0;
}

bool GameEngine::acknowledgePendingMoveReplacement() {
    if (!hasPendingMoveReplacement()) return false;
    moveReplacementEventHead =
        (moveReplacementEventHead + 1) % MOVE_REPLACEMENT_EVENT_CAP;
    --moveReplacementEventCount;
    return true;
}

void GameEngine::queueMoveReplacementEvent(uint8_t teamSlot,
                                           Game::MoveId oldMoveId,
                                           Game::MoveId newMoveId) {
    if (moveReplacementEventCount >= MOVE_REPLACEMENT_EVENT_CAP) {
        Platform::logf("[MoveLearn] replacement queue full; dropped slot=%u %u->%u\n",
                      teamSlot, oldMoveId, newMoveId);
        return;
    }
    uint8_t index = (moveReplacementEventHead + moveReplacementEventCount) %
                    MOVE_REPLACEMENT_EVENT_CAP;
    moveReplacementEvents[index].teamSlot = teamSlot;
    moveReplacementEvents[index].oldMoveId = oldMoveId;
    moveReplacementEvents[index].newMoveId = newMoveId;
    ++moveReplacementEventCount;
}

uint8_t GameEngine::pendingEvolutionSlot() const {
    return hasPendingEvolution() ? evolutionEvents[evolutionEventHead].teamSlot : 0;
}

uint16_t GameEngine::pendingEvolutionFromSpeciesId() const {
    return hasPendingEvolution() ? evolutionEvents[evolutionEventHead].fromSpeciesId : 0;
}

uint16_t GameEngine::pendingEvolutionToSpeciesId() const {
    return hasPendingEvolution() ? evolutionEvents[evolutionEventHead].toSpeciesId : 0;
}

bool GameEngine::acknowledgePendingEvolution() {
    if (!hasPendingEvolution()) return false;
    const EvolutionEvent event = evolutionEvents[evolutionEventHead];
    if (event.teamSlot >= state.teamCount ||
        event.teamSlot >= Game::TEAM_CAP) {
        evolutionEventHead =
            (evolutionEventHead + 1) % EVOLUTION_EVENT_CAP;
        --evolutionEventCount;
        return false;
    }

    Game::MonsterRuntime& mon = state.team[event.teamSlot];
    const Species* target = findSpecies(event.toSpeciesId);
    if (!target || mon.speciesId != event.fromSpeciesId) {
        evolutionEventHead =
            (evolutionEventHead + 1) % EVOLUTION_EVENT_CAP;
        --evolutionEventCount;
        return false;
    }
    if (event.consumedItem != Game::ItemId::COUNT &&
        !removeItem(event.consumedItem, 1, SaveUrgency::SOON)) {
        return false;
    }

    uint16_t oldHpMax = mon.hpMax;
    bool canReceiveHpGain = !mon.fainted && mon.hpCur > 0;
    mon.speciesId = target->id;
    sanitizeMonsterMovesForSpecies(mon, *target);
    mon.hpMax = maxHpFor(*target, mon);
    if (canReceiveHpGain && mon.hpMax > oldHpMax) {
        mon.hpCur = MathUtil::min<uint16_t>(
            mon.hpMax, mon.hpCur + (mon.hpMax - oldHpMax));
    } else {
        mon.hpCur = MathUtil::min<uint16_t>(mon.hpCur, mon.hpMax);
    }

    evolutionEventHead = (evolutionEventHead + 1) % EVOLUTION_EVENT_CAP;
    --evolutionEventCount;

    bool nextEvolutionQueued = applyLevelUpEvolutions(
        mon, event.teamSlot, true, event.oldLevel);
    if (!nextEvolutionQueued) {
        queueMoveLearnIfReady(
            mon, speciesFor(mon), event.oldLevel, event.teamSlot);
    }
    if (event.teamSlot == 0) clearMainSceneViewState();
    syncSpriteCache();
    markDirty(SaveUrgency::IMMEDIATE);
    Platform::logf(
        "[Evolution] committed slot=%u species=%u->%u level=%u hp=%u/%u\n",
        event.teamSlot, event.fromSpeciesId, event.toSpeciesId,
        mon.level, mon.hpCur, mon.hpMax);
    return true;
}

bool GameEngine::cancelPendingEvolution() {
    if (!hasPendingEvolution()) return false;
    const EvolutionEvent event = evolutionEvents[evolutionEventHead];
    evolutionEventHead = (evolutionEventHead + 1) % EVOLUTION_EVENT_CAP;
    --evolutionEventCount;
    bool validTeamSlot = event.teamSlot < state.teamCount &&
                         event.teamSlot < Game::TEAM_CAP;
    if (event.consumedItem == Game::ItemId::COUNT && validTeamSlot) {
        Game::MonsterRuntime& mon = state.team[event.teamSlot];
        queueMoveLearnIfReady(
            mon, speciesFor(mon), event.oldLevel, event.teamSlot);
        markDirty(SaveUrgency::IMMEDIATE);
    }
    Platform::logf(
        "[Evolution] cancelled slot=%u species=%u retained level=%u hp=%u/%u\n",
        event.teamSlot, event.fromSpeciesId,
        validTeamSlot ? state.team[event.teamSlot].level : 0,
        validTeamSlot ? state.team[event.teamSlot].hpCur : 0,
        validTeamSlot ? state.team[event.teamSlot].hpMax : 0);
    return true;
}

bool GameEngine::queueEvolutionEvent(
    uint8_t teamSlot, uint16_t fromSpeciesId,
    uint16_t toSpeciesId,
    uint8_t oldLevel,
    Game::ItemId consumedItem) {
    if (evolutionEventCount >= EVOLUTION_EVENT_CAP) {
        Platform::logf("[Evolution] notification queue full; dropped slot=%u %u->%u\n",
                      teamSlot, fromSpeciesId, toSpeciesId);
        return false;
    }
    for (uint8_t offset = 0; offset < evolutionEventCount; ++offset) {
        uint8_t queuedIndex =
            (evolutionEventHead + offset) % EVOLUTION_EVENT_CAP;
        if (evolutionEvents[queuedIndex].teamSlot == teamSlot) return false;
    }
    uint8_t index = (evolutionEventHead + evolutionEventCount) % EVOLUTION_EVENT_CAP;
    EvolutionEvent& event = evolutionEvents[index];
    event = {};
    event.teamSlot = teamSlot;
    event.oldLevel = oldLevel;
    event.fromSpeciesId = fromSpeciesId;
    event.toSpeciesId = toSpeciesId;
    event.consumedItem = consumedItem;
    ++evolutionEventCount;
    return true;
}

void GameEngine::removeEvolutionEventsForSlot(uint8_t teamSlot) {
    EvolutionEvent kept[EVOLUTION_EVENT_CAP] = {};
    uint8_t keptCount = 0;
    for (uint8_t offset = 0; offset < evolutionEventCount; ++offset) {
        uint8_t index =
            (evolutionEventHead + offset) % EVOLUTION_EVENT_CAP;
        if (evolutionEvents[index].teamSlot == teamSlot) continue;
        kept[keptCount++] = evolutionEvents[index];
    }
    memset(evolutionEvents, 0, sizeof(evolutionEvents));
    memcpy(evolutionEvents, kept, sizeof(EvolutionEvent) * keptCount);
    evolutionEventHead = 0;
    evolutionEventCount = keptCount;
}

void GameEngine::removeMoveReplacementEventsForSlot(uint8_t teamSlot) {
    MoveReplacementEvent kept[MOVE_REPLACEMENT_EVENT_CAP] = {};
    uint8_t keptCount = 0;
    for (uint8_t offset = 0; offset < moveReplacementEventCount; ++offset) {
        uint8_t index =
            (moveReplacementEventHead + offset) %
            MOVE_REPLACEMENT_EVENT_CAP;
        if (moveReplacementEvents[index].teamSlot == teamSlot) continue;
        kept[keptCount++] = moveReplacementEvents[index];
    }
    memset(moveReplacementEvents, 0, sizeof(moveReplacementEvents));
    memcpy(moveReplacementEvents, kept,
           sizeof(MoveReplacementEvent) * keptCount);
    moveReplacementEventHead = 0;
    moveReplacementEventCount = keptCount;
}

void GameEngine::clearPendingMoveLearn() {
    state.pendingMoveLearn = false;
    state.pendingMoveSlot = 0;
    state.pendingMoveId = 0;
    state.pendingMoveCursor = 0;
}

bool GameEngine::applyLevelUpEvolutions(Game::MonsterRuntime& mon, uint8_t teamSlot,
                                        bool notify, uint8_t oldLevel) {
    const Species& from = speciesFor(mon);
    const Species* target = levelUpEvolutionTarget(from, mon);
    if (!target) return false;
    if (!notify || teamSlot >= Game::TEAM_CAP) return false;
    return queueEvolutionEvent(
        teamSlot, mon.speciesId, target->id, oldLevel);
}

uint32_t GameEngine::applyFaintPenaltyToTeamMember(uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return 0;
    syncGameClock(Hal::ins().millis());
    Game::MonsterRuntime& mon = state.team[teamSlot];
    const Species& species = speciesFor(mon);
    uint32_t levelFloor = minimumExpForLevel(species.growthRate, mon.level);
    uint32_t availableLoss = mon.exp > levelFloor ? mon.exp - levelFloor : 0;
    uint32_t loss = mon.exp == 0 ? 0 : MathUtil::max<uint32_t>(1, mon.exp / 10);
    if (loss > availableLoss) loss = availableLoss;
    mon.exp -= loss;
    mon.hpCur = 0;
    mon.fainted = true;
    mon.lastSeenAt = gameSecondsForMinutes(state.gameMinutesTotal);
    mon.affection = mon.affection > 5 ? mon.affection - 5 : 0;
    mon.mood = mon.mood > 10 ? mon.mood - 10 : 0;
    mon.bond = Game::Bond::decrease(mon.bond, Game::Bond::FAINT_LOSS);
    markDirty(SaveUrgency::IMMEDIATE);
    return loss;
}

uint8_t GameEngine::grantAdventureBond(uint16_t steps) {
    uint8_t gain = Game::Bond::adventureGain(steps);
    if (gain == 0) return 0;

    bool changed = false;
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& mon = state.team[slot];
        bool localCompanion =
            mon.origin == Game::Origin::VISITOR &&
            contactVisit.active && contactVisit.exploring;
        if ((mon.origin == Game::Origin::VISITOR && !localCompanion) ||
            mon.fainted || mon.hpCur == 0) {
            continue;
        }
        Game::Bond::Value next = Game::Bond::increase(mon.bond, gain);
        if (next == mon.bond) continue;
        mon.bond = next;
        changed = true;
    }
    if (changed) markDirty(SaveUrgency::IMMEDIATE);
    return changed ? gain : 0;
}

void GameEngine::addWalkSteps(uint16_t steps) {
    syncGameClock(Hal::ins().millis());
    resetDailyCountersIfNeeded();
    state.stepsToday = (uint16_t)MathUtil::min<uint32_t>(60000, (uint32_t)state.stepsToday + steps);
    uint16_t earnedWalkExp = MathUtil::min<uint16_t>(50, state.stepsToday / 100);
    if (earnedWalkExp > state.walkExpToday) {
        uint16_t expGain = earnedWalkExp - state.walkExpToday;
        state.walkExpToday = earnedWalkExp;
        addExperience(expGain);
    }
    markDirty(SaveUrgency::DEFERRED);
}

void GameEngine::debugRecoverTeam() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (state.teamCount == 0) return;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP;
         ++slot) {
        Game::MonsterRuntime& mon = state.team[slot];
        mon.fainted = false;
        mon.majorStatus = Game::MajorStatus::NONE;
        mon.majorStatusTurns = 0;
        mon.hpCur = mon.hpMax;
        mon.satiety = 100;
        mon.mood = 100;
    }
    markDirty(SaveUrgency::SOON);
#endif
}

bool GameEngine::debugLevelUpActiveMonster() {
#if !STICKMON_ENABLE_DEBUG_FEATURES
    return false;
#else
    if (state.teamCount == 0 || activeMonster().level >= Game::LEVEL_MAX) {
        return false;
    }

    const Game::MonsterRuntime& mon = activeMonster();
    const Species& species = speciesFor(mon);
    uint32_t targetExp = minimumExpForLevel(
        species.growthRate, static_cast<uint8_t>(mon.level + 1));
    uint32_t requiredExp = targetExp > mon.exp ? targetExp - mon.exp : 1;
    uint8_t oldLevel = mon.level;
    addExperience(requiredExp);
    return activeMonster().level > oldLevel;
#endif
}

bool GameEngine::debugSetActiveSpecies(uint16_t speciesId) {
#if !STICKMON_ENABLE_DEBUG_FEATURES
    (void)speciesId;
    return false;
#else
    const Species* species = findSpecies(speciesId);
    if (!species) return false;

    uint8_t level = state.teamCount > 0 ? activeMonster().level : 5;
    Game::MonsterRuntime mon = createMonster(species->id, level);
    mon.origin = state.teamCount > 0 ? activeMonster().origin : Game::Origin::STARTER;
    if (state.teamCount == 0) state.teamCount = 1;
    state.team[0] = mon;
    state.activeSlot = 0;
    clearMainSceneViewState();
    syncSpriteCache();
    markDirty(SaveUrgency::SOON);
    return true;
#endif
}

uint32_t GameEngine::debugAdvanceToTimeOfDay(uint16_t targetMinutesOfDay) {
#if !STICKMON_ENABLE_DEBUG_FEATURES
    (void)targetMinutesOfDay;
    return 0;
#else
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);

    targetMinutesOfDay %= (24U * 60U);
    uint16_t current = (uint16_t)(state.gameMinutesTotal % (24UL * 60UL));
    uint32_t delta = targetMinutesOfDay >= current
        ? (uint32_t)(targetMinutesOfDay - current)
        : (uint32_t)(24U * 60U - current + targetMinutesOfDay);

    state.gameMinutesTotal += delta;
    gameClock.set(now, state.gameMinutesTotal);
    saveNow();
    return delta;
#endif
}

const char* GameEngine::debugLightSourceLabel() const {
#if STICKMON_ENABLE_DEBUG_FEATURES
    uint8_t index = debugLightSource;
    if (index >= DEBUG_LIGHT_SOURCE_COUNT) index = 0;
    return Ui::Debug::LIGHT_SOURCE_ITEMS[index];
#else
    return "";
#endif
}

void GameEngine::cycleDebugLightSource() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugLightSource = (uint8_t)((debugLightSource + 1) % DEBUG_LIGHT_SOURCE_COUNT);
#endif
}

void GameEngine::wakeFromIdle() {
    resetIdle(Hal::ins().millis());
}

void GameEngine::invalidateScene() {
    sceneDirty = true;
#if STICKMON_ENABLE_RENDER_STATS
    ++renderStatsStateWakes;
#endif
}

void GameEngine::markSaveDirty(SaveUrgency urgency) {
    uint32_t now = Hal::ins().millis();
    saveCoordinator.mark(
        now,
        urgency == SaveUrgency::DEFERRED
            ? SaveCoordinator::Priority::DEFERRED
            : SaveCoordinator::Priority::SOON);
    if (urgency == SaveUrgency::IMMEDIATE) saveNow();
}

void GameEngine::markDirty(SaveUrgency urgency) {
    invalidateScene();
    markSaveDirty(urgency);
}

void GameEngine::enterDeepSleep() {
    saveNow();
#if defined(__EMSCRIPTEN__)
    // Web 模拟器没有深睡：平台的 enterDeepSleep 是 no-op，继续往下会
    // 坠入"等待松手"（JS keyup 事件无法派进被占用的 wasm 主线程，永远
    // 检测不到释放）和 while(true) 永久睡眠两个死循环，阻塞 Emscripten
    // 主循环导致页面冻结崩溃。存档后直接返回，模拟器照常运行。
    // 此兜底覆盖所有可能触达本函数的路径（如用户自行设置了休眠超时）。
    Platform::logLine("[Power] web simulator: deep sleep skipped");
    return;
#endif
    VoiceCallService::ins().stopListening();
    Hal::ins().stopAudio();
    Hal::ins().setBrightness(0);
    Platform::display().sleep();

    Platform::logLine("[Power] Long-press B: waiting for release");
    Platform::logger().flush();

    // GPIO12 is active-low. Arming it while B is still held would wake the
    // device immediately, so wait until the long-press has been released.
    do {
        Platform::input().update();
        Platform::clock().sleepMs(10);
    } while (Platform::input().pressed(Platform::InputButton::SECONDARY));

    Platform::logLine("[Power] entering deep sleep; wake=B/timer");
    Platform::logger().flush();
    Platform::power().enterDeepSleep(
        SILENT_CARE_WAKE_INTERVAL_US, true);
    while (true) {
        Platform::clock().sleepMs(1000);
    }
}

void GameEngine::runSilentCareWake() {
    // 定时静默唤醒路径：只初始化串口与 SaveManager，不初始化 Hal/显示/
    // 资源/场景，跑一遍照护逻辑并写档后立即重新武装定时器回到深睡。
    // 本函数不返回。
    bool storageReady = saveManager.begin();
    bool normalized = false;
    bool loaded = storageReady &&
                  saveManager.load(state, mainViewState, &normalized);
    if (loaded) {
        // 深睡期间游戏时钟不走，这里一次性补上这段流逝的游戏分钟。
        state.gameMinutesTotal +=
            (uint32_t)(SILENT_CARE_WAKE_MINUTES * gameSpeed());
        // 会话累加器每次静默唤醒从 0 开始：单块内的间隔取整有轻微近似，
        // 不影响长期行为。
        Game::CareTickAccumulators acc{};
        Game::resetDailyCareCounters(state);
        uint8_t revivals = Game::applyCareMinutes(
            state, acc, SILENT_CARE_WAKE_MINUTES, gameSpeed(), true);
        saveManager.saveSnapshot(state, mainViewState);
        Platform::logf("[Power] silent care wake: elapsed=%umin revivals=%u\n",
                      (unsigned)SILENT_CARE_WAKE_MINUTES, revivals);
    } else {
        Platform::logLine("[Power] silent care wake: no save loaded, skip care");
    }
    Platform::logger().flush();
    Platform::power().enterDeepSleep(
        SILENT_CARE_WAKE_INTERVAL_US, true);
    while (true) {
        Platform::clock().sleepMs(1000);
    }
}

bool GameEngine::saveNow() {
    uint32_t now = Hal::ins().millis();
    if (currentScene) currentScene->onBeforeSave();
    syncGameClock(now);
    if (syncOwnedSpeciesToEncounterHistory()) encounterHistoryDirty = true;
    bool stateOk = saveManager.saveSnapshot(state, mainViewState);
    bool historyOk = !encounterHistoryDirty ||
        saveManager.saveEncounterHistory(encounterHistory);
    bool ok = stateOk && historyOk;
    if (historyOk) encounterHistoryDirty = false;
    saveCoordinator.recordAttempt(now, ok);
    return ok;
}

bool GameEngine::resetGame() {
    if (!saveManager.clearAll()) {
        Platform::logLine("[GameEngine] failed to clear game data");
        return false;
    }
    VoiceCallService::ins().clearCachedProfile();

    uint32_t now = Hal::ins().millis();
    initDefaultState();
    gameClock.set(now, state.gameMinutesTotal);
    encounterHistory.clear();
    encounterHistoryDirty = false;
    HatchScene::clearRuntimeProgress();
    clearMainSceneViewState();
    careAcc = Game::CareTickAccumulators{};
    debugShowWalkBoundary = false;
    debugTiltControl = false;
    debugShowBattleDrawBounds = false;
    debugLightSource = 0;
    saveCoordinator.reset(now);

    lastCareMs = now;
    lastUpdateMs = now;
    resetIdle(now);
    Hal::ins().setBrightness(state.settings.brightness);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    syncSpriteCache();

    if (!saveNow()) {
        Platform::logLine("[GameEngine] reset completed but default save failed");
    }
    return true;
}

bool GameEngine::hasEncounteredSpecies(uint16_t speciesId) const {
    if (encounterHistory.contains(speciesId)) return true;
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        if (state.team[i].speciesId == speciesId) return true;
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        if (state.storage[i].speciesId == speciesId) return true;
    }
    return false;
}

bool GameEngine::recordEncounteredSpecies(uint16_t speciesId) {
    if (!encounterHistory.add(speciesId)) return false;
    encounterHistoryDirty = true;
    Platform::logf("[EncounterHistory] unlocked species=%u total=%u\n",
                  speciesId, encounterHistory.count);
    markDirty(SaveUrgency::SOON);
    return true;
}

bool GameEngine::syncOwnedSpeciesToEncounterHistory() {
    bool changed = false;
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        changed |= encounterHistory.add(state.team[i].speciesId);
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        changed |= encounterHistory.add(state.storage[i].speciesId);
    }
    return changed;
}

bool GameEngine::loadHatchProgress(Game::HatchProgress& progress) {
    return saveManager.loadHatchProgress(progress);
}

bool GameEngine::saveHatchProgress(const Game::HatchProgress& progress) {
    return saveManager.saveHatchProgress(progress);
}

void GameEngine::clearHatchProgress() {
    saveManager.clearHatchProgress();
}

void GameEngine::switchScene(SceneID id, bool saveBeforeSwitch) {
    uint32_t now = Hal::ins().millis();
    SceneID fromId = currentId;
    resetIdle(now);
    if (saveBeforeSwitch && saveCoordinator.dirty() && !saveNow()) {
        Platform::logLine("[GameEngine] save before scene switch failed");
    }
    if (currentScene) {
        currentScene->onExit();
        currentScene.reset();
    }

    prevId = currentId;
    SceneID actualId = id;
    currentScene.reset(allocateScene(actualId));
    if (!currentScene) {
        SceneID fallback = homeScene();
        Platform::logf("[GameEngine] scene allocation failed id=%u fallback=%u\n",
                      static_cast<unsigned>(id),
                      static_cast<unsigned>(fallback));
        actualId = fallback;
        currentScene.reset(allocateScene(actualId));
    }
    if (!currentScene) {
        Platform::logLine("[GameEngine] fatal scene allocation failure; restarting");
        Platform::clock().sleepMs(50);
        Platform::power().restart();
        return;
    }
    currentId = actualId;
    currentScene->onEnter();
    uint32_t enteredAt = Hal::ins().millis();
    lastSceneUpdateMs = enteredAt;
    sceneDirty = true;
    scheduleSceneUpdate(enteredAt);
#if STICKMON_ENABLE_RENDER_STATS
    ++renderStatsSceneSwitches;
    lastLoggedDemandMode = 0xFF;
    STICKMON_RENDER_STATSF(
        "[RenderScene] t=%lu from=%s to=%s enter_ms=%lu\n",
        static_cast<unsigned long>(enteredAt),
        sceneLogName(fromId), sceneLogName(currentId),
        static_cast<unsigned long>(enteredAt - now));
#endif
}

void GameEngine::scheduleSceneUpdate(uint32_t nowMs) {
    nextSceneUpdateMs = nowMs;
    sceneUpdateScheduled = true;
}

void GameEngine::processInput(uint32_t nowMs) {
    bool rawInputActive =
        Platform::input().pressed(Platform::InputButton::PRIMARY) ||
        Platform::input().pressed(Platform::InputButton::SECONDARY);
    if (rawInputActive) resetIdle(nowMs);

    ButtonEvent events[ButtonDispatcher::MAX_EVENTS_PER_POLL];
    uint8_t count = ButtonDispatcher::ins().poll(events, ButtonDispatcher::MAX_EVENTS_PER_POLL);
    if (count > 0) resetIdle(nowMs);
    if (sceneFade != SceneFadePhase::NONE) return;
    if (resourceAlertVisible()) return;
    for (uint8_t i = 0; i < count; ++i) {
        const ButtonEvent& event = events[i];
        if (event.btn == 0 && event.action == BtnAction::LONG_PRESS &&
            supportsLongPressHome(currentId)) {
            requestScene(homeScene());
            return;
        }
        if (currentScene && currentScene->onButton(event)) {
            sceneDirty = true;
            scheduleSceneUpdate(nowMs);
#if STICKMON_ENABLE_RENDER_STATS
            ++renderStatsInputWakes;
#endif
            continue;
        }
        if (currentId == SceneID::MAIN && event.btn == 1 &&
            event.action == BtnAction::LONG_PRESS) {
            // Web 模拟器仅屏蔽主菜单长按 B 进休眠这一条路径（Web 无深睡
            // 概念，且 enterDeepSleep 在 Web 下会坠入死循环卡死页面，详见
            // 其 Emscripten 分支）；各场景内的长按 B 快捷操作正常响应。
            // 真机行为不变：主菜单长按 B 进深睡。
#if !defined(__EMSCRIPTEN__)
            enterDeepSleep();
            return;
#endif
        }
    }
}

void GameEngine::update(uint32_t nowMs) {
    lastUpdateMs = nowMs;
#if STICKMON_ENABLE_RENDER_STATS
    ++renderStatsCoreUpdates;
#endif
    CryPlayer::ins().update();
    bool alertVisible = resourceAlertVisible();
    if (alertVisible != resourceAlertWasVisible) {
        resourceAlertWasVisible = alertVisible;
        invalidateScene();
        if (!alertVisible) scheduleSceneUpdate(nowMs);
    }
    if (alertVisible) {
        sceneUpdateScheduled = false;
        gameClock.set(nowMs, state.gameMinutesTotal);
        lastCareMs = nowMs;
        return;
    }
    syncGameClock(nowMs);
    bool spriteCacheChanged = false;
    if (startupFirstFrameRendered && !startupSpriteCacheReady) {
        startupSpriteCacheReady = syncSpriteCache(1, &spriteCacheChanged);
        if (startupSpriteCacheReady) {
            PokemonSprites::setDynamicLoadingEnabled(true);
            Platform::logf("[BootTiming] sprites_ready_ms=%u\n", nowMs - bootStartedMs);
        }
    } else {
        syncSpriteCache(startupFirstFrameRendered ? 0xFF : 0,
                        &spriteCacheChanged);
    }
    if (spriteCacheChanged) {
        invalidateScene();
        scheduleSceneUpdate(nowMs);
        Platform::logf("[SpriteCache] scene wake t=%u ready=%u\n",
                      nowMs, startupSpriteCacheReady ? 1 : 0);
    }
    if (mainSceneFirstFrameRendered && startupSpriteCacheReady &&
        currentId == SceneID::MAIN &&
        static_cast<int32_t>(nowMs - nextExplorePoolPreloadMs) >= 0) {
        bool ready = ExploreScene::serviceAreaPoolCache(1);
        nextExplorePoolPreloadMs = nowMs + (ready ? 1000UL : 250UL);
    }
    tickCare(nowMs);
    if (visitSession.active) updateVisit(nowMs);
    bool sceneUpdateDue =
        sceneUpdateScheduled &&
        static_cast<int32_t>(nowMs - nextSceneUpdateMs) >= 0;
    if (currentScene && sceneFade != SceneFadePhase::HOLD && sceneUpdateDue) {
        Scene* updatingScene = currentScene.get();
        float dt = (nowMs - lastSceneUpdateMs) / 1000.0f;
        lastSceneUpdateMs = nowMs;
        sceneUpdateScheduled = false;
        SceneUpdateResult result = updatingScene->update(nowMs, dt);
        if (currentScene.get() == updatingScene) {
#if STICKMON_ENABLE_RENDER_STATS
            ++renderStatsSceneUpdates;
            if (result.redraw) ++renderStatsRedrawRequests;
            logSceneDemand(result, nowMs);
#endif
            sceneDirty = sceneDirty || result.redraw;
            if (result.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE) {
                nextSceneUpdateMs = nowMs + MathUtil::max<uint32_t>(
                    1, result.nextUpdateDelayMs);
                sceneUpdateScheduled = true;
            }
        }
    }
    SceneFadePhase fadeBefore = sceneFade;
    uint16_t fadeProgressBefore = sceneFadeProgressMs;
    updateSceneFade(nowMs);
    if (sceneFade != SceneFadePhase::NONE &&
        (sceneFade != fadeBefore || sceneFadeProgressMs != fadeProgressBefore)) {
        sceneDirty = true;
    } else if (fadeBefore != sceneFade) {
        sceneDirty = true;
    }
    if (saveCoordinator.due(nowMs)) saveNow();
}

void GameEngine::logSceneDemand(const SceneUpdateResult& result,
                                uint32_t nowMs) {
#if STICKMON_ENABLE_RENDER_STATS
    uint8_t mode = 0;
    const char* modeName = "parked";
    if (result.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE) {
        if (result.nextUpdateDelayMs <= FRAME_MS) {
            mode = 2;
            modeName = "animate";
        } else {
            mode = 1;
            modeName = "timer";
        }
    }
    if (mode == lastLoggedDemandMode && currentId == lastLoggedDemandScene) {
        return;
    }
    lastLoggedDemandMode = mode;
    lastLoggedDemandScene = currentId;
    if (result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE) {
        STICKMON_RENDER_STATSF(
            "[RenderDemand] t=%lu scene=%s mode=%s redraw=%u next=none\n",
            static_cast<unsigned long>(nowMs), sceneLogName(currentId),
            modeName, result.redraw ? 1 : 0);
    } else {
        STICKMON_RENDER_STATSF(
            "[RenderDemand] t=%lu scene=%s mode=%s redraw=%u next_ms=%lu\n",
            static_cast<unsigned long>(nowMs), sceneLogName(currentId),
            modeName, result.redraw ? 1 : 0,
            static_cast<unsigned long>(result.nextUpdateDelayMs));
    }
#else
    (void)result;
    (void)nowMs;
#endif
}

void GameEngine::emitRenderStats(uint32_t nowMs) {
#if STICKMON_ENABLE_RENDER_STATS
    uint32_t elapsed = nowMs - renderStatsStartedMs;
    if (elapsed < RENDER_STATS_INTERVAL_MS) return;

    uint32_t fixedFrames = renderStatsCoreUpdates;
    uint32_t avoided = fixedFrames > renderStatsFlushes
        ? fixedFrames - renderStatsFlushes : 0;
    uint32_t avoidedPermille = fixedFrames > 0
        ? avoided * 1000UL / fixedFrames : 0;
    uint32_t actualKb =
        renderStatsFlushes * FULL_FRAME_BYTES / 1024UL;
    uint32_t savedKb = avoided * FULL_FRAME_BYTES / 1024UL;
    uint32_t avgDrawUs = renderStatsFlushes > 0
        ? renderStatsDrawUs / renderStatsFlushes : 0;
    uint32_t avgFlushUs = renderStatsFlushes > 0
        ? renderStatsFlushUs / renderStatsFlushes : 0;
    uint32_t displayUs = renderStatsDrawUs + renderStatsFlushUs;
    uint32_t displayDutyPermille =
        elapsed > 0 ? displayUs / elapsed : 0;

    STICKMON_RENDER_STATSF(
        "[RenderStats] win_ms=%lu scene=%s idle=%u baseline_flush=%lu "
        "scene_updates=%lu redraw_req=%lu actual_flush=%lu "
        "avoided=%lu/%lu(%lu.%lu%%) input=%lu state=%lu switches=%lu "
        "spi_actual_kb=%lu spi_saved_kb=%lu "
        "display_cpu_ms=%lu display_duty=%lu.%lu%% "
        "draw_avg_us=%lu draw_max_us=%lu "
        "flush_avg_us=%lu flush_max_us=%lu\n",
        static_cast<unsigned long>(elapsed), sceneLogName(currentId),
        idleActive ? 1 : 0,
        static_cast<unsigned long>(fixedFrames),
        static_cast<unsigned long>(renderStatsSceneUpdates),
        static_cast<unsigned long>(renderStatsRedrawRequests),
        static_cast<unsigned long>(renderStatsFlushes),
        static_cast<unsigned long>(avoided),
        static_cast<unsigned long>(fixedFrames),
        static_cast<unsigned long>(avoidedPermille / 10),
        static_cast<unsigned long>(avoidedPermille % 10),
        static_cast<unsigned long>(renderStatsInputWakes),
        static_cast<unsigned long>(renderStatsStateWakes),
        static_cast<unsigned long>(renderStatsSceneSwitches),
        static_cast<unsigned long>(actualKb),
        static_cast<unsigned long>(savedKb),
        static_cast<unsigned long>(displayUs / 1000UL),
        static_cast<unsigned long>(displayDutyPermille / 10),
        static_cast<unsigned long>(displayDutyPermille % 10),
        static_cast<unsigned long>(avgDrawUs),
        static_cast<unsigned long>(renderStatsMaxDrawUs),
        static_cast<unsigned long>(avgFlushUs),
        static_cast<unsigned long>(renderStatsMaxFlushUs));

    renderStatsStartedMs = nowMs;
    renderStatsCoreUpdates = 0;
    renderStatsSceneUpdates = 0;
    renderStatsRedrawRequests = 0;
    renderStatsFlushes = 0;
    renderStatsInputWakes = 0;
    renderStatsStateWakes = 0;
    renderStatsSceneSwitches = 0;
    renderStatsDrawUs = 0;
    renderStatsFlushUs = 0;
    renderStatsMaxDrawUs = 0;
    renderStatsMaxFlushUs = 0;
#else
    (void)nowMs;
#endif
}

void GameEngine::updateSceneFade(uint32_t nowMs) {
    if (sceneFade == SceneFadePhase::NONE) return;
    if (sceneFade == SceneFadePhase::HOLD) {
        uint32_t elapsed = nowMs >= sceneFadeStartedMs ? nowMs - sceneFadeStartedMs : 0;
        if (elapsed < SCENE_FADE_HOLD_MS) return;

        switchScene(sceneFadeTarget);
        sceneFade = SceneFadePhase::IN;
        sceneFadeStartedMs = Hal::ins().millis();
        sceneFadeLastStepMs = sceneFadeStartedMs;
        sceneFadeProgressMs = 0;
        return;
    }

    uint32_t stepMs = 0;
    if (nowMs >= sceneFadeLastStepMs) {
        stepMs = MathUtil::min<uint32_t>(FRAME_MS, nowMs - sceneFadeLastStepMs);
        sceneFadeLastStepMs = nowMs;
    }
    sceneFadeProgressMs = static_cast<uint16_t>(MathUtil::min<uint32_t>(
        sceneFadeDurationMs,
        static_cast<uint32_t>(sceneFadeProgressMs) + stepMs));
    if (sceneFadeProgressMs < sceneFadeDurationMs) return;

    if (sceneFade == SceneFadePhase::OUT) {
        sceneFade = SceneFadePhase::HOLD;
        sceneFadeStartedMs = Hal::ins().millis();
        return;
    }
    sceneFade = SceneFadePhase::NONE;
}

void GameEngine::render(uint32_t nowMs) {
#if STICKMON_ENABLE_RENDER_STATS
    uint32_t drawStartedUs = Platform::clock().micros();
#else
    (void)nowMs;
#endif
    PokemonSprites::beginRenderFrame();
    if (currentScene) currentScene->render();
    renderResourceAlert();
    renderSceneFade(Hal::ins().millis());
#if STICKMON_ENABLE_RENDER_STATS
    uint32_t flushStartedUs = Platform::clock().micros();
#endif
    Hal::ins().flush();
    if (currentId == SceneID::MAIN) {
        mainSceneFirstFrameRendered = true;
    }
#if STICKMON_ENABLE_RENDER_STATS
    uint32_t flushedUs = Platform::clock().micros();
    uint32_t drawUs = flushStartedUs - drawStartedUs;
    uint32_t flushUs = flushedUs - flushStartedUs;
    ++renderStatsFlushes;
    renderStatsDrawUs += drawUs;
    renderStatsFlushUs += flushUs;
    renderStatsMaxDrawUs = MathUtil::max(renderStatsMaxDrawUs, drawUs);
    renderStatsMaxFlushUs = MathUtil::max(renderStatsMaxFlushUs, flushUs);
#endif
    if (!startupFirstFrameRendered) {
        startupFirstFrameRendered = true;
        if (startupSpriteCacheReady) PokemonSprites::setDynamicLoadingEnabled(true);
        Platform::logf("[BootTiming] first_frame_ms=%u sprites_ready=%u\n",
                      Hal::ins().millis() - bootStartedMs, startupSpriteCacheReady ? 1 : 0);
    }
}

void GameEngine::renderSceneFade(uint32_t nowMs) {
    (void)nowMs;
    if (sceneFade == SceneFadePhase::NONE) return;
    if (sceneFade == SceneFadePhase::HOLD) {
        PixelRenderer::darken(255);
        return;
    }
    uint32_t progress = MathUtil::min<uint32_t>(sceneFadeDurationMs, sceneFadeProgressMs);
    uint8_t amount = sceneFade == SceneFadePhase::OUT
        ? (uint8_t)(progress * 255UL / sceneFadeDurationMs)
        : (uint8_t)(255UL - progress * 255UL / sceneFadeDurationMs);
    PixelRenderer::darken(amount);
}

bool GameEngine::resourceAlertVisible() const {
    if (!FontResource::ins().loaded()) return true;
    if (RoomResource::ins().missing()) return true;
    return PokemonSprites::cacheStats().missingSpecies > 0;
}

void GameEngine::renderResourceAlert() {
    if (!resourceAlertVisible()) return;

    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 24;
    static constexpr int POP_Y = 34;
    static constexpr int POP_W = 192;
    static constexpr int POP_H = 64;
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);

    uint16_t bg = PixelRenderer::rgb(18, 22, 30);
    uint16_t border = PixelRenderer::rgb(241, 242, 232);
    uint16_t warn = PixelRenderer::rgb(255, 216, 72);
    uint16_t text = PixelRenderer::rgb(245, 246, 232);

    c.fillRect(POP_X, POP_Y, POP_W, POP_H, bg);
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, border);
    PixelRenderer::text(POP_X + 50, POP_Y + 8, Ui::ResourceAlert::TITLE, warn);

    int y = POP_Y + 30;
    if (RoomResource::ins().missing()) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::ROOM_MISSING, text);
        y += 16;
    }

    const auto& spriteStats = PokemonSprites::cacheStats();
    if (spriteStats.missingSpecies > 0) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::SPRITE_MISSING, text);
        if (spriteStats.firstMissingSpecies != 0) {
            char idBuf[12];
            std::snprintf(idBuf, sizeof(idBuf), "#%03u", spriteStats.firstMissingSpecies);
            PixelRenderer::text(POP_X + 130, y, idBuf, warn);
        }
        y += 16;
    }

    if (!FontResource::ins().loaded() &&
        !RoomResource::ins().missing() && spriteStats.missingSpecies == 0) {
        PixelRenderer::text(POP_X + 18, y, Ui::ResourceAlert::FONT_MISSING, text);
    }
}

void GameEngine::resetIdle(uint32_t nowMs) {
    lastActivityMs = nowMs;
    Hal::ins().setIdleBrightness(false);
    if (idleActive) {
        idleActive = false;
    }
}

void GameEngine::updateIdle(uint32_t nowMs) {
    uint32_t timeout = idleTimeoutMs();
    if (timeout == 0 || idleActive) return;
    int32_t elapsed = (int32_t)(nowMs - lastActivityMs);
    if (elapsed < 0 || (uint32_t)elapsed < timeout) return;
    if (saveCoordinator.dirty() && !saveNow()) {
        Platform::logLine("[GameEngine] save before idle failed");
    }
    idleActive = true;
    VoiceCallService::ins().stopListening();
    Hal::ins().setIdleBrightness(true);
}

uint32_t GameEngine::idleTimeoutMs() const {
    static constexpr uint32_t TIMEOUTS[] = {
        30000UL,
        120000UL,
        300000UL,
        600000UL,
        0UL,
    };
    return TIMEOUTS[idleTimeoutIndex()];
}

uint32_t GameEngine::gameMinutesTotalAt(uint32_t nowMs) const {
    return gameClock.minutesAt(nowMs, gameSpeed());
}

void GameEngine::syncGameClock(uint32_t nowMs) {
    if (gameClock.sync(nowMs, gameSpeed(), state.gameMinutesTotal)) {
        markDirty(SaveUrgency::DEFERRED);
    }
}

void GameEngine::resetGameClockAnchor(uint32_t nowMs) {
    syncGameClock(nowMs);
    gameClock.set(nowMs, state.gameMinutesTotal);
}

void GameEngine::initDefaultState() {
    saveManager.reset(state);
    exploreItemEffects.reset();
    contactVisit = ContactVisitSession{};
    evolutionEventHead = 0;
    evolutionEventCount = 0;
    moveReplacementEventHead = 0;
    moveReplacementEventCount = 0;
    state.teamCount = 0;
    state.storageCount = 0;
    state.activeSlot = 0;
    state.gameMinutesTotal = Game::INITIAL_GAME_MINUTES;
    state.careDay = 0;
    state.careExpToday = 0;
    state.bag = Game::BagState{};
    state.bag.potion = 0;
    state.bag.candy = 0;
    state.bag.revive = 0;
    state.room = Game::RoomState{};
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) state.room.food[i] = 0;
    state.coins = 0;
}

bool GameEngine::sanitizeMonsterMovesForSpecies(Game::MonsterRuntime& mon,
                                                const Species& species) {
    bool changed = false;
    Game::MoveId basicMove = basicMoveIdForSpecies(species);
    if (!isBasicFirstMoveForSpecies(species, mon.move1Id)) {
        mon.move1Id = basicMove;
        changed = true;
    }

    if (mon.move2Id != 0 &&
        (!canRetainSpecialMove(species, mon.move2Id, mon.level) ||
         mon.move2Id == mon.move1Id)) {
        mon.move2Id = 0;
        mon.moveProficiency[1] = 0;
        changed = true;
    }
    if (mon.move3Id != 0 &&
        (!canRetainSpecialMove(species, mon.move3Id, mon.level) ||
         mon.move3Id == mon.move1Id || mon.move3Id == mon.move2Id)) {
        mon.move3Id = 0;
        mon.moveProficiency[2] = 0;
        changed = true;
    }
    const Game::MoveId moves[Game::MOVE_SLOT_COUNT] = {
        mon.move1Id,
        mon.move2Id,
        mon.move3Id,
    };
    for (uint8_t slot = 0; slot < Game::MOVE_SLOT_COUNT; ++slot) {
        uint8_t normalized = slot == 0
            ? Game::MOVE_PROFICIENCY_MAX
            : (moves[slot] == 0
                ? 0
                : MathUtil::min<uint8_t>(mon.moveProficiency[slot], Game::MOVE_PROFICIENCY_MAX));
        if (mon.moveProficiency[slot] != normalized) {
            mon.moveProficiency[slot] = normalized;
            changed = true;
        }
    }
    return changed;
}

void GameEngine::sanitizeMonsterMoves() {
    bool changed = false;
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        const Species* species = findSpecies(state.team[i].speciesId);
        if (species) changed |= sanitizeMonsterMovesForSpecies(state.team[i], *species);
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        const Species* species = findSpecies(state.storage[i].speciesId);
        if (species) changed |= sanitizeMonsterMovesForSpecies(state.storage[i], *species);
    }
    if (changed) markDirty(SaveUrgency::DEFERRED);
}

void GameEngine::resetDailyCountersIfNeeded() {
    if (Game::resetDailyCareCounters(state)) markDirty(SaveUrgency::DEFERRED);
}

void GameEngine::grantCareExperience(uint8_t baseAmount, bool weakGain,
                                     uint8_t teamSlot) {
    if (baseAmount == 0 || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return;
    }
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);
    resetDailyCountersIfNeeded();

    Game::MonsterRuntime& mon = state.team[teamSlot];
    uint16_t cap = careDailyCapForLevel(mon.level);
    if (state.careExpToday >= cap) return;

    uint16_t amount = weakGain || mon.level > 15
        ? 1
        : (uint16_t)baseAmount * careExpMultiplierForLevel(mon.level);
    amount = MathUtil::min<uint16_t>(amount, cap - state.careExpToday);
    if (amount == 0) return;

    state.careExpToday += amount;
    addExperienceToTeamMember(teamSlot, amount);
    markDirty(SaveUrgency::DEFERRED);
}

void GameEngine::tickCare(uint32_t nowMs) {
    if (nowMs - lastCareMs < 60000UL) return;
    resetDailyCountersIfNeeded();
    uint32_t elapsedMin = (nowMs - lastCareMs) / 60000UL;
    lastCareMs = nowMs;
    bool homeRecoveryActive = currentId != SceneID::EXPLORE &&
                              exploreTravel == ExploreTravelPhase::NONE;
    // 核心照护逻辑与深度睡眠静默唤醒路径共用（game/CareTicker.h）。
    uint8_t revivals = Game::applyCareMinutes(state, careAcc, elapsedMin,
                                              gameSpeed(), homeRecoveryActive);
    if (revivals > 0) {
        Platform::logf("[Care] faint rest complete for %u monster(s)\n", revivals);
    }
    markDirty(SaveUrgency::DEFERRED);
}

bool GameEngine::syncSpriteCache(uint8_t loadBudget, bool* cacheChanged) {
    uint16_t teamSpecies[Game::TEAM_CAP] = {};
    uint8_t count = state.teamCount;
    if (count > Game::TEAM_CAP) count = Game::TEAM_CAP;
    for (uint8_t i = 0; i < count; ++i) {
        teamSpecies[i] = state.team[i].speciesId;
    }
    return PokemonSprites::syncTeamCache(
        teamSpecies, count, loadBudget, cacheChanged);
}

uint32_t GameEngine::randomIvPacked() const {
    uint32_t packed = 0;
    for (uint8_t i = 0; i < Game::STAT_COUNT; ++i) {
        Game::setIv(packed, i, GameRandom::random(0, Game::IV_MAX + 1));
    }
    return packed;
}

bool GameEngine::queueNextPendingMove(Game::MonsterRuntime& mon, const Species& species,
                                      uint8_t teamSlot, uint16_t startIndex) {
    const uint16_t count = learnsetEntryCountForSpecies(species);
    for (uint16_t index = startIndex; index < count; ++index) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(species, index);
        if (!entry || entry->level > mon.level) break;
        if (!canLearnAsSpecialMove(species, entry->moveId) ||
            mon.move2Id == entry->moveId || mon.move3Id == entry->moveId) {
            continue;
        }
        state.pendingMoveLearn = true;
        state.pendingMoveSlot = teamSlot;
        state.pendingMoveId = entry->moveId;
        state.pendingMoveCursor = index + 1;
        return true;
    }
    state.pendingMoveLearn = false;
    state.pendingMoveSlot = 0;
    state.pendingMoveId = 0;
    state.pendingMoveCursor = 0;
    return false;
}

void GameEngine::queueMoveLearnIfReady(Game::MonsterRuntime& mon, const Species& species,
                                       uint8_t oldLevel, uint8_t teamSlot) {
    if (state.pendingMoveLearn) return;
    uint16_t startIndex = 0;
    const uint16_t count = learnsetEntryCountForSpecies(species);
    while (startIndex < count) {
        const LearnsetEntry* entry = learnsetEntryForSpecies(species, startIndex);
        if (!entry || entry->level > oldLevel) break;
        ++startIndex;
    }
    queueNextPendingMove(mon, species, teamSlot, startIndex);
}
