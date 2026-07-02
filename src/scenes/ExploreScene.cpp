#include "scenes/ExploreScene.h"
#include <cstdio>
#include <cstring>
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/BattleSystem.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
enum PickupId : uint8_t {
    PICKUP_NONE = 0,
    PICKUP_BALL,
    PICKUP_COIN,
    PICKUP_POTION,
    PICKUP_SUPER_POTION,
    PICKUP_ANTIDOTE,
    PICKUP_RARE_CANDY,
};

struct ExploreWeights {
    uint16_t none;
    uint16_t ball;
    uint16_t coin;
    uint16_t potion;
    uint16_t superPotion;
    uint16_t antidote;
    uint16_t candy;
    uint16_t encounter;
    uint8_t minSteps;
    uint8_t stepRange;
    uint8_t maxEvolutionStage;
    uint16_t maxBaseStatTotal;
};

static constexpr ExploreWeights BIOME_WEIGHTS[] = {
    // none ball coin potion super antidote candy battle steps range stage maxBST
    {1800, 2500, 2500, 1500, 200, 300, 30, 1170, 26, 55, 0, 330},
    {1500, 800, 2000, 2000, 800, 800, 50, 2050, 34, 65, 1, 430},
    {800, 500, 1000, 700, 1000, 700, 100, 5200, 42, 80, 2, 0},
};

static constexpr uint8_t WILD_LEVEL_MIN = 1;
static constexpr uint8_t WILD_LEVEL_MAX = 10;
static constexpr uint8_t WILD_LEVEL_COUNT = WILD_LEVEL_MAX - WILD_LEVEL_MIN + 1;
static constexpr uint8_t WILD_LEVEL_WEIGHTS[WILD_LEVEL_COUNT] = {
    2, 3, 4, 5, 6, 5, 4, 3, 2, 1,
};

const ExploreWeights& biomeWeights(ExploreScene::Biome biome) {
    uint8_t idx = static_cast<uint8_t>(biome);
    if (idx >= static_cast<uint8_t>(ExploreScene::Biome::COUNT)) idx = 0;
    return BIOME_WEIGHTS[idx];
}

const char* biomeName(ExploreScene::Biome biome) {
    uint8_t idx = static_cast<uint8_t>(biome);
    if (idx >= static_cast<uint8_t>(ExploreScene::Biome::COUNT)) idx = 0;
    return Ui::Explore::AREA_ITEMS[idx];
}

uint16_t biomeFieldColor(ExploreScene::Biome biome) {
    switch (biome) {
    case ExploreScene::Biome::RIVERSIDE: return PixelRenderer::rgb(36, 74, 82);
    case ExploreScene::Biome::DEEP_FOREST: return PixelRenderer::rgb(30, 58, 44);
    case ExploreScene::Biome::GRASS:
    default: return PixelRenderer::rgb(37, 71, 58);
    }
}

uint16_t biomeAccentColor(ExploreScene::Biome biome) {
    switch (biome) {
    case ExploreScene::Biome::RIVERSIDE: return PixelRenderer::rgb(77, 165, 196);
    case ExploreScene::Biome::DEEP_FOREST: return PixelRenderer::rgb(112, 164, 84);
    case ExploreScene::Biome::GRASS:
    default: return PixelRenderer::rgb(92, 222, 112);
    }
}

const Species* preEvolutionOf(const Species& species) {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    for (uint8_t i = 0; i < count; ++i) {
        if (table[i].evolveTo == species.id) return &table[i];
    }
    // Eevee uses a branch evolution in this data table, so only one target can be
    // represented by evolveTo. Keep the encounter stage filter aware of all branches.
    switch (species.id) {
    case 134:
    case 135:
    case 136:
    case 196:
    case 197:
    case 470:
    case 471:
        return findSpecies(133);
    default:
        break;
    }
    return nullptr;
}

uint8_t evolutionStage(const Species& species) {
    uint8_t stage = 0;
    const Species* current = &species;
    while (const Species* pre = preEvolutionOf(*current)) {
        if (++stage >= 2) return stage;
        current = pre;
    }
    return stage;
}

