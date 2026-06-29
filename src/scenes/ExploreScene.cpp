#include "scenes/ExploreScene.h"
#include <cstdio>
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
}

void ExploreScene::onEnter() {
    resetWalk();
}

void ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }

    if (event.action != BtnAction::PRESSED) return false;

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
        if (event.btn == 0) {
            if (battleCursor == 0) attackWild();
            else if (battleCursor == 1) tryCapture();
            else fleeEncounter();
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
    if (steps >= targetSteps) {
        rollEncounter();
    } else {
        rollPickupEvent();
        if (!toast || Hal::ins().millis() > toastUntil) {
            toast = Ui::Explore::CONTINUE;
            toastUntil = Hal::ins().millis() + 650;
        }
    }
}

void ExploreScene::rollPickupEvent() {
    uint32_t r = random(0, 10000);
    uint8_t pickup = PICKUP_NONE;
    if (r < 30 && GameEngine::ins().gameState().stepsToday >= 5000) pickup = PICKUP_RARE_CANDY;
    else if (r < 130) pickup = PICKUP_SUPER_POTION;
    else if (r < 330) pickup = PICKUP_ANTIDOTE;
    else if (r < 630) pickup = PICKUP_POTION;
    else if (r < 1130) pickup = PICKUP_BALL;
    else if (r < 1930) pickup = PICKUP_COIN;
    if (pickup != PICKUP_NONE) resolvePickup(pickup);
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    bool ok = true;
    const char* itemName = nullptr;
    switch (pickupId) {
    case PICKUP_BALL:
        GameEngine::ins().addBalls(1);
        itemName = Ui::Explore::PICKUP_BALL;
        break;
    case PICKUP_COIN: {
        uint8_t level = GameEngine::ins().activeMonster().level;
        uint32_t upper = 10 + min<uint8_t>(40, level);
        uint32_t coins = random(10, upper + 1);
        GameEngine::ins().addCoins(coins);
        snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::PICKUP_COIN_FMT, (unsigned long)coins);
        toast = toastBuf;
        toastUntil = Hal::ins().millis() + 1500;
        return;
    }
    case PICKUP_POTION:
        ok = GameEngine::ins().addPotion(1);
        itemName = Ui::Explore::PICKUP_POTION;
        break;
    case PICKUP_SUPER_POTION:
        ok = GameEngine::ins().addSuperPotion(1);
        itemName = Ui::Explore::PICKUP_SUPER_POTION;
        break;
    case PICKUP_ANTIDOTE:
        ok = GameEngine::ins().addAntidote(1);
        itemName = Ui::Explore::PICKUP_ANTIDOTE;
        break;
    case PICKUP_RARE_CANDY:
        GameEngine::ins().addCandy(1);
        itemName = Ui::Explore::PICKUP_CANDY;
        break;
    default:
        return;
    }

    toast = toastBuf;
    if (ok) snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::PICKUP_FMT, itemName);
    else toast = Ui::Shop::BAG_FULL;
    toastUntil = Hal::ins().millis() + 1500;
}

void ExploreScene::rollEncounter() {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    wild = &table[random(0, count)];
    int wildLevel = (int)GameEngine::ins().activeMonster().level + (int)random(-1, 2);
    if (wildLevel < 3) wildLevel = 3;
    if (wildLevel > Game::LEVEL_MAX) wildLevel = Game::LEVEL_MAX;
    wildRuntime = GameEngine::ins().createMonster(wild->id, (uint8_t)wildLevel);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    battleCursor = 0;
    phase = Phase::ENCOUNTER;
    toast = Ui::Explore::WILD_APPEARED;
    toastUntil = Hal::ins().millis() + 1200;
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
        toast = Ui::Explore::CANNOT_MOVE;
    } else if (result.effectiveness == 0) {
        toast = Ui::Explore::NO_EFFECT;
    } else if (result.special) {
        const MoveInfo* move = findMove(activeSpecies.specialMoveId);
        snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::SPECIAL_DAMAGE_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN,
                 result.damage);
        toast = toastBuf;
    } else {
        snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::DAMAGE_FMT, result.damage);
        toast = toastBuf;
        if (result.damage > 0 && activeMon.proficiency < 100) {
            activeMon.proficiency++;
            GameEngine::ins().markDirty(false);
        }
    }
    if (wildHp == 0) toast = nullptr;
    toastUntil = Hal::ins().millis() + 1000;

    if (wildHp == 0) {
        uint16_t expGain = max<uint16_t>(1, wildRuntime.level * 3);
        GameEngine::ins().addExperience(expGain);
        GameEngine::ins().grantEffortFrom(*wild);
        GameEngine::ins().addCoins(10);
        snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::BATTLE_WIN_FMT, expGain);
        toast = toastBuf;
        phase = Phase::RESULT;
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
        toast = Ui::Explore::WILD_CANNOT_MOVE;
    } else if (result.effectiveness == 0) {
        toast = Ui::Explore::NO_EFFECT;
    } else {
        snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::WILD_DAMAGE_FMT, result.damage);
        toast = toastBuf;
    }
    toastUntil = Hal::ins().millis() + 1000;
}

