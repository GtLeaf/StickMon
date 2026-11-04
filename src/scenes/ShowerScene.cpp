#include "scenes/ShowerScene.h"

#include <cstdio>

#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/GameRandom.h"
#include "hardware/Hal.h"
#include "presentation/ShowerTiming.h"

using namespace ShowerTiming;

void ShowerScene::onEnter() {
    cancelTouch();
    mode = Mode::MENU;
    bath.spawnCursor = 0;
    ui.enter();
    resetBathSession();
}

void ShowerScene::resetBathSession() {
    for (Foam& bubble : bath.foam) bubble = Foam{};
    soapUsed = false;
    bath.atmosphereTarget = false;
    bath.lastGrowthMs = 0;
    bath.restSlot = 0;
    ui.resetSession();
    soapRewarded = false;
    brushRewarded = false;
    rinseRewarded = false;
    completionHearts = 0;
    completionMessage[0] = '\0';
}

void ShowerScene::enterMode(Mode next, uint32_t nowMs) {
    ui.cancelTouch();
    mode = next;
    modeStartedMs = nowMs;
}

SceneUpdateResult ShowerScene::update(uint32_t nowMs, float dtSeconds) {
    RenderDemand demand;
    const Mode modeBefore = mode;
    const float atmosphereBefore = ui.state.atmosphereAlpha;
    const auto menuStep = ui.updateMenu();
    demand.changed(menuStep.changed);
    const auto motion = ui.readMotion(nowMs, dtSeconds);
    bool foamMoving = false;
    switch (mode) {
    case Mode::SOAPING:
        updateSoaping(nowMs, dtSeconds, motion.dynX, motion.dynY, motion.shake);
        break;
    case Mode::BRUSHING:
        foamMoving = updateBrushing(nowMs, dtSeconds, motion.dynX, motion.dynY, motion.shake);
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
            return SceneUpdateResult::frame();
        }
        break;
    default:
        break;
    }
    ui.updateAtmosphere(bath.atmosphereTarget, dtSeconds);
    demand.changed(mode != modeBefore || ui.state.atmosphereAlpha != atmosphereBefore);
    const bool effects = ui.updateEffects(nowMs, bath.atmosphereTarget, demand);
    const bool modeAnimating =
        ((mode == Mode::SOAPING || mode == Mode::BRUSHING) && ui.usesMotionInput()) || foamMoving ||
        mode == Mode::RINSING || mode == Mode::COMPLETE;
    demand.animate(modeAnimating || effects || menuStep.active);
    if (mode == Mode::INCOMPLETE) {
        const uint32_t elapsed = nowMs - modeStartedMs;
        if (elapsed < INCOMPLETE_DURATION_MS)
            demand.wakeIn(INCOMPLETE_DURATION_MS - elapsed);
    }
    return demand.result();
}

bool ShowerScene::onButton(const ButtonEvent& event) {
    cancelTouch();
    return ui.onButton(event);
}

bool ShowerScene::activateShower(Ui::ShowerAction action, uint8_t value) {
    const uint32_t nowMs = Hal::ins().millis();
    switch (action) {
    case Ui::ShowerAction::EXIT_CHOICE:
        if (value) {
            ui.clearWater();
            enterMode(Mode::INCOMPLETE, nowMs);
        } else enterMode(Mode::MENU, nowMs);
        break;
    case Ui::ShowerAction::MENU:
        enterMode(Mode::MENU, nowMs);
        if (value) ui.clearWater();
        break;
    case Ui::ShowerAction::BEGIN_SOAP:
        beginSoap();
        break;
    case Ui::ShowerAction::REQUEST_EXIT:
        requestExit(nowMs);
        break;
    case Ui::ShowerAction::CHOOSE_SOAP:
        chooseSoap();
        break;
    case Ui::ShowerAction::BEGIN_BRUSH: {
        enterMode(Mode::BRUSHING, nowMs);
        const auto geometry = ui.layout();
        ui.state.brushX = geometry.centerX;
        ui.state.brushY = geometry.centerY;
        break;
    }
    case Ui::ShowerAction::BEGIN_RINSE:
        beginRinse(nowMs);
        break;
    }
    return true;
}