uint16_t baseStatTotal(const Species& species) {
    return species.stats.hp + species.stats.atk + species.stats.def +
           species.stats.spa + species.stats.spd + species.stats.spe;
}

uint8_t stageFallbackMinLevel(uint8_t stage) {
    if (stage >= 2) return 30;
    if (stage == 1) return 16;
    return 3;
}

uint8_t minEncounterLevel(const Species& species) {
    const Species* pre = preEvolutionOf(species);
    if (!pre) return WILD_LEVEL_MIN;
    if (pre->evolveMethod == EvolutionMethod::LEVEL && pre->evolveLevel > 0) {
        return pre->evolveLevel;
    }
    return stageFallbackMinLevel(evolutionStage(species));
}

uint8_t maxEncounterLevel(const Species& species) {
    if (species.evolveMethod == EvolutionMethod::LEVEL && species.evolveLevel > 0) {
        return species.evolveLevel > 3 ? species.evolveLevel - 1 : species.evolveLevel;
    }
    if (species.evolveTo != 0) {
        uint8_t nextStageMin = stageFallbackMinLevel(evolutionStage(species) + 1);
        return nextStageMin > 3 ? nextStageMin - 1 : nextStageMin;
    }
    return Game::LEVEL_MAX;
}

uint8_t clampEncounterLevel(const Species& species, int level, const ExploreWeights& weights) {
    (void)weights;
    uint8_t minLevel = max<uint8_t>(WILD_LEVEL_MIN, minEncounterLevel(species));
    uint8_t maxLevel = min<uint8_t>(WILD_LEVEL_MAX, maxEncounterLevel(species));
    if (maxLevel < minLevel) maxLevel = minLevel;
    if (level < minLevel) level = minLevel;
    if (level > maxLevel) level = maxLevel;
    return (uint8_t)level;
}

bool canEncounterInBiome(const Species& species, int level, const ExploreWeights& weights) {
    if (evolutionStage(species) > weights.maxEvolutionStage) return false;
    if (weights.maxBaseStatTotal > 0 && baseStatTotal(species) > weights.maxBaseStatTotal) return false;
    uint8_t minLevel = max<uint8_t>(WILD_LEVEL_MIN, minEncounterLevel(species));
    uint8_t maxLevel = min<uint8_t>(WILD_LEVEL_MAX, maxEncounterLevel(species));
    return maxLevel >= minLevel && level >= minLevel && level <= maxLevel;
}

uint8_t rollWildLevel() {
    uint16_t total = 0;
    for (uint8_t weight : WILD_LEVEL_WEIGHTS) total += weight;

    uint16_t r = random(0, total);
    for (uint8_t i = 0; i < WILD_LEVEL_COUNT; ++i) {
        if (r < WILD_LEVEL_WEIGHTS[i]) return WILD_LEVEL_MIN + i;
        r -= WILD_LEVEL_WEIGHTS[i];
    }
    return 5;
}
}

void ExploreScene::onEnter() {
    phase = Phase::SELECT;
    biomeCursor = 0;
    resultMessage = nullptr;
    exitAfterFaint = false;
}

void ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    serviceBattleLog(nowMs);
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }

    if (event.action != BtnAction::PRESSED) return false;

    if (phase == Phase::SELECT) {
        uint8_t optionCount = static_cast<uint8_t>(Biome::COUNT) + 1;
        if (event.btn == 0) {
            if (biomeCursor >= static_cast<uint8_t>(Biome::COUNT)) {
                GameEngine::ins().requestScene(SceneID::MENU);
            } else {
                activeBiome = static_cast<Biome>(biomeCursor);
                resetWalk();
            }
            return true;
        }
        if (event.btn == 1) {
            biomeCursor = (biomeCursor + 1) % optionCount;
            return true;
        }
    }

    if (phase == Phase::WALKING) {
        if (event.btn == 0) {
            walk();
            return true;
        }
        if (event.btn == 1) {
            GameEngine::ins().requestScene(SceneID::MENU);
            return true;
        }
    }

    if (phase == Phase::ENCOUNTER) {
        if (battleLogBusy()) return true;
        if (event.btn == 0) {
            if (battleCursor == 0) attackWild();
            else if (battleCursor == 1) tryCapture();
            else fleeEncounter();
            if (exitAfterFaint) {
                GameEngine::ins().requestScene(SceneID::MENU);
                return true;
            }
            return true;
        }
        if (event.btn == 1) {
            battleCursor = (battleCursor + 1) % 3;
            return true;
        }
    }

    if (phase == Phase::RESULT) {
        if (event.btn == 0) {
            resetWalk();
            return true;
        }
        if (event.btn == 1) {
            GameEngine::ins().requestScene(SceneID::MENU);
            return true;
        }
    }

    return false;
}