void ExploreScene::finishPlayerFaint() {
    uint16_t loss = GameEngine::ins().applyActiveFaintPenalty();
    snprintf(toastBuf, sizeof(toastBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, loss);
    toast = toastBuf;
    toastUntil = Hal::ins().millis() + 1500;
    lastCaptureSuccess = false;
    phase = Phase::RESULT;
}

void ExploreScene::tryCapture() {
    if (!wild) return;
    if (!GameEngine::ins().consumeBall()) {
        toast = Ui::Explore::NO_BALLS;
        toastUntil = Hal::ins().millis() + 1200;
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
            toast = Ui::Explore::CAPTURE_SUCCESS;
        } else {
            lastCaptureSuccess = false;
            toast = Ui::Shop::BAG_FULL;
        }
    } else {
        toast = Ui::Explore::BROKE_FREE;
        toastUntil = Hal::ins().millis() + 900;
        wildCounterattack();
        if (phase == Phase::RESULT) return;
        return;
    }
    phase = Phase::RESULT;
    toastUntil = Hal::ins().millis() + 1200;
}

void ExploreScene::fleeEncounter() {
    GameEngine::ins().addCoins(2);
    lastCaptureSuccess = false;
    phase = Phase::RESULT;
    toast = Ui::Explore::RELEASED;
    toastUntil = Hal::ins().millis() + 1200;
}

void ExploreScene::resetWalk() {
    phase = Phase::WALKING;
    steps = 0;
    targetSteps = 30 + random(0, 91);
    wild = nullptr;
    wildHp = wildHpMax = 0;
    toast = Ui::Explore::START;
    toastUntil = Hal::ins().millis() + 900;
}

void ExploreScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(11, 22, 27));
    if (phase != Phase::ENCOUNTER) {
        c.fillRect(0, 0, Hal::DISPLAY_W, 24, PixelRenderer::rgb(25, 25, 40));
        PixelRenderer::text(4, 5, Ui::EXPLORE, PixelRenderer::rgb(67, 213, 224), 1);
    }

    switch (phase) {
    case Phase::WALKING: renderWalking(); break;
    case Phase::ENCOUNTER: renderEncounter(); break;
    case Phase::RESULT: renderResult(); break;
    }
    renderToast();
}

void ExploreScene::renderWalking() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(8, 38, 119, 112, PixelRenderer::rgb(37, 71, 58));
    c.drawRect(8, 38, 119, 112, PixelRenderer::rgb(94, 135, 99));
    for (int i = 0; i < 7; ++i) {
        int x = 16 + i * 16;
        int h = 18 + (i % 3) * 8;
        c.fillTriangle(x, 136, x + 8, 136 - h, x + 16, 136, PixelRenderer::rgb(58, 120, 75));
    }
    PixelRenderer::text(18, 48, Ui::Explore::GRASS_PATH, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::bar(18, 66, 98, 10, (steps * 100) / targetSteps,
                       PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(51, 61, 52));

    char buf[32];
    snprintf(buf, sizeof(buf), Ui::Explore::STEP_FMT, steps, targetSteps);
    PixelRenderer::text(35, 82, buf, PixelRenderer::rgb(198, 215, 193), 1);
}

