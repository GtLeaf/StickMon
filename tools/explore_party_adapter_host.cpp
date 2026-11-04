#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {
using ProbePlatform = DesktopPlatform;
unsigned blobWrites = 0;
bool blobFailWrites = false;
class ProbeBlobStore final : public Platform::IBlobStore {
public:
    explicit ProbeBlobStore(DesktopPlatform& platform) : platform_(platform) {}
    bool initialize() override { return platform_.initialize(); }
    size_t blobSize(const char* ns, const char* key) override { return platform_.blobSize(ns, key); }
    bool readBlob(const char* ns, const char* key, void* data, size_t size) override {
        return platform_.readBlob(ns, key, data, size);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t size) override {
        ++blobWrites;
        return !blobFailWrites && platform_.writeBlob(ns, key, data, size);
    }
    bool removeBlob(const char* ns, const char* key) override { return platform_.removeBlob(ns, key); }
    bool clearNamespace(const char* ns) override { return platform_.clearNamespace(ns); }
private:
    DesktopPlatform& platform_;
};
unsigned randomCalls = 0, cases = 0;
uint32_t hash = 2166136261U;
uint32_t countRandom(uint32_t low, uint32_t) { ++randomCalls; return low; }
void value(uint32_t v) {
    for (unsigned byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (v & 255)) * 16777619U;
        v >>= 8;
    }
}
void monster(const Game::MonsterRuntime& m) {
    value(m.speciesId); value(m.hpCur); value(m.hpMax); value(m.fainted);
    value(static_cast<unsigned>(m.origin)); value(static_cast<unsigned>(m.majorStatus));
    value(m.majorStatusTurns); value(static_cast<uint8_t>(m.bond));
    value(m.exp); value(m.affection); value(m.mood); value(m.statusFlags);
}
void capture(const ExploreScene& scene, ProbePlatform& platform) {
    auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    value(state.teamCount); value(state.activeSlot); value(state.stepsToday);
    for (const auto& m : state.team) monster(m);
    value(scene.adventureBondGain); value(scene.adventureBondSettled);
    value(static_cast<unsigned>(scene.phase));
    value(static_cast<unsigned>(engine.exploreTravel));
    value(static_cast<unsigned>(engine.sceneFade));
    value(static_cast<unsigned>(engine.sceneFadeTarget));
    const auto& save = engine.saveCoordinator;
    value(save.dirty_); value(save.soon_); value(save.lastSaveMs_);
    value(save.dirtySinceMs_); value(save.soonSinceMs_); value(save.lastMutationMs_);
    value(save.due(platform.millis() + 1999));
    value(save.due(platform.millis() + 2000));
    value(save.due(platform.millis() + 299999));
    value(save.due(platform.millis() + 300000));
    value(blobWrites);
    Game::GameState saved;
    MainSceneViewState view;
    const bool loaded = engine.saveManager.load(saved, view);
    value(loaded);
    if (loaded) for (const auto& m : saved.team) monster(m);
    value(randomCalls);
}
void report(const char* category) {
    assert(randomCalls == 0);
    std::printf("%s cases=%u state-save-and-rng=%08x rng-calls=%u\n", category, cases, hash, randomCalls);
    std::fflush(stdout);
    hash = 2166136261U; cases = 0;
}
void fixture(ProbePlatform& platform) {
    auto& engine = GameEngine::ins();
    engine.currentScene.reset();
    engine.currentId = SceneID::EXPLORE;
    engine.prevId = SceneID::MAIN;
    engine.sceneFade = GameEngine::SceneFadePhase::NONE;
    engine.sceneFadeTarget = SceneID::MAIN;
    engine.exploreTravel = ExploreTravelPhase::ACTIVE;
    engine.contactVisit = {};
    engine.saveWritesBlocked = false;
    engine.encounterHistory.clear();
    engine.encounterHistoryDirty = false;
    auto& state = engine.gameState();
    GameRandom::setRangeProvider(nullptr);
    GameRandom::seed(1234);
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(7, 30);
    state.team[1] = Game::MonsterFactory::create(25, 28);
    state.team[0].bond = 0; state.team[1].bond = 0;
    state.gameMinutesTotal = 420;
    engine.gameClock.set(platform.millis(), state.gameMinutesTotal);
    engine.saveCoordinator.reset(0);
    engine.clearMainSceneViewState();
    platform.blobs_.clear(); platform.logs_.clear(); blobWrites = 0;
    blobFailWrites = false;
    randomCalls = 0;
    GameRandom::setRangeProvider(countRandom);
}
void health(Game::MonsterRuntime& m, unsigned mode) {
    m.hpCur = mode == 0 ? 0 : 1;
    m.fainted = mode == 1;
    m.hpMax = mode == 2 ? 0 : 20;
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    ProbePlatform platform(argv[1]);
    ProbeBlobStore blobs(platform);
    const auto& base = platform.serviceBundle();
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, base.audio, base.imu, base.power, blobs, base.resources,
        base.memory, base.peers};
    Platform::bind(services);
    assert(platform.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    platform.advanceMs(4000);
    auto& state = engine.gameState();
    const std::array<std::array<uint16_t, 2>, 12> hp = {{{0, 0}, {0, 20}, {1, 0}, {1, 1},
        {1, 10}, {9, 10}, {10, 10}, {11, 10}, {1, 65535}, {65530, 65535}, {65535, 65535}, {1, 19}}};
    for (uint16_t steps : {0, 1, 9, 10, 11, 19, 20, 65530, 65535}) {
        for (uint8_t count : {0, 1, 2}) {
            for (unsigned index = 0; index < hp.size(); ++index) {
                for (unsigned flags = 0; flags < 4; ++flags) {
                    for (bool visitor : {false, true}) {
                        fixture(platform);
                        state.teamCount = count;
                        for (unsigned slot = 0; slot < 2; ++slot) {
                            const auto& sample = hp[slot ? hp.size() - index - 1 : index];
                            auto& m = state.team[slot]; m.hpCur = sample[0]; m.hpMax = sample[1];
                            m.fainted = flags & (1U << slot);
                            m.origin = visitor ? Game::Origin::VISITOR : Game::Origin::STARTER;
                            m.majorStatus = Game::MajorStatus::POISON; m.majorStatusTurns = 7;
                        }
                        ExploreScene scene; scene.steps = steps;
                        scene.recoverTeamForCompletedSteps();
                        value(steps); value(index); value(flags); value(visitor);
                        capture(scene, platform); ++cases;
                    }
                }
            }
        }
    }
    report("recovery");
    for (uint16_t steps : {0, 1, 15, 40, 80}) {
        for (uint8_t count : {0, 1, 2}) {
            for (unsigned visitors = 0; visitors < 4; ++visitors) {
                for (unsigned visit = 0; visit < 4; ++visit) {
                    for (unsigned faint = 0; faint < 4; ++faint) {
                        for (int bond : {0, 99, 100}) {
                            fixture(platform); state.teamCount = count;
                            engine.contactVisit.active = visit & 1;
                            engine.contactVisit.exploring = visit & 2;
                            for (unsigned slot = 0; slot < 2; ++slot) {
                                state.team[slot].origin = visitors & (1U << slot) ? Game::Origin::VISITOR : Game::Origin::STARTER;
                                state.team[slot].fainted = faint & (1U << slot);
                                state.team[slot].bond = bond;
                            }
                            ExploreScene scene;
                            const auto gain = engine.grantAdventureBond(steps);
                            value(steps); value(visitors); value(visit); value(faint); value(bond); value(gain);
                            capture(scene, platform); ++cases;
                        }
                    }
                }
            }
        }
    }
    report("bond-and-immediate-save");
    for (unsigned mode : {0, 1, 2}) {
        fixture(platform);
        blobFailWrites = mode == 1;
        engine.saveWritesBlocked = mode == 2;
        ExploreScene scene;
        value(mode); value(engine.grantAdventureBond(40)); capture(scene, platform); ++cases;
    }
    report("save-failure-and-write-protection");
    for (uint8_t count : {0, 1, 2}) {
        for (unsigned first = 0; first < 6; ++first) {
            for (unsigned second = 0; second < 6; ++second) {
                fixture(platform); state.teamCount = count;
                for (unsigned slot = 0; slot < 2; ++slot) {
                    unsigned mode = slot ? second : first;
                    state.team[slot].majorStatus = mode < 2 ? Game::MajorStatus::NONE : (mode == 5 ? static_cast<Game::MajorStatus>(255) : Game::MajorStatus::POISON);
                    state.team[slot].majorStatusTurns = mode & 1 ? 5 : 0;
                    state.team[slot].fainted = mode >= 4;
                }
                ExploreScene scene;
                value(first); value(second); value(engine.clearExploreBattleStatuses());
                capture(scene, platform);
                assert(!engine.clearExploreBattleStatuses()); ++cases;
            }
        }
    }
    report("status-cleanup");
    for (uint8_t count : {0, 1, 2}) {
        for (unsigned first = 0; first < 4; ++first) {
            for (unsigned second = 0; second < 4; ++second) {
                for (unsigned zeroSpecies = 0; zeroSpecies < 4; ++zeroSpecies) {
                    fixture(platform); state.teamCount = count;
                    health(state.team[0], first); health(state.team[1], second);
                    if (zeroSpecies & 1) state.team[0].speciesId = 0;
                    if (zeroSpecies & 2) state.team[1].speciesId = 0;
                    ExploreScene scene; scene.phase = ExploreScene::Phase::WALKING;
                    const auto frame = scene.exploreRouteFrame();
                    value(first); value(second); value(zeroSpecies);
                    value(frame.leader ? frame.leader->id : 0);
                    value(frame.follower != nullptr);
                    value(frame.follower ? frame.follower->id : 0);
                    capture(scene, platform); ++cases;
                }
            }
        }
    }
    report("visible-leader-and-follower");
    for (uint16_t steps : {0, 40, 80}) {
        for (bool visitor : {false, true}) {
            for (bool local : {false, true}) {
                for (bool faint : {false, true}) {
                    for (int bond : {0, 100}) {
                        for (bool returningFainted : {false, true}) {
                            fixture(platform);
                            engine.contactVisit.active = local; engine.contactVisit.exploring = local;
                            state.team[1].origin = visitor ? Game::Origin::VISITOR : Game::Origin::STARTER;
                            state.team[0].fainted = faint;
                            state.team[0].bond = state.team[1].bond = bond;
                            state.team[0].majorStatus = Game::MajorStatus::POISON;
                            state.team[1].majorStatusTurns = 3;
                            ExploreScene scene; scene.steps = steps;
                            scene.beginExploreEnding();
                            assert(scene.phase == ExploreScene::Phase::EXITING);
                            assert(engine.exploreTravel == ExploreTravelPhase::RETURNING);
                            for (const auto& member : state.team) {
                                assert(member.majorStatus == Game::MajorStatus::NONE);
                                assert(member.majorStatusTurns == 0);
                            }
                            value(steps); value(visitor); value(local); value(faint); value(bond); value(returningFainted);
                            capture(scene, platform);
                            const int firstBond = state.team[0].bond, secondBond = state.team[1].bond;
                            scene.steps = 80;
                            scene.beginExploreEnding();
                            assert(state.team[0].bond == firstBond && state.team[1].bond == secondBond);
                            scene.completeExploreReturn(returningFainted);
                            scene.settleAdventureBond();
                            assert(state.team[0].bond == firstBond && state.team[1].bond == secondBond);
                            capture(scene, platform); ++cases;
                        }
                    }
                }
            }
        }
    }
    report("return-settlement-once");
#if STICKMON_ENABLE_DEBUG_FEATURES
    fixture(platform);
    ExploreScene debug; debug.debugBattleMode = true; debug.steps = 80;
    debug.beginExploreEnding();
    assert(!debug.adventureBondSettled && debug.adventureBondGain == 0);
    assert(state.team[0].bond == 0 && state.team[1].bond == 0 && blobWrites == 0);
#endif
    GameRandom::setRangeProvider(nullptr);
}
