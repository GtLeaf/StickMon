#include <cassert>

#include "game/ItemInventory.h"
#include "game/ShopService.h"

int main() {
    using Game::ItemId;
    using Game::ShopService::BuyResult;
    using Game::ShopService::Category;
    using Game::ShopService::SellResult;

    Game::GameState state;
    assert(Game::ShopService::buyPrice(ItemId::POTION) == 60);
    assert(Game::ShopService::buyPrice(ItemId::CANDY) == 2000);
    assert(Game::ShopService::sellPrice(ItemId::POTION) == 30);
    assert(Game::ShopService::sellPrice(ItemId::NUGGET) == 500);

    uint8_t dailyCount = Game::ShopService::buyItemCount(Category::DAILY, state);
    assert(dailyCount == 2);
    const ItemId expectedDaily[] = {
        ItemId::NORMAL_FOOD, ItemId::SOAP_0,
    };
    for (uint8_t i = 0; i < dailyCount; ++i) {
        assert(Game::ShopService::buyItemAt(Category::DAILY, state, i) ==
               expectedDaily[i]);
    }

    uint8_t foodBefore = state.room.food[0];
    uint32_t coinsBefore = state.coins;
    assert(Game::ShopService::buy(state, ItemId::NORMAL_FOOD) ==
           BuyResult::BOUGHT);
    assert(state.room.food[0] == foodBefore + 1);
    assert(state.coins == coinsBefore - 20);

    state.coins = 0;
    uint8_t potionBefore = state.bag.potion;
    assert(Game::ShopService::buy(state, ItemId::POTION) ==
           BuyResult::NOT_ENOUGH_COINS);
    assert(state.bag.potion == potionBefore);

    state.coins = 99999;
    state.bag.potion = Game::ITEM_STACK_CAP;
    assert(Game::ShopService::buy(state, ItemId::POTION) ==
           BuyResult::BAG_FULL);
    assert(state.coins == 99999);

    state.explorePoolRerollCounts[0] = 1;
    assert(Game::ShopService::buyItemCount(Category::DAILY, state) == 6);
    assert(Game::ShopService::buyItemAt(Category::DAILY, state, 1) ==
           ItemId::TASTY_FOOD);
    state.explorePoolRerollCounts[1] = 1;
    assert(Game::ShopService::buyItemCount(Category::DAILY, state) == 10);
    assert(Game::ShopService::buyItemAt(Category::DAILY, state, 9) ==
           ItemId::SOAP_2);
    state.explorePoolRerollCounts[2] = 1;
    assert(Game::ShopService::buyItemCount(Category::DAILY, state) == 12);
    assert(Game::ShopService::buyItemAt(Category::DAILY, state, 7) ==
           ItemId::CANDY);
    state.coins = 5000;
    state.candyPurchasesToday = Game::DAILY_CANDY_PURCHASE_CAP;
    assert(Game::ShopService::buy(state, ItemId::CANDY) ==
           BuyResult::DAILY_LIMIT);
    state.candyPurchasesToday = 0;
    assert(Game::ShopService::buy(state, ItemId::CANDY) ==
           BuyResult::BOUGHT);
    assert(state.candyPurchasesToday == 1);

    state.bag.nugget = 1;
    state.coins = 99800;
    assert(Game::ShopService::sell(state, ItemId::NUGGET) ==
           SellResult::SOLD);
    assert(state.bag.nugget == 0 && state.coins == 99999);
    assert(Game::ShopService::sell(state, ItemId::NUGGET) ==
           SellResult::NO_STOCK);

    state.bag.antidote = 1;
    uint8_t sellCount = Game::ShopService::sellItemCount(state);
    bool foundAntidote = false;
    for (uint8_t i = 0; i < sellCount; ++i) {
        foundAntidote |= Game::ShopService::sellItemAt(state, i) ==
                         ItemId::ANTIDOTE;
    }
    assert(foundAntidote);
    return 0;
}