void ExploreScene::renderEncounter() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(197, 220, 192));
    c.fillRect(0, 96, Hal::DISPLAY_W, 92, PixelRenderer::rgb(228, 225, 190));
    c.fillEllipse(94, 88, 35, 9, PixelRenderer::rgb(155, 181, 142));
    c.fillEllipse(35, 152, 39, 11, PixelRenderer::rgb(171, 159, 128));

    if (wild) drawMonsterBlock(*wild, 96, 66);
    drawMonsterBlock(GameEngine::ins().activeSpecies(), 36, 124);
    renderBattleHud();
    renderCommandBox();
}

void ExploreScene::renderResult() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(8, 52, 119, 96, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(8, 52, 119, 96, PixelRenderer::rgb(95, 110, 126));
    PixelRenderer::text(20, 70, lastCaptureSuccess ? Ui::Explore::CAPTURE_SUCCESS : Ui::Explore::RESULT_END,
                        lastCaptureSuccess ? PixelRenderer::rgb(92, 222, 112) : PixelRenderer::rgb(241, 242, 232), 1);
    if (wild) PixelRenderer::text(20, 88, wild->name, PixelRenderer::rgb(255, 216, 72), 1);
}

void ExploreScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    if (phase == Phase::ENCOUNTER) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(14, 210, 107, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(22, 216, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

void ExploreScene::drawWildBlock(int x, int y) {
    if (!wild) return;
    drawMonsterBlock(*wild, x, y);
}

void ExploreScene::drawMonsterBlock(const Species& species, int x, int y) {
    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 32, 25, 7, PixelRenderer::rgb(23, 27, 34));
    c.fillRect(x - 18, y - 22, 36, 40, species.colorA);
    c.fillRect(x - 11, y - 13, 22, 23, species.colorB);
    c.fillCircle(x - 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
    c.fillCircle(x + 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
}

void ExploreScene::renderBattleHud() {
    auto& c = PixelRenderer::canvas();
    char buf[24];

    if (wild) {
        c.fillRect(5, 8, 78, 43, PixelRenderer::rgb(248, 248, 232));
        c.drawRect(5, 8, 78, 43, PixelRenderer::rgb(74, 91, 75));
        PixelRenderer::text(10, 12, wild->name, PixelRenderer::rgb(25, 31, 40), 1);
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, wildRuntime.level);
        PixelRenderer::text(54, 12, buf, PixelRenderer::rgb(25, 31, 40), 1);
        PixelRenderer::bar(12, 33, 62, 7, wildHpMax ? (wildHp * 100 / wildHpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    }

    const auto& active = GameEngine::ins().activeMonster();
    const Species& species = GameEngine::ins().activeSpecies();
    c.fillRect(58, 124, 72, 53, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(58, 124, 72, 53, PixelRenderer::rgb(74, 91, 75));
    PixelRenderer::text(64, 128, species.name, PixelRenderer::rgb(25, 31, 40), 1);
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, active.level);
    PixelRenderer::text(100, 128, buf, PixelRenderer::rgb(25, 31, 40), 1);
    PixelRenderer::bar(66, 149, 56, 7, active.hpMax ? (active.hpCur * 100 / active.hpMax) : 0,
                       PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    snprintf(buf, sizeof(buf), Ui::Explore::WILD_HP_FMT, active.hpCur, active.hpMax);
    PixelRenderer::text(66, 160, buf, PixelRenderer::rgb(25, 31, 40), 1);
}

void ExploreScene::renderCommandBox() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 188, Hal::DISPLAY_W, 52, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(0, 188, Hal::DISPLAY_W, 52, PixelRenderer::rgb(74, 91, 75));
    c.drawFastVLine(67, 190, 48, PixelRenderer::rgb(174, 182, 151));
    c.drawFastHLine(2, 214, 131, PixelRenderer::rgb(174, 182, 151));

    if (toast && Hal::ins().millis() <= toastUntil) {
        PixelRenderer::text(12, 204, toast, PixelRenderer::rgb(25, 31, 40), 1);
        return;
    }

    static constexpr int xs[] = {20, 82, 20};
    static constexpr int ys[] = {198, 198, 222};
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
