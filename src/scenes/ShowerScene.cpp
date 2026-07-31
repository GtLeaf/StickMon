#include "scenes/ShowerScene.h"

#include <cmath>

#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "core/UiMotion.h"
#include "game/GameRandom.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "platform/api/FlashStorage.h"

namespace {
constexpr int MONSTER_CENTER_X = 99;
constexpr int MONSTER_CENTER_Y = 78;
constexpr int MENU_CENTER_X = 210;
constexpr int MENU_CENTER_Y = Hal::DISPLAY_H / 2;
constexpr int MENU_SPACING = 46;
constexpr int MENU_INDICATOR_X = 176;
constexpr int WATER_RIGHT_X = 176;
constexpr int MENU_GRADIENT_X = MENU_INDICATOR_X;
constexpr float SHAKE_THRESHOLD_G = 0.38f;
constexpr float GRAVITY_SMOOTHING_HZ = 2.0f;
constexpr float TOOL_SHAKE_GAIN = 60.0f;
constexpr uint32_t SOAP_SWING_COOLDOWN_MS = 180;
constexpr uint32_t BRUSH_RUB_COOLDOWN_MS = 150;
constexpr uint32_t RINSE_FOAM_INTERVAL_MS = 350;
constexpr uint32_t RINSE_MIN_DURATION_MS = 1800;
constexpr uint16_t ATMOSPHERE_FADE_OUT_PER_SECOND = 420;
constexpr uint32_t COMPLETE_DURATION_MS = 1500;
constexpr uint32_t INCOMPLETE_DURATION_MS = 700;
constexpr uint32_t EXP_FLOAT_DURATION_MS = 1100;
constexpr uint32_t EXP_FLOAT_FADE_START_MS = 500;
constexpr float EXP_FLOAT_RISE_PX = 24.0f;
constexpr float EXP_FLOAT_BASE_X = 138.0f;
constexpr float HEART_FLOAT_X = MONSTER_CENTER_X - 3.0f;
constexpr float HEART_FLOAT_Y = 46.0f;
constexpr uint32_t HOP_DURATION_MS = 320;
constexpr uint32_t WIGGLE_DURATION_MS = 220;
constexpr uint32_t WIGGLE_FLIP_INTERVAL_MS = 70;
constexpr uint32_t RUB_REACTION_COOLDOWN_MS = 1500;
constexpr uint32_t TURN_HOLD_MS = 2600;
constexpr uint32_t SOAP_HOP_INTERVAL_MS = 900;
constexpr uint32_t RINSE_SHAKE_INTERVAL_MS = 800;
constexpr uint32_t FOAM_GROWTH_INTERVAL_MS = 600;
constexpr uint8_t SMALL_FOAM_BRUSH_TARGET = 2;
constexpr uint8_t LARGE_FOAM_BRUSH_TARGET = 4;
constexpr float LOWER_FOAM_Y = 88.0f;
constexpr float UPPER_FOAM_FALL_SPEED = 24.0f;
constexpr float LOWER_FOAM_FALL_SPEED = 14.0f;
constexpr uint8_t BODY_FOAM_STAGE_COUNT = 4;
constexpr uint32_t RINSE_WORST_CASE_MS =
    BODY_FOAM_STAGE_COUNT * RINSE_FOAM_INTERVAL_MS +
    (255000UL + ATMOSPHERE_FADE_OUT_PER_SECOND - 1) /
        ATMOSPHERE_FADE_OUT_PER_SECOND;
static_assert(RINSE_WORST_CASE_MS <= 3000,
              "all body foam and atmosphere must rinse away within 3 seconds");
static_assert(
    static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_5) -
        static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_0) + 1 == 6,
    "shower bubble assets must stay contiguous");
static_assert(
    static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_2) -
        static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) + 1 ==
        Game::SOAP_VARIANT_COUNT,
    "shower soap assets must stay contiguous");

GameAssets::Kind showerKind(GameAssets::Kind first, uint8_t offset) {
    return static_cast<GameAssets::Kind>(
        static_cast<uint16_t>(first) + offset);
}

float clampFloat(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}
}

void ShowerScene::onEnter() {
    mode = Mode::MENU;
    menuCursor = 0;
    menuAnimCursor = 0.0f;
    foamSpawnCursor = 0;
    gravityX = 0.0f;
    gravityY = 0.0f;
    gravityZ = 0.0f;
    accelSeeded = false;
    soapX = MONSTER_CENTER_X;
    soapY = MONSTER_CENTER_Y;
    brushX = MONSTER_CENTER_X;
    brushY = MONSTER_CENTER_Y;
    waterSpawnCarry = 0.0f;
    toast = nullptr;
    toastUntilMs = 0;
    resetBathSession();
    for (WaterDrop& drop : water) drop = WaterDrop{};
    for (ExpFloat& floater : expFloats) floater = ExpFloat{};
}

void ShowerScene::resetBathSession() {
    for (Foam& bubble : foam) bubble = Foam{};
    soapUsed = false;
    atmosphereTarget = false;
    atmosphereAlpha = 0.0f;
    lastFoamGrowthMs = 0;
    hopStartMs = 0;
    wiggleStartMs = 0;
    turnUntilMs = 0;
    turnFlip = false;
    foamRestSlot = 0;
    lastRubReactionMs = 0;
    lastRinseShakeMs = 0;
    soapRewarded = false;
    brushRewarded = false;
    rinseRewarded = false;
    completionHearts = 0;
    exitConfirmYes = false;
}

