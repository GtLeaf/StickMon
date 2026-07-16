#include "core/GameEngine.h"
#include <Arduino.h>
#include <algorithm>
#include <cstdio>
#include "assets/PokemonSprites.h"
#include "core/ButtonDispatcher.h"
#include "core/FontResource.h"
#include "core/ResourcePack.h"
#include "core/ResourceFS.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "hardware/EspNowLink.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"
#include "scenes/MainScene.h"
#include "scenes/MenuScene.h"
#include "scenes/SocialScene.h"
#include "scenes/ShopScene.h"
#include "scenes/ExploreScene.h"
#include "scenes/SettingsScene.h"
#include "scenes/HatchScene.h"

namespace {
static constexpr uint16_t HP_RECOVERY_INTERVAL_MIN = 5;
static constexpr uint8_t HP_RECOVERY_PERCENT_PER_TICK = 10;
static constexpr uint8_t HP_RECOVERY_EMPTY_GAIN_PER_TICK = 1;
static constexpr uint32_t FAINT_REST_SECONDS = 60UL * 60UL;
static constexpr uint32_t CLOCK_SAVE_INTERVAL_MS = 15000UL;
static constexpr uint16_t GAME_MINUTES_PER_DAY = 24U * 60U;
static constexpr uint16_t BASE_SLEEP_START_MINUTE = 22U * 60U;
static constexpr uint16_t BASE_SLEEP_END_MINUTE = 6U * 60U;
static constexpr int16_t NATURE_SLEEP_OFFSET_MINUTE = 30;
static constexpr uint8_t SATIETY_DECAY_AWAKE_INTERVAL_MIN = 1;
static constexpr uint8_t SATIETY_DECAY_SLEEP_INTERVAL_MIN = 3;
static constexpr uint8_t SATIETY_DECAY_MAX_DROP_PER_TICK = 4;
static constexpr uint8_t DEBUG_LIGHT_SOURCE_COUNT = 6;
static constexpr uint16_t SCENE_FADE_HOLD_MS = 500;

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

struct MonsterSleepSchedule {
    uint16_t startMinute;
    uint16_t endMinute;
};

constexpr bool isMinuteInSleepWindow(uint16_t minutesOfDay,
                                     uint16_t startMinute,
                                     uint16_t endMinute) {
    return minutesOfDay < endMinute || minutesOfDay >= startMinute;
}

static_assert(!isMinuteInSleepWindow(21U * 60U + 59U,
                                    BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 21:59 must be awake time");
static_assert(isMinuteInSleepWindow(22U * 60U,
                                   BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 22:00 must be sleep time");
static_assert(isMinuteInSleepWindow(5U * 60U + 59U,
                                   BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 05:59 must be sleep time");
static_assert(!isMinuteInSleepWindow(6U * 60U,
                                    BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE),
              "baseline 06:00 must be awake time");

MonsterSleepSchedule sleepScheduleForNature(uint8_t nature) {
    uint8_t boosted = natureBoostStat(nature);
    uint8_t lowered = natureLowerStat(nature);
    if (boosted == 5 && lowered != 5) {
        return {
            static_cast<uint16_t>(BASE_SLEEP_START_MINUTE + NATURE_SLEEP_OFFSET_MINUTE),
            static_cast<uint16_t>(BASE_SLEEP_END_MINUTE - NATURE_SLEEP_OFFSET_MINUTE),
        };
    }
    if (lowered == 5 && boosted != 5) {
        return {
            static_cast<uint16_t>(BASE_SLEEP_START_MINUTE - NATURE_SLEEP_OFFSET_MINUTE),
            static_cast<uint16_t>(BASE_SLEEP_END_MINUTE + NATURE_SLEEP_OFFSET_MINUTE),
        };
    }
    return {BASE_SLEEP_START_MINUTE, BASE_SLEEP_END_MINUTE};
}

bool isScheduledSleepMinute(uint16_t minutesOfDay, uint8_t nature) {
    MonsterSleepSchedule schedule = sleepScheduleForNature(nature);
    return isMinuteInSleepWindow(minutesOfDay, schedule.startMinute, schedule.endMinute);
}

bool isSleepCareTime(uint32_t gameMinutesTotal, uint8_t nature) {
    uint16_t minutesOfDay = (uint16_t)(gameMinutesTotal % GAME_MINUTES_PER_DAY);
    return isScheduledSleepMinute(minutesOfDay, nature);
}

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

uint32_t gameSecondsForMinutes(uint32_t minutes) {
    uint64_t seconds = static_cast<uint64_t>(minutes) * 60ULL;
    return seconds > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : static_cast<uint32_t>(seconds);
}
}

GameEngine& GameEngine::ins() {
    static GameEngine instance;
    return instance;
}

bool GameEngine::begin() {
    bootStartedMs = millis();
    if (!Hal::ins().begin()) {
        Serial.println("[GameEngine] Hal init failed");
        return false;
    }
    PixelRenderer::bind(&Hal::ins().canvas());
    if (!ResourceFS::ins().begin()) {
        Serial.println("[GameEngine] external resource FS unavailable");
    }
    ResourcePack::ins().begin();
    FontResource::ins().begin();
    RoomResource::ins().begin();
    saveManager.begin();
    uint32_t savedClock = 0;
    bool hasSavedClock = saveManager.loadClock(savedClock);
    bool loadedState = saveManager.load(state);
    if (!loadedState) {
        initDefaultState();
        clearMainSceneViewState();
    } else if (!saveManager.loadMainSceneView(mainViewState)) {
        clearMainSceneViewState();
    }
    if (hasSavedClock && savedClock > state.gameMinutesTotal) {
        state.gameMinutesTotal = savedClock;
        markDirty(false);
    }
    if (!loadedState) saveNow();
    if (state.settings.idleTimeoutIndex >= 5) {
        state.settings.idleTimeoutIndex = 0;
        markDirty(false);
    }
    if (state.room.selectedFood >= Game::ROOM_FOOD_COUNT ||
        (state.room.food[state.room.selectedFood] == 0 && foodCount() > 0)) {
        selectFirstAvailableFood(state.room);
        markDirty(false);
    }
    sanitizeMonsterMoves();
    resetDailyCountersIfNeeded();
    Hal::ins().setBrightness(state.settings.brightness);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    EspNowLink::ins().beginStub();
    startupFirstFrameRendered = false;
    PokemonSprites::setDynamicLoadingEnabled(false);
    startupSpriteCacheReady = syncSpriteCache(0);
    switchScene(state.oobeDone ? SceneID::MAIN : SceneID::HATCH);
    uint32_t now = Hal::ins().millis();
    clockAnchorMs = now;
    clockAnchorMinutes = state.gameMinutesTotal;
    lastSavedClockMinutes = state.gameMinutesTotal;
    lastInputMs = lastFrameMs = lastUpdateMs = lastCareMs = lastSaveMs = lastActivityMs = lastClockSaveMs = now;
    Serial.printf("[BootTiming] init_ms=%u sprites_ready=%u\n",
                  now - bootStartedMs, startupSpriteCacheReady ? 1 : 0);
    return true;
}

void GameEngine::run() {
    M5.update();
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
    if (now - lastFrameMs >= frameMs) {
        update(now);
        render(now);
        lastFrameMs = now;
        didWork = true;
    }

    if (!didWork) delay(1);
}

void GameEngine::requestScene(SceneID id) {
    if (id == currentId) return;
    switchScene(id);
}

bool GameEngine::fadeToScene(SceneID id, uint16_t durationMs) {
    if (id == currentId || sceneFade != SceneFadePhase::NONE) return false;
    sceneFadeTarget = id;
    sceneFadeDurationMs = max<uint16_t>(1, durationMs);
    sceneFadeStartedMs = Hal::ins().millis();
    sceneFadeLastStepMs = sceneFadeStartedMs;
    sceneFadeProgressMs = 0;
    sceneFade = SceneFadePhase::OUT;
    resetIdle(sceneFadeStartedMs);
    return true;
}

bool GameEngine::sceneFadeActive() const {
    return sceneFade != SceneFadePhase::NONE;
}

bool GameEngine::sceneFadeInActive() const {
    return sceneFade == SceneFadePhase::IN;
}

void GameEngine::beginExploreDeparture(uint8_t area) {
    exploreArea = area;
    exploreTravel = ExploreTravelPhase::DEPARTING;
    requestScene(SceneID::MAIN);
}

void GameEngine::markExploreActive() {
    if (exploreTravel == ExploreTravelPhase::DEPARTING) {
        exploreTravel = ExploreTravelPhase::ACTIVE;
        activeMonster().lastExploredAt = gameSecondsForMinutes(gameMinutesTotal());
        markDirty(false);
    }
}

void GameEngine::beginExploreReturn(bool fainted) {
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
    if (statusCleared) markDirty(true);
}

void GameEngine::beginDebugBattle() {
    debugBattleRequested = true;
    debugMenuReturnRequested = false;
    requestScene(SceneID::EXPLORE);
}

bool GameEngine::consumeDebugBattleRequest() {
    bool requested = debugBattleRequested;
    debugBattleRequested = false;
    return requested;
}

void GameEngine::endDebugBattle() {
    debugBattleRequested = false;
    debugMenuReturnRequested = true;
    requestScene(SceneID::MENU);
}

bool GameEngine::consumeDebugMenuReturnRequest() {
    bool requested = debugMenuReturnRequested;
    debugMenuReturnRequested = false;
    return requested;
}

float GameEngine::gameSpeed() const {
    static constexpr float SPEEDS[] = {1.0f, 2.0f, 4.0f, 8.0f};
    uint8_t idx = state.settings.speedIndex;
    if (idx >= 4) idx = 0;
    return SPEEDS[idx];
}

uint32_t GameEngine::gameMinutesTotal() const {
    return gameMinutesTotalAt(Hal::ins().millis());
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
    clockAnchorMs = now;
    clockAnchorMinutes = state.gameMinutesTotal;
    markDirty(false);
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
    markDirty(false);
}

uint8_t GameEngine::hungerValue() const {
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
    mon.nature = random(0, Game::NATURE_COUNT);
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    mon.caughtAt = gameSecondsForMinutes(gameMinutesTotal());
    mon.lastSeenAt = mon.caughtAt;
    mon.lastExploredAt = mon.caughtAt;
    mon.lastWindowGazeAt = mon.caughtAt;
    return mon;
}

bool GameEngine::switchActiveMonster() {
    if (state.teamCount < 2) return false;
    return moveTeamMemberToFront(1);
}

bool GameEngine::moveTeamMemberToFront(uint8_t slot) {
    if (slot == 0) return true;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) return false;
    Game::MonsterRuntime selected = state.team[slot];
    for (int8_t i = slot; i > 0; --i) {
        state.team[i] = state.team[i - 1];
    }
    state.team[0] = selected;
    state.activeSlot = 0;
    clearMainSceneViewState();
    syncSpriteCache();
    markDirty(true);
    return true;
}

bool GameEngine::depositTeamMemberToStorage(uint8_t slot) {
    if (state.teamCount <= 1) return false;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) return false;
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
    markDirty(true);
    return true;
}

bool GameEngine::withdrawStorageMemberToTeam(uint8_t slot) {
    if (state.teamCount >= Game::TEAM_CAP) return false;
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return false;

    state.team[state.teamCount++] = state.storage[slot];
    for (uint8_t i = slot; i + 1 < state.storageCount && i + 1 < Game::STORAGE_CAP; ++i) {
        state.storage[i] = state.storage[i + 1];
    }
    state.storageCount--;
    if (state.storageCount < Game::STORAGE_CAP) {
        state.storage[state.storageCount] = Game::MonsterRuntime{};
    }
    syncSpriteCache();
    markDirty(true);
    return true;
}

bool GameEngine::releaseStorageMember(uint8_t slot) {
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return false;

    for (uint8_t i = slot; i + 1 < state.storageCount && i + 1 < Game::STORAGE_CAP; ++i) {
        state.storage[i] = state.storage[i + 1];
    }
    state.storageCount--;
    if (state.storageCount < Game::STORAGE_CAP) {
        state.storage[state.storageCount] = Game::MonsterRuntime{};
    }
    markDirty(true);
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
    count = (uint8_t)min<uint16_t>(Game::ITEM_STACK_CAP, (uint16_t)count + amount);
    if (!hadFood) state.room.selectedFood = foodIndex;
    markDirty(true);
    return true;
}

bool GameEngine::selectFood(uint8_t foodIndex) {
    foodIndex = clampFoodIndex(foodIndex);
    if (state.room.food[foodIndex] == 0) return false;
    state.room.selectedFood = foodIndex;
    markDirty(true);
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
    markDirty(true);
    return FoodPlacementResult::ADDED;
}

FoodConsumeResult GameEngine::consumeBowlFood() {
    FoodConsumeResult result;
    if (state.room.bowlCount == 0) return result;
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

    Game::MonsterRuntime& mon = activeMonster();
    result.satietyBefore = mon.satiety;
    result.moodBefore = mon.mood;
    bool wasFull = mon.satiety >= 100;
    bool normalFood = foodIndex == Game::ROOM_NORMAL_FOOD_INDEX;
    uint8_t satietyGain = normalFood ? 25 : 22;
    uint8_t moodGain = foodIndex == 1 ? 5 : 3;
    mon.satiety = (uint8_t)min<uint16_t>(100, (uint16_t)mon.satiety + satietyGain);
    mon.mood = (uint8_t)min<uint16_t>(100, (uint16_t)mon.mood + moodGain);
    result.consumed = true;
    result.satietyAfter = mon.satiety;
    result.moodAfter = mon.mood;
    result.lastBite = state.room.bowlCount == 0;
    result.becameFull = !wasFull && mon.satiety >= 100;
    grantCareExperience(normalFood ? 1 : 6, wasFull);
    markDirty(true);
    return result;
}

bool GameEngine::addBalls(uint8_t amount) {
    return addItem(Game::ItemId::POKE_BALL, amount);
}

bool GameEngine::consumeBall() {
    if (state.bag.pokeBall == 0) return false;
    state.bag.pokeBall--;
    markDirty(true);
    return true;
}

bool GameEngine::addGreatBalls(uint8_t amount) {
    return addItem(Game::ItemId::GREAT_BALL, amount);
}

bool GameEngine::consumeGreatBall() {
    if (state.bag.greatBall == 0) return false;
    state.bag.greatBall--;
    markDirty(true);
    return true;
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

bool GameEngine::usePotion() {
    if (state.bag.potion == 0) return false;
    Game::MonsterRuntime& mon = activeMonster();
    if (mon.fainted || mon.hpCur >= mon.hpMax) return false;
    state.bag.potion--;
    mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + 20);
    markDirty(true);
    return true;
}

bool GameEngine::useSuperPotion() {
    if (state.bag.superPotion == 0) return false;
    Game::MonsterRuntime& mon = activeMonster();
    if (mon.fainted || mon.hpCur >= mon.hpMax) return false;
    state.bag.superPotion--;
    mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + 50);
    markDirty(true);
    return true;
}

bool GameEngine::addAntidote(uint8_t amount) {
    return addItem(Game::ItemId::ANTIDOTE, amount);
}

bool GameEngine::useAntidote() {
    if (state.bag.antidote == 0) return false;
    Game::MonsterRuntime& mon = activeMonster();
    if (mon.majorStatus == Game::MajorStatus::NONE) return false;
    state.bag.antidote--;
    mon.majorStatus = Game::MajorStatus::NONE;
    mon.majorStatusTurns = 0;
    markDirty(true);
    return true;
}

uint8_t GameEngine::itemCount(Game::ItemId item) const {
    switch (item) {
    case Game::ItemId::POKE_BALL: return state.bag.pokeBall;
    case Game::ItemId::GREAT_BALL: return state.bag.greatBall;
    case Game::ItemId::HEAVY_BALL: return state.bag.heavyBall;
    case Game::ItemId::TIMER_BALL: return state.bag.timerBall;
    case Game::ItemId::NORMAL_FOOD: return state.room.food[0];
    case Game::ItemId::POTION: return state.bag.potion;
    case Game::ItemId::SUPER_POTION: return state.bag.superPotion;
    case Game::ItemId::ANTIDOTE: return state.bag.antidote;
    case Game::ItemId::CANDY: return state.bag.candy;
    default: return 0;
    }
}

bool GameEngine::addItem(Game::ItemId item, uint8_t amount, bool immediate) {
    if (amount == 0) return true;

    uint8_t* count = nullptr;
    switch (item) {
    case Game::ItemId::POKE_BALL: count = &state.bag.pokeBall; break;
    case Game::ItemId::GREAT_BALL: count = &state.bag.greatBall; break;
    case Game::ItemId::HEAVY_BALL: count = &state.bag.heavyBall; break;
    case Game::ItemId::TIMER_BALL: count = &state.bag.timerBall; break;
    case Game::ItemId::NORMAL_FOOD: count = &state.room.food[0]; break;
    case Game::ItemId::POTION: count = &state.bag.potion; break;
    case Game::ItemId::SUPER_POTION: count = &state.bag.superPotion; break;
    case Game::ItemId::ANTIDOTE: count = &state.bag.antidote; break;
    case Game::ItemId::CANDY: count = &state.bag.candy; break;
    default: return false;
    }
    if (!count || *count >= Game::ITEM_STACK_CAP) return false;
    *count = static_cast<uint8_t>(min<uint16_t>(
        Game::ITEM_STACK_CAP, static_cast<uint16_t>(*count) + amount));
    markDirty(immediate);
    return true;
}

bool GameEngine::removeItem(Game::ItemId item, uint8_t amount, bool immediate) {
    if (amount == 0) return true;

    uint8_t* count = nullptr;
    switch (item) {
    case Game::ItemId::POKE_BALL: count = &state.bag.pokeBall; break;
    case Game::ItemId::GREAT_BALL: count = &state.bag.greatBall; break;
    case Game::ItemId::HEAVY_BALL: count = &state.bag.heavyBall; break;
    case Game::ItemId::TIMER_BALL: count = &state.bag.timerBall; break;
    case Game::ItemId::NORMAL_FOOD: count = &state.room.food[0]; break;
    case Game::ItemId::POTION: count = &state.bag.potion; break;
    case Game::ItemId::SUPER_POTION: count = &state.bag.superPotion; break;
    case Game::ItemId::ANTIDOTE: count = &state.bag.antidote; break;
    case Game::ItemId::CANDY: count = &state.bag.candy; break;
    default: return false;
    }
    if (!count || *count < amount) return false;
    *count -= amount;
    uint8_t selectedFood = selectedFoodIndex();
    if (item == Game::ItemId::NORMAL_FOOD && state.room.food[selectedFood] == 0) {
        selectFirstAvailableFood(state.room);
    }
    markDirty(immediate);
    return true;
}

bool GameEngine::spendCoins(uint32_t amount) {
    if (state.coins < amount) return false;
    state.coins -= amount;
    markDirty(true);
    return true;
}

void GameEngine::addCoins(uint32_t amount) {
    uint64_t total = static_cast<uint64_t>(state.coins) + amount;
    state.coins = total > 99999 ? 99999 : static_cast<uint32_t>(total);
    markDirty(true);
}

bool GameEngine::recordCapture(uint16_t speciesId) {
    return recordCapture(createMonster(speciesId, 5));
}

bool GameEngine::recordCapture(const Game::MonsterRuntime& monster) {
    return recordCapture(monster, Game::MET_AREA_UNKNOWN);
}

bool GameEngine::recordCapture(const Game::MonsterRuntime& monster, uint8_t metArea) {
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;

    Game::MonsterRuntime mon = monster;
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    resetMovesForLevel(mon, *species);
    mon.origin = Game::Origin::CAPTURED;
    mon.metArea = metArea;
    mon.caughtAt = gameSecondsForMinutes(gameMinutesTotal());
    mon.lastSeenAt = mon.caughtAt;
    mon.lastExploredAt = mon.caughtAt;
    mon.lastWindowGazeAt = mon.caughtAt;

    if (state.teamCount < Game::TEAM_CAP) {
        state.team[state.teamCount++] = mon;
        syncSpriteCache();
    } else if (state.storageCount < Game::STORAGE_CAP) {
        state.storage[state.storageCount++] = mon;
    } else {
        return false;
    }
    addCoins(20);
    markDirty(true);
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
        uint8_t gain = min<uint16_t>(amount, min<uint16_t>(roomByStat, roomByTotal));
        if (gain == 0) continue;
        *value += gain;
        changed = true;
    }

    if (changed) {
        const Species& species = speciesFor(mon);
        uint16_t oldMax = mon.hpMax;
        mon.hpMax = maxHpFor(species, mon);
        if (mon.hpMax > oldMax) mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + (mon.hpMax - oldMax));
        markDirty(false);
    }
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
        markDirty(false);
        return result;
    }