void ExploreScene::walk() {
    uint8_t stepGain = 8 + random(0, 9);
    steps += stepGain;
    GameEngine::ins().addWalkSteps(stepGain);

    bool triggered = rollSceneEvent(false);
    if (phase == Phase::ENCOUNTER) return;

    if (steps >= targetSteps) {
        if (!triggered) rollSceneEvent(true);
        if (phase == Phase::ENCOUNTER) return;
        resetRouteSegment();
        triggered = true;
    }

    (void)triggered;
}

bool ExploreScene::rollSceneEvent(bool forceEvent) {
    uint32_t r = random(0, 10000);
    const ExploreWeights& weights = biomeWeights(activeBiome);
    uint32_t cursor = weights.none;
    if (!forceEvent && r < cursor) return false;

    uint8_t pickup = PICKUP_NONE;
    if (forceEvent && r < cursor) r = cursor;
    cursor += weights.ball;
    if (r < cursor) pickup = PICKUP_BALL;
    else {
        cursor += weights.coin;
        if (r < cursor) pickup = PICKUP_COIN;
        else {
            cursor += weights.potion;
            if (r < cursor) pickup = PICKUP_POTION;
            else {
                cursor += weights.superPotion;
                if (r < cursor) pickup = PICKUP_SUPER_POTION;
                else {
                    cursor += weights.antidote;
                    if (r < cursor) pickup = PICKUP_ANTIDOTE;
                    else {
                        cursor += weights.candy;
                        if (r < cursor && GameEngine::ins().gameState().stepsToday >= 5000) pickup = PICKUP_RARE_CANDY;
                        else {
                            cursor += weights.encounter;
                            if (r < cursor) {
                                rollEncounter();
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (pickup == PICKUP_NONE) return false;
    resolvePickup(pickup);
    return true;
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    switch (pickupId) {
    case PICKUP_BALL:
        GameEngine::ins().addBalls(1);
        break;
    case PICKUP_COIN: {
        uint8_t level = GameEngine::ins().activeMonster().level;
        uint32_t upper = 10 + min<uint8_t>(40, level);
        uint32_t coins = random(10, upper + 1);
        GameEngine::ins().addCoins(coins);
        return;
    }
    case PICKUP_POTION:
        GameEngine::ins().addPotion(1);
        break;
    case PICKUP_SUPER_POTION:
        GameEngine::ins().addSuperPotion(1);
        break;
    case PICKUP_ANTIDOTE:
        GameEngine::ins().addAntidote(1);
        break;
    case PICKUP_RARE_CANDY:
        GameEngine::ins().addCandy(1);
        break;
    default:
        return;
    }
}

void ExploreScene::rollEncounter() {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    const ExploreWeights& weights = biomeWeights(activeBiome);
    int wildLevel = rollWildLevel();

    uint8_t candidates[32];
    uint8_t candidateCount = 0;
    for (uint8_t i = 0; i < count && candidateCount < sizeof(candidates); ++i) {
        if (canEncounterInBiome(table[i], wildLevel, weights)) candidates[candidateCount++] = i;
    }
    if (candidateCount == 0) {
        for (uint8_t i = 0; i < count && candidateCount < sizeof(candidates); ++i) {
            if (evolutionStage(table[i]) <= weights.maxEvolutionStage) candidates[candidateCount++] = i;
        }
    }

    wild = candidateCount > 0 ? &table[candidates[random(0, candidateCount)]] : &table[random(0, count)];
    uint8_t clampedLevel = clampEncounterLevel(*wild, wildLevel, weights);
    wildRuntime = GameEngine::ins().createMonster(wild->id, clampedLevel);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    battleCursor = 0;
    fleeAttempts = 0;
    phase = Phase::ENCOUNTER;
    clearBattleLogs();
    enqueueBattleLog(Ui::Explore::WILD_APPEARED);
}

void ExploreScene::clearBattleLogs() {
    battleLogHead = 0;
    battleLogCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleResultPending = false;
    for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
        battleLogVisible[i][0] = '\0';
    }
}

void ExploreScene::enqueueBattleLog(const char* text) {
    if (!text || !text[0]) return;
    if (battleLogCount >= BATTLE_LOG_QUEUE_CAP) {
        battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
        battleLogCount--;
    }
    uint8_t tail = (battleLogHead + battleLogCount) % BATTLE_LOG_QUEUE_CAP;
    strncpy(battleLogQueue[tail], text, BATTLE_LOG_LEN - 1);
    battleLogQueue[tail][BATTLE_LOG_LEN - 1] = '\0';
    battleLogCount++;
    serviceBattleLog(Hal::ins().millis());
}

void ExploreScene::serviceBattleLog(uint32_t nowMs) {
    if (battleLogActive && (int32_t)(nowMs - battleLogUntil) < 0) return;
    if (battleLogCount == 0) {
        battleLogActive = false;
        battleLogVisibleCount = 0;
        for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            battleLogVisible[i][0] = '\0';
        }
        if (battleResultPending) {
            battleResultPending = false;
            phase = Phase::RESULT;
        }
        return;
    }

    if (battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        battleLogVisibleCount++;
    } else {
        strncpy(battleLogVisible[0], battleLogVisible[1], BATTLE_LOG_LEN - 1);
        battleLogVisible[0][BATTLE_LOG_LEN - 1] = '\0';
    }
    uint8_t line = battleLogVisibleCount - 1;
    strncpy(battleLogVisible[line], battleLogQueue[battleLogHead], BATTLE_LOG_LEN - 1);
    battleLogVisible[line][BATTLE_LOG_LEN - 1] = '\0';
    battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
    battleLogCount--;
    battleLogActive = true;
    battleLogUntil = nowMs + 1000;
}

bool ExploreScene::battleLogBusy() const {
    return battleLogActive || battleLogCount > 0 || battleResultPending;
}

void ExploreScene::attackWild() {
    if (!wild || wildHp == 0) return;
    auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }
    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    wildRuntime.hpCur = wildHp;
    bool specialMove = BattleSystem::rollSpecialMove(activeMon);
    auto result = BattleSystem::calcBasicDamage(activeMon, activeSpecies, wildRuntime, *wild, specialMove);
    wildHp = result.damage >= wildHp ? 0 : wildHp - result.damage;
    wildRuntime.hpCur = wildHp;

    if (result.statusBlocked) {
        enqueueBattleLog(Ui::Explore::CANNOT_MOVE);
    } else if (result.effectiveness == 0) {
        const MoveInfo* move = findMove(result.special ? activeSpecies.specialMoveId : activeSpecies.basicMoveId);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
    } else {
        const MoveInfo* move = findMove(result.special ? activeSpecies.specialMoveId : activeSpecies.basicMoveId);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        if (result.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
        if (result.effectiveness > 100) enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
        else if (result.effectiveness < 100) enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::DAMAGE_FMT, result.damage);
        enqueueBattleLog(logBuf);
        if (result.damage > 0 && activeMon.proficiency < 100) {
            activeMon.proficiency++;
            GameEngine::ins().markDirty(false);
        }
    }

    if (wildHp == 0) {
        uint16_t expGain = max<uint16_t>(1, wildRuntime.level * 3);
        GameEngine::ins().addExperience(expGain);
        GameEngine::ins().grantEffortFrom(*wild);
        GameEngine::ins().addCoins(10);
        enqueueBattleLog(Ui::Explore::BATTLE_WIN);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::EXP_GAIN_FMT, expGain);
        enqueueBattleLog(logBuf);
        snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::BATTLE_WIN_FMT, expGain);
        resultMessage = resultBuf;
        battleResultPending = true;
        lastCaptureSuccess = false;
        return;
    }

    wildCounterattack();
}

void ExploreScene::wildCounterattack() {
    if (!wild || wildHp == 0) return;
    auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }

    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    bool specialMove = BattleSystem::rollSpecialMove(wildRuntime);
    auto result = BattleSystem::calcBasicDamage(wildRuntime, *wild, activeMon, activeSpecies, specialMove);
    activeMon.hpCur = result.damage >= activeMon.hpCur ? 0 : activeMon.hpCur - result.damage;
    GameEngine::ins().markDirty(false);

    if (activeMon.hpCur == 0) {
        finishPlayerFaint();
        return;
    }

    if (result.statusBlocked) {
        enqueueBattleLog(Ui::Explore::WILD_CANNOT_MOVE);
    } else if (result.effectiveness == 0) {
        const MoveInfo* move = findMove(result.special ? wild->specialMoveId : wild->basicMoveId);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
    } else {
        const MoveInfo* move = findMove(result.special ? wild->specialMoveId : wild->basicMoveId);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        if (result.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
        if (result.effectiveness > 100) enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
        else if (result.effectiveness < 100) enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_DAMAGE_FMT, result.damage);
        enqueueBattleLog(logBuf);
    }
}

void ExploreScene::finishPlayerFaint() {
    uint32_t loss = GameEngine::ins().applyActiveFaintPenalty();
    snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, (unsigned long)loss);
    resultMessage = resultBuf;
    lastCaptureSuccess = false;
    wild = nullptr;
    wildHp = wildHpMax = 0;
    exitAfterFaint = true;
}

