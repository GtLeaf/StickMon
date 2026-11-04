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
#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace {
uint32_t randomState = 1, randomCalls = 0, randomHash = 2166136261U;
unsigned randomMode = 0;
uint32_t stateHash = 2166136261U;
unsigned cases = 0;

void hashValue(uint32_t& hash, uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (value & 255)) * 16777619U;
        value >>= 8;
    }
}

uint32_t randomRange(uint32_t low, uint32_t high) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    uint32_t value = high > low ? low + randomState % (high - low) : low;
    if (randomMode == 1) value = low;
    if (randomMode == 2) value = high > low ? high - 1 : low;
    ++randomCalls;
    hashValue(randomHash, low);
    hashValue(randomHash, high);
    hashValue(randomHash, value);
    return value;
}

void beginRandom(uint32_t seed, unsigned mode = 0) {
    randomState = seed;
    randomMode = mode;
    randomCalls = 0;
    randomHash = 2166136261U;
    GameRandom::setRangeProvider(randomRange);
}

void value(uint32_t v) { hashValue(stateHash, v); }

void capture(const ExploreScene& scene, const Game::GameState& state) {
    value(static_cast<unsigned>(scene.phase));
    value(scene.mapBlockCount);
    value(scene.currentMapBlock);
    for (auto area : scene.mapBlocks) value(area);
    value(scene.mapEncounterCount);
    value(scene.encounterCooldownSteps);
    value(scene.expeditionSeed);
    value(scene.expeditionBossScheduled);
    value(scene.expeditionNormalBossScheduled);
    value(scene.expeditionBossSpeciesId);
    value(scene.expeditionBossLevel);
    value(scene.expeditionBossExperiencePercent);
    value(static_cast<unsigned>(scene.expeditionSpecialKind));
    value(scene.routePickupItem);
    value(scene.routePickupIndex);
    value(scene.routePickupAvailable);
    value(scene.routePickupFinalReward);
    value(scene.routeGuaranteedEncounterIndex);
    value(scene.routeGuaranteedEncounterPending);
    value(scene.routeBossIndex);
    value(scene.routeBossPending);
    value(scene.currentRoutePath);
    value(scene.wildRuntime.speciesId);
    value(scene.wildRuntime.level);
    value(scene.wildRuntime.ivPacked);
    value(scene.wildHp);
    value(state.coins);
    value(state.stepsToday);
    for (unsigned item = 0; item < static_cast<unsigned>(Game::ItemId::COUNT); ++item) {
        value(Game::ItemInventory::count(state, static_cast<Game::ItemId>(item)));
    }
    if (scene.resultMessage) {
        for (const unsigned char* text = reinterpret_cast<const unsigned char*>(scene.resultMessage); *text; ++text) value(*text);
    }
    value(randomCalls);
    value(randomHash);
}

void report(const char* category, unsigned area) {
    std::printf("%s area=%u cases=%u state-and-rng=%08x\n", category, area, cases, stateHash);
    cases = 0;
    stateHash = 2166136261U;
}

void fixture(Game::GameState& state, DesktopPlatform& desktop, uint16_t steps = 5000) {
    GameRandom::setRangeProvider(nullptr);
    GameRandom::seed(1234);
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(7, 30);
    state.team[1] = Game::MonsterFactory::create(25, 28);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.stepsToday = steps;
    state.gameMinutesTotal = 420;
    state.specialBossDefeatedMask = 7;
    for (auto& count : state.explorePoolRerollCounts) count = 1;
    state.normalBossPitySlotIndex = ExploreSpecial::slotIndexFor(state.gameMinutesTotal);
    GameEngine::ins().gameClock.set(desktop.millis(), state.gameMinutesTotal);
    GameEngine::ins().saveCoordinator.reset(desktop.millis());
}

