#include "scenes/HatchScene.h"
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/GameEngine.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
constexpr uint8_t INTERACTION_THRESHOLD = 10;
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
    return constrain(camera, 0.0f, maxCamera);
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
}

void HatchScene::onExit() {
    savedElapsed = elapsed;
    savedPokeCount = pokeCount;
    savedWipeCount = wipeCount;
    persistProgress(true);
}

void HatchScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    elapsed += dtSeconds;
    persistProgress(false);
    if (elapsed >= GameEngine::ins().gameState().hatchSeconds) complete();
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
    persistProgress(true);
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
    uint8_t style = 0;
    uint16_t totalInteraction = pokeCount + wipeCount;
    if (totalInteraction >= INTERACTION_THRESHOLD) {
        if (pokeCount > wipeCount) style = 1;
        else if (wipeCount > pokeCount) style = 2;
    }
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
    uint16_t total = GameEngine::ins().gameState().hatchSeconds;
    if (total == 0) return 100;
    uint16_t value = (uint16_t)((elapsed * 100.0f) / total);
    return value > 100 ? 100 : (uint8_t)value;
}

int8_t HatchScene::eggShakeOffset(uint32_t nowMs) const {
    uint8_t progress = hatchProgress();
    uint16_t interval = 2400 - (uint16_t)progress * 19;
    if (interval < 360) interval = 360;
    uint16_t phase = nowMs % interval;
    if (phase > 230) return 0;
    uint8_t frame = phase / 46;
    static constexpr int8_t OFFSETS[] = {-3, 3, -2, 2, 0};
    return OFFSETS[frame % 5];
}

void HatchScene::drawRoom() {
    RoomRenderer::draw(hatchCameraY(), hatchSceneIsNight());
}

void HatchScene::drawEgg() {
    auto& c = PixelRenderer::canvas();
    RoomResource& room = RoomResource::ins();
    int x = room.available() ? room.bedX() : Hal::DISPLAY_W / 2;
    int y = room.available()
        ? static_cast<int>(room.roomY() + room.bedY() - 16 - hatchCameraY())
        : 84;
    x += eggShakeOffset(Hal::ins().millis());
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, x, y)) {
        c.fillRect(x - 12, y - 16, 24, 32, PixelRenderer::rgb(255, 216, 72));
    }
}

void HatchScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(168, 2, 68, 22, PixelRenderer::rgb(34, 39, 47));
    c.drawRect(168, 2, 68, 22, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(176, 5, Ui::Hatch::TITLE, PixelRenderer::rgb(245, 246, 232));
}

void HatchScene::drawToast() {
    if (!toast) return;
    if ((int32_t)(Hal::ins().millis() - toastUntil) >= 0) {
        toast = nullptr;
        return;
    }
    auto& c = PixelRenderer::canvas();
    c.fillRect(72, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(98, 8, toast, PixelRenderer::rgb(255, 255, 255));
}
