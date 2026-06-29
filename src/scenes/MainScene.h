#pragma once

#include "core/Scene.h"
#include "game/Species.h"

class MainScene : public Scene {
public:
    void onEnter() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    struct RenderItem {
        int16_t z;
        void (MainScene::*draw)();
    };

    void drawBackground();
    void drawFloor();
    void drawFood();
    void drawShadow();
    void drawMonster();
    void drawStateEffect();
    void drawHud();
    void drawToast();
    void sortAndDraw(RenderItem* items, uint8_t count);

    const Species* active = nullptr;
    float monsterX = 66.0f;
    float monsterY = 140.0f;
    float velocity = 14.0f;
    float bob = 0.0f;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
};
