#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "game/ExplorePartyService.h"
#include "game/GameRandom.h"

namespace {
unsigned cases = 0, randomCalls = 0;
uint32_t countRandom(uint32_t low, uint32_t) { ++randomCalls; return low; }

// Independent copies of the pre-extraction gameplay effects. These do not
// call the new service, TeamRoster, ExploreRunRules, or Bond helpers.
namespace Legacy {
bool healthy(const Game::MonsterRuntime& monster) {
    return !monster.fainted && monster.hpCur > 0;
}
uint8_t leader(const Game::GameState& state) {
    const bool first = state.teamCount > 0 && healthy(state.team[0]);
    const bool second = state.teamCount > 1 && healthy(state.team[1]);
    return !first && second ? 1 : 0;
}
bool follower(const Game::GameState& state) {
    return state.teamCount > 1 && healthy(state.team[0]) && healthy(state.team[1]);
}
bool recover(Game::GameState& state, uint16_t steps) {
    if (steps == 0 || steps % 10 != 0) return false;
    bool changed = false;
    for (unsigned slot = 0; slot < state.teamCount && slot < 2; ++slot) {
        auto& monster = state.team[slot];
        if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 || monster.hpCur >= monster.hpMax) continue;
        const unsigned healing = (static_cast<unsigned>(monster.hpMax) + 9) / 10;
        const unsigned next = static_cast<unsigned>(monster.hpCur) + healing;
        monster.hpCur = static_cast<uint16_t>(next > monster.hpMax ? monster.hpMax : next);
        changed = true;
    }
    return changed;
}
uint8_t bond(Game::GameState& state, uint16_t steps, bool visitors) {
    const uint8_t gain = steps == 0 ? 0 : (steps < 15 ? 1 : (steps < 40 ? 2 : (steps < 80 ? 3 : 4)));
    if (!gain) return 0;
    bool changed = false;
    for (unsigned slot = 0; slot < state.teamCount && slot < 2; ++slot) {
        auto& monster = state.team[slot];
        if ((monster.origin == Game::Origin::VISITOR && !visitors) || monster.fainted || monster.hpCur == 0) continue;
        const int next = std::min(100, int(monster.bond) + gain);
        if (next == monster.bond) continue;
        monster.bond = static_cast<int8_t>(next);
        changed = true;
    }
    return changed ? gain : 0;
}
bool clear(Game::GameState& state) {
    bool changed = false;
    for (unsigned slot = 0; slot < state.teamCount && slot < 2; ++slot) {
        auto& monster = state.team[slot];
        if (monster.majorStatus == Game::MajorStatus::NONE && monster.majorStatusTurns == 0) continue;
        monster.majorStatus = Game::MajorStatus::NONE;
        monster.majorStatusTurns = 0;
        changed = true;
    }
    return changed;
}
}  // namespace Legacy

void same(const char* operation, unsigned expectedResult, unsigned result,
          const Game::GameState& expected, const Game::GameState& actual) {
    if (expectedResult != result || std::memcmp(&expected, &actual, sizeof(actual)) != 0) {
        std::fprintf(stderr, "%s case=%u expected-result=%u actual-result=%u\n", operation, cases, expectedResult, result);
        assert(false && "effect or non-target state differs from frozen legacy semantics");
    }
    ++cases;
}

Game::GameState fixture() {
    Game::GameState state{};
    state.oobeDone = true;
    state.coins = 12345; state.stepsToday = 5798; state.activeSlot = 1;
    state.storageCount = 1; state.storage[0].hpCur = 7; state.storage[0].bond = -10;
    state.storage[0].majorStatus = Game::MajorStatus::SLEEP;
    state.team[0].speciesId = 7; state.team[1].speciesId = 25;
    for (unsigned slot = 0; slot < 2; ++slot) {
        auto& m = state.team[slot];
        m.move1Id = 33; m.move2Id = 73; m.exp = 123456;
        m.nature = 17; m.affection = 41; m.mood = 53; m.satiety = 67;
        m.majorStatus = Game::MajorStatus::POISON; m.majorStatusTurns = 7;
        m.gender = 2; m.reservedStatusBits = 19; m.lastExploredAt = 654321;
    }
    return state;
}
void clone(const Game::GameState& state, Game::GameState& expected, Game::GameState& actual) {
    std::memcpy(&expected, &state, sizeof(state));
    std::memcpy(&actual, &state, sizeof(state));
}
void health(Game::MonsterRuntime& monster, unsigned mode) {
    monster.hpCur = mode == 0 ? 0 : 1;
    monster.fainted = mode == 1;
    monster.hpMax = mode == 2 ? 0 : 20;
}
}  // namespace

