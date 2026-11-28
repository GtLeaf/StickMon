#pragma once

#include <algorithm>

#include "game/GameState.h"

namespace Game {
namespace CoinWallet {

constexpr uint32_t CAP = 9999999U;

// Return the amount credited so reward notices match the balance.
inline uint32_t credit(GameState& state, uint32_t amount) {
    const uint32_t balance = std::min(state.coins, CAP);
    const uint32_t credited = std::min(amount, CAP - balance);
    state.coins = balance + credited;
    return credited;
}

}  // namespace CoinWallet
}  // namespace Game