    uint8_t oldMood = mon.mood;
    uint8_t oldAffection = mon.affection;
    mon.petCountToday++;
    mon.mood = (uint8_t)min<uint16_t>(100, (uint16_t)mon.mood + 5);
    mon.affection = (uint8_t)min<uint16_t>(255, (uint16_t)mon.affection + 2);
    result.outcome = PetOutcome::REWARDED;
    result.moodGain = mon.mood - oldMood;
    result.affectionGain = mon.affection - oldAffection;
    grantCareExperience(2);
    markDirty(true);
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
    state.team[0].caughtAt = gameSecondsForMinutes(gameMinutesTotal());
    state.team[0].lastSeenAt = state.team[0].caughtAt;
    state.team[0].lastExploredAt = state.team[0].caughtAt;
    state.team[0].lastWindowGazeAt = state.team[0].caughtAt;
    state.bag = Game::BagState{};
    state.room = Game::RoomState{};
    state.coins = 50;
    state.oobeDone = true;
    markDirty(true);
    requestScene(SceneID::MAIN);
}

void GameEngine::addExperience(uint32_t amount) {
    addExperienceToTeamMember(0, amount);
}

uint32_t GameEngine::addExperienceToTeamMember(uint8_t teamSlot, uint32_t amount) {
    if (amount == 0 || teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return 0;
    Game::MonsterRuntime& mon = state.team[teamSlot];
    const Species& species = speciesFor(mon);
    uint8_t oldLevel = mon.level;
    uint16_t oldHpMax = mon.hpMax;
    uint32_t oldExp = mon.exp;
    uint32_t maxExp = minimumExpForLevel(species.growthRate, Game::LEVEL_MAX);
    uint64_t totalExp = static_cast<uint64_t>(mon.exp) + amount;
    mon.exp = totalExp > maxExp ? maxExp : static_cast<uint32_t>(totalExp);
    mon.level = levelForExp(species.growthRate, mon.exp);
    mon.hpMax = maxHpFor(species, mon);
    if (mon.hpMax > oldHpMax) {
        mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + (mon.hpMax - oldHpMax));
    }
    if (mon.level < oldLevel) mon.level = oldLevel;
    bool leveledUp = mon.level > oldLevel;
    if (leveledUp) {
        state.pendingLevelUp = true;
        state.pendingLevelUpLevel = mon.level;
        queueMoveLearnIfReady(mon, species, oldLevel, teamSlot);
    }
    markDirty(leveledUp);
    return mon.exp > oldExp ? mon.exp - oldExp : 0;
}

