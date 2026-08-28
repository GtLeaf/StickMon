#include <cstdint>

#include "game/BattleTurnController.h"
#include "game/GameRandom.h"
#include "game/Species.h"

namespace {
uint32_t nextRangeValue = 0;

uint32_t testRange(uint32_t minimum, uint32_t maximum) {
    if (maximum <= minimum) return minimum;
    return minimum + nextRangeValue++ % (maximum - minimum);
}

Game::MonsterRuntime monster(uint16_t speciesId, uint8_t level = 20) {
    const Species* species = findSpecies(speciesId);
    Game::MonsterRuntime value;
    value.speciesId = speciesId;
    value.level = level;
    value.hpMax = maxHpFor(*species, value);
    value.hpCur = value.hpMax;
    resetMovesForLevel(value, *species);
    return value;
}
} // namespace

int main() {
    GameRandom::setRangeProvider(testRange);
    const Species& charmander = *findSpecies(4);
    const Species& squirtle = *findSpecies(7);
    Game::MonsterRuntime player = monster(4);
    Game::MonsterRuntime wild = monster(7);
    BattleSystem::BattleActorState playerState;
    BattleSystem::BattleActorState wildState;
    BattleTurnController controller;

    playerState.lockedMoveId = 98;
    wildState.lockedMoveId = 33;
    auto plan = controller.planAiTurn(
        player, charmander, playerState, wild, squirtle, wildState);
    if (plan.count != 2 ||
        plan.actions[0].side != BattleTurnController::Side::PLAYER ||
        plan.actions[0].moveId != 98) {
        return 1;
    }

    playerState.lockedMoveId = 33;
    plan = controller.planAiTurn(
        player, charmander, playerState, wild, squirtle, wildState);
    if (plan.actions[0].side != BattleTurnController::Side::PLAYER) return 2;

    wild = monster(4);
    nextRangeValue = 1;
    plan = controller.planAiTurn(
        player, charmander, playerState, wild, charmander, wildState);
    if (plan.actions[0].side != BattleTurnController::Side::WILD) return 3;

    playerState.chargingMoveId = 76;
    playerState.chargingSpecialSlot = 1;
    plan = controller.planAiTurn(
        player, charmander, playerState, wild, charmander, wildState);
    const auto* playerAction = plan.actionFor(BattleTurnController::Side::PLAYER);
    if (!playerAction || playerAction->moveId != 76 ||
        playerAction->specialSlot != 1) {
        return 4;
    }
    if (!plan.hasActionAfter(0, plan.actions[1].side) ||
        plan.hasActionAfter(1, plan.actions[0].side)) {
        return 5;
    }

    playerState = BattleSystem::BattleActorState{};
    wildState = BattleSystem::BattleActorState{};
    auto wildOnly = controller.planWildOnly(
        player, charmander, playerState, wild, charmander, wildState);
    if (wildOnly.count != 1 ||
        wildOnly.actions[0].side != BattleTurnController::Side::WILD ||
        wildOnly.actionFor(BattleTurnController::Side::PLAYER)) {
        return 6;
    }

    controller.reset();
    controller.planAiTurn(
        player, charmander, playerState, wild, charmander, wildState);
    Game::MoveId wildMemory = controller.wildAiMemory().lastMoveId;
    if (controller.playerAiMemory().lastMoveId == 0 || wildMemory == 0) return 7;
    controller.resetPlayerAi();
    if (controller.playerAiMemory().lastMoveId != 0 ||
        controller.wildAiMemory().lastMoveId != wildMemory) {
        return 8;
    }
    controller.reset();
    if (controller.wildAiMemory().lastMoveId != 0) return 9;
    return 0;
}
