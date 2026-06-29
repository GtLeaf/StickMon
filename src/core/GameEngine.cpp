#include "core/GameEngine.h"
#include <Arduino.h>
#include <algorithm>
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
    if (!saveManager.load(state)) {
        initDefaultState();
        saveNow();
    }
    if (state.settings.idleTimeoutIndex >= 5) {
        state.settings.idleTimeoutIndex = 0;
        markDirty(false);
    }
    Hal::ins().setBrightness(state.settings.brightness);
    ButtonDispatcher::ins().setLongPressMs(state.settings.longPressMs);
    EspNowLink::ins().beginStub();
    switchScene(state.oobeDone ? SceneID::MAIN : SceneID::HATCH);
    lastInputMs = lastFrameMs = lastUpdateMs = lastCareMs = lastSaveMs = lastActivityMs = Hal::ins().millis();
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
        if (!idleActive || !idleRendered) {
            render(now);
            idleRendered = idleActive;
        }
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

void GameEngine::cycleGameSpeed() {
    state.settings.speedIndex = (state.settings.speedIndex + 1) % 4;
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
    uint8_t slot = state.activeSlot;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) slot = 0;
    return state.team[slot];
}

Game::MonsterRuntime& GameEngine::activeMonster() {
    uint8_t slot = state.activeSlot;
    if (slot >= state.teamCount || slot >= Game::TEAM_CAP) slot = 0;
    return state.team[slot];
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
    state.activeSlot = (state.activeSlot + 1) % state.teamCount;
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

bool GameEngine::addAntidote(uint8_t amount) {
    if (state.bag.antidote >= 30) return false;
    state.bag.antidote = min<uint8_t>(30, state.bag.antidote + amount);
    markDirty(true);
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
    uint16_t starterId = 906;
    if (starterStyle == 1) starterId = 4;
    else if (starterStyle == 2) starterId = 656;
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

void GameEngine::addExperience(uint16_t amount) {
    if (amount == 0) return;
    Game::MonsterRuntime& mon = activeMonster();
    mon.exp = min<uint16_t>(60000, mon.exp + amount);
    markDirty(false);
}

uint16_t GameEngine::applyActiveFaintPenalty() {
    Game::MonsterRuntime& mon = activeMonster();
    uint16_t loss = mon.exp == 0 ? 0 : max<uint16_t>(1, mon.exp / 10);
    if (loss > mon.exp) loss = mon.exp;
    mon.exp -= loss;
    mon.hpCur = 0;
    mon.fainted = true;
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

void GameEngine::markDirty(bool immediate) {
    saveDirty = true;
    if (immediate) saveNow();
}

bool GameEngine::saveNow() {
    bool ok = saveManager.save(state);
    saveDirty = !ok;
    lastSaveMs = Hal::ins().millis();
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
    if (currentScene) currentScene->onExit();
    prevId = currentId;
    currentId = id;

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
    resetIdle(Hal::ins().millis());
}

void GameEngine::processInput(uint32_t nowMs) {
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
    idleRendered = false;
    if (idleActive) {
        idleActive = false;
        Hal::ins().setIdleBrightness(false);
    }
}

void GameEngine::updateIdle(uint32_t nowMs) {
    uint32_t timeout = idleTimeoutMs();
    if (timeout == 0 || idleActive) return;
    if (nowMs - lastActivityMs < timeout) return;
    idleActive = true;
    idleRendered = false;
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

void GameEngine::initDefaultState() {
    saveManager.reset(state);
    const Species& starter = starterSpecies();
    state.teamCount = 1;
    state.activeSlot = 0;
    state.team[0] = createMonster(starter.id, 5);
    state.team[0].origin = Game::Origin::STARTER;
}

void GameEngine::tickCare(uint32_t nowMs) {
    if (nowMs - lastCareMs < 60000UL) return;
    uint32_t elapsedMin = (nowMs - lastCareMs) / 60000UL;
    lastCareMs = nowMs;
    for (uint8_t i = 0; i < state.teamCount; ++i) {
        Game::MonsterRuntime& mon = state.team[i];
        uint8_t satietyDrop = min<uint32_t>(elapsedMin, 4);
        mon.satiety = (mon.satiety > satietyDrop) ? mon.satiety - satietyDrop : 0;
        uint8_t targetMood = mon.satiety > 60 ? 75 : (mon.satiety > 25 ? 55 : 35);
        if (mon.fainted) targetMood = 20;
        if (mon.mood < targetMood) mon.mood++;
        else if (mon.mood > targetMood) mon.mood--;
    }
    markDirty(false);
}

uint32_t GameEngine::randomIvPacked() const {
    uint32_t packed = 0;
    for (uint8_t i = 0; i < Game::STAT_COUNT; ++i) {
        Game::setIv(packed, i, random(0, Game::IV_MAX + 1));
    }
    return packed;
}
