#include "scenes/ShowerScene.h"

#include <Arduino.h>
#include <cmath>

#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
constexpr int MONSTER_CENTER_X = 99;
constexpr int MONSTER_CENTER_Y = 78;
constexpr int MENU_CENTER_X = 210;
constexpr int MENU_CENTER_Y = Hal::DISPLAY_H / 2;
constexpr int MENU_SPACING = 46;
constexpr int MENU_INDICATOR_X = 176;
constexpr int WATER_RIGHT_X = 176;
constexpr int MENU_GRADIENT_X = MENU_INDICATOR_X;
constexpr float SOAP_TILT_THRESHOLD = 0.24f;
constexpr uint32_t SOAP_SWING_COOLDOWN_MS = 140;
constexpr uint32_t BRUSH_RUB_COOLDOWN_MS = 150;
constexpr uint32_t RINSE_FOAM_INTERVAL_MS = 350;
constexpr uint32_t RINSE_MIN_DURATION_MS = 1800;
constexpr uint16_t ATMOSPHERE_FADE_OUT_PER_SECOND = 420;
constexpr uint32_t COMPLETE_DURATION_MS = 1500;
constexpr uint32_t INCOMPLETE_DURATION_MS = 700;
constexpr uint32_t FOAM_GROWTH_INTERVAL_MS = 600;
constexpr uint8_t SMALL_FOAM_BRUSH_TARGET = 2;
constexpr uint8_t LARGE_FOAM_BRUSH_TARGET = 4;
constexpr float LOWER_FOAM_Y = 88.0f;
constexpr float MONSTER_FOOT_Y = 110.0f;
constexpr float UPPER_FOAM_FALL_SPEED = 24.0f;
constexpr float LOWER_FOAM_FALL_SPEED = 14.0f;
constexpr uint8_t BODY_FOAM_STAGE_COUNT = 5;
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
    static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_7) -
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
    lastSoapTiltSign = 0;
    soapX = MONSTER_CENTER_X;
    soapY = MONSTER_CENTER_Y;
    brushX = MONSTER_CENTER_X;
    brushY = MONSTER_CENTER_Y;
    previousBrushX = brushX;
    previousBrushY = brushY;
    waterSpawnCarry = 0.0f;
    toast = nullptr;
    toastUntilMs = 0;
    resetBathSession();
    for (WaterDrop& drop : water) drop = WaterDrop{};
}

void ShowerScene::resetBathSession() {
    for (Foam& bubble : foam) bubble = Foam{};
    soapUsed = false;
    atmosphereTarget = false;
    atmosphereAlpha = 0.0f;
    lastFoamGrowthMs = 0;
    soapRewarded = false;
    brushRewarded = false;
    rinseRewarded = false;
    exitConfirmYes = false;
}

void ShowerScene::enterMode(Mode next, uint32_t nowMs) {
    mode = next;
    modeStartedMs = nowMs;
    lastSoapTiltSign = 0;
}

void ShowerScene::update(uint32_t nowMs, float dtSeconds) {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    Hal::ins().readAccel(ax, ay, az);

    switch (mode) {
    case Mode::SOAPING:
        updateSoaping(nowMs, dtSeconds, ax, ay);
        break;
    case Mode::BRUSHING:
        updateBrushing(nowMs, dtSeconds, ax, ay);
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
        previousBrushX = brushX;
        previousBrushY = brushY;
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
    enterMode(Mode::RINSING, nowMs);
}

void ShowerScene::updateSoaping(uint32_t nowMs, float dtSeconds,
                                float ax, float ay) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    float smoothing = 1.0f - expf(-9.0f * dt);
    float targetX = clampFloat(MONSTER_CENTER_X + ax * 54.0f, 44.0f, 161.0f);
    float targetY = clampFloat(MONSTER_CENTER_Y + ay * 42.0f, 33.0f, 122.0f);
    soapX += (targetX - soapX) * smoothing;
    soapY += (targetY - soapY) * smoothing;
    int8_t sign = ax > SOAP_TILT_THRESHOLD
        ? 1 : (ax < -SOAP_TILT_THRESHOLD ? -1 : 0);
    if (sign == 0) return;
    if (lastSoapTiltSign == 0) {
        lastSoapTiltSign = sign;
        return;
    }
    if (sign != lastSoapTiltSign &&
        nowMs - lastSoapSwingMs >= SOAP_SWING_COOLDOWN_MS) {
        lastSoapTiltSign = sign;
        lastSoapSwingMs = nowMs;
        spawnFoam();
    }
}

