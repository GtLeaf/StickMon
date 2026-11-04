#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "assets/GameAssets.h"
#define private public
#include "game/ExploreEcology.h"
#include "game/ExploreGrass.h"
#undef private
#include "game/ExploreEncounters.h"
#include "game/ExploreRouteGeometry.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "hardware/Hal.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#include "ui/common/ExploreRouteUiContent.h"

namespace {
unsigned randomCalls = 0;
uint32_t visualRandom(uint32_t minimum, uint32_t) { ++randomCalls; return minimum; }
class Input final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return true; }
};
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle(); Input input;
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger, input,
        bundle.display, bundle.audio, bundle.imu, bundle.power, bundle.blobs,
        bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services); assert(desktop.begin());
    auto& engine = GameEngine::ins(); assert(engine.begin());
    auto& state = engine.gameState();
    std::unique_ptr<ExploreScene> scene;
    const Ui::ExploreRouteLayout layout{Hal::DISPLAY_W, Hal::DISPLAY_H};
    auto setup = [&](uint8_t area) {
        scene.reset(); engine.currentScene.reset();
        engine.currentId = SceneID::MAIN; engine.prevId = SceneID::MAIN;
        engine.exploreTravel = ExploreTravelPhase::NONE;
        engine.sceneFade = GameEngine::SceneFadePhase::NONE;
        engine.evolutionEventHead = engine.evolutionEventCount = 0;
        engine.moveReplacementEventHead = engine.moveReplacementEventCount = 0;
        state = Game::GameState{}; state.oobeDone = true; state.tutorialFlags = 0xff;
        state.gameMinutesTotal = 420; state.teamCount = 2;
        state.team[0] = Game::MonsterFactory::create(161, 35);
        state.team[1] = Game::MonsterFactory::create(41, 30);
        for (auto& reroll : state.explorePoolRerollCounts) reroll = 1;
        engine.gameClock.set(desktop.millis(), state.gameMinutesTotal);
        uint16_t team[] = {161, 41}; assert(PokemonSprites::syncTeamCache(team, 2));
        GameRandom::seed(1234 + area);
        scene = std::make_unique<ExploreScene>(); scene->onEnter();
        scene->activeArea = static_cast<ExploreScene::Area>(area);
        scene->resetWalk();
        assert(scene->phase == Ui::ExplorePhase::WALKING);
        scene->routeMoving = scene->routeFollowerMoving = scene->autoWalkActive = false;
        scene->areaPreviewLoadPending = false;
    };
    auto pixels = [&] {
        PixelRenderer::clear(0); scene->render();
        const auto fb = desktop.frameBuffer();
        return std::vector<uint16_t>(fb.pixels, fb.pixels + fb.width * fb.height);
    };
    auto capture = [&](const std::string& name) {
        const auto frame = pixels();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file && std::fwrite(frame.data(), sizeof(uint16_t), frame.size(), file) == frame.size());
        std::fclose(file);
    };
    auto poses = [&] {
        std::vector<ExploreEcology::Pose> result;
        for (uint8_t i = 0; i < scene->ui.ecology.count(); ++i) result.push_back(scene->ui.ecology.pose(i));
        return result;
    };
    auto samePoses = [&](const std::vector<ExploreEcology::Pose>& expected) {
        assert(expected.size() == scene->ui.ecology.count());
        for (uint8_t i = 0; i < expected.size(); ++i)
            assert(ExploreEcology::samePose(expected[i], scene->ui.ecology.pose(i)));
    };
    unsigned actors = 0;
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
        setup(area);
        auto& ui = scene->ui;
        actors += ui.ecology.count();
        ExploreEcology::Simulation reference;
        uint8_t hidden = 0;
        for (uint8_t i = 0; i < scene->activePool.count; ++i)
            if (ExplorePool::isRare(scene->activePool.entries[i].rarity) &&
                !engine.hasEncounteredSpecies(scene->activePool.entries[i].speciesId)) hidden |= 1U << i;
        reference.reset(scene->generatedMap, scene->currentRoutePath, scene->activePool, hidden, desktop.millis());
        assert(reference.count() == ui.ecology.count());
        for (uint8_t i = 0; i < reference.count(); ++i)
            assert(ExploreEcology::samePose(reference.pose(i), ui.ecology.pose(i)));
        for (uint8_t i = 0; i < ui.ecology.count(); ++i) {
            assert(PokemonSprites::findCachedSpeciesSprite(ui.ecology.pose(i).speciesId,
                PokemonSprites::SpriteKind::ICON_0));
        }
        const auto beforeState = state;
        const auto mapFingerprint = ExploreMapGenerator::fingerprint(scene->generatedMap);
        capture("area-" + std::to_string(area) + "-start");
        GameRandom::setRangeProvider(visualRandom); randomCalls = 0;
        for (int tick = 0; tick < 120; ++tick) {
            desktop.advanceMs(66); ui.updateRouteVisual(desktop.millis(), true);
            if (tick == 59) capture("area-" + std::to_string(area) + "-active");
        }
        assert(randomCalls == 0); GameRandom::setRangeProvider(nullptr);
        assert(std::memcmp(&beforeState, &state, sizeof(state)) == 0);
        assert(mapFingerprint == ExploreMapGenerator::fingerprint(scene->generatedMap));
        // Render is repeatable, consumes no simulation time and never reloads sprites.
        pixels(); const auto reloads = PokemonSprites::cacheStats().reloadCount;
        const auto beforeRender = poses(); const auto first = pixels();
        assert(first == pixels() && PokemonSprites::cacheStats().reloadCount == reloads);
        samePoses(beforeRender);
        capture("area-" + std::to_string(area) + "-end");
        // Actual Scene::update must honor each modal/pause owner, including the child menu.
        for (int pause = 0; pause < 5; ++pause) {
            const auto beforePause = poses();
            scene->phase = pause == 2 ? Ui::ExplorePhase::ENCOUNTER :
                pause == 3 ? Ui::ExplorePhase::PICKUP : Ui::ExplorePhase::WALKING;
            scene->exploreMenuOpen = pause == 0;
            scene->exploreSubViewOpen = pause == 1;
            scene->pendingFrostFall = pause == 4;
            scene->frostFallAtMs = desktop.millis() + 100000;
            for (int wait = 0; wait < 5; ++wait) {
                desktop.advanceMs(2000); scene->update(desktop.millis(), 2);
            }
            samePoses(beforePause);
            scene->exploreMenuOpen = scene->exploreSubViewOpen = scene->pendingFrostFall = false;
        }
        scene->phase = Ui::ExplorePhase::WALKING;
        auto resumedReference = ui.ecology;
        const auto pausedPoses = poses();
        scene->update(desktop.millis(), 0);
        samePoses(pausedPoses);
        // Rebase once on resume, then advance the next active frame normally.
        const int cameraX = std::clamp(int(std::lround(scene->routeWorldX)) - layout.width / 2,
            0, ExploreMapGenerator::WIDTH * 26 - layout.width);
        const int cameraY = std::clamp(int(std::lround(scene->routeWorldY)) - layout.height / 2,
            0, ExploreMapGenerator::HEIGHT * 26 - layout.height);
        resumedReference.update(desktop.millis(), false, scene->routeWorldX, scene->routeWorldY,
            {float(cameraX), float(cameraY), float(layout.width), float(layout.height)});
        desktop.advanceMs(66); scene->update(desktop.millis(), .066f);
        resumedReference.update(desktop.millis(), true, scene->routeWorldX, scene->routeWorldY,
            {float(cameraX), float(cameraY), float(layout.width), float(layout.height)});
        assert(ui.ecology.lastUpdateMs_ == resumedReference.lastUpdateMs_);
        for (uint8_t i = 0; i < ui.ecology.count(); ++i) {
            assert(ExploreEcology::samePose(ui.ecology.pose(i), resumedReference.pose(i)));
            assert(ui.ecology.actors_[i].elapsedMs == resumedReference.actors_[i].elapsedMs);
            assert(ui.ecology.actors_[i].pendingMs == resumedReference.actors_[i].pendingMs);
        }
        assert(ui.ecology.group_.pendingMs == resumedReference.group_.pendingMs);
        assert(ui.ecology.group_.clockMs == resumedReference.group_.clockMs);
        // A new map starts a fresh deterministic ecosystem and drops old visual state.
        assert(scene->resetRouteSegment());
        reference.reset(scene->generatedMap, scene->currentRoutePath, scene->activePool, hidden, desktop.millis());
        assert(reference.count() == ui.ecology.count());
        for (uint8_t i = 0; i < reference.count(); ++i)
            assert(ExploreEcology::samePose(reference.pose(i), ui.ecology.pose(i)));
    }
    assert(actors > 0);

    // A family member absent from the pool still has resident artwork at route entry.
    setup(0); scene->activePool = {};
    scene->activePool.count = 1;
    scene->activePool.entries[0] = {183, 10, ExplorePool::Rarity::COMMON};
    uint16_t species[36]{};
    const auto count = scene->ui.routeSpriteSpecies(scene->activePool, species, 36);
    for (uint16_t family : {uint16_t(184), uint16_t(183), uint16_t(298)})
        assert(std::find(species, species + count, family) != species + count);
    uint16_t bounded[2] = {999, 777};
    assert(scene->ui.routeSpriteSpecies(scene->activePool, bounded, 1) == 1 && bounded[1] == 777);
    PokemonSprites::setPinnedDynamicSpecies(species, count);
    assert(PokemonSprites::preloadDynamicSpecies(species, count));

    // Controlled terrain and poses exercise cover ordering independent of random spawning.
    auto terrain = [&](uint16_t base, uint8_t area) {
        auto& map = scene->generatedMap; map = {};
        map.seed = 773; map.areaIndex = area; map.pathCount = 1;
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, base);
        map.paths[0].pointCount = 14;
        for (uint8_t i = 0; i < 14; ++i) map.paths[0].points[i] = {uint8_t(i + 1), 5};
        scene->currentRoutePath = 0; scene->routeBossPending = false;
        scene->routePickupAvailable = false;
        scene->routeWorldX = 260; scene->routeWorldY = 182;
        scene->routeFollowerWorldX = -1000; scene->routeFollowerWorldY = -1000;
        scene->ui.resetRouteVisual(scene->activePool, 0);
    };
    terrain(390, 2);
    ExplorePool::Pool rare{}; rare.count = 1;
    rare.entries[0] = {133, 10, ExplorePool::Rarity::RARE};
    uint16_t rareId = 133; assert(PokemonSprites::preloadDynamicSpecies(&rareId, 1));
    scene->ui.resetRouteVisual(rare, 1);
    assert(scene->ui.ecology.count() && scene->ui.ecology.pose(0).hidden);
    const auto hiddenPose = scene->ui.ecology.pose(0);
    scene->routeWorldX = hiddenPose.x + 60; scene->routeWorldY = hiddenPose.y;
    capture("rare-hidden");
    scene->beginEncounter(*findSpecies(133), 10);
    for (uint8_t i = 0; i < scene->ui.ecology.count(); ++i) assert(!scene->ui.ecology.pose(i).hidden);
    scene->phase = Ui::ExplorePhase::WALKING;
    capture("rare-revealed");

    uint16_t art[] = {161, 41, 129, 133};
    PokemonSprites::setPinnedDynamicSpecies(art, 4);
    assert(PokemonSprites::preloadDynamicSpecies(art, 4));
    auto poseAt = [&](uint16_t species, float x, float y) -> ExploreEcology::Pose& {
        auto& simulation = scene->ui.ecology;
        simulation.count_ = 1;
        auto& pose = simulation.actors_[0].pose;
        pose = {}; pose.speciesId = species; pose.x = x; pose.y = y;
        return pose;
    };
    // Opaque bridge pixels cover swimming wildlife, while flying bodies pass above the deck.
    terrain(48, 1);
    const int bridgeCell = 5 * ExploreMapGenerator::WIDTH + 7;
    scene->generatedMap.layers[2][bridgeCell] = 1627;
    auto canvasPixels = [&] {
        const auto fb = desktop.frameBuffer();
        return std::vector<uint16_t>(fb.pixels, fb.pixels + fb.width * fb.height);
    };
    PixelRenderer::clear(0xf81f);
    assert(GameAssets::drawExploreTile(1627, 7 * 26, 5 * 26, 0));
    const auto deck = canvasPixels();
    PixelRenderer::clear(0xf81f);
    const auto empty = canvasPixels();
    scene->ui.drawCover(scene->exploreRouteFrame(), 260, 7 * 26, 5 * 26, 8 * 26, 6 * 26,
        0, 0, layout, false, true);
    assert(canvasPixels() == deck && deck != empty);
    PixelRenderer::clear(0xf81f);
    scene->ui.drawCover(scene->exploreRouteFrame(), 260, 7 * 26, 5 * 26, 8 * 26, 6 * 26,
        0, 0, layout, true, false);
    assert(canvasPixels() == empty);
    // Both native upward-ladder tiles share their lower landing depth, including airborne actors.
    terrain(4501, 3);
    scene->generatedMap.layers[2][bridgeCell] = 4772;
    scene->generatedMap.layers[1][bridgeCell + ExploreMapGenerator::WIDTH] = 4773;
    PixelRenderer::clear(0xf81f);
    assert(GameAssets::drawExploreTile(4772, 7 * 26, 5 * 26, 0));
    assert(GameAssets::drawExploreTile(4773, 7 * 26, 6 * 26, 0));
    const auto ladder = canvasPixels();
    for (bool airborne : {false, true}) {
        PixelRenderer::clear(0xf81f);
        scene->ui.drawCover(scene->exploreRouteFrame(), 168, 7 * 26, 5 * 26, 8 * 26, 7 * 26,
            0, 0, layout, airborne, false);
        assert(canvasPixels() == ladder && ladder != empty);
        PixelRenderer::clear(0xf81f);
        scene->ui.drawCover(scene->exploreRouteFrame(), 169, 7 * 26, 5 * 26, 8 * 26, 7 * 26,
            0, 0, layout, airborne, false);
        assert(canvasPixels() == empty);
    }
    for (int cover = 0; cover < 4; ++cover) {
        terrain(cover == 1 ? 48 : cover == 3 ? 4501 : 390, cover == 3 ? 3 : 2);
        const int cell = 5 * ExploreMapGenerator::WIDTH + 7;
        if (cover == 0) scene->generatedMap.layers[1][cell] = 804;
        if (cover == 1) scene->generatedMap.layers[2][cell] = 1627;
        if (cover == 2) scene->generatedMap.layers[1][cell] = 391;
        if (cover == 3) {
            scene->generatedMap.layers[2][cell] = 4772;
            scene->generatedMap.layers[1][cell + ExploreMapGenerator::WIDTH] = 4773;
        }
        for (bool airborne : {false, true}) {
            auto& pose = poseAt(airborne ? 41 : cover == 1 ? 129 : 161, 195, 154);
            pose.airborne = airborne; pose.lift = airborne ? 12 : 0;
            pose.surface = cover == 1 ? ExploreEcology::Surface::WATER :
                cover == 3 ? ExploreEcology::Surface::ICE : ExploreEcology::Surface::GRASS;
            for (int offset : {-10, 10}) {
                pose.y = 156 + offset;
                capture("cover-" + std::to_string(cover) + (airborne ? "-air" : "-ground") + std::to_string(offset));
            }
        }
    }
    terrain(390, 2);
    auto& grassPose = poseAt(161, 195, 156);
    auto tick = [&] { desktop.advanceMs(90); scene->ui.updateRouteVisual(desktop.millis(), true); };
    tick(); grassPose.x += 12; tick();
    const int grassCell = int(grassPose.y / 26) * ExploreMapGenerator::WIDTH + int(grassPose.x / 26);
    assert(scene->ui.grass.frame(grassCell).phase);
    capture("grass-contact");
    const auto grassHeld = scene->ui.grass.frame(grassCell);
    desktop.advanceMs(10000); scene->ui.updateRouteVisual(desktop.millis(), false);
    assert(scene->ui.grass.frame(grassCell) == grassHeld);
    for (int i = 0; i < 10; ++i) tick();
    assert(!scene->ui.grass.frame(grassCell).phase);
    capture("grass-rest");
    // No lazy decode even when the wildlife artwork is absent.
    auto& missing = poseAt(65535, 195, 156); missing.activity = ExploreEcology::Activity::DOZE;
    const auto reloads = PokemonSprites::cacheStats().reloadCount;
    scene->ui.drawWildlife(scene->exploreRouteFrame(), 0, 0, 0, layout);
    assert(PokemonSprites::cacheStats().reloadCount == reloads);
    // A static child page may stop scheduling updates until the next input.
    scene->ui.updateRouteVisual(desktop.millis(), false);
    const auto beforeStall = poses();
    const auto heldGrass = scene->ui.grass.frame(grassCell);
    const auto elapsed = scene->ui.ecology.actors_[0].elapsedMs;
    const auto pending = scene->ui.ecology.actors_[0].pendingMs;
    desktop.advanceMs(10000);
    scene->ui.updateRouteVisual(desktop.millis(), true);
    samePoses(beforeStall);
    assert(scene->ui.ecology.actors_[0].elapsedMs == elapsed);
    assert(scene->ui.ecology.actors_[0].pendingMs == pending);
    assert(scene->ui.ecology.lastUpdateMs_ == desktop.millis());
    assert(scene->ui.grass.frame(grassCell) == heldGrass);
    engine.currentScene.reset(); scene.reset();
    std::printf("ecology-ui=ok areas=6 actors=%u\n", actors);
}
