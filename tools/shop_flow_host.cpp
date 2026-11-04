#include <cassert>
#include <cstdint>
#include <vector>

#include "core/SaveCodec.h"
#include "core/features/ShopFlow.h"
#include "game/CareTicker.h"
#include "game/ItemInventory.h"

namespace {

using Flow = Features::ShopFlow;
using Game::ItemId;
using Game::ShopService::BuyResult;
using Game::ShopService::SellResult;

std::vector<uint8_t> snapshot(const Game::GameState& state) {
    std::vector<uint8_t> bytes(SaveCodec::MAX_ENCODED_BYTES);
    size_t written = 0;
    assert(SaveCodec::encode(state, MainSceneViewState{}, 1,
                             bytes.data(), bytes.size(), written));
    bytes.resize(written);
    return bytes;
}

void clearStock(Game::GameState& state) {
    for (uint8_t index = 0; index < static_cast<uint8_t>(ItemId::COUNT); ++index) {
        const ItemId item = static_cast<ItemId>(index);
        const uint8_t count = Game::ItemInventory::count(state, item);
        if (count) assert(Game::ItemInventory::remove(state, item, count));
    }
}

void catalogAndNavigation() {
    Game::GameState state;
    clearStock(state);
    Flow flow;
    flow.bind(state);
    flow.reset(true);
    assert(flow.viewMode == Flow::ViewMode::CATEGORY);
    assert(flow.currentItemCount() == Flow::CATEGORY_COUNT);
    flow.cursor = Flow::CATEGORY_EXPLORE;
    assert(flow.activateCategory());
    assert(flow.viewMode == Flow::ViewMode::EXPLORE);
    assert(flow.cursor == 0 && flow.selectedItem() == Flow::POTION);
    assert(flow.itemAtIndex(1) == Flow::ANTIDOTE);
    assert(flow.itemAtIndex(2) == Flow::HONEY);
    assert(flow.currentItemCount() == 4);
    flow.cursor = 3;
    assert(flow.selectedItem() == Flow::BACK);
    flow.returnToCategories();
    assert(flow.viewMode == Flow::ViewMode::CATEGORY);
    assert(flow.cursor == Flow::CATEGORY_EXPLORE);
    flow.cursor = Flow::CATEGORY_BACK;
    assert(!flow.activateCategory());

    flow.reset(false);
    assert(flow.viewMode == Flow::ViewMode::DAILY);
    assert(flow.currentItem() == Flow::FOOD);
    assert(flow.itemAtIndex(1) == Flow::SOAP_0);
    assert(flow.itemAtIndex(2) == Flow::BACK);
    assert(flow.ownedCountFor(Flow::FOOD) == 0);
    assert(Flow::buy(state, flow.currentItem()) == BuyResult::BOUGHT);
    assert(flow.ownedCountFor(Flow::FOOD) == 1);

    // Catalog reads must remain live after an unlock or an inventory mutation.
    state.explorePoolRerollCounts[3] = 1;
    assert(flow.itemCountForCategory(Flow::CATEGORY_DAILY, false) == 11);
    assert(flow.itemCountForCategory(Flow::CATEGORY_EXPLORE, false) == 16);
    assert(flow.itemForCategory(Flow::CATEGORY_EXPLORE, 7) == Flow::MAX_POTION);
    assert(Game::ItemInventory::add(state, ItemId::NUGGET));
    flow.cursor = Flow::CATEGORY_SELL;
    assert(flow.activateCategory());
    assert(flow.viewMode == Flow::ViewMode::SELL);
    assert(flow.selectedItem() == Flow::FOOD);
    flow.cursor = 1;
    assert(flow.selectedItem() == Flow::NUGGET);
    assert(Flow::sell(state, flow.selectedItem()) == SellResult::SOLD);
    assert(flow.selectedItem() == Flow::BACK);
    assert(flow.sellItemCount() == 2);
}

void atomicFailuresAndFoodSelection() {
    Game::GameState state;
    clearStock(state);
    state.coins = 99999;
    auto before = snapshot(state);
    assert(Flow::buy(state, Flow::MAX_POTION) == BuyResult::LOCKED);
    assert(snapshot(state) == before);
    assert(Flow::buy(state, Flow::BACK) == BuyResult::INVALID_ITEM);
    assert(snapshot(state) == before);
    assert(Flow::sell(state, Flow::BACK) == SellResult::INVALID_ITEM);
    assert(snapshot(state) == before);

    state.coins = 0;
    before = snapshot(state);
    assert(Flow::buy(state, Flow::POTION) == BuyResult::NOT_ENOUGH_COINS);
    assert(snapshot(state) == before);
    state.coins = 1000;
    assert(Game::ItemInventory::add(state, ItemId::POTION, Game::ITEM_STACK_CAP));
    before = snapshot(state);
    assert(Flow::buy(state, Flow::POTION) == BuyResult::BAG_FULL);
    assert(snapshot(state) == before);

    state.explorePoolRerollCounts[1] = 1;
    assert(Flow::buy(state, Flow::TASTY_FOOD) == BuyResult::BOUGHT);
    assert(state.room.selectedFood == Game::ROOM_TASTY_FOOD_INDEX);
    assert(Game::ItemInventory::add(state, ItemId::DRY_FOOD));
    assert(Flow::sell(state, Flow::TASTY_FOOD) == SellResult::SOLD);
    assert(state.room.selectedFood == Game::ROOM_DRY_FOOD_INDEX);
    assert(Game::ItemInventory::count(state, ItemId::TASTY_FOOD) == 0);
}

void candyDayAndLastSale() {
    Game::GameState state;
    clearStock(state);
    state.explorePoolRerollCounts[2] = 1;
    state.coins = 10000;
    assert(Flow::buy(state, Flow::CANDY) == BuyResult::BOUGHT);
    assert(state.bag.candy == 1 && state.candyPurchasesToday == 1);
    assert(state.coins == 8000);
    const auto before = snapshot(state);
    assert(Flow::buy(state, Flow::CANDY) == BuyResult::DAILY_LIMIT);
    assert(snapshot(state) == before);

    // The application synchronizes the day before calling the feature. Use
    // that same shared daily reset here, rather than giving ShopFlow a clock.
    state.gameMinutesTotal += Game::GAME_MINUTES_PER_DAY;
    assert(Game::resetDailyCareCounters(state));
    assert(state.candyPurchasesToday == 0);
    assert(Flow::buy(state, Flow::CANDY) == BuyResult::BOUGHT);
    assert(state.bag.candy == 2 && state.candyPurchasesToday == 1);
    assert(state.coins == 6000);

    clearStock(state);
    assert(Game::ItemInventory::add(state, ItemId::NUGGET));
    state.coins = 99800;
    Flow flow;
    flow.bind(state);
    flow.cursor = Flow::CATEGORY_SELL;
    assert(flow.activateCategory());
    assert(flow.currentItemCount() == 2 && flow.selectedItem() == Flow::NUGGET);
    assert(Flow::sell(state, flow.selectedItem()) == SellResult::SOLD);
    assert(state.coins == 99999 && state.bag.nugget == 0);
    assert(flow.currentItemCount() == 1 && flow.selectedItem() == Flow::BACK);
    const auto after = snapshot(state);
    assert(Flow::sell(state, Flow::NUGGET) == SellResult::NO_STOCK);
    assert(snapshot(state) == after);
}

void everyItemMatchesSharedRules() {
    // Feature item IDs have a legacy visual order. Check every conversion
    // against the actual ItemId to prevent a UI from buying another product.
    for (uint8_t index = 0; index < static_cast<uint8_t>(ItemId::HEART_SCALE); ++index) {
        const ItemId item = static_cast<ItemId>(index);
        const Flow::Item selected = Flow::itemFromGameItemId(item);
        assert(selected != Flow::BACK && Flow::gameItemIdFor(selected) == item);
        Game::GameState actual;
        clearStock(actual);
        actual.explorePoolRerollCounts[4] = 1;
        actual.coins = 50000;
        Game::GameState reference = actual;
        assert(Flow::buy(actual, selected) == Game::ShopService::buy(reference, item));
        assert(snapshot(actual) == snapshot(reference));
        assert(Game::ItemInventory::add(actual, item));
        assert(Game::ItemInventory::add(reference, item));
        assert(Flow::sell(actual, selected) == Game::ShopService::sell(reference, item));
        assert(snapshot(actual) == snapshot(reference));
    }
    assert(Flow::itemFromGameItemId(ItemId::HEART_SCALE) == Flow::BACK);
}

}  // namespace

int main() {
    catalogAndNavigation();
    atomicFailuresAndFoodSelection();
    candyDayAndLastSale();
    everyItemMatchesSharedRules();
}
