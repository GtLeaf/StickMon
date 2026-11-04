#include "scenes/ShopScene.h"
#include "core/GameEngine.h"

void ShopScene::onEnter() { ui.onEnter(); }
void ShopScene::onExit() { ui.onExit(); ui.cancelTouch(); }
SceneUpdateResult ShopScene::update(uint32_t nowMs, float dtSeconds) {
    return ui.update(nowMs, dtSeconds);
}
void ShopScene::render() { ui.render(); }
bool ShopScene::onButton(const ButtonEvent& event) { return ui.onButton(event); }
bool ShopScene::touchNavigation() const { return ui.touchNavigation(); }
bool ShopScene::onTouch(const Platform::IInputDevice::TouchEvent& event) { return ui.onTouch(event); }
void ShopScene::cancelTouch() { ui.cancelTouch(); }
int ShopScene::touchTarget(int16_t x, int16_t y) { return ui.touchTarget(x, y); }
bool ShopScene::onTap(int target) { return ui.onTap(target); }

const Game::GameState& ShopScene::shopState() const { return GameEngine::ins().gameState(); }

Game::ShopService::BuyResult ShopScene::buyShopItem(Features::ShopFlow::Item item) {
    auto& engine = GameEngine::ins();
    // Synchronize the game day before applying the shared candy purchase cap.
    if (item == Features::ShopFlow::CANDY) engine.candyPurchaseAvailable();
    const auto result = Features::ShopFlow::buy(engine.gameState(), item);
    if (result == Game::ShopService::BuyResult::BOUGHT)
        engine.markDirty(SaveUrgency::SOON);
    return result;
}

Game::ShopService::SellResult ShopScene::sellShopItem(Features::ShopFlow::Item item) {
    auto& engine = GameEngine::ins();
    const auto result = Features::ShopFlow::sell(engine.gameState(), item);
    if (result == Game::ShopService::SellResult::SOLD)
        engine.markDirty(SaveUrgency::SOON);
    return result;
}

void ShopScene::leaveShop() { GameEngine::ins().requestScene(SceneID::MENU); }
