#pragma once

#include "core/Scene.h"

class HatchScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    float elapsed = 0.0f;
    uint16_t pokeCount = 0;
    uint16_t wipeCount = 0;
    uint16_t lastSavedSecond = 0;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;

    void complete();
    void persistProgress(bool force);
    uint8_t hatchProgress() const;
    int8_t eggShakeOffset(uint32_t nowMs) const;
    void drawRoom();
    void drawEgg();
    void drawHud();
    void drawToast();
};