void ExploreScene::tryCapture() {
    if (!wild) return;
    if (!GameEngine::ins().consumeBall()) {
        enqueueBattleLog(Ui::Explore::NO_BALLS);
        return;
    }

    uint8_t chance = 35;
    if (wild->evolveTo == 0) chance = 25;
    if (wild->stats.hp < 45) chance += 20;
    uint8_t hpMissing = wildHpMax > 0 ? (uint8_t)((wildHpMax - wildHp) * 50 / wildHpMax) : 0;
    chance = min<uint8_t>(95, chance + hpMissing);
    lastCaptureSuccess = random(0, 100) < chance;
    if (lastCaptureSuccess) {
        wildRuntime.hpCur = wildHp;
        if (GameEngine::ins().recordCapture(wildRuntime)) {
            resultMessage = wild ? wild->name : nullptr;
        } else {
            lastCaptureSuccess = false;
            resultMessage = Ui::Shop::BAG_FULL;
        }
    } else {
        enqueueBattleLog(Ui::Explore::BROKE_FREE);
        wildCounterattack();
        if (phase == Phase::RESULT) return;
        return;
    }
    phase = Phase::RESULT;
}

void ExploreScene::fleeEncounter() {
    if (!wild) return;
    const auto& activeMon = GameEngine::ins().activeMonster();
    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    uint16_t activeSpeed = statFor(activeSpecies, activeMon, 5);
    uint16_t wildSpeed = statFor(*wild, wildRuntime, 5);
    bool escaped = wildSpeed == 0 || activeSpeed >= wildSpeed;
    if (!escaped) {
        fleeAttempts++;
        uint16_t odds = (((uint32_t)activeSpeed * 128) / wildSpeed + 30 * fleeAttempts) & 0xFF;
        escaped = random(0, 256) < odds;
    }

    if (!escaped) {
        enqueueBattleLog(Ui::Explore::FLEE_FAILED);
        wildCounterattack();
        return;
    }

    GameEngine::ins().addCoins(2);
    lastCaptureSuccess = false;
    phase = Phase::RESULT;
    resultMessage = Ui::Explore::RELEASED;
}