bool GameEngine::acknowledgePendingLevelUp() {
    if (!state.pendingLevelUp) return false;
    state.pendingLevelUp = false;
    state.pendingLevelUpLevel = 0;
    markDirty(true);
    return true;
}

bool GameEngine::resolvePendingMoveLearn(bool learn) {
    if (!state.pendingMoveLearn) return false;
    const uint8_t teamSlot = state.pendingMoveSlot;
    const uint16_t nextCursor = state.pendingMoveCursor;
    bool applied = false;
    if (teamSlot < state.teamCount && teamSlot < Game::TEAM_CAP) {
        Game::MonsterRuntime& mon = state.team[teamSlot];
        const Species* species = findSpecies(mon.speciesId);
        if (learn && species && mon.level >= moveLearnLevelForSpecies(*species, state.pendingMoveId) &&
            canLearnAsSpecialMove(*species, state.pendingMoveId) &&
            mon.move2Id != state.pendingMoveId && mon.move3Id != state.pendingMoveId) {
            if (mon.move2Id == 0) {
                mon.move2Id = state.pendingMoveId;
                mon.moveProficiency[1] = 0;
            } else {
                mon.move3Id = state.pendingMoveId;
                mon.moveProficiency[2] = 0;
            }
            applied = true;
        }

        state.pendingMoveLearn = false;
        state.pendingMoveSlot = 0;
        state.pendingMoveId = 0;
        state.pendingMoveCursor = 0;
        if (species) queueNextPendingMove(mon, *species, teamSlot, nextCursor);
    } else {
        state.pendingMoveLearn = false;
        state.pendingMoveSlot = 0;
        state.pendingMoveId = 0;
        state.pendingMoveCursor = 0;
    }
    markDirty(true);
    return applied;
}

