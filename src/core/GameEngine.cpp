#include "core/GameEngine.h"
#include <Arduino.h>
#include <algorithm>
#include "assets/PokemonSprites.h"
#include "core/ButtonDispatcher.h"
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
static constexpr uint32_t FAINT_RECOVERY_SECONDS = 86400UL;
static constexpr uint32_t CLOCK_SAVE_INTERVAL_MS = 15000UL;

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
}

GameEngine& GameEngine::ins() {
    static GameEngine instance;
    return instance;
}

bool GameEngine::begin() {
    if (!Hal::ins().begin()) {
        Serial.println("[GameEngine] Hal init failed");
        return false;
    }
    PixelRenderer::bind(&Hal::ins().canvas());
    saveManager.begin();
    uint32_t savedClock = 0;
    bool hasSavedClock = saveManager.loadClock(savedClock);
    bool loadedState = saveManager.load(state);
    if (!loadedState) {
        initDefaultState();
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
    Hal::ins().setBrightness(state.settings.brightness);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    EspNowLink::ins().beginStub();
    syncSpriteCache();
    switchScene(state.oobeDone ? SceneID::MAIN : SceneID::HATCH);
    uint32_t now = Hal::ins().millis();
    clockAnchorMs = now;
    clockAnchorMinutes = state.gameMinutesTotal;
    lastSavedClockMinutes = state.gameMinutesTotal;
    lastInputMs = lastFrameMs = lastUpdateMs = lastCareMs = lastSaveMs = lastActivityMs = lastClockSaveMs = now;
    return true;
}

void GameEngine::run() {
    M5.update();
    uint32_t now = Hal::ins().millis();
    bool didWork = false;

    if (now - lastInputMs >= INPUT_SAMPLE_MS) {
        lastInputMs = now;
        processInput(now);
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
    mon.ivPacked = randomIvPacked();
    mon.nature = random(0, Game::NATURE_COUNT);
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    mon.caughtAt = Hal::ins().millis() / 1000;
    mon.lastSeenAt = mon.caughtAt;
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
    syncSpriteCache();
    markDirty(true);
    return true;
}

void GameEngine::addFood(uint8_t amount) {
    state.bag.normalFood = min<uint8_t>(30, state.bag.normalFood + amount);
    markDirty(true);
}

bool GameEngine::consumeFood() {
    if (state.bag.normalFood == 0) return false;
    state.bag.normalFood--;
    Game::MonsterRuntime& mon = activeMonster();
    mon.satiety = min<uint8_t>(100, mon.satiety + 15);
    mon.mood = min<uint8_t>(100, mon.mood + 3);
    markDirty(true);
    return true;
}

void GameEngine::addBalls(uint8_t amount) {
    state.bag.pokeBall = min<uint8_t>(99, state.bag.pokeBall + amount);
    markDirty(true);
}

bool GameEngine::consumeBall() {
    if (state.bag.pokeBall == 0) return false;
    state.bag.pokeBall--;
    markDirty(true);
    return true;
}

bool GameEngine::addGreatBalls(uint8_t amount) {
    if (state.bag.greatBall >= 50) return false;
    state.bag.greatBall = min<uint8_t>(50, state.bag.greatBall + amount);
    markDirty(true);
    return true;
}

bool GameEngine::consumeGreatBall() {
    if (state.bag.greatBall == 0) return false;
    state.bag.greatBall--;
    markDirty(true);
    return true;
}

void GameEngine::addCandy(uint8_t amount) {
    state.bag.candy = min<uint8_t>(30, state.bag.candy + amount);
    markDirty(true);
}

bool GameEngine::addPotion(uint8_t amount) {
    if (state.bag.potion >= 30) return false;
    state.bag.potion = min<uint8_t>(30, state.bag.potion + amount);
    markDirty(true);
    return true;
}

bool GameEngine::addSuperPotion(uint8_t amount) {
    if (state.bag.superPotion >= 20) return false;
    state.bag.superPotion = min<uint8_t>(20, state.bag.superPotion + amount);
    markDirty(true);
    return true;
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
    if (state.bag.antidote >= 30) return false;
    state.bag.antidote = min<uint8_t>(30, state.bag.antidote + amount);
    markDirty(true);
    return true;
}

uint8_t GameEngine::itemCount(Game::ItemId item) const {
    switch (item) {
    case Game::ItemId::POKE_BALL: return state.bag.pokeBall;
    case Game::ItemId::GREAT_BALL: return state.bag.greatBall;
    case Game::ItemId::HEAVY_BALL: return state.bag.heavyBall;
    case Game::ItemId::TIMER_BALL: return state.bag.timerBall;
    case Game::ItemId::NORMAL_FOOD: return state.bag.normalFood;
    case Game::ItemId::POTION: return state.bag.potion;
    case Game::ItemId::SUPER_POTION: return state.bag.superPotion;
    case Game::ItemId::ANTIDOTE: return state.bag.antidote;
    case Game::ItemId::CANDY: return state.bag.candy;
    default: return 0;
    }
}

bool GameEngine::removeItem(Game::ItemId item, uint8_t amount, bool immediate) {
    if (amount == 0) return true;

    uint8_t* count = nullptr;
    switch (item) {
    case Game::ItemId::POKE_BALL: count = &state.bag.pokeBall; break;
    case Game::ItemId::GREAT_BALL: count = &state.bag.greatBall; break;
    case Game::ItemId::HEAVY_BALL: count = &state.bag.heavyBall; break;
    case Game::ItemId::TIMER_BALL: count = &state.bag.timerBall; break;
    case Game::ItemId::NORMAL_FOOD: count = &state.bag.normalFood; break;
    case Game::ItemId::POTION: count = &state.bag.potion; break;
    case Game::ItemId::SUPER_POTION: count = &state.bag.superPotion; break;
    case Game::ItemId::ANTIDOTE: count = &state.bag.antidote; break;
    case Game::ItemId::CANDY: count = &state.bag.candy; break;
    default: return false;
    }
    if (!count || *count < amount) return false;
    *count -= amount;
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
    state.coins += amount;
    if (state.coins > 99999) state.coins = 99999;
    markDirty(true);
}

bool GameEngine::recordCapture(uint16_t speciesId) {
    return recordCapture(createMonster(speciesId, 5));
}

bool GameEngine::recordCapture(const Game::MonsterRuntime& monster) {
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;

    Game::MonsterRuntime mon = monster;
    mon.hpMax = maxHpFor(*species, mon);
    mon.hpCur = mon.hpMax;
    mon.origin = Game::Origin::CAPTURED;
    mon.caughtAt = Hal::ins().millis() / 1000;
    mon.lastSeenAt = mon.caughtAt;

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
    Game::MonsterRuntime& mon = activeMonster();
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

void GameEngine::petMonster() {
    Game::MonsterRuntime& mon = activeMonster();
    if (mon.petCountToday < 4) {
        mon.petCountToday++;
        mon.mood = min<uint8_t>(100, mon.mood + 5);
        mon.affection = min<uint8_t>(255, mon.affection + 2);
        mon.lastPettedAt = Hal::ins().millis() / 1000;
        markDirty(true);
    }
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
    state.team[0].origin = Game::Origin::HATCHED;
    state.team[0].caughtAt = Hal::ins().millis() / 1000;
    state.team[0].lastSeenAt = state.team[0].caughtAt;
    state.oobeDone = true;
    markDirty(true);
    requestScene(SceneID::MAIN);
}

void GameEngine::addExperience(uint32_t amount) {
    if (amount == 0) return;
    Game::MonsterRuntime& mon = activeMonster();
    const Species& species = speciesFor(mon);
    uint8_t oldLevel = mon.level;
    uint16_t oldHpMax = mon.hpMax;
    uint32_t maxExp = minimumExpForLevel(species.growthRate, Game::LEVEL_MAX);
    mon.exp = min<uint32_t>(maxExp, mon.exp + amount);
    mon.level = levelForExp(species.growthRate, mon.exp);
    mon.hpMax = maxHpFor(species, mon);
    if (mon.hpMax > oldHpMax) {
        mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + (mon.hpMax - oldHpMax));
    }
    if (mon.level < oldLevel) mon.level = oldLevel;
    markDirty(false);
}

uint32_t GameEngine::applyActiveFaintPenalty() {
    Game::MonsterRuntime& mon = activeMonster();
    const Species& species = speciesFor(mon);
    uint32_t levelFloor = minimumExpForLevel(species.growthRate, mon.level);
    uint32_t availableLoss = mon.exp > levelFloor ? mon.exp - levelFloor : 0;
    uint32_t loss = mon.exp == 0 ? 0 : max<uint32_t>(1, mon.exp / 10);
    if (loss > availableLoss) loss = availableLoss;
    mon.exp -= loss;
    mon.hpCur = 0;
    mon.fainted = true;
    mon.lastSeenAt = Hal::ins().millis() / 1000;
    mon.affection = mon.affection > 5 ? mon.affection - 5 : 0;
    mon.mood = mon.mood > 10 ? mon.mood - 10 : 0;
    markDirty(true);
    return loss;
}

void GameEngine::addWalkSteps(uint16_t steps) {
    state.stepsToday = min<uint16_t>(60000, state.stepsToday + steps);
    uint16_t expGain = steps / 100;
    if (expGain > 0 && state.walkExpToday < 50) {
        expGain = min<uint16_t>(expGain, 50 - state.walkExpToday);
        addExperience(expGain);
        state.walkExpToday += expGain;
    }
    markDirty(false);
}

void GameEngine::debugRecoverActiveMonster() {
    if (state.teamCount == 0) return;
    Game::MonsterRuntime& mon = activeMonster();
    mon.fainted = false;
    mon.statusBits = Game::STATUS_NONE;
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
    syncSpriteCache();
    markDirty(true);
    return true;
}

void GameEngine::markDirty(bool immediate) {
    saveDirty = true;
    if (immediate) saveNow();
}

bool GameEngine::saveNow() {
    uint32_t now = Hal::ins().millis();
    syncGameClock(now);
    bool ok = saveManager.save(state);
    persistGameClock(now, true);
    saveDirty = !ok;
    lastSaveMs = now;
    return ok;
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
}

void GameEngine::processInput(uint32_t nowMs) {
    bool rawInputActive = Hal::ins().btnA_raw() || Hal::ins().btnB_raw();
    if (rawInputActive) resetIdle(nowMs);

    ButtonEvent events[ButtonDispatcher::MAX_EVENTS_PER_POLL];
    uint8_t count = ButtonDispatcher::ins().poll(events, ButtonDispatcher::MAX_EVENTS_PER_POLL);
    if (count > 0) resetIdle(nowMs);
    for (uint8_t i = 0; i < count; ++i) {
        if (currentScene && currentScene->onButton(events[i])) continue;
    }
}

void GameEngine::update(uint32_t nowMs) {
    float dt = (nowMs - lastUpdateMs) / 1000.0f;
    lastUpdateMs = nowMs;
    syncGameClock(nowMs);
    syncSpriteCache();
    persistGameClock(nowMs);
    tickCare(nowMs);
    if (currentScene) currentScene->update(nowMs, dt * gameSpeed());
    if (saveDirty && nowMs - lastSaveMs >= 300000UL) saveNow();
}

void GameEngine::render(uint32_t nowMs) {
    (void)nowMs;
    if (currentScene) currentScene->render();
    Hal::ins().flush();
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
    if (nowMs - lastActivityMs < timeout) return;
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
    const Species& starter = starterSpecies();
    state.teamCount = 1;
    state.activeSlot = 0;
    state.gameMinutesTotal = 0;
    state.team[0] = createMonster(starter.id, 5);
    state.team[0].origin = Game::Origin::STARTER;
}

void GameEngine::tickCare(uint32_t nowMs) {
    if (nowMs - lastCareMs < 60000UL) return;
    uint32_t elapsedMin = (nowMs - lastCareMs) / 60000UL;
    lastCareMs = nowMs;
    uint32_t scaledHpRecoveryMin = (uint32_t)((float)elapsedMin * gameSpeed());
    hpRecoveryMinuteAcc = min<uint32_t>(60000, hpRecoveryMinuteAcc + scaledHpRecoveryMin);
    uint16_t hpRecoveryTicks = hpRecoveryMinuteAcc / HP_RECOVERY_INTERVAL_MIN;
    hpRecoveryMinuteAcc %= HP_RECOVERY_INTERVAL_MIN;
    uint32_t nowSec = nowMs / 1000;

    for (uint8_t i = 0; i < state.teamCount; ++i) {
        Game::MonsterRuntime& mon = state.team[i];
        if (i == 0) {
            uint8_t satietyDrop = min<uint32_t>(elapsedMin, 4);
            mon.satiety = (mon.satiety > satietyDrop) ? mon.satiety - satietyDrop : 0;
        }
        uint8_t targetMood = mon.satiety > 60 ? 75 : (mon.satiety > 25 ? 55 : 35);
        if (mon.fainted) targetMood = 20;
        if (mon.mood < targetMood) mon.mood++;
        else if (mon.mood > targetMood) mon.mood--;

        if (mon.fainted) {
            if (mon.lastSeenAt == 0) mon.lastSeenAt = nowSec;
            uint32_t elapsedFaintSec = nowSec >= mon.lastSeenAt ? nowSec - mon.lastSeenAt : 0;
            uint32_t scaledFaintSec = (uint32_t)((float)elapsedFaintSec * gameSpeed());
            if (scaledFaintSec >= FAINT_RECOVERY_SECONDS) {
                mon.fainted = false;
                mon.hpCur = max<uint16_t>(1, mon.hpMax / 2);
            }
            continue;
        }

        if (hpRecoveryTicks > 0 && mon.hpCur < mon.hpMax && mon.satiety > 0) {
            uint16_t gainPerTick = max<uint16_t>(1, mon.hpMax / 20);
            uint32_t gain = (uint32_t)gainPerTick * hpRecoveryTicks;
            mon.hpCur = min<uint16_t>(mon.hpMax, mon.hpCur + gain);
        }
    }
    markDirty(false);
}

void GameEngine::syncSpriteCache() {
    uint16_t teamSpecies[Game::TEAM_CAP] = {};
    uint8_t count = state.teamCount;
    if (count > Game::TEAM_CAP) count = Game::TEAM_CAP;
    for (uint8_t i = 0; i < count; ++i) {
        teamSpecies[i] = state.team[i].speciesId;
    }
    PokemonSprites::syncTeamCache(teamSpecies, count);
}

uint32_t GameEngine::randomIvPacked() const {
    uint32_t packed = 0;
    for (uint8_t i = 0; i < Game::STAT_COUNT; ++i) {
        Game::setIv(packed, i, random(0, Game::IV_MAX + 1));
    }
    return packed;
}
