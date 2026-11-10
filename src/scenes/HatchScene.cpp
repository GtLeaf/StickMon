#include "scenes/HatchScene.h"
#include <algorithm>
#include <cmath>
#include "core/GameEngine.h"
#include "hardware/Hal.h"
#include "game/HatchService.h"

namespace {
float savedElapsed = 0.0f;
uint16_t savedPokeCount = 0;
uint16_t savedWipeCount = 0;
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
    ui.enter(hatchFrame(Hal::ins().millis()));
}

void HatchScene::onExit() {
    ui.leave();
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

    demand.changed(applyAction(ui.readMotion(nowMs, demand)));
    ui.updateVisuals(hatchFrame(nowMs), demand);
    float remaining = GameEngine::ins().gameState().hatchSeconds - elapsed;

    float nextSecond = remaining - floorf(remaining);
    if (nextSecond < 0.001f) nextSecond = 1.0f;
    demand.wakeIn(static_cast<uint32_t>(ceilf(nextSecond * 1000.0f)));

    float speed = GameEngine::ins().gameSpeed();
    uint32_t completionDelay = static_cast<uint32_t>(
        std::max(1.0f, remaining * 1000.0f /
                           std::max(0.01f, speed)));
    demand.wakeIn(completionDelay);

    ui.updateToast(nowMs, demand);
    return demand.result();
}

bool HatchScene::applyAction(Ui::HatchAction action) {
    switch (action) {
    case Ui::HatchAction::MENU:
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    case Ui::HatchAction::WIPE:
        if (wipeCount < UINT16_MAX) ++wipeCount;
        return true;
    case Ui::HatchAction::POKE:
        if (pokeCount < UINT16_MAX) ++pokeCount;
        return true;
    case Ui::HatchAction::NONE: return false;
    }
    return false;
}

bool HatchScene::onButton(const ButtonEvent& event) {
    const auto action = ui.onButton(event, Hal::ins().millis());
    return applyAction(action);
}

void HatchScene::render() { ui.render(hatchFrame(Hal::ins().millis())); }

int HatchScene::touchTarget(int16_t x, int16_t y) { return ui.touchTarget(x, y); }

bool HatchScene::onTap(int target) {
    const auto action = ui.onTap(target, Hal::ins().millis());
    return applyAction(action);
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

bool HatchScene::persistProgress(bool force) {
    if (GameEngine::ins().gameState().oobeDone) return true;
    uint16_t elapsedSeconds = (uint16_t)elapsed;
    if (!force && elapsedSeconds < lastSavedSecond + 15) return true;

    Game::HatchProgress progress;
    progress.elapsedSeconds = elapsedSeconds;
    progress.pokeCount = pokeCount;
    progress.wipeCount = wipeCount;
    if (GameEngine::ins().saveHatchProgress(progress)) {
        lastSavedSecond = elapsedSeconds;
        return true;
    }
    return false;
}

Ui::HatchFrame HatchScene::hatchFrame(uint32_t nowMs) const {
    const auto& engine = GameEngine::ins();
    const auto total = engine.gameState().hatchSeconds;
    const uint16_t minutes = engine.gameMinutesOfDay();
    return {nowMs, Game::HatchService::progressPercent(elapsed, total),
        Game::HatchService::remainingSeconds(elapsed, total),
        minutes < 6 * 60 || minutes >= 18 * 60};
}
