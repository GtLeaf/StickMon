#include "app/ExploreSession.h"
#include "core/SaveCodec.h"
#include "game/GameRandom.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
uint32_t randomState = 0x512ab39dU;
uint32_t randomCalls = 0;
uint32_t randomHash = 2166136261U;
uint32_t captures = 0;

void hashValue(uint32_t& hash, uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (value & 255U)) * 16777619U;
        value >>= 8;
    }
}

uint32_t deterministicRange(uint32_t low, uint32_t high) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const uint32_t value = low + randomState % (high - low);
    ++randomCalls;
    hashValue(randomHash, low);
    hashValue(randomHash, high);
    hashValue(randomHash, value);
    return value;
}

std::vector<uint8_t> encoded(const Game::GameState& game) {
    std::vector<uint8_t> bytes(SaveCodec::MAX_ENCODED_BYTES);
    size_t written = 0;
    assert(SaveCodec::encode(game, MainSceneViewState{}, 1, bytes.data(),
                             bytes.size(), written));
    bytes.resize(written);
    return bytes;
}

uint32_t stateHash(const Game::GameState& game) {
    uint32_t hash = 2166136261U;
    for (uint8_t byte : encoded(game)) hash = (hash ^ byte) * 16777619U;
    return hash;
}

unsigned effectMask(const AmoledV1::ExploreSession::Effects& effect) {
    assert(!effect.startBattle);
    return unsigned(effect.save) | unsigned(effect.fullRender) << 1 |
        unsigned(effect.statusChanged) << 2 | unsigned(effect.mapChanged) << 3 |
        unsigned(effect.invalidateWorld) << 4 | unsigned(effect.startStep) << 5 |
        unsigned(effect.startExit) << 6 | unsigned(effect.completed) << 7;
}