void ShowerScene::updateBrushing(uint32_t nowMs, float dtSeconds,
                                 float ax, float ay) {
    float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    float smoothing = 1.0f - expf(-9.0f * dt);
    float targetX = clampFloat(MONSTER_CENTER_X + ax * 54.0f, 44.0f, 161.0f);
    float targetY = clampFloat(MONSTER_CENTER_Y + ay * 42.0f, 33.0f, 122.0f);
    brushX += (targetX - brushX) * smoothing;
    brushY += (targetY - brushY) * smoothing;
    float dx = brushX - previousBrushX;
    float dy = brushY - previousBrushY;
    if (dx * dx + dy * dy >= 9.0f &&
        nowMs - lastBrushRubMs >= BRUSH_RUB_COOLDOWN_MS) {
        rubFoamAt(brushX, brushY, nowMs);
        previousBrushX = brushX;
        previousBrushY = brushY;
        lastBrushRubMs = nowMs;
    }

    for (Foam& bubble : foam) {
        if (!bubble.active) continue;
        if (bubble.stage == 2 &&
            bubble.brushProgress >= LARGE_FOAM_BRUSH_TARGET) {
            if (bubble.y < LOWER_FOAM_Y) {
                bubble.y = min(LOWER_FOAM_Y,
                               bubble.y + UPPER_FOAM_FALL_SPEED * dt);
            }
            if (bubble.y >= LOWER_FOAM_Y &&
                nowMs - lastFoamGrowthMs >= FOAM_GROWTH_INTERVAL_MS) {
                bubble.stage = 3;
                bubble.brushProgress = 0;
                bubble.bornMs = nowMs;
                lastFoamGrowthMs = nowMs;
            }
        } else if (bubble.stage == 3 || bubble.stage == 4) {
            bubble.y = min(MONSTER_FOOT_Y,
                           bubble.y + LOWER_FOAM_FALL_SPEED * dt);
            if (bubble.stage == 4 && bubble.y >= MONSTER_FOOT_Y) {
                atmosphereTarget = true;
                awardBathStage(BathRewardStage::BRUSH);
            }
        }
    }
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

    if (nowMs - lastRinseFoamMs >= RINSE_FOAM_INTERVAL_MS) {
        rinseAllFoamOneStage();
        lastRinseFoamMs = nowMs;
    }
    if (!anyFoam()) atmosphereTarget = false;
    if (nowMs - modeStartedMs >= RINSE_MIN_DURATION_MS &&
        !anyFoam() && atmosphereAlpha <= 1.0f) {
        for (WaterDrop& drop : water) drop.active = false;
        if (soapRewarded) {
            awardBathStage(BathRewardStage::RINSE);
            soapUsed = false;
            enterMode(Mode::COMPLETE, nowMs);
        } else {
            resetBathSession();
            enterMode(Mode::MENU, nowMs);
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
        atmosphereAlpha = min(target, atmosphereAlpha + speed * dt);
    } else if (atmosphereAlpha > target) {
        atmosphereAlpha = max(target, atmosphereAlpha - speed * dt);
    }
}

void ShowerScene::spawnFoam() {
    static constexpr int8_t OFFSETS[][2] = {
        {-20, -3}, {-7, 3}, {8, -2}, {20, 4},
        {-15, 5}, {2, -5}, {15, 2}, {-3, 4},
    };
    uint32_t nowMs = Hal::ins().millis();
    for (uint8_t spawned = 0; spawned < 3; ++spawned) {
        Foam* target = nullptr;
        for (Foam& bubble : foam) {
            if (!bubble.active) {
                target = &bubble;
                break;
            }
        }
        if (!target) break;
        uint8_t pattern = foamSpawnCursor %
            (sizeof(OFFSETS) / sizeof(OFFSETS[0]));
        const int8_t* offset = OFFSETS[pattern];
        bool upperZone = (foamSpawnCursor & 1U) == 0;
        target->x = clampFloat(
            MONSTER_CENTER_X + offset[0], 47.0f, 151.0f);
        target->y = clampFloat(
            MONSTER_CENTER_Y + (upperZone ? -24.0f : 20.0f) + offset[1],
            34.0f, 113.0f);
        target->stage = foamSpawnCursor % 3 == 0 ? 1 : 0;
        target->brushProgress = 0;
        target->bornMs = nowMs;
        target->active = true;
        foamSpawnCursor = (foamSpawnCursor + 1) % FOAM_CAP;
    }
    awardBathStage(BathRewardStage::SOAP);
}

void ShowerScene::rubFoamAt(float x, float y, uint32_t nowMs) {
    Foam* nearest = nullptr;
    float bestDistance = 24.0f * 24.0f;
    for (Foam& bubble : foam) {
        if (!bubble.active) continue;
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
    if (nearest->stage <= 1) {
        if (nearest->brushProgress < SMALL_FOAM_BRUSH_TARGET) return;
        nearest->stage = 2;
        nearest->brushProgress = 0;
        nearest->bornMs = nowMs;
    } else if (nearest->stage == 3 &&
               nearest->brushProgress >= LARGE_FOAM_BRUSH_TARGET &&
               nowMs - lastFoamGrowthMs >= FOAM_GROWTH_INTERVAL_MS &&
               !hasFoamStage(4, nearest)) {
        nearest->stage = 4;
        nearest->brushProgress = 0;
        nearest->bornMs = nowMs;
        lastFoamGrowthMs = nowMs;
    }
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

bool ShowerScene::anyFoam() const {
    for (const Foam& bubble : foam) {
        if (bubble.active) return true;
    }
    return false;
}

bool ShowerScene::hasFoamStage(uint8_t stage, const Foam* except) const {
    for (const Foam& bubble : foam) {
        if (&bubble != except && bubble.active && bubble.stage == stage) {
            return true;
        }
    }
    return false;
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
    snprintf(toastBuffer, sizeof(toastBuffer),
             Ui::Shower::EXP_GAIN_FMT, static_cast<unsigned>(experience));
    toast = toastBuffer;
    toastUntilMs = Hal::ins().millis() + 1200;
}

void ShowerScene::render() {
    if (!GameAssets::draw(GameAssets::Kind::SHOWER_BACKGROUND, 0, 0)) {
        PixelRenderer::clear(PixelRenderer::rgb(21, 36, 30));
    }

    drawAtmosphere();
    drawMonster();
    drawFoam();
    drawWater();
    drawTool();
    drawMenu();
    if (mode == Mode::SOAP_SELECT) drawSoapPicker();
    if (mode == Mode::COMPLETE) drawHearts();
    if (mode == Mode::EXIT_CONFIRM) drawExitConfirm();
    if (mode == Mode::INCOMPLETE) drawIncomplete();
    drawToast();
}

void ShowerScene::drawAtmosphere() const {
    if (atmosphereAlpha <= 0.0f) return;
    const auto* frame = PokemonSprites::findSpeciesSprite(
        GameEngine::ins().activeSpecies().id,
        PokemonSprites::SpriteKind::FRONT);
    int drawHeight = 84;
    if (frame) {
        uint8_t width = pgm_read_byte(&frame->width);
        uint8_t height = pgm_read_byte(&frame->height);
        if (width > 0 && height > 0) {
            float scale = min(108.0f / width, 112.0f / height);
            if (scale > 1.4f) scale = 1.4f;
            drawHeight = static_cast<int>(height * scale);
        }
    }
    int monsterBottom = MONSTER_CENTER_Y + drawHeight / 2;
    int centerY = monsterBottom - 10;
    GameAssets::drawCenteredAlpha(
        GameAssets::Kind::SHOWER_BUBBLE_5,
        MONSTER_CENTER_X, centerY, 1.0f,
        static_cast<uint8_t>(clampFloat(atmosphereAlpha, 0.0f, 255.0f)));
}

void ShowerScene::drawMonster() const {
    const auto* frame = PokemonSprites::findSpeciesSprite(
        GameEngine::ins().activeSpecies().id,
        PokemonSprites::SpriteKind::FRONT);
    if (!frame) return;
    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    if (width == 0 || height == 0) return;
    float scale = min(108.0f / width, 112.0f / height);
    if (scale > 1.4f) scale = 1.4f;
    int drawW = static_cast<int>(width * scale);
    int drawH = static_cast<int>(height * scale);
    PokemonSprites::drawFrameScaled(
        frame,
        MONSTER_CENTER_X - drawW / 2,
        MONSTER_CENTER_Y - drawH / 2,
        scale);
}

void ShowerScene::drawFoam() const {
    for (uint8_t stage = 0; stage < BODY_FOAM_STAGE_COUNT; ++stage) {
        for (const Foam& bubble : foam) {
            if (!bubble.active || bubble.stage != stage) continue;
            float scale = stage == 4 ? 1.15f : 1.0f;
            GameAssets::drawCentered(
                showerKind(GameAssets::Kind::SHOWER_BUBBLE_0, bubble.stage),
                static_cast<int>(bubble.x), static_cast<int>(bubble.y), scale);
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
    float target = static_cast<float>(menuCursor);
    while (target - menuAnimCursor > MENU_COUNT / 2.0f) target -= MENU_COUNT;
    while (target - menuAnimCursor < -MENU_COUNT / 2.0f) target += MENU_COUNT;
    float diff = target - menuAnimCursor;
    if (fabsf(diff) < 0.04f) menuAnimCursor = target;
    else menuAnimCursor += diff * 0.25f;

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
    for (uint8_t i = 0; i < 3; ++i) {
        int x = 52 + i * 36;
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

void ShowerScene::drawToast() {
    if (!toast) return;
    if (static_cast<int32_t>(Hal::ins().millis() - toastUntilMs) >= 0) {
        toast = nullptr;
        return;
    }
    PixelRenderer::fillRectAlpha(
        39, 103, 100, 22, PixelRenderer::rgb(18, 22, 29), 225);
    PixelRenderer::text(55, 106, toast, PixelRenderer::rgb(255, 232, 150), 1);
}
