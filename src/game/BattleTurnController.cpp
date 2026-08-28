#include "game/BattleTurnController.h"

#include "game/GameRandom.h"

const BattleTurnController::Action*
BattleTurnController::TurnPlan::actionFor(Side side) const {
    for (uint8_t index = 0; index < count; ++index) {
        if (actions[index].side == side) return &actions[index];
    }
    return nullptr;
}

bool BattleTurnController::TurnPlan::hasActionAfter(uint8_t index,
                                                     Side side) const {
    for (uint8_t next = static_cast<uint8_t>(index + 1); next < count; ++next) {
        if (actions[next].side == side) return true;
    }
    return false;
}

void BattleTurnController::reset() {
    playerAiMemory_ = BattleSystem::BattleAiMemory{};
    wildAiMemory_ = BattleSystem::BattleAiMemory{};
}

void BattleTurnController::resetPlayerAi() {
    playerAiMemory_ = BattleSystem::BattleAiMemory{};
}

BattleTurnController::Action BattleTurnController::chooseAction(
    Side side,
    const Game::MonsterRuntime& attacker,
    const Species& attackerSpecies,
    const BattleSystem::BattleActorState& attackerState,
    const Game::MonsterRuntime& defender,
    const Species& defenderSpecies,
    const BattleSystem::BattleActorState& defenderState,
    BattleSystem::BattleAiMemory& memory) {
    Action action;
    action.side = side;
    if (BattleSystem::isChargingMove(attackerState)) {
        action.specialSlot = attackerState.chargingSpecialSlot;
        action.moveId = attackerState.chargingMoveId;
        return action;
    }
    if (attackerState.lockedMoveId != 0) {
        action.specialSlot = attackerState.lockedSpecialSlot;
        action.moveId = attackerState.lockedMoveId;
        return action;
    }

    action.specialSlot = BattleSystem::chooseAiMoveSlot(
        attacker, attackerSpecies, attackerState,
        defender, defenderSpecies, defenderState, memory);
    action.moveId = BattleSystem::moveIdForAction(
        attacker, attackerSpecies, action.specialSlot);
    return action;
}

BattleTurnController::TurnPlan BattleTurnController::planAiTurn(
    const Game::MonsterRuntime& player,
    const Species& playerSpecies,
    const BattleSystem::BattleActorState& playerState,
    const Game::MonsterRuntime& wild,
    const Species& wildSpecies,
    const BattleSystem::BattleActorState& wildState) {
    Action playerAction = chooseAction(
        Side::PLAYER, player, playerSpecies, playerState,
        wild, wildSpecies, wildState, playerAiMemory_);
    Action wildAction = chooseAction(
        Side::WILD, wild, wildSpecies, wildState,
        player, playerSpecies, playerState, wildAiMemory_);

    int8_t playerPriority = BattleSystem::movePriority(playerAction.moveId);
    int8_t wildPriority = BattleSystem::movePriority(wildAction.moveId);
    uint16_t playerSpeed = BattleSystem::effectiveSpeed(
        player, playerSpecies, playerState);
    uint16_t wildSpeed = BattleSystem::effectiveSpeed(
        wild, wildSpecies, wildState);
    bool playerFirst = playerPriority > wildPriority ||
        (playerPriority == wildPriority &&
         (playerSpeed > wildSpeed ||
          (playerSpeed == wildSpeed && GameRandom::range(0, 2) == 0)));

    TurnPlan plan;
    plan.actions[0] = playerFirst ? playerAction : wildAction;
    plan.actions[1] = playerFirst ? wildAction : playerAction;
    plan.count = 2;
    return plan;
}

BattleTurnController::TurnPlan BattleTurnController::planWildOnly(
    const Game::MonsterRuntime& player,
    const Species& playerSpecies,
    const BattleSystem::BattleActorState& playerState,
    const Game::MonsterRuntime& wild,
    const Species& wildSpecies,
    const BattleSystem::BattleActorState& wildState) {
    TurnPlan plan;
    plan.actions[0] = chooseAction(
        Side::WILD, wild, wildSpecies, wildState,
        player, playerSpecies, playerState, wildAiMemory_);
    plan.count = 1;
    return plan;
}