void ExploreScene::resetWalk() {
    phase = Phase::WALKING;
    resetRouteSegment();
    wild = nullptr;
    wildHp = wildHpMax = 0;
    battleResultPending = false;
    resultMessage = nullptr;
}

void ExploreScene::resetRouteSegment() {
    const ExploreWeights& weights = biomeWeights(activeBiome);
    steps = 0;
    targetSteps = weights.minSteps + random(0, weights.stepRange + 1);
}

void ExploreScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(11, 22, 27));

    switch (phase) {
    case Phase::SELECT: renderBiomeMenu(); break;
    case Phase::WALKING: renderWalking(); break;
    case Phase::ENCOUNTER: renderEncounter(); break;
    case Phase::RESULT: renderResult(); break;
    }
}

void ExploreScene::renderBiomeMenu() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);
    int rowStep = 28;
    int startY = 10;
    int sepGap = 6;
    uint8_t count = static_cast<uint8_t>(Biome::COUNT) + 1;
    for (uint8_t i = 0; i < count; ++i) {
        int y = startY + i * rowStep;
        bool active = i == biomeCursor;
        uint16_t color = active ? 0xFFE0 : 0xFFFF;
        if (active) c.fillRect(4, y, 4, 16, 0xFFE0);
        PixelRenderer::text(14, y, Ui::Explore::AREA_ITEMS[i], color, 1);
        PixelRenderer::text(112, y, Ui::Explore::AREA_DESCS[i],
                            active ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF, 1);
        if (i < count - 1) c.fillRect(4, y + rowStep - sepGap, Hal::DISPLAY_W - 8, 1, 0x7BEF);
    }
}