void ShowerScene::enterMode(Mode next, uint32_t nowMs) {
    mode = next;
    modeStartedMs = nowMs;
}

SceneUpdateResult ShowerScene::update(uint32_t nowMs, float dtSeconds) {
    RenderDemand demand;
    Mode modeBefore = mode;
    float atmosphereBefore = atmosphereAlpha;
    float menuTarget = static_cast<float>(menuCursor);
    while (menuTarget - menuAnimCursor > MENU_COUNT / 2.0f) {
        menuTarget -= MENU_COUNT;
    }
    while (menuTarget - menuAnimCursor < -MENU_COUNT / 2.0f) {
        menuTarget += MENU_COUNT;
    }
    UiMotion::StepResult menuStep = UiMotion::lerp(
        menuAnimCursor, menuTarget, 0.25f, 0.04f);
    demand.changed(menuStep.changed);

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    Hal::ins().readAccel(ax, ay, az);

    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    if (!accelSeeded) {
        gravityX = ax;
        gravityY = ay;
        gravityZ = az;
        accelSeeded = true;
    }
    float gravityAlpha = 1.0f - expf(-GRAVITY_SMOOTHING_HZ * dt);
    gravityX += (ax - gravityX) * gravityAlpha;
    gravityY += (ay - gravityY) * gravityAlpha;
    gravityZ += (az - gravityZ) * gravityAlpha;
    float dynX = ax - gravityX;
    float dynY = ay - gravityY;
    float dynZ = az - gravityZ;
    float shake = sqrtf(dynX * dynX + dynY * dynY + dynZ * dynZ);

    switch (mode) {
    case Mode::SOAPING:
        updateSoaping(nowMs, dtSeconds, dynX, dynY, shake);
        break;
    case Mode::BRUSHING:
        updateBrushing(nowMs, dtSeconds, dynX, dynY, shake);
        break;
    case Mode::RINSING:
        updateRinsing(nowMs, dtSeconds);
        break;
    case Mode::COMPLETE:
        if (nowMs - modeStartedMs >= COMPLETE_DURATION_MS) {
            resetBathSession();
            enterMode(Mode::MENU, nowMs);
        }
        break;
    case Mode::INCOMPLETE:
        if (nowMs - modeStartedMs >= INCOMPLETE_DURATION_MS) {
            GameEngine::ins().requestScene(SceneID::MENU);
        }
        break;
    default:
        break;
    }
    updateAtmosphere(dtSeconds);
    demand.changed(mode != modeBefore ||
                   atmosphereAlpha != atmosphereBefore);
    bool effectActive = false;
    for (ExpFloat& floater : expFloats) {
        if (!floater.active) continue;
        if (nowMs - floater.bornMs >= EXP_FLOAT_DURATION_MS) {
            floater.active = false;
            demand.redraw();
        } else {
            effectActive = true;
        }
    }
    if (demand.expired(toast != nullptr, nowMs, toastUntilMs)) toast = nullptr;

    bool atmosphereMoving =
        atmosphereTarget ? atmosphereAlpha < 255.0f : atmosphereAlpha > 0.0f;
    bool timedReaction =
        (hopStartMs != 0 && nowMs - hopStartMs < HOP_DURATION_MS) ||
        (wiggleStartMs != 0 && nowMs - wiggleStartMs < WIGGLE_DURATION_MS) ||
        nowMs < turnUntilMs;
    if (hopStartMs != 0 && nowMs - hopStartMs >= HOP_DURATION_MS) {
        hopStartMs = 0;
        demand.redraw();
    }
    if (wiggleStartMs != 0 &&
        nowMs - wiggleStartMs >= WIGGLE_DURATION_MS) {
        wiggleStartMs = 0;
        demand.redraw();
    }
    if (turnUntilMs != 0 && nowMs >= turnUntilMs) {
        turnUntilMs = 0;
        demand.redraw();
    }
    bool modeAnimating =
        mode == Mode::SOAPING || mode == Mode::BRUSHING ||
        mode == Mode::RINSING || mode == Mode::COMPLETE;
    bool animating =
        modeAnimating || atmosphereMoving || effectActive || timedReaction ||
        menuStep.active;
    demand.animate(animating);
    if (mode == Mode::INCOMPLETE) {
        uint32_t elapsed = nowMs - modeStartedMs;
        if (elapsed < INCOMPLETE_DURATION_MS) {
            demand.wakeIn(INCOMPLETE_DURATION_MS - elapsed);
        }
    }
    return demand.result();
}

bool ShowerScene::onButton(const ButtonEvent& event) {
    if (event.action != BtnAction::PRESSED &&
        event.action != BtnAction::LONG_PRESS) {
        return false;
    }

    uint32_t nowMs = Hal::ins().millis();
    if (mode == Mode::EXIT_CONFIRM) {
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            exitConfirmYes = !exitConfirmYes;
        } else if (event.btn == 0) {
            if (exitConfirmYes) {
                for (WaterDrop& drop : water) drop.active = false;
                enterMode(Mode::INCOMPLETE, nowMs);
            } else {
                enterMode(Mode::MENU, nowMs);
            }
        }
        return true;
    }
    if (mode == Mode::INCOMPLETE) return true;

    if (mode == Mode::SOAPING || mode == Mode::BRUSHING || mode == Mode::RINSING) {
        if (event.btn == 1) {
            enterMode(Mode::MENU, nowMs);
            for (WaterDrop& drop : water) drop.active = false;
            return true;
        }
        return true;
    }
    if (mode == Mode::COMPLETE) return true;

    if (mode == Mode::SOAP_SELECT) {
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
            enterMode(Mode::MENU, nowMs);
        } else if (event.btn == 1) {
            int8_t next = nextOwnedSoap(soapCursor, 1);
            if (next >= 0) soapCursor = static_cast<uint8_t>(next);
        } else if (event.btn == 0) {
            beginSoap();
        }
        return true;
    }

    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        requestExit(nowMs);
        return true;
    }
    if (event.btn == 1) {
        menuCursor = (menuCursor + 1) % MENU_COUNT;
        return true;
    }
    if (event.btn != 0) return false;

    switch (menuCursor) {
    case 0:
        chooseSoap();
        break;
    case 1:
        enterMode(Mode::BRUSHING, nowMs);
        brushX = MONSTER_CENTER_X;
        brushY = MONSTER_CENTER_Y;
        break;
    case 2:
        beginRinse(nowMs);
        break;
    default:
        requestExit(nowMs);
        break;
    }
    return true;
}