uint32_t GameEngine::applyActiveFaintPenalty() {
    syncGameClock(Hal::ins().millis());
    Game::MonsterRuntime& mon = activeMonster();
    const Species& species = speciesFor(mon);
    uint32_t levelFloor = minimumExpForLevel(species.growthRate, mon.level);
    uint32_t availableLoss = mon.exp > levelFloor ? mon.exp - levelFloor : 0;
    uint32_t loss = mon.exp == 0 ? 0 : max<uint32_t>(1, mon.exp / 10);
    if (loss > availableLoss) loss = availableLoss;
    mon.exp -= loss;
    mon.hpCur = 0;
    mon.fainted = true;
    mon.lastSeenAt = gameSecondsForMinutes(state.gameMinutesTotal);
    mon.affection = mon.affection > 5 ? mon.affection - 5 : 0;
    mon.mood = mon.mood > 10 ? mon.mood - 10 : 0;
    markDirty(true);
    return loss;
}

void GameEngine::addWalkSteps(uint16_t steps) {
    syncGameClock(Hal::ins().millis());
    resetDailyCountersIfNeeded();
    state.stepsToday = (uint16_t)min<uint32_t>(60000, (uint32_t)state.stepsToday + steps);
    uint16_t earnedWalkExp = min<uint16_t>(50, state.stepsToday / 100);
    if (earnedWalkExp > state.walkExpToday) {
        uint16_t expGain = earnedWalkExp - state.walkExpToday;
        state.walkExpToday = earnedWalkExp;
        addExperience(expGain);
    }
    markDirty(false);
}

