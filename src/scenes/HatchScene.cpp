#include "scenes/HatchScene.h"
#include <cstdio>
#include "assets/PokemonSprites.h"
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
    c.fillRect(0, 0, Hal::DISPLAY_W, 42, PixelRenderer::rgb(165, 202, 214));
    c.fillRect(18, 10, 50, 26, PixelRenderer::rgb(238, 247, 230));
    c.drawRect(18, 10, 50, 26, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(43, 10, 43, 35, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(18, 23, 67, 23, PixelRenderer::rgb(95, 130, 138));
    c.fillRect(0, 42, Hal::DISPLAY_W, Hal::DISPLAY_H - 42, PixelRenderer::rgb(226, 209, 174));
    for (int y = 56; y < Hal::DISPLAY_H; y += 16) {
        c.drawLine(0, y, Hal::DISPLAY_W, y, PixelRenderer::rgb(206, 187, 151));
    }
    c.fillRect(38, 58, 164, 58, PixelRenderer::rgb(240, 225, 188));
    c.drawRect(38, 58, 164, 58, PixelRenderer::rgb(173, 140, 101));
}

void HatchScene::drawEgg() {
    auto& c = PixelRenderer::canvas();
    int x = 120 + eggShakeOffset(Hal::ins().millis());
    int y = 87;
    c.fillEllipse(x, 115, 25, 6, PixelRenderer::rgb(159, 139, 117));
    uint8_t w = pgm_read_byte(&PokemonSprites::EGG_FRAME.width);
    uint8_t h = pgm_read_byte(&PokemonSprites::EGG_FRAME.height);
    uint32_t length = pgm_read_dword(&PokemonSprites::EGG_FRAME.length);
    if (length > 0) {
        PixelRenderer::drawRgb565Rle(x - w / 2, y - h / 2, w, h, PokemonSprites::SPRITE_RLE,
                                     pgm_read_dword(&PokemonSprites::EGG_FRAME.offset), length);
        return;
    }
    c.fillEllipse(x, y, 24, 31, PixelRenderer::rgb(240, 232, 184));
    c.fillEllipse(x, y + 1, 18, 24, PixelRenderer::rgb(255, 248, 214));
    c.drawEllipse(x, y, 24, 31, PixelRenderer::rgb(109, 92, 62));
    c.fillCircle(x - 12, y - 7, 4, PixelRenderer::rgb(255, 145, 67));
    c.fillCircle(x + 12, y + 8, 4, PixelRenderer::rgb(71, 169, 226));
    c.fillCircle(x + 1, y - 18, 3, PixelRenderer::rgb(102, 190, 90));
}

void HatchScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(168, 2, 68, 22, PixelRenderer::rgb(34, 39, 47));
    c.drawRect(168, 2, 68, 22, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(176, 5, Ui::ROOM, PixelRenderer::rgb(245, 246, 232));
}

void HatchScene::drawToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(72, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(98, 8, toast, PixelRenderer::rgb(255, 255, 255));
}