void ShowerScene::requestExit(uint32_t nowMs) {
    if (soapUsed && !rinseRewarded) {
        exitConfirmYes = false;
        enterMode(Mode::EXIT_CONFIRM, nowMs);
        return;
    }
    GameEngine::ins().requestScene(SceneID::MENU);
}

void ShowerScene::chooseSoap() {
    if (soapUsed) {
        toast = Ui::Shower::ALREADY_SOAPED;
        toastUntilMs = Hal::ins().millis() + 1200;
        return;
    }
    int8_t owned = nextOwnedSoap(-1, 1);
    if (owned < 0) {
        toast = Ui::Shower::NO_SOAP;
        toastUntilMs = Hal::ins().millis() + 1200;
        return;
    }
    soapCursor = static_cast<uint8_t>(owned);
    enterMode(Mode::SOAP_SELECT, Hal::ins().millis());
}

void ShowerScene::beginSoap() {
    Game::ItemId item = Game::itemIdForSoapIndex(soapCursor);
    if (!GameEngine::ins().removeItem(item, 1, SaveUrgency::IMMEDIATE)) {
        chooseSoap();
        return;
    }
    soapUsed = true;
    soapX = MONSTER_CENTER_X;
    soapY = MONSTER_CENTER_Y;
    enterMode(Mode::SOAPING, Hal::ins().millis());
}

void ShowerScene::beginRinse(uint32_t nowMs) {
    for (WaterDrop& drop : water) drop.active = false;
    waterSpawnCarry = 0.0f;
    lastRinseFoamMs = nowMs;
    lastRinseShakeMs = nowMs;
    enterMode(Mode::RINSING, nowMs);
}

void ShowerScene::updateSoaping(uint32_t nowMs, float dtSeconds,
                                float dynX, float dynY, float shake) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    float smoothing = 1.0f - expf(-9.0f * dt);
    float targetX = clampFloat(MONSTER_CENTER_X + dynX * TOOL_SHAKE_GAIN,
                               44.0f, 161.0f);
    float targetY = clampFloat(MONSTER_CENTER_Y + dynY * TOOL_SHAKE_GAIN,
                               33.0f, 122.0f);
    soapX += (targetX - soapX) * smoothing;
    soapY += (targetY - soapY) * smoothing;
    if (shake >= SHAKE_THRESHOLD_G &&
        nowMs - lastSoapSwingMs >= SOAP_SWING_COOLDOWN_MS) {
        lastSoapSwingMs = nowMs;
        spawnFoam();
    }
}

void ShowerScene::updateBrushing(uint32_t nowMs, float dtSeconds,
                                 float dynX, float dynY, float shake) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    float smoothing = 1.0f - expf(-9.0f * dt);
    float targetX = clampFloat(MONSTER_CENTER_X + dynX * TOOL_SHAKE_GAIN,
                               44.0f, 161.0f);
    float targetY = clampFloat(MONSTER_CENTER_Y + dynY * TOOL_SHAKE_GAIN,
                               33.0f, 122.0f);
    brushX += (targetX - brushX) * smoothing;
    brushY += (targetY - brushY) * smoothing;
    if (shake >= SHAKE_THRESHOLD_G &&
        nowMs - lastBrushRubMs >= BRUSH_RUB_COOLDOWN_MS) {
        rubFoamAt(brushX, brushY, nowMs);
        lastBrushRubMs = nowMs;
    }

    for (Foam& bubble : foam) {
        if (!bubble.active) continue;
        float restY = foamRestY(bubble.stage) + bubble.restYOffset;
        if (bubble.stage == 2 && bubble.y < restY) {
            bubble.y = MathUtil::min(restY,
                           bubble.y + UPPER_FOAM_FALL_SPEED * dt);
            bubble.x += clampFloat(bubble.restX - bubble.x, -18.0f * dt,
                                   18.0f * dt);
        } else if (bubble.stage == 3) {
            bubble.y = MathUtil::min(restY,
                           bubble.y + LOWER_FOAM_FALL_SPEED * dt);
            bubble.x += clampFloat(bubble.restX - bubble.x, -12.0f * dt,
                                   12.0f * dt);
        }
    }
    checkAtmosphereThreshold(nowMs);
}

