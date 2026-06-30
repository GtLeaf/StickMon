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
    enum class AiMode : uint8_t {
        IDLE,
        WANDER,
        SEEK_FOOD,
    };

    struct RenderItem {
        int16_t z;
        void (MainScene::*draw)();
    };

    void updateMonsterAi(uint32_t nowMs, float dtSeconds);
    void chooseAiGoal(uint32_t nowMs);
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
    float monsterX = 98.0f;
    float monsterY = 91.0f;
    float targetX = 98.0f;
    float targetY = 91.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    AiMode aiMode = AiMode::IDLE;
    uint32_t nextAiDecisionMs = 0;
    bool facingRight = true;
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
};
