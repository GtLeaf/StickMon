#pragma once

#include "core/Scene.h"
#include "ui/ShopUi.h"

// Application-side adapter. The selected UI owns layout and input state;
// transactions and persistence remain shared across UI families.
class ShopScene : public Scene, private Ui::ShopHost {
public:
    ShopScene() : ui(*this) {}
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
private:
    Ui::ShopUi ui;
    const Game::GameState& shopState() const override;
    Game::ShopService::BuyResult buyShopItem(Features::ShopFlow::Item item) override;
    Game::ShopService::SellResult sellShopItem(Features::ShopFlow::Item item) override;
    void leaveShop() override;
};