void ShowerScene::updateRinsing(uint32_t nowMs, float dtSeconds) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    waterSpawnCarry += dt * 96.0f;
    while (waterSpawnCarry >= 1.0f) {
        spawnWaterDrop();
        waterSpawnCarry -= 1.0f;
    }

    for (WaterDrop& drop : water) {
        if (!drop.active) continue;
        drop.vy += 260.0f * dt;
        drop.x += drop.vx * dt;
        drop.y += drop.vy * dt;
        if (drop.y >= 121.0f) {
            if (fabsf(drop.vy) > 35.0f && drop.x > 25.0f && drop.x < 155.0f) {
                drop.y = 120.0f;
                drop.vy = -drop.vy * 0.24f;
                drop.vx += drop.x < MONSTER_CENTER_X ? -18.0f : 18.0f;
            } else {
                drop.active = false;
            }
        }
        if (drop.x < 0.0f || drop.x >= WATER_RIGHT_X) drop.active = false;
    }

    if (nowMs - lastRinseShakeMs >= RINSE_SHAKE_INTERVAL_MS) {
        lastRinseShakeMs = nowMs;
        startWiggle(nowMs);
        for (uint8_t i = 0; i < 3; ++i) {
            float direction = (i % 2 == 0) ? 1.0f : -1.0f;
            spawnSplashDrop(MONSTER_CENTER_X + direction * (10 + i * 6),
                            MONSTER_CENTER_Y - 12.0f,
                            direction * (50.0f + i * 20.0f), -45.0f);
        }
    }

    if (nowMs - lastRinseFoamMs >= RINSE_FOAM_INTERVAL_MS) {
        rinseAllFoamOneStage();
        lastRinseFoamMs = nowMs;
    }
    if (!anyFoam()) atmosphereTarget = false;
    if (nowMs - modeStartedMs >= RINSE_MIN_DURATION_MS &&
        !anyFoam() && atmosphereAlpha <= 1.0f) {
        for (WaterDrop& drop : water) drop.active = false;
        awardBathStage(BathRewardStage::RINSE);
        soapUsed = false;
        completionHearts = static_cast<uint8_t>(
            (soapRewarded ? 1 : 0) + (brushRewarded ? 1 : 0) +
            (rinseRewarded ? 1 : 0));
        enterMode(Mode::COMPLETE, nowMs);
        // 收尾反应由爱心数决定:3 个叫声、2 个跳一下、1 个无动作
        hopStartMs = 0;
        if (completionHearts >= 3) {
            CryPlayer::ins().play(GameEngine::ins().activeSpecies().id);
        } else if (completionHearts == 2) {
            startHop(10.0f, nowMs);
        }
    }
}

void ShowerScene::updateAtmosphere(float dtSeconds) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    float target = atmosphereTarget ? 255.0f : 0.0f;
    float speed = atmosphereTarget
        ? 300.0f
        : static_cast<float>(ATMOSPHERE_FADE_OUT_PER_SECOND);
    if (atmosphereAlpha < target) {
        atmosphereAlpha = MathUtil::min(target, atmosphereAlpha + speed * dt);
    } else if (atmosphereAlpha > target) {
        atmosphereAlpha = MathUtil::max(target, atmosphereAlpha - speed * dt);
    }
}

void ShowerScene::spawnFoam() {
    // 1~2 坨泡沫,按轮询点位均匀分布在精灵身上(带小幅抖动)。
    static constexpr int8_t SPOTS[][2] = {
        {-20, -26}, {18, -20}, {-10, -6}, {24, 2},
        {-24, 8}, {8, 16}, {-14, 26}, {20, 28},
    };
    uint32_t nowMs = Hal::ins().millis();
    uint8_t count = 1 + (uint8_t)GameRandom::random(2);
    for (uint8_t spawned = 0; spawned < count; ++spawned) {
        Foam* target = nullptr;
        for (Foam& bubble : foam) {
            if (!bubble.active) {
                target = &bubble;
                break;
            }
        }
        if (!target) break;
        const int8_t* spot = SPOTS[foamSpawnCursor %
            (sizeof(SPOTS) / sizeof(SPOTS[0]))];
        target->x = clampFloat(
            MONSTER_CENTER_X + spot[0] + static_cast<float>(GameRandom::random(-3, 4)),
            47.0f, 151.0f);
        target->y = clampFloat(
            MONSTER_CENTER_Y + spot[1] + static_cast<float>(GameRandom::random(-3, 4)),
            34.0f, 113.0f);
        target->restX = target->x;
        target->restYOffset = 0.0f;
        target->stage = GameRandom::random(3) == 0 ? 1 : 0;
        target->brushProgress = 0;
        target->bornMs = nowMs;
        target->active = true;
        foamSpawnCursor = (foamSpawnCursor + 1) % FOAM_CAP;
    }
    if (nowMs - hopStartMs >= SOAP_HOP_INTERVAL_MS) startHop(6.0f, nowMs);
    awardBathStage(BathRewardStage::SOAP);
}

void ShowerScene::rubFoamAt(float x, float y, uint32_t nowMs) {
    Foam* nearest = nullptr;
    float bestDistance = 24.0f * 24.0f;
    for (Foam& bubble : foam) {
        if (!bubble.active || bubble.stage >= 4) continue;
        float dx = x - bubble.x;
        float dy = y - bubble.y;
        float distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            nearest = &bubble;
            bestDistance = distance;
        }
    }
    if (!nearest) return;
    if (nearest->brushProgress < 255) ++nearest->brushProgress;
    tryMergeFoam(*nearest, nowMs);
    if (nowMs - lastRubReactionMs >= RUB_REACTION_COOLDOWN_MS) {
        lastRubReactionMs = nowMs;
        if (GameRandom::random(100) < 35) startTurn(nowMs);
    }
}