Game::GameState fixture() {
    Game::GameState game{};
    game.teamCount = 2;
    game.storageCount = 1;
    game.team[0].speciesId = 7;
    game.team[0].hpCur = 1;
    game.team[0].hpMax = 99;
    game.team[0].bond = 10;
    game.team[0].majorStatus = Game::MajorStatus::POISON;
    game.team[0].majorStatusTurns = 3;
    game.team[1].speciesId = 25;
    game.team[1].hpCur = 1;
    game.team[1].hpMax = 11;
    game.team[1].bond = 20;
    game.team[1].origin = Game::Origin::VISITOR;
    game.team[1].majorStatus = Game::MajorStatus::BURN;
    game.team[1].majorStatusTurns = 2;
    game.storage[0] = game.team[0];
    return game;
}
}  // namespace

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void capture(const char* label, ExploreSession& session,
                        const Game::GameState& game,
                        const ExploreSession::Effects& effect = {}) {
        const auto& route = session.state_;
        std::printf("%s %u state=%08x effects=%x/%u/%u/%u route=%u/%u/%u/%u/%u/%u/%u/%u/%u/%u latch=%u rng=%u/%08x\n",
            label, ++captures, stateHash(game), effectMask(effect),
            unsigned(effect.notice), unsigned(effect.item), unsigned(effect.coins),
            unsigned(route.exploreRouteSteps), unsigned(route.exploreRouteIndex),
            unsigned(route.exploreRouteMoving), unsigned(route.exploreRoutePaused),
            unsigned(route.exploreRouteExiting), unsigned(route.exploreRoutePool.count),
            unsigned(route.exploreItemEffects.repelStepsRemaining()),
            unsigned(route.exploreItemEffects.honeyEncounterPending()),
            unsigned(route.exploreRouteAutoWalk), unsigned(session.battleActive_),
            unsigned(session.returnSettled_), randomCalls, randomHash);
    }

    static void leaders() {
        for (uint8_t count : {0, 1, 2, 3, 255}) {
            for (uint8_t firstHp : {0, 1}) for (uint8_t secondHp : {0, 1}) {
                for (bool firstFainted : {false, true}) {
                    for (bool secondFainted : {false, true}) {
                        for (uint16_t firstSpecies : {0, 7}) {
                            for (uint16_t secondSpecies : {0, 25}) {
                                auto game = fixture();
                                game.teamCount = count;
                                game.team[0].speciesId = firstSpecies;
                                game.team[1].speciesId = secondSpecies;
                                game.team[0].hpCur = firstHp;
                                game.team[1].hpCur = secondHp;
                                game.team[0].fainted = firstFainted;
                                game.team[1].fainted = secondFainted;
                                const auto before = encoded(game);
                                const bool first = count > 0 && firstHp > 0 && !firstFainted;
                                const bool second = count > 1 && secondHp > 0 && !secondFainted;
                                const uint8_t leader = !first && second ? 1 : 0;
                                assert(ExploreSession::healthy(game.team[0]) ==
                                       (firstHp > 0 && !firstFainted));
                                assert(ExploreSession::leaderSlot(game) == leader);
                                assert(ExploreSession::hasCompanion(game) ==
                                       (leader == 0 && count > 1 && secondSpecies != 0 && second));
                                assert(encoded(game) == before);
                                ExploreSession session;
                                capture("leader", session, game);
                                std::printf("selection=%u/%u\n", unsigned(leader),
                                    unsigned(ExploreSession::hasCompanion(game)));
                            }
                        }
                    }
                }
            }
        }
        assert(randomCalls == 0);
    }

    static void recovery() {
        for (uint16_t maximum : {0, 1, 9, 10, 11, 99, 65535}) {
            for (uint16_t current : {uint16_t(0), uint16_t(1), maximum,
                 uint16_t(maximum < 65535 ? maximum + 1 : maximum)}) {
                for (bool fainted : {false, true}) {
                    for (uint8_t count : {0, 1, 2, 255}) {
                        for (uint16_t steps : {0, 1, 9, 10, 11, 20, 65530, 65535}) {
                            auto game = fixture();
                            game.teamCount = count;
                            game.team[0].hpMax = maximum;
                            game.team[0].hpCur = current;
                            game.team[0].fainted = fainted;
                            game.team[0].speciesId = 0;
                            auto expected = game;
                            bool changed = false;
                            if (steps != 0 && steps % 10 == 0) {
                                for (unsigned slot = 0; slot < std::min<unsigned>(count, 2); ++slot) {
                                    auto& monster = expected.team[slot];
                                    if (monster.fainted || monster.hpCur == 0 ||
                                        monster.hpMax == 0 || monster.hpCur >= monster.hpMax) continue;
                                    monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
                                        monster.hpMax, uint32_t(monster.hpCur) +
                                        (uint32_t(monster.hpMax) + 9) / 10));
                                    changed = true;
                                }
                            }
                            ExploreSession session;
                            session.state_.exploreRouteSteps = steps;
                            assert(session.recoverTeam(game) == changed);
                            assert(encoded(game) == encoded(expected));
                            const auto effect = session.takeEffects();
                            assert(effectMask(effect) == 0);
                            capture("recover", session, game, effect);
                            std::printf("changed=%u\n", unsigned(changed));
                        }
                    }
                }
            }
        }
        assert(randomCalls == 0);
    }

    static void completedSteps() {
        ExploreSession session;
        auto game = fixture();
        auto& route = session.state_;
        route.exploreRouteMap.pathCount = 1;
        route.exploreRouteMap.paths[0].pointCount = 4;
        route.exploreRouteMap.paths[0].points[1] = {2, 2};
        route.exploreRouteMapBlockCount = 1;
        route.exploreRouteIndex = 1;
        route.exploreRouteSteps = 8;
        route.exploreRouteEncounterCooldownSteps = 100;
        game.stepsToday = 59999;
        for (unsigned step = 9; step <= 20; ++step) {
            route.exploreRouteMoving = true;
            session.completeStep(game, step * 360);
            const auto effect = session.takeEffects();
            assert(effectMask(effect) == (step % 10 == 0 ? 4U : 0U));
            assert(route.exploreRouteSteps == step && game.stepsToday == 60000);
            assert(route.exploreRouteEncounterCooldownSteps == 108 - step);
            assert(game.team[0].hpCur == (step < 10 ? 1 : step < 20 ? 11 : 21));
            assert(game.team[1].hpCur == (step < 10 ? 1 : step < 20 ? 3 : 5));
            assert(game.team[0].majorStatus == Game::MajorStatus::POISON);
            assert(game.team[1].majorStatus == Game::MajorStatus::BURN);
            capture("step", session, game, effect);
            const auto once = encoded(game);
            session.completeStep(game, step * 360);
            assert(encoded(game) == once && effectMask(session.takeEffects()) == 0);
        }
        route.exploreRouteSteps = 65535;
        route.exploreRouteMoving = true;
        session.completeStep(game, 8000);
        assert(route.exploreRouteSteps == 0 && game.team[0].hpCur == 21);
        assert(effectMask(session.takeEffects()) == 0);
        capture("step-wrap", session, game);
        assert(randomCalls == 0);
    }

    static void settlement() {
        for (uint16_t steps : {0, 1, 14, 15, 39, 40, 79, 80, 65535}) {
            for (uint8_t count : {0, 1, 2, 255}) {
                for (uint8_t variant : {0, 1, 2, 3, 4, 5}) {
                    auto game = fixture();
                    game.teamCount = count;
                    game.team[0].speciesId = 0;
                    if (variant == 1) game.team[0].fainted = true;
                    if (variant == 2) game.team[0].hpCur = 0;
                    if (variant == 3) game.team[0].bond = 99;
                    if (variant == 4) game.team[0].bond = 100;
                    if (variant == 5) {
                        game.team[0].majorStatus = Game::MajorStatus::NONE;
                        game.team[0].majorStatusTurns = 5;
                        game.team[1].fainted = true;
                    }
                    auto expected = game;
                    const unsigned gain = steps == 0 ? 0 : steps < 15 ? 1 :
                        steps < 40 ? 2 : steps < 80 ? 3 : 4;
                    for (unsigned slot = 0; slot < std::min<unsigned>(count, 2); ++slot) {
                        auto& monster = expected.team[slot];
                        monster.majorStatus = Game::MajorStatus::NONE;
                        monster.majorStatusTurns = 0;
                        if (!monster.fainted && monster.hpCur > 0 &&
                            monster.origin != Game::Origin::VISITOR) {
                            monster.bond = static_cast<int8_t>(std::min<int>(100, monster.bond + gain));
                        }
                    }
                    ExploreSession session;
                    session.state_.exploreRouteSteps = steps;
                    session.state_.exploreItemEffects.activateHoney();
                    session.state_.exploreItemEffects.activateMaxRepel();
                    session.settleReturn(game);
                    assert(encoded(game) == encoded(expected));
                    assert(!session.state_.exploreItemEffects.honeyEncounterPending());
                    assert(session.state_.exploreItemEffects.repelStepsRemaining() == 0);
                    auto effect = session.takeEffects();
                    assert(effectMask(effect) == 1 && session.returnSettled_);
                    capture("return", session, game, effect);
                    const auto once = encoded(game);
                    session.settleReturn(game);
                    session.finishReturn(game);
                    assert(encoded(game) == once && effectMask(session.takeEffects()) == 0);
                    capture("return-again", session, game);
                }
            }
        }
        assert(randomCalls == 0);
    }

    static void lifecycle() {
        ExploreSession session, other;
        auto game = fixture();
        auto otherGame = fixture();
        session.state_.exploreRouteSteps = 40;
        session.state_.exploreRoutePool.count = 3;
        session.battleActive_ = true;
        session.state_.exploreRouteMoving = true;
        session.state_.exploreRoutePaused = true;
        session.state_.exploreRouteExiting = true;
        session.state_.exploreRouteAutoWalk = true;
        session.finishReturn(game);
        assert(effectMask(session.takeEffects()) == 1);
        assert(!session.state_.exploreRoutePool.count && !session.battleActive_);
        assert(!session.state_.exploreRouteMoving && !session.state_.exploreRoutePaused &&
               !session.state_.exploreRouteExiting && !session.state_.exploreRouteAutoWalk);
        assert(game.team[0].bond == 13 && otherGame.team[0].bond == 10);
        assert(!other.returnSettled_);
        capture("finished", session, game);
        assert(!session.begin(game, 255));
        assert(session.returnSettled_);
        assert(session.begin(game, 0));
        assert(!session.returnSettled_ && session.state_.exploreRouteSteps == 0);
        assert(effectMask(session.takeEffects()) == 0);
        capture("new-run", session, game);
        session.state_.exploreRouteSteps = 1;
        session.finishReturn(game);
        assert(game.team[0].bond == 14 && effectMask(session.takeEffects()) == 1);
        capture("new-return", session, game);
        assert(randomCalls > 0);
    }
};
}  // namespace AmoledV1

int main(int argc, char** argv) {
    assert(argc == 2);
    GameRandom::setRangeProvider(deterministicRange);
    using Probe = AmoledV1::AmoledAppTestAccess;
    if (!std::strcmp(argv[1], "leaders")) Probe::leaders();
    else if (!std::strcmp(argv[1], "recovery")) Probe::recovery();
    else if (!std::strcmp(argv[1], "steps")) Probe::completedSteps();
    else if (!std::strcmp(argv[1], "settlement")) Probe::settlement();
    else if (!std::strcmp(argv[1], "lifecycle")) Probe::lifecycle();
    else return 2;
    std::printf("captures=%u rng=%u/%08x\n", captures, randomCalls, randomHash);
    GameRandom::setRangeProvider(nullptr);
}