void ShowerScene::requestExit(uint32_t nowMs) {
    if (soapUsed && !rinseRewarded) {
        ui.state.exitConfirmYes = false;
        enterMode(Mode::EXIT_CONFIRM, nowMs);
        return;
    }
    GameEngine::ins().requestScene(SceneID::MENU);
}

void ShowerScene::chooseSoap() {
    if (soapUsed) {
        ui.state.toast = Ui::Shower::ALREADY_SOAPED;
        ui.state.toastUntilMs = Hal::ins().millis() + 1200;
        return;
    }
    int8_t owned = nextOwnedSoap(-1, 1);
    if (owned < 0) {
        ui.state.toast = Ui::Shower::NO_SOAP;
        ui.state.toastUntilMs = Hal::ins().millis() + 1200;
        return;
    }
    ui.state.soapCursor = static_cast<uint8_t>(owned);
    enterMode(Mode::SOAP_SELECT, Hal::ins().millis());
}

void ShowerScene::beginSoap() {
    Game::ItemId item = Game::itemIdForSoapIndex(ui.state.soapCursor);
    if (!GameEngine::ins().removeItem(item, 1, SaveUrgency::IMMEDIATE)) {
        chooseSoap();
        return;
    }
    soapUsed = true;
    const auto geometry = ui.layout();
    ui.state.soapX = geometry.centerX;
    ui.state.soapY = geometry.centerY;
    enterMode(Mode::SOAPING, Hal::ins().millis());
}

void ShowerScene::beginRinse(uint32_t nowMs) {
    ui.beginRinse(nowMs);
    lastRinseFoamMs = nowMs;
    enterMode(Mode::RINSING, nowMs);
}

void ShowerScene::updateSoaping(uint32_t nowMs, float dtSeconds,
                                float dynX, float dynY, float shake) {
    ui.moveTool(true, dtSeconds, {dynX, dynY, shake});
    if (shake >= SHAKE_THRESHOLD_G &&
        nowMs - lastSoapSwingMs >= SOAP_SWING_COOLDOWN_MS) {
        lastSoapSwingMs = nowMs;
        spawnFoam();
    }
}

bool ShowerScene::updateBrushing(uint32_t nowMs, float dtSeconds,
                                 float dynX, float dynY, float shake) {
    ui.moveTool(false, dtSeconds, {dynX, dynY, shake});
    const float dt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    if (shake >= SHAKE_THRESHOLD_G &&
        nowMs - lastBrushRubMs >= BRUSH_RUB_COOLDOWN_MS) {
        rubFoamAt(ui.state.brushX, ui.state.brushY, nowMs);
        lastBrushRubMs = nowMs;
    }

    const bool moving = Game::BathInteraction::update(bath, bathLayout(), dt);
    checkAtmosphereThreshold(nowMs);
    return moving;
}

void ShowerScene::updateRinsing(uint32_t nowMs, float dtSeconds) {
    ui.updateRinseWater(nowMs, dtSeconds, bath.spawnCursor);

    if (nowMs - lastRinseFoamMs >= RINSE_FOAM_INTERVAL_MS) {
        rinseAllFoamOneStage();
        lastRinseFoamMs = nowMs;
    }
    if (!anyFoam()) bath.atmosphereTarget = false;
    if (nowMs - modeStartedMs >= RINSE_MIN_DURATION_MS &&
        !anyFoam() && ui.state.atmosphereAlpha <= 1.0f) {
        ui.clearWater();
        awardBathStage(BathRewardStage::RINSE);
        soapUsed = false;
        completionHearts = static_cast<uint8_t>(
            (soapRewarded ? 1 : 0) + (brushRewarded ? 1 : 0) +
            (rinseRewarded ? 1 : 0));
        GameEngine::ins().applyBathCompletionRecovery(completionHearts);
        snprintf(completionMessage, sizeof(completionMessage),
                 Ui::Shower::COMPLETE_MESSAGE_FMT,
                 GameEngine::ins().activeSpecies().name);
        enterMode(Mode::COMPLETE, nowMs);
        // Completion reactions retain the original score-dependent order.
        ui.state.hopStartMs = 0;
        if (completionHearts >= 3) {
            CryPlayer::ins().play(GameEngine::ins().activeSpecies().id);
        } else if (completionHearts == 2) {
            ui.startHop(10.0f, nowMs);
        }
    }
}

