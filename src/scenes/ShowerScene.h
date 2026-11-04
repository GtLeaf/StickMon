#pragma once

#include "core/Scene.h"
#include "game/BathService.h"
#include "game/BathInteraction.h"
#include "ui/ShowerUi.h"

class ShowerScene : public Scene, private Ui::ShowerHost {
public:
    ShowerScene() : ui(*this) {}
    void onEnter() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    int touchTarget(int16_t x, int16_t y) override { return ui.touchTarget(x, y); }
    bool onTap(int target) override { return ui.onTap(target); }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override { return ui.onTouch(event); }
    void cancelTouch() override { Scene::cancelTouch(); ui.cancelTouch(); }
    void onExit() override { cancelTouch(); }

private:
    using Mode = Ui::ShowerMode;
    using Foam = Game::BathInteraction::Foam;
    Ui::ShowerUi ui;
    Mode mode = Mode::MENU;
    Game::BathInteraction::State bath;
    uint32_t lastSoapSwingMs = 0;
    uint32_t lastBrushRubMs = 0;
    uint32_t lastRinseFoamMs = 0;
    uint32_t modeStartedMs = 0;
    bool soapUsed = false;
    bool soapRewarded = false;
    bool brushRewarded = false;
    bool rinseRewarded = false;
    uint8_t completionHearts = 0;
    char completionMessage[64] = {};

    Ui::ShowerFrame showerFrame() const override;
    int8_t nextShowerSoap(int8_t from, int8_t direction = 1) const override;
    bool activateShower(Ui::ShowerAction action, uint8_t value = 0) override;
    void enterMode(Mode next, uint32_t nowMs);
    void resetBathSession();
    void requestExit(uint32_t nowMs);
    void chooseSoap();
    void beginSoap();
    void beginRinse(uint32_t nowMs);
    void updateSoaping(uint32_t nowMs, float dtSeconds, float dynX, float dynY,
                       float shake);
    bool updateBrushing(uint32_t nowMs, float dtSeconds, float dynX, float dynY,
                        float shake);
    void updateRinsing(uint32_t nowMs, float dtSeconds);
    void spawnFoam();
    void rubFoamAt(float x, float y, uint32_t nowMs);
    bool tryMergeFoam(Foam& source, uint32_t nowMs);
    Foam* findMergePartner(const Foam& source);
    void rinseAllFoamOneStage();
    bool anyFoam() const;
    uint8_t foamLevelTotal() const;
    void checkAtmosphereThreshold(uint32_t nowMs);
    Game::BathInteraction::Layout bathLayout() const;
    int monsterBottomY() const;
    float foamRestY(uint8_t stage) const;
    int8_t nextOwnedSoap(int8_t from, int8_t direction) const;
    void awardBathStage(Game::BathService::Stage stage);
};
