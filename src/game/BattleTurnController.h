#pragma once

#include <cstdint>

#include "game/BattleSystem.h"

class BattleTurnController {
public:
    enum class Side : uint8_t {
        PLAYER = 0,
        WILD,
    };

    struct Action {
        Side side = Side::PLAYER;
        uint8_t specialSlot = BattleSystem::SPECIAL_SLOT_NONE;
        Game::MoveId moveId = 0;
    };

    struct TurnPlan {
        Action actions[2] = {};
        uint8_t count = 0;

        const Action* actionFor(Side side) const;
        bool hasActionAfter(uint8_t index, Side side) const;
    };

    void reset();
    void resetPlayerAi();

    TurnPlan planAiTurn(
        const Game::MonsterRuntime& player,
        const Species& playerSpecies,
        const BattleSystem::BattleActorState& playerState,
        const Game::MonsterRuntime& wild,
        const Species& wildSpecies,
        const BattleSystem::BattleActorState& wildState);

    TurnPlan planWildOnly(
        const Game::MonsterRuntime& player,
        const Species& playerSpecies,
        const BattleSystem::BattleActorState& playerState,
        const Game::MonsterRuntime& wild,
        const Species& wildSpecies,
        const BattleSystem::BattleActorState& wildState);

    const BattleSystem::BattleAiMemory& playerAiMemory() const {
        return playerAiMemory_;
    }
    const BattleSystem::BattleAiMemory& wildAiMemory() const {
        return wildAiMemory_;
    }

private:
    static Action chooseAction(
        Side side,
        const Game::MonsterRuntime& attacker,
        const Species& attackerSpecies,
        const BattleSystem::BattleActorState& attackerState,
        const Game::MonsterRuntime& defender,
        const Species& defenderSpecies,
        const BattleSystem::BattleActorState& defenderState,
        BattleSystem::BattleAiMemory& memory);

    BattleSystem::BattleAiMemory playerAiMemory_;
    BattleSystem::BattleAiMemory wildAiMemory_;
};
