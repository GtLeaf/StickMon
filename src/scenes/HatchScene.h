#pragma once

#include "core/Scene.h"
#include "ui/HatchUi.h"

class HatchScene : public Scene {
public:
    static void clearRuntimeProgress();

    void onEnter() override;
    void onExit() override;
    bool onBeforeSave() override { return persistProgress(true); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;

private:
    float elapsed = 0.0f;
    uint16_t pokeCount = 0;
    uint16_t wipeCount = 0;
    uint16_t lastSavedSecond = 0;
    Ui::HatchUi ui;

    void complete();
    bool persistProgress(bool force);
    Ui::HatchFrame hatchFrame(uint32_t nowMs) const;
    bool applyAction(Ui::HatchAction action);
};
