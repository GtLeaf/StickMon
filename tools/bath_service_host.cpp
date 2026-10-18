#include <cassert>

#include "game/BathService.h"
#include "game/CareExperience.h"

int main() {
    using Game::BathService::Stage;

    Game::GameState state;
    state.teamCount = 1;
    state.team[0].level = 5;
    state.team[0].mood = 91;
    state.bag.soap[0] = 1;
    state.bag.soap[2] = 2;

    assert(Game::CareExperience::budgetFor(state.team[0]) == 18);
    assert(Game::BathService::hpRecoveryPercentForScore(0) == 0);
    assert(Game::BathService::hpRecoveryPercentForScore(1) == 45);
    assert(Game::BathService::hpRecoveryPercentForScore(2) == 70);
    assert(Game::BathService::hpRecoveryPercentForScore(3) == 100);
    assert(Game::BathService::nextOwnedSoap(state, -1) == 0);
    assert(Game::BathService::nextOwnedSoap(state, 0) == 2);
    assert(Game::BathService::consumeSoap(state, 0));
    assert(!Game::BathService::consumeSoap(state, 0));

    auto soap = Game::BathService::applyStageReward(state, Stage::SOAP);
    auto brush = Game::BathService::applyStageReward(state, Stage::BRUSH);
    auto rinse = Game::BathService::applyStageReward(state, Stage::RINSE);
    assert(soap.experience == 1 && soap.moodGain == 0);
    assert(brush.experience == 2 && brush.moodGain == 2);
    assert(rinse.experience == 3 && rinse.moodGain == 7);
    assert(state.team[0].mood == 100);

    state.team[0].hpMax = 100;
    state.team[0].hpCur = 40;
    assert(Game::BathService::applyCompletionRecovery(state, 1) == 45);
    assert(state.team[0].hpCur == 85);
    assert(Game::BathService::applyCompletionRecovery(state, 2) == 15);
    assert(state.team[0].hpCur == 100);

    state.team[0].hpCur = 40;
    assert(Game::BathService::applyCompletionRecovery(state, 2) == 60);
    assert(state.team[0].hpCur == 100);

    state.team[0].hpCur = 98;
    assert(Game::BathService::applyCompletionRecovery(state, 3) == 2);
    assert(state.team[0].hpCur == 100);
    state.team[0].fainted = true;
    state.team[0].hpCur = 10;
    assert(Game::BathService::applyCompletionRecovery(state, 3) == 0);
    assert(state.team[0].hpCur == 10);

    state.team[0].fainted = false;
    auto capped = Game::BathService::applyStageReward(state, Stage::RINSE);
    assert(capped.experience == 0);
    return 0;
}