int main() {
    using namespace Game::ExplorePartyService;
    GameRandom::seed(0x12345678);
    const auto expectedNextRandom = GameRandom::next();
    GameRandom::seed(0x12345678);
    GameRandom::setRangeProvider(countRandom);
    const std::array<unsigned, 5> counts = {0, 1, 2, 3, 255};
    for (auto count : counts) {
        for (unsigned first = 0; first < 4; ++first) {
            for (unsigned second = 0; second < 4; ++second) {
                for (unsigned zeroSpecies = 0; zeroSpecies < 4; ++zeroSpecies) {
                    auto state = fixture(); state.teamCount = count;
                    health(state.team[0], first); health(state.team[1], second);
                    if (zeroSpecies & 1) state.team[0].speciesId = 0;
                    if (zeroSpecies & 2) state.team[1].speciesId = 0;
                    Game::GameState expected, actual; clone(state, expected, actual);
                    assert(healthy(actual.team[0]) == Legacy::healthy(expected.team[0]));
                    assert(healthy(actual.team[1]) == Legacy::healthy(expected.team[1]));
                    assert(leaderSlot(actual) == Legacy::leader(expected));
                    assert(hasHealthyFollower(actual) == Legacy::follower(expected));
                    same("queries", 0, 0, expected, actual);
                }
            }
        }
    }
    const std::array<std::array<uint16_t, 2>, 12> hp = {{{0, 0}, {0, 20}, {1, 0}, {1, 1},
        {1, 10}, {9, 10}, {10, 10}, {11, 10}, {1, 65535}, {65530, 65535}, {65535, 65535}, {1, 19}}};
    for (uint16_t steps : {0, 1, 9, 10, 11, 19, 20, 65530, 65535}) {
        for (auto count : counts) {
            for (unsigned sample = 0; sample < hp.size(); ++sample) {
                for (unsigned flags = 0; flags < 4; ++flags) {
                    for (bool visitors : {false, true}) {
                        auto state = fixture(); state.teamCount = count;
                        for (unsigned slot = 0; slot < 2; ++slot) {
                            const auto& pair = hp[slot ? hp.size() - sample - 1 : sample];
                            state.team[slot].hpCur = pair[0]; state.team[slot].hpMax = pair[1];
                            state.team[slot].fainted = flags & (1U << slot);
                            state.team[slot].origin = visitors ? Game::Origin::VISITOR : Game::Origin::STARTER;
                        }
                        Game::GameState expected, actual; clone(state, expected, actual);
                        const auto oldResult = Legacy::recover(expected, steps);
                        const auto result = recoverAfterCompletedSteps(actual, steps);
                        same("recovery", oldResult, result, expected, actual);
                    }
                }
            }
        }
    }
    const std::array<std::array<Game::Origin, 2>, 5> origins = {{{Game::Origin::STARTER, Game::Origin::BEFRIENDED},
        {Game::Origin::VISITOR, Game::Origin::STARTER}, {Game::Origin::STARTER, Game::Origin::VISITOR},
        {Game::Origin::VISITOR, Game::Origin::VISITOR}, {Game::Origin::UNKNOWN, Game::Origin::GIFT}}};
    for (uint16_t steps : {0, 1, 14, 15, 39, 40, 79, 80, 65535}) {
        for (auto count : counts) {
            for (const auto& origin : origins) {
                for (bool visitors : {false, true}) {
                    for (unsigned first = 0; first < 4; ++first) {
                        for (unsigned second = 0; second < 4; ++second) {
                            for (int bond : {-128, -100, -1, 0, 96, 99, 100, 127}) {
                                auto state = fixture(); state.teamCount = count;
                                health(state.team[0], first); health(state.team[1], second);
                                for (unsigned slot = 0; slot < 2; ++slot) {
                                    state.team[slot].origin = origin[slot]; state.team[slot].bond = bond;
                                }
                                Game::GameState expected, actual; clone(state, expected, actual);
                                const auto oldResult = Legacy::bond(expected, steps, visitors);
                                const auto result = grantAdventureBond(actual, steps, visitors);
                                same("bond", oldResult, result, expected, actual);
                                if (!visitors) {
                                    std::memcpy(&actual, &state, sizeof(state));
                                    same("default-visitor-policy", oldResult, grantAdventureBond(actual, steps), expected, actual);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (auto count : counts) {
        for (unsigned first = 0; first < 8; ++first) {
            for (unsigned second = 0; second < 8; ++second) {
                for (unsigned flags = 0; flags < 4; ++flags) {
                    auto state = fixture(); state.teamCount = count;
                    for (unsigned slot = 0; slot < 2; ++slot) {
                        const unsigned mode = slot ? second : first;
                        state.team[slot].majorStatus = mode < 2 ? Game::MajorStatus::NONE : static_cast<Game::MajorStatus>(mode == 7 ? 255 : mode - 1);
                        state.team[slot].majorStatusTurns = mode & 1 ? 255 : 0;
                        state.team[slot].fainted = flags & (1U << slot);
                        state.team[slot].origin = Game::Origin::VISITOR;
                    }
                    Game::GameState expected, actual; clone(state, expected, actual);
                    const auto oldResult = Legacy::clear(expected);
                    same("clear-status", oldResult, clearBattleStatuses(actual), expected, actual);
                    same("clear-status-idempotent", 0, clearBattleStatuses(actual), expected, actual);
                }
            }
        }
    }
    assert(randomCalls == 0 && GameRandom::next() == expectedNextRandom);
    GameRandom::setRangeProvider(nullptr);
    std::printf("party-service-cases=%u non-target-state=unchanged rng-calls=%u\n", cases, randomCalls);
}