void plainPath(ExploreScene& scene, uint8_t area, uint8_t points = 12) {
    scene.activeArea = static_cast<ExploreScene::Area>(area);
    scene.phase = ExploreScene::Phase::WALKING;
    scene.mapBlockCount = 3;
    for (auto& map : scene.mapBlocks) map = area;
    scene.generatedMap = {};
    scene.generatedMap.areaIndex = area;
    scene.generatedMap.pathCount = 1;
    auto& path = scene.generatedMap.paths[0];
    path.pointCount = points;
    for (uint8_t i = 0; i < points; ++i) path.points[i] = {uint8_t(i % 12 + 2), uint8_t(i / 12 + 2)};
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    engine.currentScene.reset();
    auto& state = engine.gameState();
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        for (uint32_t seed : {1U, 17U, 391U, 20260713U, 0x12345678U, 0xCAFEBABEU}) {
            for (uint8_t pity : {uint8_t(0), uint8_t(3)}) {
                for (uint16_t steps : {uint16_t(4999), uint16_t(5000)}) {
                    fixture(state, desktop, steps);
                    state.normalBossMissCount[area] = pity;
                    ExploreScene scene;
                    scene.activeArea = static_cast<ExploreScene::Area>(area);
                    beginRandom(seed);
                    scene.resetWalk();
                    assert(scene.mapBlockCount >= 3 && scene.mapBlockCount <= 9);
                    value(seed); value(pity); value(steps);
                    for (uint8_t block = 0; block < scene.mapBlockCount; ++block) {
                        if (block) {
                            scene.pendingEntryEdge = ExploreMapGenerator::opposite(scene.generatedMap.paths[scene.currentRoutePath].exit.edge);
                            scene.currentMapBlock = block;
                            assert(scene.resetRouteSegment());
                        }
                        value(ExploreMapGenerator::fingerprint(scene.generatedMap));
                        capture(scene, state);
                    }
                    ++cases;
                }
            }
        }
        report("generated-maps", area);
        for (uint16_t steps : {uint16_t(4999), uint16_t(5000)}) {
            for (uint8_t points : {uint8_t(1), uint8_t(2), uint8_t(3), uint8_t(6), uint8_t(12), uint8_t(20)}) {
                for (bool final : {false, true}) {
                    for (uint8_t cooldown : {uint8_t(0), uint8_t(1), uint8_t(5), uint8_t(255)}) {
                        for (unsigned mode = 0; mode < 3; ++mode) {
                            fixture(state, desktop, steps);
                            ExploreScene scene;
                            plainPath(scene, area, points);
                            if (final) scene.mapBlockCount = 1;
                            scene.encounterCooldownSteps = cooldown;
                            beginRandom(391, mode);
                            scene.placeRoutePickup();
                            value(steps); value(points); value(final); value(cooldown); value(mode);
                            capture(scene, state);
                            ++cases;
                        }
                    }
                }
            }
        }
        report("pickup-placement", area);
        for (uint8_t pickup = 0; pickup < 18; ++pickup) {
            for (bool full : {false, true}) {
                for (bool final : {false, true}) {
                    for (unsigned mode = 1; mode <= 2; ++mode) {
                        fixture(state, desktop);
                        if (full) {
                            for (unsigned item = 0; item < static_cast<unsigned>(Game::ItemId::COUNT); ++item) {
                                Game::ItemInventory::add(state, static_cast<Game::ItemId>(item), 99);
                            }
                        }
                        ExploreScene scene;
                        plainPath(scene, area);
                        scene.routePickupFinalReward = final;
                        beginRandom(17, mode);
                        scene.resolvePickup(pickup);
                        value(pickup); value(full); value(final); value(mode);
                        capture(scene, state);
                        ++cases;
                    }
                }
            }
        }
        report("pickup-resolution", area);
        for (uint8_t cooldown : {uint8_t(0), uint8_t(1), uint8_t(5)}) {
            for (uint8_t count : {uint8_t(0), uint8_t(1), uint8_t(2)}) {
                for (bool guaranteed : {false, true}) {
                    for (bool bypass : {false, true}) {
                        for (bool repel : {false, true}) {
                            for (unsigned mode = 1; mode <= 2; ++mode) {
                                fixture(state, desktop);
                                ExploreScene scene;
                                plainPath(scene, area);
                                scene.encounterCooldownSteps = cooldown;
                                scene.mapEncounterCount = count;
                                beginRandom(17, mode);
                                const bool encountered = scene.rollRandomEncounter(guaranteed, bypass, repel);
                                value(cooldown); value(count); value(guaranteed); value(bypass); value(repel); value(mode); value(encountered);
                                capture(scene, state);
                                ++cases;
                            }
                        }
                    }
                }
            }
        }
        report("encounter-gates", area);
    }
    GameRandom::setRangeProvider(nullptr);
}
