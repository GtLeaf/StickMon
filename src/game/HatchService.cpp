#include "game/HatchService.h"

#include <algorithm>
#include <cmath>

#include "game/CareTicker.h"
#include "game/MonsterFactory.h"

namespace Game::HatchService {

void initializeNewGame(GameState& state) {
    state = GameState{};
    state.teamCount = 0;
    state.bag.potion = 0;
    state.bag.candy = 0;
    state.bag.revive = 0;
    for (auto& food : state.room.food) food = 0;
    state.coins = 0;
}

uint8_t starterStyle(uint16_t pokeCount, uint16_t wipeCount) {
    if (static_cast<uint32_t>(pokeCount) + wipeCount < 10) return 0;
    return pokeCount > wipeCount ? 1 : wipeCount > pokeCount ? 2 : 0;
}

bool finish(GameState& state, uint8_t style) {
    if (state.oobeDone) return false;
    const uint16_t speciesId = style == 1 ? 4 : style == 2 ? 7 : 1;
    state.teamCount = 1;
    state.activeSlot = 0;
    state.team[0] = MonsterFactory::create(speciesId, 5);
    auto& monster = state.team[0];
    monster.origin = Origin::HATCHED;
    monster.metArea = MET_AREA_HATCHED;
    monster.metAt = gameSecondsForMinutes(state.gameMinutesTotal);
    monster.lastSeenAt = monster.metAt;
    monster.lastExploredAt = monster.metAt;
    monster.lastWindowGazeAt = monster.metAt;
    state.bag = BagState{};
    state.room = RoomState{};
    state.coins = INITIAL_COINS;
    state.oobeDone = true;
    return true;
}

uint8_t progressPercent(float elapsedSeconds, uint16_t totalSeconds) {
    if (totalSeconds == 0) return 100;
    return static_cast<uint8_t>(std::clamp(
        elapsedSeconds * 100.0f / totalSeconds, 0.0f, 100.0f));
}

uint16_t remainingSeconds(float elapsedSeconds, uint16_t totalSeconds) {
    return static_cast<uint16_t>(std::ceil(std::clamp(
        totalSeconds - elapsedSeconds, 0.0f, static_cast<float>(totalSeconds))));
}

int8_t shakeOffset(uint8_t progress, uint32_t nowMs) {
    const uint16_t interval = std::max<int>(360, 2400 - progress * 19);
    const uint16_t phase = nowMs % interval;
    if (phase > 230) return 0;
    static constexpr int8_t OFFSETS[] = {-3, 3, -2, 2, 0};
    return OFFSETS[(phase / 46) % 5];
}

}  // namespace Game::HatchService
