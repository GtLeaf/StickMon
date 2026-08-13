#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace Game {
namespace ItemInventory {

enum class UseResult : uint8_t {
    USED = 0,
    NO_STOCK,
    INVALID_TARGET,
    FAINTED,
    HP_FULL,
    STATUS_NORMAL,
    NO_FAINTED_TARGET,
    NOT_USABLE,
};

uint8_t count(const GameState& state, ItemId item);
bool add(GameState& state, ItemId item, uint8_t amount = 1);
bool remove(GameState& state, ItemId item, uint8_t amount = 1);

bool usableFromHomeBag(ItemId item);
uint8_t preferredTarget(const GameState& state, ItemId item);
UseResult useOnTeam(GameState& state, ItemId item, uint8_t teamSlot);

uint8_t homeBagItemCount(const GameState& state);
ItemId homeBagItemAt(const GameState& state, uint8_t visibleIndex);

}  // namespace ItemInventory
}  // namespace Game
