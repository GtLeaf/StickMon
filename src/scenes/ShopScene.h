#pragma once

#include "core/Scene.h"

class ShopScene : public Scene {
public:
    void onEnter() override {}
    void onExit() override {}
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum Item : uint8_t {
        BALL = 0,
        GREAT_BALL,
        FOOD,
        POTION,
        ANTIDOTE,
        CANDY,
        BACK,
        COUNT,
    };

    uint8_t cursor = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;

    void buyCurrent();
    void renderList();
    void renderToast();
    static uint16_t priceFor(Item item);
};