bool ShowerScene::tryMergeFoam(Foam& source, uint32_t nowMs) {
    uint8_t requiredProgress = source.stage <= 1
        ? SMALL_FOAM_BRUSH_TARGET
        : LARGE_FOAM_BRUSH_TARGET;
    if (source.brushProgress < requiredProgress ||
        nowMs - lastFoamGrowthMs < FOAM_GROWTH_INTERVAL_MS) {
        return false;
    }
    if (source.stage >= 2 &&
        source.y + 0.5f < foamRestY(source.stage)) {
        return false;
    }

    Foam* partner = findMergePartner(source);
    if (!partner) return false;

    if (source.stage >= 3) return false;

    uint8_t nextStage = source.stage <= 1
        ? 2
        : static_cast<uint8_t>(source.stage + 1);
    source.x = (source.x + partner->x) * 0.5f;
    source.y = (source.y + partner->y) * 0.5f;
    source.stage = nextStage;
    source.brushProgress = 0;
    source.bornMs = nowMs;
    if (nextStage >= 2) {
        // 滑落后的静止点适当错开:横向铺开,纵向离地 10px 以内
        foamRestSlot = (foamRestSlot + 1) % 5;
        int spread = (static_cast<int>(foamRestSlot) - 2) * 14;
        source.restX = clampFloat(MONSTER_CENTER_X + spread, 47.0f, 151.0f);
        source.restYOffset =
            -static_cast<float>((foamRestSlot * 4 + nextStage * 3) % 11);
    }
    if (nextStage >= 3) source.y = foamRestY(nextStage) + source.restYOffset;
    partner->active = false;
    partner->brushProgress = 0;
    lastFoamGrowthMs = nowMs;
    startHop(3.0f, nowMs);
    checkAtmosphereThreshold(nowMs);
    return true;
}

uint8_t ShowerScene::foamLevelTotal() const {
    // 只统计下层的泡沫:1、2级各算 1 沱,3级算 2 沱,4级算 4 沱
    uint8_t total = 0;
    for (const Foam& bubble : foam) {
        if (!bubble.active || bubble.y < LOWER_FOAM_Y) continue;
        total += bubble.stage >= 3 ? 4 : (bubble.stage == 2 ? 2 : 1);
    }
    return total;
}

void ShowerScene::checkAtmosphereThreshold(uint32_t nowMs) {
    if (atmosphereTarget) return;
    if (foamLevelTotal() <= 6) return;
    // 下层至少一沱 3 级(code stage 2)以上,且总量 >6,合成第六级全身气泡
    bool hasLarge = false;
    for (const Foam& bubble : foam) {
        if (bubble.active && bubble.stage >= 2 && bubble.y >= LOWER_FOAM_Y) {
            hasLarge = true;
            break;
        }
    }
    if (!hasLarge) return;
    for (Foam& bubble : foam) bubble = Foam{};
    atmosphereTarget = true;
    awardBathStage(BathRewardStage::BRUSH);
    startHop(3.0f, nowMs);
}

ShowerScene::Foam* ShowerScene::findMergePartner(const Foam& source) {
    Foam* nearest = nullptr;
    float bestDistance = 1000000.0f;
    bool sourceUpper = source.y < LOWER_FOAM_Y;
    for (Foam& candidate : foam) {
        if (!candidate.active || &candidate == &source) continue;

        bool compatible = false;
        if (source.stage <= 1) {
            compatible = candidate.stage <= 1 &&
                         (candidate.y < LOWER_FOAM_Y) == sourceUpper;
        } else {
            compatible = candidate.stage == source.stage &&
                         candidate.y + 0.5f >= foamRestY(candidate.stage);
        }
        if (!compatible) continue;

        float dx = source.x - candidate.x;
        float dy = source.y - candidate.y;
        float distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            nearest = &candidate;
            bestDistance = distance;
        }
    }
    return nearest;
}

void ShowerScene::rinseAllFoamOneStage() {
    for (Foam& bubble : foam) {
        if (!bubble.active) continue;
        if (bubble.stage > 0) --bubble.stage;
        else bubble.active = false;
    }
}

void ShowerScene::spawnWaterDrop() {
    for (uint8_t i = 0; i < WATER_CAP; ++i) {
        WaterDrop& drop = water[i];
        if (drop.active) continue;
        drop.active = true;
        drop.x = 22.0f + static_cast<float>((i * 29 + foamSpawnCursor * 7) % 139);
        drop.y = -4.0f - static_cast<float>((i * 7) % 42);
        drop.vx = static_cast<float>((static_cast<int>(i) % 7) - 3) * 4.0f;
        drop.vy = 125.0f + static_cast<float>((i * 13) % 90);
        return;
    }
}

void ShowerScene::spawnSplashDrop(float x, float y, float vx, float vy) {
    for (uint8_t i = 0; i < WATER_CAP; ++i) {
        WaterDrop& drop = water[i];
        if (drop.active) continue;
        drop.active = true;
        drop.x = x;
        drop.y = y;
        drop.vx = vx;
        drop.vy = vy;
        return;
    }
}

bool ShowerScene::anyFoam() const {
    for (const Foam& bubble : foam) {
        if (bubble.active) return true;
    }
    return false;
}

int ShowerScene::monsterBottomY() const {
    const auto* frame = PokemonSprites::findSpeciesSprite(
        GameEngine::ins().activeSpecies().id,
        PokemonSprites::SpriteKind::FRONT);
    if (!frame) return MONSTER_CENTER_Y + 42;
    uint8_t width = FlashStorage::readByte(&frame->width);
    uint8_t height = FlashStorage::readByte(&frame->height);
    if (width == 0 || height == 0) return MONSTER_CENTER_Y + 42;
    float scale = MathUtil::min(108.0f / width, 112.0f / height);
    if (scale > 1.4f) scale = 1.4f;
    return MathUtil::min(Hal::DISPLAY_H - 1,
               MONSTER_CENTER_Y + static_cast<int>(height * scale) / 2);
}