void GameEngine::debugRecoverActiveMonster() {
    if (state.teamCount == 0) return;
    Game::MonsterRuntime& mon = activeMonster();
    mon.fainted = false;
    mon.majorStatus = Game::MajorStatus::NONE;
    mon.majorStatusTurns = 0;
    mon.hpCur = mon.hpMax;
    mon.satiety = 100;
    mon.mood = 100;
    markDirty(true);
}

bool GameEngine::debugSetActiveSpecies(uint16_t speciesId) {
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
    markDirty(true);
    return true;
}

uint32_t GameEngine::debugAdvanceToTimeOfDay(uint16_t targetMinutesOfDay) {
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);

    targetMinutesOfDay %= (24U * 60U);
    uint16_t current = (uint16_t)(state.gameMinutesTotal % (24UL * 60UL));
    uint32_t delta = targetMinutesOfDay >= current
        ? (uint32_t)(targetMinutesOfDay - current)
        : (uint32_t)(24U * 60U - current + targetMinutesOfDay);

    state.gameMinutesTotal += delta;
    clockAnchorMs = now;
    clockAnchorMinutes = state.gameMinutesTotal;
    saveNow();
    return delta;
}

const char* GameEngine::debugLightSourceLabel() const {
    uint8_t index = debugLightSource;
    if (index >= DEBUG_LIGHT_SOURCE_COUNT) index = 0;
    return Ui::Debug::LIGHT_SOURCE_ITEMS[index];
}

