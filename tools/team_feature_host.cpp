#include <cassert>
#include <cstring>
#include "core/features/TeamFeature.h"
#include "game/TeamRoster.h"

int main() {
    using namespace Features::Team;
    Game::GameState state{};
    state.teamCount = 0;
    assert(memberCount(state) == 0 && formalCount(state) == 0);
    assert(!capabilities(state, 0, false).member);

    state.teamCount = 2;
    state.team[0].origin = Game::Origin::STARTER;
    state.team[1].origin = Game::Origin::BEFRIENDED;
    const auto before = state;
    const auto first = capabilities(state, 0, false);
    const auto partner = capabilities(state, 1, false);
    assert(first.member && !first.visitor && !first.first && first.leave);
    assert(partner.member && !partner.visitor && partner.first && partner.leave);
    assert(partner.first == Game::TeamRoster::canMoveToFront(state, 1));
    assert(capabilities(state, 1, true).first);
    assert(!capabilities(state, 1, true).leave);
    assert(!capabilities(state, 2, false).member);
    assert(!capabilities(state, 255, false).first);
    assert(std::memcmp(&state, &before, sizeof(state)) == 0);

    state.team[1].origin = Game::Origin::VISITOR;
    const auto visitor = capabilities(state, 1, false);
    assert(formalCount(state) == 1);
    assert(visitor.member && visitor.visitor && !visitor.first && !visitor.leave);
    assert(!Game::TeamRoster::canMoveToFront(state, 1));
    assert(!capabilities(state, 0, false).leave);

    state.teamCount = 1;
    assert(memberCount(state) == 1 && formalCount(state) == 1);
    assert(!capabilities(state, 1, false).member);
    assert(!capabilities(state, 0, false).leave);

    state.teamCount = 255;
    assert(memberCount(state) == Game::TEAM_CAP);
    assert(formalCount(state) == 1);
    assert(!capabilities(state, Game::TEAM_CAP, false).member);
}