float ShowerScene::foamRestY(uint8_t stage) const {
    static constexpr uint8_t FOAM_HEIGHTS[BODY_FOAM_STAGE_COUNT] = {
        8, 16, 24, 32,
    };
    if (stage >= BODY_FOAM_STAGE_COUNT) stage = BODY_FOAM_STAGE_COUNT - 1;
    return monsterBottomY() - FOAM_HEIGHTS[stage] * 0.5f;
}

int8_t ShowerScene::nextOwnedSoap(int8_t from, int8_t direction) const {
    for (uint8_t step = 1; step <= Game::SOAP_VARIANT_COUNT; ++step) {
        int index = from + direction * step;
        while (index < 0) index += Game::SOAP_VARIANT_COUNT;
        index %= Game::SOAP_VARIANT_COUNT;
        if (GameEngine::ins().soapCount(static_cast<uint8_t>(index)) > 0) {
            return static_cast<int8_t>(index);
        }
    }
    return -1;
}

void ShowerScene::awardBathStage(BathRewardStage stage) {
    bool* rewarded = nullptr;
    switch (stage) {
    case BathRewardStage::SOAP:
        rewarded = &soapRewarded;
        break;
    case BathRewardStage::BRUSH:
        rewarded = &brushRewarded;
        break;
    case BathRewardStage::RINSE:
        rewarded = &rinseRewarded;
        break;
    }
    if (!rewarded || *rewarded) return;
    *rewarded = true;
    uint8_t experience = GameEngine::ins().grantBathReward(stage);
    if (experience == 0) return;
    uint32_t nowMs = Hal::ins().millis();
    spawnExpFloat(experience, nowMs);
    spawnHeartFloat(nowMs);
    startHop(10.0f, nowMs);
}

void ShowerScene::startHop(float heightPx, uint32_t nowMs) {
    hopStartMs = nowMs;
    hopHeightPx = heightPx;
}

void ShowerScene::startWiggle(uint32_t nowMs) {
    wiggleStartMs = nowMs;
}

void ShowerScene::startTurn(uint32_t nowMs) {
    // 换一面擦:翻到另一面并保持一段时间,期间不再翻转
    if (nowMs < turnUntilMs) return;
    turnFlip = !turnFlip;
    turnUntilMs = nowMs + TURN_HOLD_MS;
}

void ShowerScene::spawnExpFloat(uint8_t amount, uint32_t nowMs) {
    uint8_t slot = 0;
    uint32_t oldest = UINT32_MAX;
    for (uint8_t i = 0; i < EXP_FLOAT_CAP; ++i) {
        if (!expFloats[i].active) {
            slot = i;
            break;
        }
        if (expFloats[i].bornMs < oldest) {
            oldest = expFloats[i].bornMs;
            slot = i;
        }
    }
    ExpFloat& floater = expFloats[slot];
    floater.x = EXP_FLOAT_BASE_X + static_cast<float>((slot % 2) * 14);
    floater.y = MONSTER_CENTER_Y + 16.0f;
    floater.bornMs = nowMs;
    snprintf(floater.text, sizeof(floater.text),
             Ui::Shower::EXP_GAIN_FMT, static_cast<unsigned>(amount));
    floater.heart = false;
    floater.active = true;
}

void ShowerScene::spawnHeartFloat(uint32_t nowMs) {
    uint8_t slot = 0;
    uint32_t oldest = UINT32_MAX;
    for (uint8_t i = 0; i < EXP_FLOAT_CAP; ++i) {
        if (!expFloats[i].active) {
            slot = i;
            break;
        }
        if (expFloats[i].bornMs < oldest) {
            oldest = expFloats[i].bornMs;
            slot = i;
        }
    }
    ExpFloat& floater = expFloats[slot];
    floater.x = HEART_FLOAT_X;
    floater.y = HEART_FLOAT_Y;
    floater.bornMs = nowMs;
    floater.text[0] = '\0';
    floater.heart = true;
    floater.active = true;
}

void ShowerScene::render() {
    if (!GameAssets::draw(GameAssets::Kind::SHOWER_BACKGROUND, 0, 0)) {
        PixelRenderer::clear(PixelRenderer::rgb(21, 36, 30));
    }

    drawMonster();
    drawAtmosphere();
    drawFoam();
    drawWater();
    drawTool();
    drawMenu();
    if (mode == Mode::SOAP_SELECT) drawSoapPicker();
    if (mode == Mode::COMPLETE) drawHearts();
    if (mode == Mode::EXIT_CONFIRM) drawExitConfirm();
    if (mode == Mode::INCOMPLETE) drawIncomplete();
    drawExpFloats();
    drawToast();
}

void ShowerScene::drawAtmosphere() const {
    if (atmosphereAlpha <= 0.0f) return;
    int centerY = monsterBottomY() - 20;
    GameAssets::drawCenteredAlpha(
        GameAssets::Kind::SHOWER_BUBBLE_5,
        MONSTER_CENTER_X, centerY, 1.0f,
        static_cast<uint8_t>(clampFloat(atmosphereAlpha, 0.0f, 255.0f)));
}

