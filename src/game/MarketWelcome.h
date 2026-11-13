#pragma once
#include "game/ItemInventory.h"
namespace Game::MarketWelcome {
enum class Result : uint8_t { ALREADY, GRANTED, BAG_FULL };
inline Result grant(GameState& state) {
    if (state.marketTutorGifted) return Result::ALREADY;
    if (!ItemInventory::add(state, ItemId::HEART_SCALE)) return Result::BAG_FULL;
    state.marketTutorGifted = 1;
    return Result::GRANTED;
}
}
