#include "scenes/HatchScene.h"
#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
constexpr uint8_t INTERACTION_THRESHOLD = 10;
float savedElapsed = 0.0f;
uint16_t savedPokeCount = 0;
uint16_t savedWipeCount = 0;
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
    auto& c = PixelRenderer::canvas();
    PixelRenderer::clear(PixelRenderer::rgb(194, 219, 224));
    c.fillRect(0, 0, Hal::DISPLAY_W, 54, PixelRenderer::rgb(165, 202, 214));
    c.fillRect(12, 18, 39, 30, PixelRenderer::rgb(238, 247, 230));
    c.drawRect(12, 18, 39, 30, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(31, 18, 31, 47, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(12, 33, 50, 33, PixelRenderer::rgb(95, 130, 138));
    c.fillRect(0, 54, Hal::DISPLAY_W, Hal::DISPLAY_H - 54, PixelRenderer::rgb(226, 209, 174));
    for (int y = 70; y < Hal::DISPLAY_H; y += 18) {
        c.drawLine(0, y, Hal::DISPLAY_W, y, PixelRenderer::rgb(206, 187, 151));
    }
    c.fillRect(8, 72, 119, 112, PixelRenderer::rgb(240, 225, 188));
    c.drawRect(8, 72, 119, 112, PixelRenderer::rgb(173, 140, 101));
}

void HatchScene::drawEgg() {
    auto& c = PixelRenderer::canvas();
    int x = 67 + eggShakeOffset(Hal::ins().millis());
    int y = 138;
    c.fillEllipse(x, 169, 25, 7, PixelRenderer::rgb(159, 139, 117));
    c.fillEllipse(x, y, 27, 38, PixelRenderer::rgb(240, 232, 184));
    c.fillEllipse(x, y + 1, 20, 29, PixelRenderer::rgb(255, 248, 214));
    c.drawEllipse(x, y, 27, 38, PixelRenderer::rgb(109, 92, 62));
    c.fillCircle(x - 13, y - 8, 4, PixelRenderer::rgb(255, 145, 67));
    c.fillCircle(x + 13, y + 9, 4, PixelRenderer::rgb(71, 169, 226));
    c.fillCircle(x + 1, y - 22, 3, PixelRenderer::rgb(102, 190, 90));
}

void HatchScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(4, 2, Ui::ROOM, PixelRenderer::rgb(245, 246, 232));
}

void HatchScene::drawToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(16, 62, 103, 18, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(34, 67, toast, PixelRenderer::rgb(255, 255, 255));
}