void ShowerScene::drawMonster() const {
    uint16_t speciesId = GameEngine::ins().activeSpecies().id;
    const auto* frame = PokemonSprites::findSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) return;
    uint32_t nowMs = Hal::ins().millis();
    bool flip = false;
    if (wiggleStartMs != 0 && nowMs - wiggleStartMs < WIGGLE_DURATION_MS) {
        flip = ((nowMs - wiggleStartMs) / WIGGLE_FLIP_INTERVAL_MS) % 2 == 1;
    } else if (nowMs < turnUntilMs) {
        flip = turnFlip;
    }
    uint8_t width = FlashStorage::readByte(&frame->width);
    uint8_t height = FlashStorage::readByte(&frame->height);
    if (width == 0 || height == 0) return;
    float scale = MathUtil::min(108.0f / width, 112.0f / height);
    if (scale > 1.4f) scale = 1.4f;
    int drawW = static_cast<int>(width * scale);
    int drawH = static_cast<int>(height * scale);
    int yOffset = 0;
    if (hopStartMs != 0 && nowMs - hopStartMs < HOP_DURATION_MS) {
        float progress =
            static_cast<float>(nowMs - hopStartMs) / HOP_DURATION_MS;
        yOffset = -static_cast<int>(
            hopHeightPx * 4.0f * progress * (1.0f - progress) + 0.5f);
    }
    PokemonSprites::drawFrameScaled(
        frame,
        MONSTER_CENTER_X - drawW / 2,
        MONSTER_CENTER_Y - drawH / 2 + yOffset,
        scale, flip);
}

void ShowerScene::drawFoam() const {
    for (uint8_t stage = 0; stage < BODY_FOAM_STAGE_COUNT; ++stage) {
        for (const Foam& bubble : foam) {
            if (!bubble.active || bubble.stage != stage) continue;
            GameAssets::drawCentered(
                showerKind(GameAssets::Kind::SHOWER_BUBBLE_0, bubble.stage),
                static_cast<int>(bubble.x), static_cast<int>(bubble.y), 1.0f);
        }
    }
}

void ShowerScene::drawWater() const {
    if (mode != Mode::RINSING) return;
    auto& c = PixelRenderer::canvas();
    uint16_t waterColor = PixelRenderer::rgb(74, 190, 232);
    uint16_t highlight = PixelRenderer::rgb(220, 250, 255);
    uint32_t elapsed = Hal::ins().millis() - modeStartedMs;
    PixelRenderer::fillRectAlpha(20, 0, 143, 121, waterColor, 58);
    for (uint8_t stream = 0; stream < 12; ++stream) {
        int x = 22 + stream * 12;
        int width = 7 + (stream % 3);
        int phase = static_cast<int>((elapsed / 3 + stream * 23) % 76);
        PixelRenderer::fillRectAlpha(x, phase - 76, width, 58, highlight, 88);
        PixelRenderer::fillRectAlpha(x, phase, width, 58, highlight, 88);
        c.drawFastVLine(x + width / 2, 0, 121,
                        PixelRenderer::rgb(145, 226, 249));
    }
    for (const WaterDrop& drop : water) {
        if (!drop.active) continue;
        int x = static_cast<int>(drop.x);
        int y = static_cast<int>(drop.y);
        c.drawFastVLine(x, y, drop.vy > 180.0f ? 11 : 7, highlight);
        c.drawPixel(x, y, highlight);
    }
    c.fillEllipse(MONSTER_CENTER_X, 122, 70, 8, waterColor);
    c.drawFastHLine(30, 119, 120, highlight);
    GameAssets::drawCentered(
        GameAssets::Kind::SHOWER_SPRINKLER,
        MONSTER_CENTER_X, 19, 1.05f);
}

void ShowerScene::drawTool() const {
    if (mode == Mode::SOAPING) {
        GameAssets::drawCentered(
            showerKind(GameAssets::Kind::SHOWER_SOAP_0, soapCursor),
            static_cast<int>(soapX), static_cast<int>(soapY));
    } else if (mode == Mode::BRUSHING) {
        GameAssets::drawCentered(
            GameAssets::Kind::SHOWER_BRUSH,
            static_cast<int>(brushX), static_cast<int>(brushY));
    }
}

void ShowerScene::drawMenu() {
    auto& c = PixelRenderer::canvas();
    for (int x = MENU_GRADIENT_X; x < Hal::DISPLAY_W; ++x) {
        uint8_t alpha = static_cast<uint8_t>(
            (x - MENU_GRADIENT_X) * 190 /
            (Hal::DISPLAY_W - MENU_GRADIENT_X - 1));
        PixelRenderer::fillRectAlpha(
            x, 0, 1, Hal::DISPLAY_H, PixelRenderer::rgb(0, 0, 0), alpha);
    }
    c.fillRect(MENU_INDICATOR_X, MENU_CENTER_Y - 8, 3, 16,
               PixelRenderer::rgb(255, 216, 72));
    for (uint8_t i = 0; i < MENU_COUNT; ++i) {
        float rawOffset = static_cast<float>(i) - menuAnimCursor;
        while (rawOffset > MENU_COUNT / 2.0f) rawOffset -= MENU_COUNT;
        while (rawOffset < -MENU_COUNT / 2.0f) rawOffset += MENU_COUNT;
        int centerY = MENU_CENTER_Y + static_cast<int>(rawOffset * MENU_SPACING);
        if (centerY < -24 || centerY > Hal::DISPLAY_H + 24) continue;
        bool selected =
            (mode == Mode::MENU && fabsf(rawOffset) < 0.5f) ||
            ((mode == Mode::SOAP_SELECT || mode == Mode::SOAPING) && i == 0) ||
            (mode == Mode::BRUSHING && i == 1) ||
            (mode == Mode::RINSING && i == 2);
        float selectedScale = selected ? 1.2f : 1.0f;
        uint16_t color = selected
            ? PixelRenderer::rgb(255, 231, 139)
            : PixelRenderer::rgb(241, 242, 232);
        if (i == 0) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_SOAP,
                MENU_CENTER_X, centerY, 0.72f * selectedScale);
        } else if (i == 1) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_BRUSH,
                MENU_CENTER_X, centerY, 0.58f * selectedScale);
        } else if (i == 2) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_SPRINKLER,
                MENU_CENTER_X, centerY, 0.78f * selectedScale);
        } else {
            int half = static_cast<int>(7.2f * selectedScale);
            c.drawLine(MENU_CENTER_X - half, centerY - half,
                       MENU_CENTER_X + half, centerY + half, color);
            c.drawLine(MENU_CENTER_X + half, centerY - half,
                       MENU_CENTER_X - half, centerY + half, color);
        }
    }
}

