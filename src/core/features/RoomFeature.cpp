#include "core/features/RoomFeature.h"

#include <algorithm>
#include "game/ItemInventory.h"

namespace Features { namespace Room {

uint8_t normalizedFoodIndex(uint8_t index) {
    return index < Game::ROOM_FOOD_COUNT ? index : 0;
}

uint8_t selectedFoodIndex(const Game::GameState& state) {
    return normalizedFoodIndex(state.room.selectedFood);
}

uint8_t foodStock(const Game::GameState& state, uint8_t index) {
    return Game::ItemInventory::count(state, Game::itemIdForFoodIndex(normalizedFoodIndex(index)));
}

uint16_t totalFoodStock(const Game::GameState& state) {
    uint16_t total = 0;
    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index)
        total += foodStock(state, index);
    return total;
}

uint8_t displayedFoodStock(const Game::GameState& state) {
    return static_cast<uint8_t>(std::min<uint16_t>(totalFoodStock(state), 255));
}

bool canShower(const Game::GameState& state) {
    return state.oobeDone && state.teamCount > 0;
}

Action roomAction(uint8_t index) {
    switch (index) {
    case 0: return Action::FOOD;
    case 1: return Action::SHOWER;
    case 2: return Action::BACK;
    default: return Action::NONE;
    }
}

bool foodBack(uint8_t index) {
    return index >= Game::ROOM_FOOD_COUNT;
}

} }  // namespace Features::Room
