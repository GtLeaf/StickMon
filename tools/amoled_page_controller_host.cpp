#include "AmoledApp.h"
#include "core/FontResource.h"
#include "core/SaveManager.h"
#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "game/ShopService.h"
#include "game/Species.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

namespace AmoledV1 {
// Page tests initialize only durable game/scene state. All interactions use
// the same public touch, update and render entry points as the device.
struct AmoledPageTestAccess {
    static const Game::GameState& state(const AmoledApp& app) {
        return app.gameState;
    }
    static AppSceneFlow::Scene scene(const AmoledApp& app) {
        return app.sceneFlow.current();
    }
    static void enter(AmoledApp& app, AppSceneFlow::Scene scene) {
        app.sceneFlow.openSubScene(scene);
        app.forceFullRender();
    }
    static void recordEncounter(AmoledApp& app, uint16_t species) {
        assert(app.recordEncounteredSpecies(species));
    }
    static Game::MoveId startLevel(AmoledApp& app, uint32_t nowMs) {
        for (uint8_t index = 0; index < speciesCount(); ++index) {
            const auto& species = speciesTable()[index];
            for (uint16_t i = 0; i < learnsetEntryCountForSpecies(species); ++i) {
                const auto* entry = learnsetEntryForSpecies(species, i);
                if (!entry || entry->level <= 1 ||
                    !canLearnAsSpecialMove(species, entry->moveId)) continue;
                auto monster = Game::MonsterFactory::create(species.id, entry->level - 1);
                if (!monster.move2Id || !monster.move3Id ||
                    entry->moveId == monster.move2Id || entry->moveId == monster.move3Id) continue;
                unsigned newMoves = 0;
                for (uint16_t j = 0; j < learnsetEntryCountForSpecies(species); ++j) {
                    const auto* other = learnsetEntryForSpecies(species, j);
                    if (other && other->level == entry->level &&
                        canLearnAsSpecialMove(species, other->moveId) &&
                        other->moveId != monster.move2Id && other->moveId != monster.move3Id)
                        ++newMoves;
                }
                if (newMoves != 1) continue;
                monster.level = entry->level;
                if (levelUpEvolutionTarget(species, monster, app.gameState.gameMinutesTotal)) continue;
                monster.moveProficiency[1] = 70;
                monster.moveProficiency[2] = 80;
                app.gameState.team[0] = monster;
                app.openProgressionScene(AppSceneFlow::Scene::HOME, 0,
                                         entry->level - 1, nowMs);
                return entry->moveId;
            }
        }
        assert(false && "test species needs a special move before evolution");
        return 0;
    }
    static void startEvolution(AmoledApp& app, uint32_t nowMs) {
        app.gameState.team[0] = Game::MonsterFactory::create(133, 20);
        assert(Game::ItemInventory::add(app.gameState, Game::ItemId::WATER_STONE));
        app.openEvolutionProgression(AppSceneFlow::Scene::HOME, 0, 133, 134,
                                     nowMs, Game::ItemId::WATER_STONE);
    }
};
}  // namespace AmoledV1