void ShowerScene::drawSoapPicker() const {
    auto& c = PixelRenderer::canvas();
    PixelRenderer::fillRectAlpha(
        7, 96, 166, 34, PixelRenderer::rgb(13, 18, 25), 210);
    GameAssets::drawCentered(
        showerKind(GameAssets::Kind::SHOWER_SOAP_0, soapCursor),
        27, 113, 0.85f);
    char count[24];
    snprintf(count, sizeof(count), "%s x%u", Ui::Shower::SOAP_NAMES[soapCursor],
             static_cast<unsigned>(GameEngine::ins().soapCount(soapCursor)));
    PixelRenderer::text(48, 105, count, PixelRenderer::rgb(255, 244, 210), 1);
}

void ShowerScene::drawHearts() const {
    auto& c = PixelRenderer::canvas();
    uint16_t color = PixelRenderer::rgb(242, 74, 97);
    uint32_t elapsed = Hal::ins().millis() - modeStartedMs;
    uint8_t count = MathUtil::min<uint8_t>(completionHearts, 3);
    if (count == 0) return;
    int startX = 88 - (count - 1) * 18;
    for (uint8_t i = 0; i < count; ++i) {
        int x = startX + i * 36;
        int y = 23 - static_cast<int>((elapsed / 90 + i * 3) % 6);
        c.fillCircle(x - 3, y, 4, color);
        c.fillCircle(x + 3, y, 4, color);
        c.fillTriangle(x - 7, y + 1, x + 7, y + 1, x, y + 10, color);
    }
}

void ShowerScene::drawExitConfirm() const {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 25;
    static constexpr int POP_Y = 27;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 82;
    PixelRenderer::fillRectAlpha(
        POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 29), 235);
    c.drawRect(POP_X, POP_Y, POP_W, POP_H,
               PixelRenderer::rgb(241, 242, 232));
    PixelRenderer::text(POP_X + 39, POP_Y + 12, Ui::Shower::EXIT_FOAM,
                        PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(POP_X + 55, POP_Y + 31, Ui::Shower::EXIT_QUESTION,
                        PixelRenderer::rgb(241, 242, 232), 1);
    uint16_t yesColor = exitConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !exitConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 57, POP_Y + 57,
                        Ui::Shower::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 128, POP_Y + 57,
                        Ui::Shower::NO, noColor, 1);
}

void ShowerScene::drawIncomplete() const {
    PixelRenderer::fillRectAlpha(
        39, 50, 162, 34, PixelRenderer::rgb(13, 18, 25), 235);
    PixelRenderer::text(55, 59, Ui::Shower::INCOMPLETE,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

void ShowerScene::drawExpFloats() {
    uint32_t nowMs = Hal::ins().millis();
    for (ExpFloat& floater : expFloats) {
        if (!floater.active) continue;
        uint32_t age = nowMs - floater.bornMs;
        if (age >= EXP_FLOAT_DURATION_MS) continue;
        float progress = static_cast<float>(age) / EXP_FLOAT_DURATION_MS;
        int y = static_cast<int>(floater.y - EXP_FLOAT_RISE_PX * progress);
        uint8_t brightness = 255;
        if (age > EXP_FLOAT_FADE_START_MS) {
            float fade = static_cast<float>(age - EXP_FLOAT_FADE_START_MS) /
                         (EXP_FLOAT_DURATION_MS - EXP_FLOAT_FADE_START_MS);
            brightness = static_cast<uint8_t>(255.0f * (1.0f - fade));
        }
        int x = static_cast<int>(floater.x);
        if (floater.heart) {
            uint16_t heartColor = PixelRenderer::rgb(
                static_cast<uint8_t>((242 * brightness) / 255),
                static_cast<uint8_t>((74 * brightness) / 255),
                static_cast<uint8_t>((97 * brightness) / 255));
            auto& c = PixelRenderer::canvas();
            c.fillCircle(x - 1, y, 2, heartColor);
            c.fillCircle(x + 1, y, 2, heartColor);
            c.fillTriangle(x - 3, y + 1, x + 3, y + 1, x, y + 5, heartColor);
            continue;
        }
        uint16_t color = PixelRenderer::rgb(brightness, brightness, brightness);
        PixelRenderer::text(x, y, floater.text, color, 1);
    }
}

void ShowerScene::drawToast() {
    if (!toast) return;
    PixelRenderer::fillRectAlpha(
        39, 103, 100, 22, PixelRenderer::rgb(18, 22, 29), 225);
    PixelRenderer::text(55, 106, toast, PixelRenderer::rgb(255, 232, 150), 1);
}
