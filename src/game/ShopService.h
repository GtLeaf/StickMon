#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace Game {
namespace ShopService {

enum class Category : uint8_t {
    DAILY = 0,
    EXPLORE,
    SELL,
};

enum class BuyResult : uint8_t {
    BOUGHT = 0,
    LOCKED,
    NOT_ENOUGH_COINS,
    BAG_FULL,
    DAILY_LIMIT,
    INVALID_ITEM,
};

enum class SellResult : uint8_t {
    SOLD = 0,
    NO_STOCK,
    INVALID_ITEM,
};

uint16_t buyPrice(ItemId item);
uint16_t sellPrice(ItemId item);
const char* shortName(ItemId item);
const char* shortDescription(ItemId item);

uint8_t buyItemCount(Category category, const GameState& state);
ItemId buyItemAt(Category category, const GameState& state,
                 uint8_t visibleIndex);
uint8_t sellItemCount(const GameState& state);
ItemId sellItemAt(const GameState& state, uint8_t visibleIndex);

BuyResult buy(GameState& state, ItemId item);
SellResult sell(GameState& state, ItemId item);

}  // namespace ShopService
}  // namespace Game
