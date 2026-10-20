#include <cassert>
#include <cstring>
#include <iostream>

#include "app/InventoryController.h"
#include "app/TeamController.h"
#include "game/MonsterFactory.h"
#include "ui/UiMetrics.h"

namespace {
using AmoledV1::InventoryController;
using AmoledV1::TeamController;
using AppSceneFlow::Scene;
using Game::ItemId;

uint8_t findItem(const InventoryController& controller,
                 const Game::GameState& state, Scene page, ItemId item) {
    for (uint8_t index = 0; index < controller.itemCount(state, page); ++index) {
        if (controller.itemAt(state, page, index) == item) return index;
    }
    assert(false);
    return 0;
}

void checkInventoryFlow() {
    Game::GameState game;
    InventoryController controller;
    controller.open();
    const auto before = game;
    const uint8_t firstDaily = controller.dailyCount(game);
    const uint8_t firstExplore = controller.exploreCount(game);
    assert(controller.itemCount(game, Scene::SHOP) == firstDaily + firstExplore);
    assert(controller.itemAt(game, Scene::SHOP, firstDaily) ==
        Game::ShopService::buyItemAt(Game::ShopService::Category::EXPLORE, game, 0));

    const auto selection = controller.selectItem(game, Scene::SHOP, 0);
    assert(selection == InventoryController::Selection::DETAIL);
    assert(controller.canConfirm(game));
    assert(controller.shopModel(game).detailItem == controller.state().pendingItem);
    // Opening, browsing and dismissing pages must never transact implicitly.
    controller.closeDetail();
    assert(std::memcmp(&game, &before, sizeof(game)) == 0);

    // Unlock changes are read from the shared service, not a stale page list.
    game.explorePoolRerollCounts[0] = 1;
    assert(controller.dailyCount(game) > firstDaily);
    assert(controller.shopModel(game).dailyItemCount == controller.dailyCount(game));

    game.bag.potion = 1;
    controller.selectCategory(Game::ShopService::Category::SELL);
    const uint8_t potionRow = findItem(controller, game, Scene::SHOP, ItemId::POTION);
    assert(controller.selectItem(game, Scene::SHOP, potionRow) ==
        InventoryController::Selection::DETAIL);
    assert(controller.state().pendingItemAction == InventoryController::PendingItemAction::SELL);
    assert(Game::ShopService::sell(game, ItemId::POTION) == Game::ShopService::SellResult::SOLD);
    // The details retain the selected item when its last unit disappears;
    // a second confirmation is rejected instead of selling the next row.
    assert(controller.shopModel(game).detailItem == ItemId::POTION);
    assert(!controller.canConfirm(game));
    assert(controller.state().itemConfirmOpen);
    controller.close();
    assert(!controller.canConfirm(game));

    game.bag.heartScale = 1;
    controller.open();
    const uint8_t scaleRow = findItem(controller, game, Scene::BAG, ItemId::HEART_SCALE);
    assert(controller.selectItem(game, Scene::BAG, scaleRow) ==
        InventoryController::Selection::OPEN_MOVE_MANAGER);
    assert(!controller.state().itemConfirmOpen);
    controller.setBattleBagMode(true);
    const auto battleBag = controller.bagModel(game);
    assert(battleBag.battleMode && !battleBag.exploreOnly);
    assert(battleBag.dailyItemCount == Game::ItemInventory::homeBagDailyItemCount(game));
    assert(battleBag.itemCount == Game::ItemInventory::homeBagItemCount(game));
    assert(controller.selectItem(game, Scene::BAG, scaleRow) ==
        InventoryController::Selection::CANNOT_USE);
    assert(!controller.state().itemConfirmOpen);

    game.bag.potion = 1;
    const uint8_t usableRow = findItem(controller, game, Scene::BAG, ItemId::POTION);
    assert(controller.selectItem(game, Scene::BAG, usableRow) ==
        InventoryController::Selection::DETAIL);
    controller.beginTargetSelection(Scene::BATTLE);
    assert(controller.state().selectingItemTarget);
    assert(controller.state().pendingItem == ItemId::POTION);
    assert(controller.state().itemTargetReturnScene == Scene::BATTLE);
    assert(!controller.state().itemConfirmOpen);
    controller.finishTargetSelection();
    assert(!controller.state().selectingItemTarget);
    assert(controller.state().pendingItem == ItemId::COUNT);
    // Whether using an item consumes a battle turn is an App decision.
    assert(controller.state().battleBagMode);
}

void checkScrollLifetime() {
    InventoryController controller;
    controller.dragList(-60, 100);
    controller.dragList(-30, 100);
    assert(controller.tickScroll(100));
    assert(controller.state().itemScroll == 100);
    assert(controller.state().itemVelocity == 0);
    Game::GameState game;
    controller.dragList(5, 100);
    controller.dragList(5, 100);
    assert(controller.selectItem(game, Scene::SHOP, 0) == InventoryController::Selection::DETAIL);
    assert(!controller.tickScroll(100));
    assert(controller.state().itemScroll == 90);
    controller.closeDetail();
    for (int frame = 0; frame < 100; ++frame) controller.tickScroll(100);
    assert(controller.state().itemVelocity == 0);
    assert(controller.state().itemScroll >= 0);
    controller.selectCategory(Game::ShopService::Category::DAILY);
    assert(controller.state().itemScroll == 0);
}

void checkStatusPaging() {
    Game::GameState game;
    TeamController controller;
    controller.openStatus(1);
    controller.dragStatus(180);
    assert(controller.state().teamStatusSlideX == 60);
    controller.releaseStatus(10);
    assert(controller.tick(190).statusCompleted);
    assert(controller.state().teamStatusPage == 0);
    controller.dragStatus(-110);
    controller.releaseStatus(200);
    auto mid = controller.tick(290);
    assert(mid.statusChanged && !mid.statusCompleted);
    assert(controller.state().teamStatusSlideX < -110);
    controller.interruptStatusSlide();
    assert(controller.state().teamStatusPage == 1);
    assert(controller.statusModel(game).slideOffsetX == 0);

    controller.beginStatusSlide(255, 400);
    controller.tick(580);
    assert(controller.state().teamStatusPage == TeamController::STATUS_PAGE_COUNT - 1);
    controller.dragStatus(-150);
    assert(controller.state().teamStatusSlideX == -50);
    controller.releaseStatus(UINT32_MAX - 90);
    assert(controller.tick(100).statusCompleted);

    controller.openContactStatus(3);
    const auto model = controller.statusModel(game, &game.storage[3]);
    assert(model.monster == &game.storage[3]);
    controller.closeStatus();
    assert(controller.statusModel(game).monster == nullptr);
    assert(controller.state().teamStatusContactSlot == 0xFF);
}

void checkMovesAndRecall() {
    Game::GameState game;
    game.teamCount = 1;
    game.team[0] = Game::MonsterFactory::create(1, 30);
    game.bag.heartScale = 1;
    TeamController controller;
    assert(!controller.openMoves(game, Game::TEAM_CAP));
    assert(controller.openMoves(game, 0));
    const auto unchanged = game.team[0];
    auto selected = controller.selectMove(0, false, 100);
    assert(selected.action == TeamController::MoveAction::EMPTY);
    assert(controller.state().teamMovesSelectedItem == 0xFF);
    selected = controller.selectMove(0, true, 100);
    assert(selected.action == TeamController::MoveAction::REDRAW);
    controller.tick(220);
    assert(controller.movesModel(game).detailProgress > 0.5f);
    controller.tick(340);
    controller.dragMoves(-300);
    controller.clampMovesScroll();
    const int expandedScroll = controller.state().teamMovesScroll;
    assert(expandedScroll > 0);
    controller.selectMove(0, true, 400);
    controller.tick(640);
    assert(controller.state().teamMovesSelectedItem == 0xFF);
    assert(controller.state().teamMovesScroll == 0);
    assert(std::memcmp(&game.team[0], &unchanged, sizeof(unchanged)) == 0);

    assert(controller.state().teamMovesRecallCount > 0);
    controller.beginRecallSelection();
    selected = controller.selectMove(0, true, 700);
    assert(selected.action == TeamController::MoveAction::REDRAW);
    selected = controller.selectMove(0, true, 800);
    assert(selected.action == TeamController::MoveAction::RECALL);
    assert(selected.replacementSlot == 1);
    assert(game.bag.heartScale == 1);
    assert(Game::MoveManagementService::recallMove(game, 0,
        selected.moveId, selected.replacementSlot));
    controller.completeRecall(game);
    assert(game.bag.heartScale == 0);
    assert(controller.movesModel(game).mode == AmoledV1::TeamMovesViewModel::Mode::MANAGE);
    assert(game.team[0].move2Id == selected.moveId);
    assert(game.team[0].move1Id == unchanged.move1Id);
    controller.openTeam();
    assert(!controller.state().teamMovesOpen);
    assert(!controller.state().teamStatusOpen);
}
}

int main() {
    checkInventoryFlow();
    checkScrollLifetime();
    checkStatusPaging();
    checkMovesAndRecall();
    std::cout << "team/inventory controllers: browsing, live stock, page gestures, animation, recall and battle targeting passed\n";
}
