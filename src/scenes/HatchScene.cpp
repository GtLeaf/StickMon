#include "scenes/HatchScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/GameEngine.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "game/HatchService.h"
#include "presentation/PixelRenderer.h"

namespace {
float savedElapsed = 0.0f;
uint16_t savedPokeCount = 0;
uint16_t savedWipeCount = 0;

bool hatchSceneIsNight() {
    uint16_t minutes = GameEngine::ins().gameMinutesOfDay();
    return minutes < 6 * 60 || minutes >= 18 * 60;
}

float hatchCameraY() {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) return 0.0f;
    float maxCamera = static_cast<float>(room.roomY() + room.height() - Hal::DISPLAY_H);
    if (maxCamera < 0.0f) maxCamera = 0.0f;
    float camera = static_cast<float>(room.roomY() + room.bedY() - 16) - 84.0f;
    return std::max(0.0f, std::min(maxCamera, camera));
}
}

void HatchScene::clearRuntimeProgress() {
    savedElapsed = 0.0f;
    savedPokeCount = 0;
    savedWipeCount = 0;
}

void HatchScene::onEnter() {
    Game::HatchProgress progress;
    if (GameEngine::ins().loadHatchProgress(progress)) {
        elapsed = progress.elapsedSeconds;
        pokeCount = progress.pokeCount;
        wipeCount = progress.wipeCount;
    } else {
        elapsed = savedElapsed;
        pokeCount = savedPokeCount;
        wipeCount = savedWipeCount;
    }
    lastSavedSecond = (uint16_t)elapsed;
    lastRemainingSecond = hatchSecondsRemaining();
    uint32_t nowMs = Hal::ins().millis();
    lastEggOffset = eggShakeOffset(nowMs);
    lastNight = hatchSceneIsNight();
}

void HatchScene::onExit() {
    savedElapsed = elapsed;
    savedPokeCount = pokeCount;
    savedWipeCount = wipeCount;
    persistProgress(true);
}

SceneUpdateResult HatchScene::update(uint32_t nowMs, float dtSeconds) {
    RenderDemand demand;
    elapsed += dtSeconds;
    persistProgress(false);
    if (elapsed >= GameEngine::ins().gameState().hatchSeconds) {
        complete();
        return demand.result();
    }

    int8_t eggOffset = eggShakeOffset(nowMs);
    bool night = hatchSceneIsNight();
    demand.changed(eggOffset != lastEggOffset || night != lastNight);
    lastEggOffset = eggOffset;
    lastNight = night;

    uint8_t progress = hatchProgress();
    uint16_t interval = 2400 - static_cast<uint16_t>(progress) * 19;
    if (interval < 360) interval = 360;
    uint16_t phaseMs = nowMs % interval;
    demand.wakeIn(phaseMs <= 230
        ? 46 - (phaseMs % 46)
        : interval - phaseMs);

    float remaining =
        GameEngine::ins().gameState().hatchSeconds - elapsed;
    uint16_t remainingSecond = hatchSecondsRemaining();
    demand.changed(remainingSecond != lastRemainingSecond);
    lastRemainingSecond = remainingSecond;

    float nextSecond = remaining - floorf(remaining);
    if (nextSecond < 0.001f) nextSecond = 1.0f;
    demand.wakeIn(static_cast<uint32_t>(ceilf(nextSecond * 1000.0f)));

    float speed = GameEngine::ins().gameSpeed();
    uint32_t completionDelay = static_cast<uint32_t>(
        std::max(1.0f, remaining * 1000.0f /
                           std::max(0.01f, speed)));
    demand.wakeIn(completionDelay);

    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool HatchScene::onButton(const ButtonEvent& event) {
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    if (event.action != BtnAction::PRESSED) return false;
    if (event.btn == 0) {
        toast = Ui::Hatch::AQUA;
        wipeCount++;
    } else {
        toast = Ui::Hatch::WARM;
        pokeCount++;
    }
    toastUntil = Hal::ins().millis() + 700;
    return true;
}

void HatchScene::render() {
    drawRoom();
    drawEgg();
    drawHud();
    drawToast();
}

void HatchScene::complete() {
    const uint8_t style = Game::HatchService::starterStyle(pokeCount, wipeCount);
    savedElapsed = 0.0f;
    savedPokeCount = 0;
    savedWipeCount = 0;
    elapsed = 0.0f;
    pokeCount = 0;
    wipeCount = 0;
    GameEngine::ins().clearHatchProgress();
    GameEngine::ins().finishHatch(style);
}

void HatchScene::persistProgress(bool force) {
    if (GameEngine::ins().gameState().oobeDone) return;
    uint16_t elapsedSeconds = (uint16_t)elapsed;
    if (!force && elapsedSeconds < lastSavedSecond + 15) return;

    Game::HatchProgress progress;
    progress.elapsedSeconds = elapsedSeconds;
    progress.pokeCount = pokeCount;
    progress.wipeCount = wipeCount;
    if (GameEngine::ins().saveHatchProgress(progress)) {
        lastSavedSecond = elapsedSeconds;
    }
}

uint8_t HatchScene::hatchProgress() const {
    return Game::HatchService::progressPercent(
        elapsed, GameEngine::ins().gameState().hatchSeconds);
}

uint16_t HatchScene::hatchSecondsRemaining() const {
    return Game::HatchService::remainingSeconds(
        elapsed, GameEngine::ins().gameState().hatchSeconds);
}

int8_t HatchScene::eggShakeOffset(uint32_t nowMs) const {
    return Game::HatchService::shakeOffset(hatchProgress(), nowMs);
}

void HatchScene::drawRoom() {
    RoomRenderer::draw(hatchCameraY(), hatchSceneIsNight());
}

void HatchScene::drawEgg() {
    auto& c = PixelRenderer::canvas();
    RoomResource& room = RoomResource::ins();
    int x = room.available() ? room.bedX() : Hal::DISPLAY_W / 2;
    int y = room.available()
        ? static_cast<int>(room.roomY() + room.bedY() - 26 - hatchCameraY())
        : 84;
    x += eggShakeOffset(Hal::ins().millis());
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, x, y)) {
        c.fillRect(x - 12, y - 16, 24, 32, PixelRenderer::rgb(255, 216, 72));
    }
}

void HatchScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 168;
    static constexpr int PANEL_Y = 2;
    static constexpr int PANEL_W = 68;
    static constexpr int ASCII_ADVANCE = 8;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, 22,
               PixelRenderer::rgb(34, 39, 47));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, 22,
               PixelRenderer::rgb(72, 83, 98));

    uint16_t remaining = hatchSecondsRemaining();
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%02u:%02u",
             remaining / 60, remaining % 60);
    int textX = PANEL_X +
        (PANEL_W - static_cast<int>(strlen(countdown)) * ASCII_ADVANCE) / 2;
    PixelRenderer::text(textX, PANEL_Y + 3, countdown,
                        PixelRenderer::rgb(245, 246, 232));
}

void HatchScene::drawToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(72, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(98, 8, toast, PixelRenderer::rgb(255, 255, 255));
}
