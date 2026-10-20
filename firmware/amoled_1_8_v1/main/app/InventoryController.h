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

    const State& state() const { return state_; }

    // Flow, saves, sounds and transactions are handled by the application.
    void open();
    void cancelTouch();
    void beginListTouch(int row, int category = -1);
    void beginDetailTouch(int choice, const Game::GameState& gameState);
    void dragList(int deltaY, float maximum);
    void stopScroll() { state_.itemVelocity = 0.0f; }
    void setBattleBagMode(bool enabled) { state_.battleBagMode = enabled; }
    void suspendDetail() { state_.itemConfirmOpen = false; }
    void clearAction();
    void cancelTargetSelection();
    void beginItemTarget(Game::ItemId item, AppSceneFlow::Scene returnScene);
    void beginFeedback(uint8_t slot, uint16_t fromHp, uint16_t toHp, uint32_t nowMs);
    bool feedbackFrameDue(uint32_t nowMs, uint32_t intervalMs);
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