void ExploreScene::renderWalking() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(10, 28, 160, 88, biomeFieldColor(activeBiome));
    c.drawRect(10, 28, 160, 88, PixelRenderer::rgb(94, 135, 99));
    for (int i = 0; i < 10; ++i) {
        int x = 18 + i * 15;
        int h = 16 + (i % 3) * 7;
        c.fillTriangle(x, 104, x + 8, 104 - h, x + 16, 104, PixelRenderer::rgb(58, 120, 75));
    }
    PixelRenderer::text(24, 36, biomeName(activeBiome), PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::bar(24, 58, 128, 9, (steps * 100) / targetSteps,
                       biomeAccentColor(activeBiome), PixelRenderer::rgb(51, 61, 52));

    char buf[32];
    snprintf(buf, sizeof(buf), Ui::Explore::STEP_FMT, steps, targetSteps);
    PixelRenderer::text(46, 76, buf, PixelRenderer::rgb(198, 215, 193), 1);
    c.fillRect(182, 40, 42, 44, PixelRenderer::rgb(72, 83, 98));
    c.fillRect(191, 50, 24, 24, biomeAccentColor(activeBiome));
}

void ExploreScene::renderEncounter() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(197, 220, 192));
    c.fillRect(0, 78, Hal::DISPLAY_W, 24, PixelRenderer::rgb(228, 225, 190));
    c.fillEllipse(174, 77, 34, 8, PixelRenderer::rgb(155, 181, 142));
    c.fillEllipse(58, 101, 38, 9, PixelRenderer::rgb(171, 159, 128));

    if (wild) drawMonsterBlock(*wild, 174, 45);
    drawMonsterBlock(GameEngine::ins().activeSpecies(), 58, 70, true);
    renderBattleHud();
    renderCommandBox();
}

void ExploreScene::renderResult() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(42, 42, 156, 58, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(42, 42, 156, 58, PixelRenderer::rgb(95, 110, 126));
    PixelRenderer::text(58, 58, lastCaptureSuccess ? Ui::Explore::CAPTURE_SUCCESS : Ui::Explore::RESULT_END,
                        lastCaptureSuccess ? PixelRenderer::rgb(92, 222, 112) : PixelRenderer::rgb(241, 242, 232), 1);
    if (resultMessage && resultMessage[0]) {
        PixelRenderer::text(58, 78, resultMessage, PixelRenderer::rgb(255, 216, 72), 1);
    } else if (wild) {
        PixelRenderer::text(58, 78, wild->name, PixelRenderer::rgb(255, 216, 72), 1);
    }
}