void ShowerScene::spawnFoam() {
    const uint32_t nowMs = Hal::ins().millis();
    Game::BathInteraction::spawn(bath, bathLayout(), nowMs);
    if (nowMs - ui.state.hopStartMs >= SOAP_HOP_INTERVAL_MS) ui.startHop(6.0f, nowMs);
    awardBathStage(BathRewardStage::SOAP);
}

void ShowerScene::rubFoamAt(float x, float y, uint32_t nowMs) {
    const auto result = Game::BathInteraction::rub(bath, bathLayout(), x, y, nowMs);
    if (!result.touched) return;
    if (result.merged) ui.startHop(3.0f, nowMs);
    checkAtmosphereThreshold(nowMs);
    if (nowMs - ui.state.lastRubReactionMs >= RUB_REACTION_COOLDOWN_MS) {
        ui.state.lastRubReactionMs = nowMs;
        if (GameRandom::random(100) < 35) ui.startTurn(nowMs);
    }
}

bool ShowerScene::tryMergeFoam(Foam& source, uint32_t nowMs) {
    if (!Game::BathInteraction::merge(bath, bathLayout(), source, nowMs)) return false;
    ui.startHop(3.0f, nowMs);
    checkAtmosphereThreshold(nowMs);
    return true;
}

uint8_t ShowerScene::foamLevelTotal() const {
    return Game::BathInteraction::lowerWeight(bath, bathLayout());
}

void ShowerScene::checkAtmosphereThreshold(uint32_t nowMs) {
    if (Game::BathInteraction::checkAtmosphere(bath, bathLayout())) {
        awardBathStage(BathRewardStage::BRUSH);
        ui.startHop(3.0f, nowMs);
    }
}

ShowerScene::Foam* ShowerScene::findMergePartner(const Foam& source) {
    return Game::BathInteraction::partner(bath, bathLayout(), source);
}

void ShowerScene::rinseAllFoamOneStage() {
    Game::BathInteraction::rinse(bath);
}

bool ShowerScene::anyFoam() const {
    return Game::BathInteraction::anyFoam(bath);
}

int ShowerScene::monsterBottomY() const {
    return ui.layout().monsterBottomY(GameEngine::ins().activeSpecies().id);
}

Game::BathInteraction::Layout ShowerScene::bathLayout() const {
    return ui.layout().bathLayout(GameEngine::ins().activeSpecies().id);
}

float ShowerScene::foamRestY(uint8_t stage) const {
    return Game::BathInteraction::restY(bathLayout(), stage);
}

int8_t ShowerScene::nextOwnedSoap(int8_t from, int8_t direction) const {
    return Game::BathService::nextOwnedSoap(GameEngine::ins().gameState(), from, direction);
}

int8_t ShowerScene::nextShowerSoap(int8_t from, int8_t direction) const {
    return nextOwnedSoap(from, direction);
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
    uint32_t experience = GameEngine::ins().grantBathReward(stage);
    if (experience == 0) return;
    uint32_t nowMs = Hal::ins().millis();
    ui.spawnExpFloat(experience, nowMs);
    ui.spawnHeartFloat(nowMs);
    ui.startHop(10.0f, nowMs);
}

void ShowerScene::render() { ui.render(); }

Ui::ShowerFrame ShowerScene::showerFrame() const {
    Ui::ShowerFrame frame;
    frame.nowMs = Hal::ins().millis();
    frame.modeStartedMs = modeStartedMs;
    frame.mode = mode;
    frame.visuals = &ui.state;
    frame.bath = &bath;
    frame.speciesId = GameEngine::ins().activeSpecies().id;
    // Stock is only needed while drawing or interacting with the picker.
    if (mode == Mode::SOAP_SELECT) {
        frame.soapCount = GameEngine::ins().soapCount(ui.state.soapCursor);
        for (uint8_t i = 0; i < Game::SOAP_VARIANT_COUNT; ++i)
            frame.soapStocks[i] = GameEngine::ins().soapCount(i);
    }
    frame.completionHearts = completionHearts;
    frame.completionMessage = completionMessage;
    return frame;
}
