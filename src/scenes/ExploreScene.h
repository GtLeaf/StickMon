#pragma once

#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"

class ExploreScene : public Scene {
public:
    void onEnter() override;
    void onExit() override {}
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum class Phase : uint8_t {
        WALKING,
        ENCOUNTER,
        RESULT,
    };

    Phase phase = Phase::WALKING;
    uint16_t steps = 0;
    uint16_t targetSteps = 60;
    const Species* wild = nullptr;
    Game::MonsterRuntime wildRuntime;
    uint16_t wildHp = 0;
    uint16_t wildHpMax = 0;
    const char* toast = nullptr;
    char toastBuf[32] = {};
    uint32_t toastUntil = 0;
    bool lastCaptureSuccess = false;
    uint8_t battleCursor = 0;

    void walk();
    void rollEncounter();
    void rollPickupEvent();
    void resolvePickup(uint8_t pickupId);
    void attackWild();
    void wildCounterattack();
    void finishPlayerFaint();
    void tryCapture();
    void fleeEncounter();
    void resetWalk();
    void renderWalking();
    void renderEncounter();
    void renderResult();
    void renderToast();
    void drawWildBlock(int x, int y);
    void drawMonsterBlock(const Species& species, int x, int y);
    void renderBattleHud();
    void renderCommandBox();
};