namespace {
using namespace AmoledV1;
using Scene = AppSceneFlow::Scene;
using Frame = std::vector<uint16_t>;

class BlobSpy final : public Platform::IBlobStore {
public:
    explicit BlobSpy(Platform::IBlobStore& backing) : backing_(backing) {}
    bool initialize() override { return backing_.initialize(); }
    size_t blobSize(const char* ns, const char* key) override {
        return backing_.blobSize(ns, key);
    }
    bool readBlob(const char* ns, const char* key, void* out, size_t size) override {
        return backing_.readBlob(ns, key, out, size);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t size) override {
        ++writes;
        if (failWrites || (failKey && std::strcmp(key, failKey) == 0)) return false;
        return backing_.writeBlob(ns, key, data, size);
    }
    bool removeBlob(const char* ns, const char* key) override {
        ++removes;
        return backing_.removeBlob(ns, key);
    }
    bool clearNamespace(const char* ns) override {
        ++clears;
        return backing_.clearNamespace(ns);
    }
    bool failWrites = false;
    const char* failKey = nullptr;
    size_t writes = 0;
    size_t removes = 0;
    size_t clears = 0;
private:
    Platform::IBlobStore& backing_;
};

class PowerSpy final : public Platform::IPowerDevice {
public:
    explicit PowerSpy(Platform::IPowerDevice& backing) : backing_(backing) {}
    int batteryLevel() override { return backing_.batteryLevel(); }
    Platform::WakeReason wakeReason() const override { return backing_.wakeReason(); }
    uint32_t hardwareRandom() override { return backing_.hardwareRandom(); }
    size_t externalMemorySize() const override { return backing_.externalMemorySize(); }
    size_t externalMemoryFree() const override { return backing_.externalMemoryFree(); }
    void restart() override { ++restarts; }
    void enterDeepSleep(uint64_t, bool) override { ++sleeps; }
    size_t restarts = 0;
    size_t sleeps = 0;
private:
    Platform::IPowerDevice& backing_;
};

struct Fixture {
    explicit Fixture(const char* resources)
        : desktop(resources), blobs(desktop), power(desktop),
          services{desktop, desktop, desktop, desktop, desktop, desktop,
                   desktop, power, blobs, desktop, desktop, desktop},
          canvas(PixelRenderer::canvas()), pixels(AmoledUi::WIDTH * AmoledUi::HEIGHT) {
        Platform::bind(services);
        assert(desktop.begin());
        assert(Platform::resources().mount());
        FontResource::ins().begin();
        canvas.attach({pixels.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
        canvas.setCoordinateScale(1);
        canvas.setLayoutScale(1);
        canvas.setAssetScale(1);
        canvas.setNativeText(true);
        SaveManager manager;
        assert(manager.begin());
        Game::GameState seed;
        manager.reset(seed);
        seed.oobeDone = true;
        seed.teamCount = 2;
        seed.coins = 4321;
        seed.settings.brightness = 128;
        seed.settings.volume = 50;
        seed.settings.speedIndex = 0;
        seed.settings.idleTimeoutIndex = 4;
        seed.team[0] = Game::MonsterFactory::create(7, 20);
        seed.team[1] = Game::MonsterFactory::create(133, 20);
        seed.team[1].hpCur = seed.team[1].hpMax - 30;
        for (unsigned item = 0; item < static_cast<unsigned>(Game::ItemId::COUNT); ++item) {
            const auto id = static_cast<Game::ItemId>(item);
            Game::ItemInventory::remove(seed, id, Game::ItemInventory::count(seed, id));
        }
        assert(Game::ItemInventory::add(seed, Game::ItemId::POTION));
        MainSceneViewState view;
        assert(manager.saveSnapshot(seed, view));
        app.begin(desktop.millis());
        assert(app.startupStateLoaded());
        assert(scene() == Scene::HOME);
    }

    void enter(Scene target) {
        AmoledPageTestAccess::enter(app, target);
        render();
    }
    Scene scene() const { return AmoledPageTestAccess::scene(app); }
    const Game::GameState& state() const { return AmoledPageTestAccess::state(app); }
    Game::GameState saved() {
        SaveManager manager;
        Game::GameState result;
        MainSceneViewState view;
        assert(manager.begin() && manager.load(result, view));
        return result;
    }
    void render() {
        if (!app.needsRender()) return;
        const auto begin = app.renderRowBegin(), end = app.renderRowEnd();
        app.render(canvas);
        const Frame partial = pixels;
        app.forceFullRender();
        app.render(canvas);
        if (partial != pixels) {
            for (size_t i = 0; i < pixels.size(); ++i) {
                if (partial[i] == pixels[i]) continue;
                std::fprintf(stderr, "partial mismatch scene=%u time=%lu band=%u..%u first=%zu,%zu\n",
                    static_cast<unsigned>(scene()), static_cast<unsigned long>(desktop.millis()),
                    begin, end, i % AmoledUi::WIDTH, i / AmoledUi::WIDTH);
                break;
            }
        }
        assert(partial == pixels);
        app.markRendered();
    }
    void event(TouchEventType type, int x, int y) {
        desktop.advanceMs(16);
        app.handleTouch({type, static_cast<int16_t>(x), static_cast<int16_t>(y),
                         static_cast<int16_t>(x), static_cast<int16_t>(y), desktop.millis()});
        app.update(desktop.millis());
        render();
    }
    void tap(int x, int y) {
        event(TouchEventType::DOWN, x, y);
        event(TouchEventType::UP, x, y);
    }
    void cancel() {
        app.cancelTouch();
        app.update(desktop.millis());
        render();
    }
    void advance(uint32_t ms) {
        desktop.advanceMs(ms);
        app.update(desktop.millis());
        render();
    }

    DesktopPlatform desktop;
    BlobSpy blobs;
    PowerSpy power;
    Platform::Services services;
    Canvas565& canvas;
    Frame pixels;
    AmoledApp app;
};

void settingsSliders(Fixture& f) {
    f.enter(Scene::SETTINGS);
    assert(f.desktop.brightness() == 128 && f.desktop.volume() == 50);
    const auto writes = f.blobs.writes;
    f.event(TouchEventType::DOWN, 148, 108);
    f.event(TouchEventType::MOVE, 332, 108);
    assert(f.state().settings.brightness == 255 && f.desktop.brightness() == 255);
    assert(f.blobs.writes == writes && f.saved().settings.brightness == 128);
    f.cancel();
    f.event(TouchEventType::UP, 148, 108);
    assert(f.blobs.writes == writes && f.saved().settings.brightness == 128);
    f.event(TouchEventType::DOWN, 148, 108);
    f.event(TouchEventType::MOVE, 332, 108);
    f.event(TouchEventType::UP, 332, 108);
    assert(f.blobs.writes > writes && f.saved().settings.brightness == 255);

    const auto volumeWrites = f.blobs.writes;
    f.event(TouchEventType::DOWN, 332, 172);
    f.event(TouchEventType::MOVE, 148, 172);
    assert(f.state().settings.volume == 0 && f.desktop.volume() == 0);
    assert(f.blobs.writes == volumeWrites && f.saved().settings.volume == 50);
    f.event(TouchEventType::UP, 148, 172);
    assert(f.saved().settings.volume == 0);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    AmoledApp rebooted;
    rebooted.begin(f.desktop.millis());
    assert(rebooted.startupStateLoaded());
    assert(f.desktop.brightness() == 255 && f.desktop.volume() == 0);
    std::puts("settings: slider preview, cancellation, release save and startup restore verified");
}

void settingsActions(Fixture& f) {
    f.enter(Scene::SETTINGS);
    for (uint8_t speed = 1; speed <= 4; ++speed) {
        f.tap(240, 234);
        assert(f.state().settings.speedIndex == speed % 4);
        assert(f.saved().settings.speedIndex == speed % 4);
    }
    for (uint8_t idle = 0; idle < 5; ++idle) {
        f.tap(240, 294);
        assert(f.state().settings.idleTimeoutIndex == idle);
        assert(f.saved().settings.idleTimeoutIndex == idle);
    }
    const auto writes = f.blobs.writes;
    f.event(TouchEventType::DOWN, 240, 234);
    f.cancel();
    f.event(TouchEventType::UP, 240, 234);
    assert(f.state().settings.speedIndex == 0 && f.blobs.writes == writes);
    f.event(TouchEventType::DOWN, 240, 294);
    assert(f.app.prepareDisplayLock(f.desktop.millis()));
    assert(f.scene() == Scene::HOME);
    f.event(TouchEventType::UP, 240, 294);
    assert(f.state().settings.idleTimeoutIndex == 4);
    f.enter(Scene::SETTINGS);
    f.tap(240, 234);
    assert(f.state().settings.speedIndex == 1);
    std::puts("settings: speed/power cycles persist; cancelled and exited gestures cannot activate rows");
}

void settingsReset(Fixture& f) {
    f.enter(Scene::SETTINGS);
    const auto writes = f.blobs.writes;
    const auto list = f.pixels;
    f.tap(240, 354);
    const auto confirmation = f.pixels;
    assert(confirmation != list && f.blobs.writes == writes);
    f.event(TouchEventType::UP, 262, 390);
    assert(f.pixels == confirmation);
    f.event(TouchEventType::DOWN, 262, 390);
    f.cancel();
    f.event(TouchEventType::UP, 262, 390);
    assert(f.pixels == confirmation);
    f.event(TouchEventType::DOWN, 106, 390);
    f.event(TouchEventType::UP, 262, 390);
    assert(f.pixels == confirmation);
    assert(f.power.restarts == 0 && f.blobs.clears == 0 && f.blobs.writes == writes);
    assert(f.saved().coins == 4321 && f.state().coins == 4321);
    f.tap(106, 390);
    assert(f.scene() == Scene::SETTINGS && f.pixels == list);
    f.tap(240, 354);
    f.tap(40, 38);
    assert(f.scene() == Scene::SETTINGS && f.pixels == list);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    std::puts("settings: reset requires explicit confirmation; orphan, cancelled and cross-button releases rejected");
}

void teamNavigation(Fixture& f) {
    f.enter(Scene::TEAM);
    const auto roster = f.pixels;
    f.tap(100, 120);
    assert(f.pixels != roster);
    f.tap(282, 194);
    const auto status = f.pixels;
    f.event(TouchEventType::DOWN, 300, 300);
    f.event(TouchEventType::MOVE, 160, 300);
    f.cancel();
    f.event(TouchEventType::UP, 160, 300);
    assert(f.pixels == status);
    f.event(TouchEventType::DOWN, 300, 300);
    f.event(TouchEventType::MOVE, 160, 300);
    f.event(TouchEventType::UP, 160, 300);
    f.advance(200);
    assert(f.pixels != status);
    f.event(TouchEventType::DOWN, 160, 300);
    f.event(TouchEventType::MOVE, 300, 300);
    f.event(TouchEventType::UP, 300, 300);
    f.advance(200);
    assert(f.pixels == status);
    f.tap(40, 38);
    assert(f.scene() == Scene::TEAM && f.pixels == roster);
    f.tap(100, 120);
    f.tap(282, 250);
    assert(f.pixels != roster);
    const auto moves = f.pixels;
    const auto move = f.state().team[0].move1Id;
    f.tap(160, 106);
    f.advance(260);
    assert(f.scene() == Scene::TEAM && f.pixels != moves);
    assert(f.state().team[0].move1Id == move);
    f.tap(160, 106);
    f.advance(260);
    assert(f.pixels == moves);
    f.tap(40, 38);
    assert(f.scene() == Scene::TEAM && f.pixels == roster);
    f.event(TouchEventType::DOWN, 100, 120);
    f.cancel();
    f.event(TouchEventType::UP, 100, 120);
    assert(f.pixels == roster);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    assert(f.state().team[0].speciesId == 7 && f.state().team[1].speciesId == 133);
    std::puts("team: status/moves return to roster; cancelled swipes and presses preserve team");
}

void teamLeader(Fixture& f) {
    f.enter(Scene::TEAM);
    f.tap(100, 120);
    f.tap(282, 194);
    const auto firstStatus = f.pixels;
    f.tap(40, 38);
    f.tap(100, 270);
    f.tap(282, 298);
    assert(f.pixels != firstStatus);
    assert(f.state().team[0].speciesId == 7 && f.state().team[1].speciesId == 133);
    f.tap(40, 38);
    f.tap(100, 270);
    f.tap(282, 242);
    assert(f.state().team[0].speciesId == 133 && f.state().team[1].speciesId == 7);
    assert(f.saved().team[0].speciesId == 133 && f.saved().team[1].speciesId == 7);
    std::puts("team: status opens selected member; front action reorders and persists selected member");
}

void bagNavigation(Fixture& f) {
    f.enter(Scene::BAG);
    const auto bag = f.pixels;
    // Only one exploration item exists; the empty daily section remains above it.
    f.tap(64, 240);
    assert(f.pixels != bag);
    f.tap(106, 390);
    assert(f.scene() == Scene::TEAM);
    f.tap(40, 38);
    assert(f.scene() == Scene::BAG && f.pixels == bag);
    assert(Game::ItemInventory::count(f.state(), Game::ItemId::POTION) == 1);
    f.event(TouchEventType::DOWN, 64, 240);
    f.cancel();
    f.event(TouchEventType::UP, 64, 240);
    assert(f.pixels == bag);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    std::puts("bag: target-selection back restores bag; cancelled presses do not open details or consume stock");
}

void bagUse(Fixture& f) {
    f.enter(Scene::BAG);
    const auto firstHp = f.state().team[0].hpCur;
    const auto targetHp = f.state().team[1].hpCur;
    f.tap(64, 240);
    f.tap(106, 390);
    assert(f.scene() == Scene::TEAM);
    f.tap(100, 270);
    assert(f.state().team[0].hpCur == firstHp);
    assert(f.state().team[1].hpCur == targetHp + 20);
    assert(Game::ItemInventory::count(f.state(), Game::ItemId::POTION) == 0);
    assert(f.saved().team[1].hpCur == targetHp + 20);
    f.advance(1200);
    assert(f.scene() == Scene::BAG);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    assert(f.state().team[0].speciesId == 7 && f.state().team[1].speciesId == 133);
    std::puts("bag: target selection heals only selected member, consumes and saves once, then returns through bag");
}

void shopTransactions(Fixture& f) {
    f.enter(Scene::SHOP);
    f.tap(50, 230);
    f.tap(180, 174);
    const auto detail = f.pixels;
    const auto coins = f.state().coins;
    const auto writes = f.blobs.writes;
    f.event(TouchEventType::DOWN, 106, 390);
    f.cancel();
    f.event(TouchEventType::UP, 106, 390);
    assert(f.state().coins == coins && f.blobs.writes == writes && f.pixels == detail);
    f.event(TouchEventType::DOWN, 106, 390);
    f.event(TouchEventType::UP, 262, 390);
    assert(f.state().coins == coins && f.blobs.writes == writes);
    f.tap(106, 390);
    assert(Game::ItemInventory::count(f.state(), Game::ItemId::POTION) == 0);
    assert(f.state().coins == coins + Game::ShopService::sellPrice(Game::ItemId::POTION));
    assert(f.saved().coins == f.state().coins);
    assert(Game::ItemInventory::count(f.saved(), Game::ItemId::POTION) == 0);
    const auto soldWrites = f.blobs.writes;
    const auto disabled = f.pixels;
    assert(f.canvas.readPixel(106, 418) == PixelRenderer::rgb(35, 35, 35));
    f.tap(106, 390);
    assert(f.blobs.writes == soldWrites && f.pixels == disabled);
    f.tap(262, 390);
    assert(f.scene() == Scene::SHOP && f.pixels != disabled);
    f.tap(40, 38);
    assert(f.scene() == Scene::HOME);
    std::puts("shop: one sale saves once; stock-zero action stays disabled; cancelled/cross-button touches cannot transact");
}

void saveRetry(Fixture& f) {
    f.enter(Scene::SETTINGS);
    const auto savedSpeed = f.saved().settings.speedIndex;
    f.blobs.failWrites = true;
    f.tap(240, 234);
    assert(f.state().settings.speedIndex != savedSpeed);
    assert(f.saved().settings.speedIndex == savedSpeed);
    const auto failedWrites = f.blobs.writes;
    f.advance(1000);
    assert(f.blobs.writes == failedWrites);
    f.advance(1100);
    assert(f.blobs.writes > failedWrites);
    const auto secondFailure = f.blobs.writes;
    f.blobs.failWrites = false;
    // A pending save must wait until a gesture ends, even after it is due.
    f.event(TouchEventType::DOWN, 240, 300);
    f.advance(2200);
    assert(f.blobs.writes == secondFailure);
    f.cancel();
    assert(f.saved().settings.speedIndex == f.state().settings.speedIndex);
    const auto committed = f.blobs.writes;
    f.advance(3000);
    assert(f.blobs.writes == committed);
    // A newly started App sees the committed result after the retry.
    AmoledApp restored;
    restored.begin(f.desktop.millis());
    assert(AmoledPageTestAccess::state(restored).settings.speedIndex ==
           f.state().settings.speedIndex);
    std::puts("save retry: failed writes stay pending, retries are bounded, gestures defer writes and success survives restart");
}

void historyRetry(Fixture& f) {
    f.enter(Scene::SETTINGS);
    AmoledPageTestAccess::recordEncounter(f.app, 25);
    f.blobs.failKey = "encounters";
    f.tap(240, 234);
    // Snapshot succeeds while the independent history blob fails.
    assert(f.saved().settings.speedIndex == f.state().settings.speedIndex);
    SaveManager manager;
    assert(manager.begin());
    Game::EncounterHistory history;
    manager.loadEncounterHistory(history);
    assert(!history.contains(25));
    const auto failedWrites = f.blobs.writes;
    f.advance(1000);
    assert(f.blobs.writes == failedWrites);
    f.blobs.failKey = nullptr;
    f.advance(1100);
    assert(f.blobs.writes > failedWrites);
    assert(manager.loadEncounterHistory(history) && history.contains(25));
    const auto committed = f.blobs.writes;
    f.advance(3000);
    assert(f.blobs.writes == committed);
    std::puts("save retry: history-only failure stays pending after a successful snapshot and commits on retry");
}

void progressionGestures(Fixture& f, int selection) {
    const auto learnedMove = AmoledPageTestAccess::startLevel(f.app, f.desktop.millis());
    const auto oldMove2 = f.state().team[0].move2Id;
    const auto oldMove3 = f.state().team[0].move3Id;
    f.render();
    const auto levelFrame = f.pixels;
    // A press begun during the entrance cannot advance when released later.
    f.event(TouchEventType::DOWN, 180, 240);
    f.advance(850);
    f.event(TouchEventType::UP, 180, 240);
    assert(f.scene() == Scene::PROGRESSION);
    f.tap(180, 240);
    assert(f.pixels != levelFrame);
    f.tap(328, 38);  // Confirmation without a choice is inert.
    assert(f.state().team[0].move2Id == oldMove2 && f.state().team[0].move3Id == oldMove3);
    f.event(TouchEventType::DOWN, 160, 106 + selection * 66);
    f.cancel();
    f.event(TouchEventType::UP, 160, 106 + selection * 66);
    f.tap(328, 38);
    assert(f.state().team[0].move2Id == oldMove2 && f.state().team[0].move3Id == oldMove3);
    f.tap(160, 106 + selection * 66);
    assert(f.state().team[0].move2Id == oldMove2 && f.state().team[0].move3Id == oldMove3);
    f.tap(328, 38);
    assert(f.scene() == Scene::HOME);
    assert(f.state().team[0].move2Id == (selection == 1 ? learnedMove : oldMove2));
    assert(f.state().team[0].move3Id == (selection == 2 ? learnedMove : oldMove3));
    assert(f.state().team[0].moveProficiency[1] == (selection == 1 ? 0 : 70));
    assert(f.state().team[0].moveProficiency[2] == (selection == 2 ? 0 : 80));
    assert(f.saved().team[0].move2Id == f.state().team[0].move2Id);
    assert(f.saved().team[0].move3Id == f.state().team[0].move3Id);
    std::puts("growth: entrance gate, cancelled press, explicit skip/replacement and proficiency/save verified");
}

void evolutionGestures(Fixture& f, bool cancel) {
    AmoledPageTestAccess::startEvolution(f.app, f.desktop.millis());
    f.render();
    assert(f.scene() == Scene::PROGRESSION);
    const auto checkUncommitted = [&] {
        assert(f.state().team[0].speciesId == 133);
        assert(Game::ItemInventory::count(f.state(), Game::ItemId::WATER_STONE) == 1);
    };
    checkUncommitted();
    f.event(TouchEventType::DOWN, 180, 240);
    f.advance(cancel ? 610 : 200);
    // Movement does not cancel an evolution hold; a short tap remains consumed.
    f.event(TouchEventType::MOVE, 280, 340);
    f.event(TouchEventType::UP, 280, 340);
    checkUncommitted();
    f.advance(cancel ? Game::EvolutionSequence::CANCEL_COMPLETE_MS
                     : Game::EvolutionSequence::COMPLETE_MS);
    checkUncommitted();
    f.tap(180, 240);
    assert(f.scene() == Scene::HOME);
    assert(f.state().team[0].speciesId == (cancel ? 133 : 134));
    assert(Game::ItemInventory::count(f.state(), Game::ItemId::WATER_STONE) == (cancel ? 1 : 0));
    assert(f.saved().team[0].speciesId == f.state().team[0].speciesId);
    const auto committed = f.blobs.writes;
    f.event(TouchEventType::UP, 180, 240);
    f.advance(3000);
    assert(f.blobs.writes == committed);
    std::puts("evolution: consumed gestures, hold cancellation, delayed commit and saved stone/species verified");
}

void pageExitGestures(Fixture& f) {
    struct PagePress { Scene scene; int x; int y; };
    for (const auto& page : {PagePress{Scene::TEAM, 100, 120},
                             PagePress{Scene::BAG, 64, 240},
                             PagePress{Scene::SHOP, 106, 390},
                             PagePress{Scene::SETTINGS, 240, 234}}) {
        f.enter(page.scene);
        if (page.scene == Scene::SHOP) {
            f.tap(50, 230);
            f.tap(180, 174);
        }
        f.event(TouchEventType::DOWN, page.x, page.y);
        assert(f.app.prepareDisplayLock(f.desktop.millis()));
        assert(f.scene() == Scene::HOME);
        f.event(TouchEventType::UP, page.x, page.y);
        assert(f.scene() == Scene::HOME);
        assert(f.state().coins == 4321);
        assert(f.state().settings.speedIndex == 0);
        assert(f.state().team[0].speciesId == 7);
        assert(Game::ItemInventory::count(f.state(), Game::ItemId::POTION) == 1);
    }
    assert(f.blobs.clears == 0 && f.power.restarts == 0);
    std::puts("page exit: lock lifecycle consumes gestures from settings, team, bag and shop");
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 3);
    Fixture fixture(argv[1]);
    if (std::strcmp(argv[2], "settings-sliders") == 0) settingsSliders(fixture);
    else if (std::strcmp(argv[2], "settings-actions") == 0) settingsActions(fixture);
    else if (std::strcmp(argv[2], "settings-reset") == 0) settingsReset(fixture);
    else if (std::strcmp(argv[2], "team-navigation") == 0) teamNavigation(fixture);
    else if (std::strcmp(argv[2], "team-leader") == 0) teamLeader(fixture);
    else if (std::strcmp(argv[2], "bag-navigation") == 0) bagNavigation(fixture);
    else if (std::strcmp(argv[2], "bag-use") == 0) bagUse(fixture);
    else if (std::strcmp(argv[2], "shop-transactions") == 0) shopTransactions(fixture);
    else if (std::strcmp(argv[2], "save-retry") == 0) saveRetry(fixture);
    else if (std::strcmp(argv[2], "history-retry") == 0) historyRetry(fixture);
    else if (std::strcmp(argv[2], "progression-skip") == 0) progressionGestures(fixture, 0);
    else if (std::strcmp(argv[2], "progression-replace-2") == 0) progressionGestures(fixture, 1);
    else if (std::strcmp(argv[2], "progression-replace-3") == 0) progressionGestures(fixture, 2);
    else if (std::strcmp(argv[2], "evolution-cancel") == 0) evolutionGestures(fixture, true);
    else if (std::strcmp(argv[2], "evolution-commit") == 0) evolutionGestures(fixture, false);
    else if (std::strcmp(argv[2], "page-exit-gestures") == 0) pageExitGestures(fixture);
    else return 2;
    return 0;
}
