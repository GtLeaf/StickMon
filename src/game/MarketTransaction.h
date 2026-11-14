#pragma once

#include <cstdint>
#include "game/GameState.h"

namespace Game::MarketTransaction {
struct CoinChange {
    uint32_t before = 0, after = 0;
    int64_t delta() const { return int64_t(after)-int64_t(before); }
};
enum class Status : uint8_t { REJECTED, SAVE_FAILED, COMMITTED };
template<class Result> struct Outcome { Result result; Status status; };

// Rules own validation/pricing/rewards. Saving owns durability, including a
// successful single redundant copy. Only durable balance changes notify UI.
template<class Apply,class Save,class Notify>
auto commit(GameState& state,Apply apply,Save save,Notify notify) {
    const GameState before = state;
    const auto result = apply(state);
    using Result = decltype(apply(state));
    if (result != Result::OK) { state = before; return Outcome<Result>{result,Status::REJECTED}; }
    if (!save()) { state = before; return Outcome<Result>{result,Status::SAVE_FAILED}; }
    if (before.coins != state.coins) notify(CoinChange{before.coins,state.coins});
    return Outcome<Result>{result,Status::COMMITTED};
}
} // namespace Game::MarketTransaction
