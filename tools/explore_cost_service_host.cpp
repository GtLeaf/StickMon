#include <cassert>
#include <cstdio>
#include <utility>
#include "game/ExploreCostService.h"
#include "game/CareTicker.h"
#include "core/GameClockService.h"

static Game::GameState fixture() {
    Game::GameState state{};
    state.teamCount = 2;
    for (uint8_t i = 0; i < 2; ++i) {
        auto& m = state.team[i];
        m.speciesId = i ? 25 : 7;
        m.metAt = i + 1;
        m.hpCur = 10;
        m.hpMax = 100;
        m.satiety = 50;
    }
    return state;
}
int main() {
    using namespace Game::ExploreCost;
    for (uint8_t value = 0; value <= 100; ++value) {
        auto state = fixture();
        state.team[1].satiety = value;
        assert(hungrySlot(state) == (value < 20 ? 1 : -1));
        state.team[1].fainted = true;
        assert(hungrySlot(state) == -1);
        state.team[1].fainted = false;
        state.team[1].hpCur = 0;
        assert(hungrySlot(state) == -1);
        state.team[1].hpCur = 10;
        state.team[1].origin = Game::Origin::VISITOR;
        assert(hungrySlot(state) == -1);
        state.team[1].origin = Game::Origin::STARTER;
        for (auto cocoon : {11, 14, 266, 268}) {
            state.team[1].speciesId = cocoon;
            assert(hungrySlot(state) == -1);
        }
    }
    for (unsigned steps = 0; steps <= 120; ++steps) {
        auto state = fixture();
        Session session;
        session.begin();
        for (unsigned i = 0; i < steps; ++i) assert(session.step(state) == 1);
        assert(state.team[0].satiety == 50);
        assert(state.team[1].satiety == 50);
        std::swap(state.team[0], state.team[1]);
        session.finish(state);
        const unsigned spent = (steps + 5) / 6;
        assert(state.team[0].satiety == 50 - spent);
        assert(state.team[1].satiety == 50 - spent);
        session.finish(state);
        assert(session.step(state) == 0);
        assert(session.minutes == steps && state.team[0].satiety == 50 - spent);
    }
    {
        auto state = fixture();
        state.teamCount = 1;
        Session session;
        session.begin();
        for (unsigned i = 0; i < 5; ++i) session.step(state);
        state.teamCount = 2; // A new companion pays only for its own walking.
        session.step(state);
        assert(state.team[0].satiety == 50 && state.team[1].satiety == 50);
        for (unsigned i = 0; i < 5; ++i) session.step(state);
        assert(state.team[1].satiety == 50);
        session.finish(state);
        assert(state.team[0].satiety == 48 && state.team[1].satiety == 49);
    }
    for (bool boss : {false, true}) {
        for (bool switched : {false, true}) {
            auto state = fixture();
            Session session;
            session.begin();
            Battle battle;
            battle.begin(state, 0, boss);
            if (switched) { battle.enter(state, 1); battle.enter(state, 0); battle.enter(state, 1); }
            // Fainted participants still pay for their completed battle.
            state.team[0].fainted = true;
            state.team[0].hpCur = 0;
            std::swap(state.team[0], state.team[1]);
            const unsigned spent = boss ? 2 : 1;
            assert(session.settleBattle(state, battle) == (boss ? 10 : 5));
            assert(state.team[1].satiety == 50 && state.team[0].satiety == 50);
            assert(session.settleBattle(state, battle) == 0);
            session.finish(state);
            assert(state.team[1].satiety == 50 - spent);
            assert(state.team[0].satiety == 50 - (switched ? spent : 0));
            assert(session.minutes == (boss ? 10U : 5U));
        }
    }
    {
        auto state = fixture();
        state.team[0].satiety = 0;
        state.team[1].origin = Game::Origin::VISITOR;
        Session session;
        session.begin();
        Battle battle;
        battle.begin(state, 0, true);
        battle.enter(state, 1);
        for (unsigned i = 0; i < 7; ++i) session.step(state);
        session.settleBattle(state, battle);
        session.finish(state);
        assert(state.team[0].satiety == 0 && state.team[1].satiety == 50);
        assert(session.members[0].spent == 0);
    }
    {
        GameClockService clock;
        clock.start(1000, 1438);
        assert(clock.minutesAt(31000, 1) == 1438);
        clock.advance(5);
        assert(clock.minutesAt(61000, 1) == 1444); // Natural fractional minute retained.
        auto state = fixture();
        state.gameMinutesTotal = 1444;
        state.team[1].fainted = true;
        state.team[1].hpCur = 0;
        state.team[1].lastSeenAt = 100;
        Game::CareTickAccumulators care{};
        Game::applyCareMinutes(state, care, 0, 65, false);
        assert(state.team[0].hpCur == 10 && state.team[0].satiety == 50);
        assert(state.team[1].fainted && state.team[1].lastSeenAt == 1444 * 60);
    }
    std::puts("exploration costs: departure, steps, partial return, identity, joins, participants, zero floor and clock passed");
}