void ExploreScene::drawWildBlock(int x, int y) {
    if (!wild) return;
    drawMonsterBlock(*wild, x, y);
}

void ExploreScene::drawMonsterBlock(const Species& species, int x, int y, bool back) {
    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 32, 25, 7, PixelRenderer::rgb(23, 27, 34));
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, back ? PokemonSprites::SpriteKind::BACK : PokemonSprites::SpriteKind::FRONT);
    if (frame) {
        uint8_t w = pgm_read_byte(&frame->width);
        uint8_t h = pgm_read_byte(&frame->height);
        PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2);
        return;
    }
    c.fillRect(x - 18, y - 22, 36, 40, species.colorA);
    c.fillRect(x - 11, y - 13, 22, 23, species.colorB);
    c.fillCircle(x - 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
    c.fillCircle(x + 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
}

void ExploreScene::renderBattleHud() {
    auto& c = PixelRenderer::canvas();
    char buf[24];

    if (wild) {
        c.fillRect(6, 8, 88, 36, PixelRenderer::rgb(248, 248, 232));
        c.drawRect(6, 8, 88, 36, PixelRenderer::rgb(74, 91, 75));
        PixelRenderer::text(10, 10, wild->name, PixelRenderer::rgb(25, 31, 40), 1);
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, wildRuntime.level);
        PixelRenderer::text(62, 10, buf, PixelRenderer::rgb(25, 31, 40), 1);
        PixelRenderer::bar(14, 31, 70, 7, wildHpMax ? (wildHp * 100 / wildHpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    }

    const auto& active = GameEngine::ins().activeMonster();
    const Species& species = GameEngine::ins().activeSpecies();
    c.fillRect(126, 62, 108, 36, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(126, 62, 108, 36, PixelRenderer::rgb(74, 91, 75));
    PixelRenderer::text(132, 64, species.name, PixelRenderer::rgb(25, 31, 40), 1);
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, active.level);
    PixelRenderer::text(198, 64, buf, PixelRenderer::rgb(25, 31, 40), 1);
    PixelRenderer::bar(134, 82, 56, 7, active.hpMax ? (active.hpCur * 100 / active.hpMax) : 0,
                       PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    snprintf(buf, sizeof(buf), Ui::Explore::WILD_HP_FMT, active.hpCur, active.hpMax);
    PixelRenderer::text(194, 78, buf, PixelRenderer::rgb(25, 31, 40), 1);
}

void ExploreScene::renderCommandBox() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 101, Hal::DISPLAY_W, 34, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(0, 101, Hal::DISPLAY_W, 34, PixelRenderer::rgb(74, 91, 75));

    serviceBattleLog(Hal::ins().millis());
    if (phase != Phase::ENCOUNTER) return;
    if (battleLogActive || battleLogVisibleCount > 0) {
        uint8_t start = battleLogVisibleCount > BATTLE_LOG_VISIBLE_CAP
            ? battleLogVisibleCount - BATTLE_LOG_VISIBLE_CAP
            : 0;
        for (uint8_t i = start; i < battleLogVisibleCount; ++i) {
            int y = 104 + (i - start) * 15;
            PixelRenderer::text(12, y, battleLogVisible[i], PixelRenderer::rgb(25, 31, 40), 1);
        }
        return;
    }

    static constexpr int xs[] = {30, 104, 178};
    static constexpr int ys[] = {110, 110, 110};
    static constexpr const char* items[] = {
        Ui::Explore::CMD_ATTACK,
        Ui::Explore::CMD_BAG,
        Ui::Explore::CMD_FLEE,
    };
    for (uint8_t i = 0; i < 3; ++i) {
        if (battleCursor == i) PixelRenderer::text(xs[i] - 14, ys[i], ">", PixelRenderer::rgb(25, 31, 40), 1);
        PixelRenderer::text(xs[i], ys[i], items[i], PixelRenderer::rgb(25, 31, 40), 1);
    }
}