void GameEngine::cycleDebugLightSource() {
    debugLightSource = (uint8_t)((debugLightSource + 1) % DEBUG_LIGHT_SOURCE_COUNT);
}

void GameEngine::wakeFromIdle() {
    resetIdle(Hal::ins().millis());
}

void GameEngine::markDirty(bool immediate) {
    saveDirty = true;
    if (immediate) saveNow();
}

bool GameEngine::saveNow() {
    uint32_t now = Hal::ins().millis();
    if (currentScene) currentScene->onBeforeSave();
    syncGameClock(now);
    bool stateSaved = saveManager.save(state);
    bool viewSaved = saveManager.saveMainSceneView(mainViewState);
    bool ok = stateSaved && viewSaved;
    if (!viewSaved) Serial.println("[GameEngine] main view save failed");
    persistGameClock(now, true);
    saveDirty = !ok;
    lastSaveMs = now;
    return ok;
}

bool GameEngine::resetGame() {
    if (!saveManager.clearAll()) {
        Serial.println("[GameEngine] failed to clear game data");
        return false;
    }

    uint32_t now = Hal::ins().millis();
    clockAnchorMs = now;
    clockAnchorMinutes = 0;
    initDefaultState();
    HatchScene::clearRuntimeProgress();
    clearMainSceneViewState();
    hpRecoveryMinuteAcc = 0;
    satietyDecayMinuteAcc = 0;
    satietyDecayWasSleeping = false;
    debugShowWalkBoundary = false;
    debugTiltControl = false;
    debugLightSource = 0;
    saveDirty = false;

    lastSavedClockMinutes = 0;
    lastClockSaveMs = now;
    lastCareMs = now;
    lastUpdateMs = now;
    resetIdle(now);
    Hal::ins().setBrightness(state.settings.brightness);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    syncSpriteCache();

    if (!saveNow()) {
        Serial.println("[GameEngine] reset completed but default save failed");
    }
    return true;
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

void GameEngine::switchScene(SceneID id) {
    uint32_t switchStartedAt = Hal::ins().millis();
    bool traceBrightnessExit = currentId == SceneID::SETTINGS && id != SceneID::SETTINGS;
    if (traceBrightnessExit) {
        Serial.printf("[Scene] settings switch begin t=%lu from=%u to=%u configured=%u display=%u idle=%u\n",
                      (unsigned long)switchStartedAt,
                      (unsigned)currentId,
                      (unsigned)id,
                      Hal::ins().getBrightness(),
                      Hal::ins().getDisplayBrightness(),
                      Hal::ins().isIdleBrightnessActive() ? 1 : 0);
    }
    resetIdle(Hal::ins().millis());
    if (currentScene) currentScene->onExit();
    prevId = currentId;
    currentId = id;
    resetIdle(Hal::ins().millis());

    switch (id) {
    case SceneID::HATCH:
        currentScene.reset(new HatchScene());
        break;
    case SceneID::SETTINGS:
        currentScene.reset(new SettingsScene());
        break;
    case SceneID::EXPLORE:
        currentScene.reset(new ExploreScene());
        break;
    case SceneID::SHOP:
        currentScene.reset(new ShopScene());
        break;
    case SceneID::SOCIAL:
        currentScene.reset(new SocialScene());
        break;
    case SceneID::MENU:
        currentScene.reset(new MenuScene());
        break;
    case SceneID::MAIN:
    default:
        currentScene.reset(new MainScene());
        break;
    }

    if (currentScene) currentScene->onEnter();
    if (traceBrightnessExit) {
        brightnessTraceFrames = 3;
        brightnessTraceStartedMs = switchStartedAt;
        uint32_t finishedAt = Hal::ins().millis();
        Serial.printf("[Scene] settings switch end t=%lu cost=%lums scene=%u configured=%u display=%u idle=%u\n",
                      (unsigned long)finishedAt,
                      (unsigned long)(finishedAt - switchStartedAt),
                      (unsigned)currentId,
                      Hal::ins().getBrightness(),
                      Hal::ins().getDisplayBrightness(),
                      Hal::ins().isIdleBrightnessActive() ? 1 : 0);
    }
}

void GameEngine::processInput(uint32_t nowMs) {
    bool rawInputActive = Hal::ins().btnA_raw() || Hal::ins().btnB_raw();
    if (rawInputActive) resetIdle(nowMs);

    ButtonEvent events[ButtonDispatcher::MAX_EVENTS_PER_POLL];
    uint8_t count = ButtonDispatcher::ins().poll(events, ButtonDispatcher::MAX_EVENTS_PER_POLL);
    if (count > 0) resetIdle(nowMs);
    if (sceneFade != SceneFadePhase::NONE) return;
    if (resourceAlertVisible()) return;
    for (uint8_t i = 0; i < count; ++i) {
        if (currentScene && currentScene->onButton(events[i])) continue;
    }
}

void GameEngine::update(uint32_t nowMs) {
    float dt = (nowMs - lastUpdateMs) / 1000.0f;
    lastUpdateMs = nowMs;
    if (resourceAlertVisible()) {
        clockAnchorMs = nowMs;
        clockAnchorMinutes = state.gameMinutesTotal;
        lastCareMs = nowMs;
        return;
    }
    syncGameClock(nowMs);
    if (startupFirstFrameRendered && !startupSpriteCacheReady) {
        startupSpriteCacheReady = syncSpriteCache(1);
        if (startupSpriteCacheReady) {
            PokemonSprites::setDynamicLoadingEnabled(true);
            Serial.printf("[BootTiming] sprites_ready_ms=%u\n", nowMs - bootStartedMs);
        }
    } else {
        syncSpriteCache(startupFirstFrameRendered ? 0xFF : 0);
    }
    persistGameClock(nowMs);
    tickCare(nowMs);
    if (currentScene && sceneFade != SceneFadePhase::HOLD) {
        currentScene->update(nowMs, dt * gameSpeed());
    }
    updateSceneFade(nowMs);
    if (saveDirty && nowMs - lastSaveMs >= 300000UL) saveNow();
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
        stepMs = min<uint32_t>(FRAME_MS, nowMs - sceneFadeLastStepMs);
        sceneFadeLastStepMs = nowMs;
    }
    sceneFadeProgressMs = static_cast<uint16_t>(min<uint32_t>(
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
    if (brightnessTraceFrames > 0) {
        Serial.printf("[Scene] post-settings frame begin t=%lu elapsed=%lums remaining=%u scene=%u configured=%u display=%u idle=%u\n",
                      (unsigned long)nowMs,
                      (unsigned long)(nowMs - brightnessTraceStartedMs),
                      brightnessTraceFrames,
                      (unsigned)currentId,
                      Hal::ins().getBrightness(),
                      Hal::ins().getDisplayBrightness(),
                      Hal::ins().isIdleBrightnessActive() ? 1 : 0);
    }
    PokemonSprites::beginRenderFrame();
    if (currentScene) currentScene->render();
    renderResourceAlert();
    renderSceneFade(Hal::ins().millis());
    Hal::ins().flush();
    if (brightnessTraceFrames > 0) {
        uint32_t finishedAt = Hal::ins().millis();
        Serial.printf("[Scene] post-settings frame end t=%lu cost=%lums display=%u\n",
                      (unsigned long)finishedAt,
                      (unsigned long)(finishedAt - nowMs),
                      Hal::ins().getDisplayBrightness());
        brightnessTraceFrames--;
    }
    if (!startupFirstFrameRendered) {
        startupFirstFrameRendered = true;
        if (startupSpriteCacheReady) PokemonSprites::setDynamicLoadingEnabled(true);
        Serial.printf("[BootTiming] first_frame_ms=%u sprites_ready=%u\n",
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
    uint32_t progress = min<uint32_t>(sceneFadeDurationMs, sceneFadeProgressMs);
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
    idleActive = true;
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
    uint32_t elapsedMs = nowMs - clockAnchorMs;
    uint32_t scaledMinutes = (uint32_t)((double)elapsedMs * (double)gameSpeed() / 60000.0);
    return clockAnchorMinutes + scaledMinutes;
}

void GameEngine::syncGameClock(uint32_t nowMs) {
    uint32_t total = gameMinutesTotalAt(nowMs);
    if (total == state.gameMinutesTotal) return;
    state.gameMinutesTotal = total;
    saveDirty = true;
}

void GameEngine::resetGameClockAnchor(uint32_t nowMs) {
    syncGameClock(nowMs);
    clockAnchorMs = nowMs;
    clockAnchorMinutes = state.gameMinutesTotal;
}

void GameEngine::persistGameClock(uint32_t nowMs, bool force) {
    if (state.gameMinutesTotal == lastSavedClockMinutes && !force) return;
    if (!force && nowMs - lastClockSaveMs < CLOCK_SAVE_INTERVAL_MS) return;
    if (saveManager.saveClock(state.gameMinutesTotal)) {
        lastSavedClockMinutes = state.gameMinutesTotal;
        lastClockSaveMs = nowMs;
    }
}

void GameEngine::initDefaultState() {
    saveManager.reset(state);
    state.teamCount = 0;
    state.storageCount = 0;
    state.activeSlot = 0;
    state.gameMinutesTotal = 0;
    state.careDay = 0;
    state.careExpToday = 0;
    state.bag = Game::BagState{};
    state.bag.pokeBall = 0;
    state.bag.potion = 0;
    state.bag.candy = 0;
    state.room = Game::RoomState{};
    for (uint8_t i = 0; i < Game::ROOM_FOOD_COUNT; ++i) state.room.food[i] = 0;
    state.coins = 0;
}

void GameEngine::sanitizeMonsterMoves() {
    bool changed = false;
    auto sanitize = [&](Game::MonsterRuntime& mon) {
        const Species* species = findSpecies(mon.speciesId);
        if (!species) return;

        Game::MoveId basicMove = basicMoveIdForSpecies(*species);
        if (!isBasicFirstMoveForSpecies(*species, mon.move1Id)) {
            mon.move1Id = basicMove;
            changed = true;
        }

        uint8_t move2Level = moveLearnLevelForSpecies(*species, mon.move2Id);
        if (mon.move2Id != 0 &&
            (move2Level == 0 || mon.level < move2Level ||
             !canLearnAsSpecialMove(*species, mon.move2Id) ||
             mon.move2Id == mon.move1Id)) {
            mon.move2Id = 0;
            mon.moveProficiency[1] = 0;
            changed = true;
        }
        uint8_t move3Level = moveLearnLevelForSpecies(*species, mon.move3Id);
        if (mon.move3Id != 0 &&
            (move3Level == 0 || mon.level < move3Level ||
             !canLearnAsSpecialMove(*species, mon.move3Id) ||
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
                    : min<uint8_t>(mon.moveProficiency[slot], Game::MOVE_PROFICIENCY_MAX));
            if (mon.moveProficiency[slot] != normalized) {
                mon.moveProficiency[slot] = normalized;
                changed = true;
            }
        }
    };

    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        sanitize(state.team[i]);
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        sanitize(state.storage[i]);
    }
    if (changed) markDirty(false);
}

void GameEngine::resetDailyCountersIfNeeded() {
    uint32_t day = state.gameMinutesTotal / GAME_MINUTES_PER_DAY;
    if (day > 0xFFFF) day = 0xFFFF;
    if (state.careDay == (uint16_t)day) return;

    state.careDay = (uint16_t)day;
    state.careExpToday = 0;
    state.stepsToday = 0;
    state.walkExpToday = 0;
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        state.team[i].petCountToday = 0;
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        state.storage[i].petCountToday = 0;
    }
    markDirty(false);
}

void GameEngine::grantCareExperience(uint8_t baseAmount, bool weakGain) {
    if (baseAmount == 0 || state.teamCount == 0) return;
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);
    resetDailyCountersIfNeeded();

    Game::MonsterRuntime& mon = activeMonster();
    uint16_t cap = careDailyCapForLevel(mon.level);
    if (state.careExpToday >= cap) return;

    uint16_t amount = weakGain || mon.level > 15
        ? 1
        : (uint16_t)baseAmount * careExpMultiplierForLevel(mon.level);
    amount = min<uint16_t>(amount, cap - state.careExpToday);
    if (amount == 0) return;

    state.careExpToday += amount;
    addExperience(amount);
    markDirty(false);
}

void GameEngine::tickCare(uint32_t nowMs) {
    if (nowMs - lastCareMs < 60000UL) return;
    resetDailyCountersIfNeeded();
    uint32_t elapsedMin = (nowMs - lastCareMs) / 60000UL;
    lastCareMs = nowMs;
    uint32_t scaledHpRecoveryMin = (uint32_t)((float)elapsedMin * gameSpeed());
    hpRecoveryMinuteAcc = min<uint32_t>(60000, hpRecoveryMinuteAcc + scaledHpRecoveryMin);
    uint16_t hpRecoveryTicks = hpRecoveryMinuteAcc / HP_RECOVERY_INTERVAL_MIN;
    hpRecoveryMinuteAcc %= HP_RECOVERY_INTERVAL_MIN;
    uint32_t nowGameSec = gameSecondsForMinutes(state.gameMinutesTotal);

    auto updateHealthRecovery = [&](Game::MonsterRuntime& mon) {
        if (mon.fainted) {
            if (mon.lastSeenAt == 0 || mon.lastSeenAt > nowGameSec) mon.lastSeenAt = nowGameSec;
            uint32_t elapsedFaintSec = nowGameSec - mon.lastSeenAt;
            if (elapsedFaintSec >= FAINT_REST_SECONDS) {
                mon.fainted = false;
                mon.majorStatus = Game::MajorStatus::NONE;
                mon.majorStatusTurns = 0;
                mon.hpCur = max<uint16_t>(
                    1,
                    (uint16_t)((mon.hpMax * HP_RECOVERY_PERCENT_PER_TICK + 99) / 100)
                );
                mon.lastSeenAt = nowGameSec;
                Serial.printf("[Care] faint rest complete species=%u hp=%u/%u\n",
                              mon.speciesId, mon.hpCur, mon.hpMax);
            }
            return;
        }

        if (hpRecoveryTicks > 0 && mon.hpCur < mon.hpMax) {
            uint16_t gainPerTick = mon.satiety == 0
                ? HP_RECOVERY_EMPTY_GAIN_PER_TICK
                : max<uint16_t>(
                    1,
                    (uint16_t)((mon.hpMax * HP_RECOVERY_PERCENT_PER_TICK + 99) / 100)
                );
            uint32_t gain = (uint32_t)gainPerTick * hpRecoveryTicks;
            mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + gain);
        }
    };

    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        Game::MonsterRuntime& mon = state.team[i];
        if (i == 0) {
            bool sleeping = mon.majorStatus == Game::MajorStatus::SLEEP ||
                            isSleepCareTime(state.gameMinutesTotal, mon.nature);
            uint8_t decayInterval = sleeping ? SATIETY_DECAY_SLEEP_INTERVAL_MIN
                                             : SATIETY_DECAY_AWAKE_INTERVAL_MIN;
            if (sleeping != satietyDecayWasSleeping) {
                satietyDecayMinuteAcc = 0;
                satietyDecayWasSleeping = sleeping;
            }
            satietyDecayMinuteAcc = min<uint32_t>(60000, (uint32_t)satietyDecayMinuteAcc + elapsedMin);
            uint8_t satietyDrop = min<uint32_t>(
                satietyDecayMinuteAcc / decayInterval,
                SATIETY_DECAY_MAX_DROP_PER_TICK
            );
            satietyDecayMinuteAcc %= decayInterval;
            mon.satiety = (mon.satiety > satietyDrop) ? mon.satiety - satietyDrop : 0;
        }
        uint8_t targetMood = mon.satiety > 60 ? 75 : (mon.satiety > 25 ? 55 : 35);
        if (mon.fainted) targetMood = 20;
        if (mon.mood < targetMood) mon.mood++;
        else if (mon.mood > targetMood) mon.mood--;
        updateHealthRecovery(mon);
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        updateHealthRecovery(state.storage[i]);
    }
    markDirty(false);
}

bool GameEngine::syncSpriteCache(uint8_t loadBudget) {
    uint16_t teamSpecies[Game::TEAM_CAP] = {};
    uint8_t count = state.teamCount;
    if (count > Game::TEAM_CAP) count = Game::TEAM_CAP;
    for (uint8_t i = 0; i < count; ++i) {
        teamSpecies[i] = state.team[i].speciesId;
    }
    return PokemonSprites::syncTeamCache(teamSpecies, count, loadBudget);
}

uint32_t GameEngine::randomIvPacked() const {
    uint32_t packed = 0;
    for (uint8_t i = 0; i < Game::STAT_COUNT; ++i) {
        Game::setIv(packed, i, random(0, Game::IV_MAX + 1));
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
