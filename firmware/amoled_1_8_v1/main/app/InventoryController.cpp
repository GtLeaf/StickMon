#include "InventoryController.h"

#include <algorithm>
#include <cmath>

namespace AmoledV1 {

void InventoryController::open() {
    const bool battleBagMode = state_.battleBagMode;
    state_ = State{};
    state_.battleBagMode = battleBagMode;
}

void InventoryController::closeDetail() {
    state_.itemConfirmOpen = false;
    state_.pendingItem = Game::ItemId::COUNT;
    state_.pendingItemAction = PendingItemAction::NONE;
    state_.pressedShopDetailAction = -1;
    state_.shopDetailProgress = 0.0f;
    state_.shopDetailItemIndex = -1;
}

void InventoryController::close() {
    state_.itemVelocity = 0.0f;
    closeDetail();
    state_.selectingItemTarget = false;
    state_.teamItemFeedbackActive = false;
    state_.teamItemTargetSlot = 0xFF;
    state_.battleBagMode = false;
}

void InventoryController::selectCategory(Game::ShopService::Category category) {
    state_.shopCategory = category;
    state_.itemScroll = state_.itemVelocity = 0.0f;
}

InventoryController::Selection InventoryController::selectItem(
    const Game::GameState& gameState, AppSceneFlow::Scene page, uint8_t index) {
    const Game::ItemId item = itemAt(gameState, page, index);
    if (item == Game::ItemId::COUNT) return Selection::NONE;
    if (page == AppSceneFlow::Scene::BAG) {
        if (state_.battleBagMode && Game::foodIndexForItemId(item) < 0 &&
            !Game::ItemInventory::usableInBattle(item)) return Selection::CANNOT_USE;
        if (!state_.battleBagMode && item == Game::ItemId::HEART_SCALE)
            return Selection::OPEN_MOVE_MANAGER;
        state_.pendingItemAction = PendingItemAction::USE;
    } else if (page == AppSceneFlow::Scene::SHOP) {
        state_.pendingItemAction = state_.shopCategory == Game::ShopService::Category::SELL
            ? PendingItemAction::SELL : PendingItemAction::BUY;
        state_.shopDetailItemIndex = index;
        state_.shopDetailProgress = 1.0f;
    } else return Selection::NONE;
    state_.pendingItem = item;
    state_.itemConfirmOpen = true;
    return Selection::DETAIL;
}

bool InventoryController::canConfirm(const Game::GameState& gameState) const {
    return state_.itemConfirmOpen && state_.pendingItem != Game::ItemId::COUNT &&
        (state_.pendingItemAction != PendingItemAction::SELL ||
         Game::ItemInventory::count(gameState, state_.pendingItem) > 0);
}

void InventoryController::beginTargetSelection(AppSceneFlow::Scene returnScene) {
    state_.itemTargetReturnScene = returnScene;
    state_.itemConfirmOpen = false;
    state_.selectingItemTarget = true;
    state_.itemVelocity = 0.0f;
}

void InventoryController::finishTargetSelection() {
    state_.teamItemFeedbackActive = false;
    state_.teamItemTargetSlot = 0xFF;
    state_.selectingItemTarget = false;
    state_.itemConfirmOpen = false;
    state_.pendingItem = Game::ItemId::COUNT;
    state_.pendingItemAction = PendingItemAction::NONE;
    state_.pressedItemRow = -1;
}

void InventoryController::clampScroll(float maximum) {
    maximum = std::max(0.0f, maximum);
    if (state_.itemScroll <= 0.0f) {
        state_.itemScroll = 0.0f;
        if (state_.itemVelocity < 0.0f) state_.itemVelocity = 0.0f;
    } else if (state_.itemScroll >= maximum) {
        state_.itemScroll = maximum;
        if (state_.itemVelocity > 0.0f) state_.itemVelocity = 0.0f;
    }
}

bool InventoryController::tickScroll(float maximum) {
    if (state_.itemConfirmOpen || std::fabs(state_.itemVelocity) <= 0.24f) return false;
    const float previous = state_.itemScroll;
    state_.itemScroll += state_.itemVelocity;
    state_.itemVelocity *= 0.86f;
    clampScroll(maximum);
    const bool changed = std::fabs(state_.itemScroll - previous) > 0.02f;
    if (!changed || std::fabs(state_.itemVelocity) <= 0.24f) state_.itemVelocity = 0.0f;
    return changed;
}

uint8_t InventoryController::dailyCount(const Game::GameState& gameState) const {
    return Game::ShopService::buyItemCount(Game::ShopService::Category::DAILY, gameState);
}

uint8_t InventoryController::exploreCount(const Game::GameState& gameState) const {
    return Game::ShopService::buyItemCount(Game::ShopService::Category::EXPLORE, gameState);
}

uint8_t InventoryController::itemCount(const Game::GameState& gameState,
    AppSceneFlow::Scene page) const {
    if (page == AppSceneFlow::Scene::BAG) return Game::ItemInventory::homeBagItemCount(gameState);
    if (page != AppSceneFlow::Scene::SHOP) return 0;
    return state_.shopCategory == Game::ShopService::Category::SELL
        ? Game::ShopService::sellItemCount(gameState)
        : static_cast<uint8_t>(dailyCount(gameState) + exploreCount(gameState));
}

Game::ItemId InventoryController::itemAt(const Game::GameState& gameState,
    AppSceneFlow::Scene page, uint8_t index) const {
    if (page == AppSceneFlow::Scene::BAG) return Game::ItemInventory::homeBagItemAt(gameState, index);
    if (page != AppSceneFlow::Scene::SHOP) return Game::ItemId::COUNT;
    if (state_.shopCategory == Game::ShopService::Category::SELL)
        return Game::ShopService::sellItemAt(gameState, index);
    const uint8_t daily = dailyCount(gameState);
    return index < daily
        ? Game::ShopService::buyItemAt(Game::ShopService::Category::DAILY, gameState, index)
        : Game::ShopService::buyItemAt(Game::ShopService::Category::EXPLORE, gameState,
                                      static_cast<uint8_t>(index - daily));
}

ShopViewModel InventoryController::shopModel(const Game::GameState& gameState,
    const char* toast) const {
    ShopViewModel model;
    model.state = &gameState;
    model.mode = state_.shopCategory == Game::ShopService::Category::SELL
        ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
    model.scroll = state_.itemScroll;
    model.dailyItemCount = dailyCount(gameState);
    model.exploreItemCount = exploreCount(gameState);
    model.itemCount = itemCount(gameState, AppSceneFlow::Scene::SHOP);
    model.coins = gameState.coins;
    model.pressedMenuItem = state_.pressedShopCategory;
    model.pressedItem = state_.pressedItemRow;
    model.pressedDetailAction = state_.pressedShopDetailAction;
    model.detailItem = state_.pendingItem;
    model.detailItemIndex = state_.shopDetailItemIndex;
    model.detailProgress = state_.shopDetailProgress;
    model.toast = toast;
    return model;
}

ItemListViewModel InventoryController::bagModel(const Game::GameState& gameState,
    const char* toast) const {
    ItemListViewModel model;
    model.state = &gameState;
    model.mode = ItemListMode::BAG;
    model.category = state_.shopCategory;
    model.scroll = state_.itemScroll;
    model.battleMode = state_.battleBagMode;
    model.dailyItemCount = Game::ItemInventory::homeBagDailyItemCount(gameState);
    model.exploreItemCount = Game::ItemInventory::homeBagExploreItemCount(gameState);
    model.itemCount = itemCount(gameState, AppSceneFlow::Scene::BAG);
    model.coins = gameState.coins;
    model.pressedItem = state_.pressedItemRow;
    model.confirmOpen = state_.itemConfirmOpen;
    model.pendingItem = state_.pendingItem;
    model.toast = toast;
    return model;
}

}  // namespace AmoledV1
