#pragma once

#include <cstdint>

#include "game/ItemInventory.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

class InventoryController {
public:
    enum class PendingItemAction : uint8_t { NONE = 0, BUY, SELL, USE };
    enum class Selection : uint8_t { NONE, DETAIL, CANNOT_USE, OPEN_MOVE_MANAGER };

    struct State {
        float itemScroll = 0.0f;
        float itemVelocity = 0.0f;
        int pressedItemRow = -1;
        int pressedShopCategory = -1;
        int pressedShopDetailAction = -1;
        Game::ShopService::Category shopCategory = Game::ShopService::Category::DAILY;
        float shopDetailProgress = 0.0f;
        int shopDetailItemIndex = -1;
        bool itemConfirmOpen = false;
        bool battleBagMode = false;
        Game::ItemId pendingItem = Game::ItemId::COUNT;
        PendingItemAction pendingItemAction = PendingItemAction::NONE;
        bool selectingItemTarget = false;
        bool teamItemFeedbackActive = false;
        uint8_t teamItemTargetSlot = 0xFF;
        uint16_t teamItemHpFrom = 0;
        uint16_t teamItemHpTo = 0;
        uint32_t teamItemStartedMs = 0;
        uint32_t nextTeamItemFrameMs = 0;
        AppSceneFlow::Scene itemTargetReturnScene = AppSceneFlow::Scene::HOME;
    };

    State& state() { return state_; }
    const State& state() const { return state_; }

    // Flow, saves, sounds and transactions are handled by the application.
    void open();
    void close();
    void closeDetail();
    void selectCategory(Game::ShopService::Category category);
    Selection selectItem(const Game::GameState& gameState,
                         AppSceneFlow::Scene page, uint8_t index);
    bool canConfirm(const Game::GameState& gameState) const;
    void beginTargetSelection(AppSceneFlow::Scene returnScene);
    void finishTargetSelection();
    void clampScroll(float maximum);
    bool tickScroll(float maximum);

    uint8_t dailyCount(const Game::GameState& gameState) const;
    uint8_t exploreCount(const Game::GameState& gameState) const;
    uint8_t itemCount(const Game::GameState& gameState, AppSceneFlow::Scene page) const;
    Game::ItemId itemAt(const Game::GameState& gameState,
                        AppSceneFlow::Scene page, uint8_t index) const;
    ShopViewModel shopModel(const Game::GameState& gameState,
                            const char* toast = nullptr) const;
    ItemListViewModel bagModel(const Game::GameState& gameState,
                              const char* toast = nullptr) const;

private:
    State state_{};
};

}  // namespace AmoledV1
