#include "HomeScreen.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "game/ContactRoster.h"
#include "game/MonsterFactory.h"
#include "game/ItemInventory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/RenderCaches.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace AmoledV1;

void checkExpeditionTransitions(DesktopPlatform& desktop, Canvas565& canvas,
                               const Game::GameState& state);
void checkProgressionReplacement(DesktopPlatform& desktop,
                                 const Game::GameState& state);
#if STICKMON_ENABLE_DEBUG_FEATURES
void checkVisitorInvalidHostEntry(DesktopPlatform& desktop,
                                  const Game::GameState& state);
void checkVisitorDiagnostics(DesktopPlatform& desktop,
                             const Game::GameState& state);
#endif

bool runUiBehaviorCase(const char* name, Canvas565& canvas, Game::GameState& state);

static std::vector<uint16_t> pixels(AmoledUi::WIDTH * AmoledUi::HEIGHT);
static const char* outputDirectory = nullptr;

class CacheTestAllocator : public Platform::IMemoryAllocator {
public:
    bool fail = false;
    int allocations = 0;
    std::vector<void*> live;

    ~CacheTestAllocator() { assert(live.empty()); }
    void* allocate(size_t bytes, bool) override {
        if (fail) return nullptr;
        void* result = std::malloc(bytes);
        assert(result);
        ++allocations;
        live.push_back(result);
        return result;
    }
    void release(void* memory) override {
        auto found = std::find(live.begin(), live.end(), memory);
        assert(found != live.end());
        live.erase(found);
        std::free(memory);
    }
    size_t externalFree() const override { return 0; }
};

class CacheFailAllocator : public Platform::IMemoryAllocator {
public:
    explicit CacheFailAllocator(Platform::IMemoryAllocator& backing) : backing_(backing) {}
    int failures = 0;
    void* allocate(size_t bytes, bool external) override {
        if (bytes >= pixels.size() * sizeof(uint16_t)) {
            ++failures;
            return nullptr;
        }
        return backing_.allocate(bytes, external);
    }
    void release(void* memory) override { backing_.release(memory); }
    size_t externalFree() const override { return 0; }
private:
    Platform::IMemoryAllocator& backing_;
};

void checkCacheOwnership() {
    CacheTestAllocator first, second;
    const RenderCacheKey key{1, 2, 4, 3, false};
    {
        PixelCache565 cache;
        auto* buffer = cache.begin(key, first);
        assert(buffer && !cache.matches(key));
        std::fill(buffer, buffer + 12, 0x1234);
        cache.commit();
        assert(cache.matches(key));
        for (int field = 0; field < 5; ++field) {
            auto different = key;
            if (field == 0) ++different.identity;
            if (field == 1) ++different.variant;
            if (field == 2) ++different.width;
            if (field == 3) ++different.height;
            if (field == 4) different.byteSwapped = true;
            assert(!cache.matches(different));
        }
        uint16_t destination[12] = {};
        Canvas565 canvas;
        canvas.attach({destination, 4, 3, false});
        assert(cache.copyRowsTo(canvas, key, 1, 2));
        assert(destination[0] == 0 && destination[4] == 0x1234 && destination[8] == 0);
        canvas.attach({destination, 4, 3, true});
        assert(!cache.copyRowsTo(canvas, key, 0, 3));

        auto changed = key;
        ++changed.identity;
        assert(cache.begin(changed, first) == buffer);
        assert(first.allocations == 1 && !cache.matches(key) && !cache.matches(changed));
        cache.commit();
        assert(cache.matches(changed));
        cache.invalidate();
        assert(!cache.matches(changed));

        first.fail = true;
        ++changed.width;
        assert(!cache.begin(changed, first));
        assert(first.live.empty() && cache.allocatedBytes() == 0 && !cache.matches(key));
        first.fail = false;
        assert(cache.begin(key, first));
        cache.commit();
        assert(cache.begin(key, second));
        assert(first.live.empty() && second.live.size() == 1);
        cache.release();
        cache.release();
        assert(second.live.empty());
        assert(cache.begin(key, first));
    }
    assert(first.live.empty());  // Destruction releases through the allocating service.
    {
        RenderCaches caches;
        assert(caches.battleBackground.begin(key, first));
        assert(caches.exploreBackground.begin(key, first));
        assert(caches.exploreWorld.begin(key, first));
        caches.retainForScene(AppSceneFlow::Scene::BATTLE);
        assert(caches.battleBackground.data() && caches.exploreWorld.data());
        assert(!caches.exploreBackground.data());
        caches.retainForScene(AppSceneFlow::Scene::EXPLORE_MENU);
        assert(caches.battleBackground.data() && caches.exploreWorld.data());
        caches.retainForScene(AppSceneFlow::Scene::EXPLORE_ROUTE);
        assert(caches.battleBackground.data());
        caches.retainForScene(AppSceneFlow::Scene::HOME);
        assert(first.live.empty());
        assert(caches.exploreWorld.begin(key, first));
        caches.retainForScene(AppSceneFlow::Scene::EXPLORE_AREAS);
        assert(!caches.exploreWorld.data());
    }
}

void checkNativeAlphaBlend() {
    constexpr int width = 8;
    constexpr int height = 7;
    constexpr uint16_t color = 0xD3AE;
    for (bool swapped : {false, true}) {
        for (uint8_t alpha : {0U, 1U, 96U, 153U, 254U, 255U}) {
            std::vector<uint16_t> actual(width * height);
            std::vector<uint16_t> expected(width * height);
            for (size_t index = 0; index < actual.size(); ++index) {
                uint16_t value = static_cast<uint16_t>((index * 977 + 0x1234) & 0xFFFF);
                actual[index] = swapped
                    ? static_cast<uint16_t>((value << 8) | (value >> 8)) : value;
            }
            expected = actual;
            PixelRenderer::bind({actual.data(), width, height, swapped});
            Canvas565& canvas = PixelRenderer::canvas();
            canvas.setClipRect(2, 2, 4, 3);
            PixelRenderer::fillRectAlpha(-1, 1, 8, 5, color, alpha);
            for (int y = 2; y < 5; ++y) {
                for (int x = 2; x < 6; ++x) {
                    const size_t index = y * width + x;
                    uint16_t previous = expected[index];
                    if (swapped) previous = static_cast<uint16_t>(
                        (previous << 8) | (previous >> 8));
                    auto blendChannel = [alpha](int source, int destination,
                                                int maximum) {
                        const int src = source * 255 / maximum;
                        const int dst = destination * 255 / maximum;
                        return ((src * alpha + dst * (255 - alpha)) / 255) >>
                            (maximum == 63 ? 2 : 3);
                    };
                    // Match the old 8-bit blend followed by RGB565 truncation.
                    const int red = blendChannel((color >> 11) & 31,
                                                 (previous >> 11) & 31, 31);
                    const int green = blendChannel((color >> 5) & 63,
                                                   (previous >> 5) & 63, 63);
                    const int blue = blendChannel(color & 31, previous & 31, 31);
                    uint16_t result = static_cast<uint16_t>(
                        (red << 11) | (green << 5) | blue);
                    expected[index] = swapped
                        ? static_cast<uint16_t>((result << 8) | (result >> 8)) : result;
                }
            }
            assert(actual == expected);
        }
    }
}

template<class Model, class Render>
void checkPage(const char* name, const Model& model, Render render) {
    Canvas565& canvas = PixelRenderer::canvas();
    canvas.clearClipRect();
    std::fill(pixels.begin(), pixels.end(), 0xF81F);
    render(canvas, model, 0, AmoledUi::HEIGHT);
    const auto expected = pixels;
    if (outputDirectory) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/%s.ppm", outputDirectory, name);
        FILE* file = std::fopen(path, "wb");
        assert(file);
        std::fprintf(file, "P6\n368 448\n255\n");
        for (uint16_t color : expected) {
            const unsigned char rgb[] = {
                static_cast<unsigned char>(((color >> 11) & 31) * 255 / 31),
                static_cast<unsigned char>(((color >> 5) & 63) * 255 / 63),
                static_cast<unsigned char>((color & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, file);
        }
        std::fclose(file);
    }
    std::fill(pixels.begin(), pixels.end(), 0xF81F);
    for (int top = 0; top < AmoledUi::HEIGHT; top += 37) {
        const int bottom = std::min(top + 37, AmoledUi::HEIGHT);
        const auto before = pixels;
        render(canvas, model, top, bottom);
        for (size_t i = 0; i < pixels.size(); ++i) {
            const int row = i / AmoledUi::WIDTH;
            if ((row < top || row >= bottom) && pixels[i] != before[i]) {
                std::fprintf(stderr, "%s wrote outside dirty band [%d,%d) at row %d\n",
                             name, top, bottom, row);
                std::exit(1);
            }
        }
    }
    if (pixels != expected) {
        size_t mismatches = 0;
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (pixels[i] != expected[i]) {
                if (mismatches == 0) std::fprintf(stderr,
                    "%s first mismatch at (%zu,%zu)\n", name, i % 368, i / 368);
                ++mismatches;
            }
        }
        std::fprintf(stderr, "%s: %zu partial-render mismatches\n", name, mismatches);
        std::exit(1);
    }
    std::printf("%s: full and partial rendering agree\n", name);
}

void checkPageClip(Canvas565& shared) {
    std::vector<uint16_t> localPixels(pixels.size());
    Canvas565 local;
    local.attach({localPixels.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
    for (Canvas565* target : {&shared, &local}) {
        std::fill(pixels.begin(), pixels.end(), 0);
        std::fill(localPixels.begin(), localPixels.end(), 0);
        auto drawAndReturn = [&] {
            UiCommon::PageClip clip(*target, 50, 80);
            clip.setRect(10, 0, 20, 200);
            target->fillRect(0, 0, 368, 448, 0x1234);
            shared.fillRect(0, 0, 368, 448, 0x1234);
            assert(target->readPixel(10, 50) == 0x1234);
            assert(shared.readPixel(29, 79) == 0x1234);
            assert(target->readPixel(9, 60) == 0);
            assert(shared.readPixel(10, 49) == 0);
            assert(shared.readPixel(10, 80) == 0);
            clip.reset();
            target->drawPixel(2, 60, 0x5678);
            shared.drawPixel(2, 60, 0x5678);
            assert(target->readPixel(2, 60) == 0x5678);
            assert(shared.readPixel(2, 60) == 0x5678);
            return;  // The guard must clear clipping on early return as well.
        };
        drawAndReturn();
        target->drawPixel(2, 2, 0x5678);
        shared.drawPixel(2, 2, 0x5678);
        assert(target->readPixel(2, 2) == 0x5678);
        assert(shared.readPixel(2, 2) == 0x5678);
        {
            UiCommon::PageClip empty(*target, 90, 80);
            target->drawPixel(2, 2, 0);
            shared.drawPixel(2, 2, 0);
        }
        assert(target->readPixel(2, 2) == 0x5678);
        assert(shared.readPixel(2, 2) == 0x5678);
    }
}

int main(int argc, char** argv) {
    assert(argc >= 2);
    const char* selectedCase = nullptr;
    if (argc > 2 && std::strcmp(argv[2], "--case") == 0) {
        if (argc != 4) return 2;
        selectedCase = argv[3];
    } else if (argc > 2) {
        outputDirectory = argv[2];
    }
    if (selectedCase && std::strcmp(selectedCase, "cache-lifecycle") == 0) {
        checkCacheOwnership();
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "alpha-blend-native") == 0) {
        checkNativeAlphaBlend();
        return 0;
    }
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    desktop.begin();
    assert(Platform::resources().mount());
    FontResource::ins().begin();
    Canvas565& canvas = PixelRenderer::canvas();
    canvas.attach({pixels.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
    canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1);
    canvas.setAssetScale(1);
    canvas.setNativeText(true);
    if (selectedCase && std::strcmp(selectedCase, "page-clip") == 0) {
        checkPageClip(canvas);
        return 0;
    }
    RenderCaches caches;
    auto renderExploreScreenCached = [&](Canvas565& canvas, const ExploreViewModel& model,
                                  uint16_t top, uint16_t bottom) {
        renderExploreScreen(canvas, model, caches.exploreBackground, top, bottom);
    };
    auto renderExploreRouteScreenCached = [&](Canvas565& canvas, const ExploreRouteViewModel& model,
                                  uint16_t top, uint16_t bottom) {
        renderExploreRouteScreen(canvas, model, caches.exploreWorld, top, bottom);
    };
    auto renderBattleScreenCached = [&](Canvas565& canvas, const BattleViewModel& model,
                                  uint16_t top, uint16_t bottom) {
        renderBattleScreen(canvas, model, caches.battleBackground, top, bottom);
    };
    Game::GameState state{};
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    state.team[1] = Game::MonsterFactory::create(4, 5);
    state.storageCount = 2;
    state.storage[0] = state.team[0];
    state.storage[1] = state.team[1];

    if (selectedCase && std::strcmp(selectedCase, "battle-prewarm") == 0) {
        BattleViewModel model{};
        model.state = &state;
        model.battleBackground = GameAssets::Kind::BATTLE_BG_GRASS;
        model.playerSpeciesId = state.team[0].speciesId;
        model.wildSpeciesId = state.team[1].speciesId;
        model.playerLevel = model.wildLevel = 5;
        model.playerHp = model.wildHp = 100;
        std::fill(pixels.begin(), pixels.end(), 0x1234);
        renderBattleScreen(canvas, model, caches.battleBackground, 0, AmoledUi::HEIGHT);
        const auto coldPixels = pixels;
        caches.battleBackground.release();
        std::fill(pixels.begin(), pixels.end(), 0xF81F);
        assert(prepareBattleBackground(canvas, caches.battleBackground,
                                       model.battleBackground));
        caches.retainForScene(AppSceneFlow::Scene::EXPLORE_ROUTE);
        assert(caches.battleBackground.data());
        renderBattleScreen(canvas, model, caches.battleBackground, 0, AmoledUi::HEIGHT);
        assert(pixels == coldPixels);
        return 0;
    }

    if (selectedCase && std::strcmp(selectedCase, "expedition-transition") == 0) {
        checkExpeditionTransitions(desktop, canvas, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "progression-replace") == 0) {
        checkProgressionReplacement(desktop, state);
        return 0;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (selectedCase && std::strcmp(selectedCase, "visitor-invalid-entry") == 0) {
        checkVisitorInvalidHostEntry(desktop, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "visitor-diagnostics") == 0) {
        checkVisitorDiagnostics(desktop, state);
        return 0;
    }
#endif
    if (selectedCase) {
        if (runUiBehaviorCase(selectedCase, canvas, state)) return 0;
        std::fprintf(stderr, "unknown behavior case: %s\n", selectedCase);
        return 2;
    }

    HomeViewModel home{};
    home.moodHearts = 5;
    home.monsterCount = 2;
    home.monsters[0] = {70, 80};
    home.monsters[1] = {100, 60};
    checkPage("home", home, renderHomeScreen);
    checkPage("menu", MenuViewModel{}, renderMainMenu);
#if STICKMON_ENABLE_DEBUG_FEATURES
    DebugViewModel debug;
    checkPage("debug", debug, renderDebugScreen);
    debug.category = DebugViewModel::Category::TOUCH_TEST;
    TouchTest::State touchTest;
    debug.touchTest = &touchTest;
    checkPage("touch-test", debug, renderDebugScreen);
    for (auto point : TouchTest::TARGETS) {
        touchTest.down({point.x, std::min(447, point.y + 43)});
        touchTest.up({point.x + 2, std::min(447, point.y + 45)});
    }
    checkPage("touch-test-complete", debug, renderDebugScreen);
#endif
    ShopViewModel shop{};
    shop.state = &state;
    shop.dailyItemCount = Game::ShopService::buyItemCount(
        Game::ShopService::Category::DAILY, state);
    shop.exploreItemCount = Game::ShopService::buyItemCount(
        Game::ShopService::Category::EXPLORE, state);
    shop.itemCount = shop.dailyItemCount + shop.exploreItemCount;
    checkPage("shop", shop, renderShopScreen);
    shop.detailItem = Game::ItemId::POTION;
    checkPage("shop-detail", shop, renderShopScreen);
    // Visual fill and hit testing share the exact native rectangles.
    assert(canvas.readPixel(262, 420) == PixelRenderer::rgb(46, 37, 44));
    assert(itemConfirmChoiceAt(262, 420) == 1);
    assert(itemConfirmChoiceAt(192, 425) == 1);
    assert(itemConfirmChoiceAt(331, 425) == 1);
    assert(itemConfirmChoiceAt(332, 420) == -1);
    assert(itemConfirmChoiceAt(262, 426) == -1);
    assert(itemConfirmChoiceAt(367, 447) == -1);
    assert(itemConfirmChoiceAt(36, 354) == 0);
    assert(itemConfirmChoiceAt(175, 425) == 0);

    ItemListViewModel battleBag{};
    battleBag.state = &state;
    battleBag.mode = ItemListMode::BAG;
    battleBag.exploreOnly = true;
    battleBag.exploreItemCount =
        Game::ItemInventory::homeBagExploreItemCount(state);
    battleBag.itemCount = battleBag.exploreItemCount;
    checkPage("bag-explore-only", battleBag, renderItemListScreen);

    TeamViewModel team{}; team.state = &state;
    checkPage("team", team, renderTeamScreen);
    TeamMovesViewModel moves{}; moves.state = &state;
    checkPage("moves", moves, renderTeamMovesScreen);
    RoomMenuViewModel room{}; room.state = &state;
    checkPage("room-menu", room, renderRoomMenuScreen);
    RoomFoodViewModel food{}; food.state = &state;
    checkPage("food", food, renderRoomFoodScreen);
    CommunicationViewModel communication{};
    checkPage("communication", communication, renderCommunicationScreen);
    ComputerViewModel computer{}; computer.state = &state;
    checkPage("computer", computer, renderComputerScreen);
    computer.page = ComputerViewModel::Page::STORAGE;
    checkPage("storage", computer, renderComputerScreen);
    Game::GameState contactState = state;
    contactState.teamCount = 1;
    contactState.storage[1].level = 8;
    contactState.storage[1].bond = 52;
    computer.state = &contactState;
    computer.contactActionOpen = true;
    computer.contactActionSlot = 1;
    computer.contactCanDelete = true;
    checkPage("storage-action", computer, renderComputerScreen);
    computer.storageScroll = CONTACT_ROW_HEIGHT;
    checkPage("storage-scrolled-action", computer, renderComputerScreen);
    computer.storageScroll = 0.0f;
    computer.contactConfirmOpen = true;
    computer.contactConfirmYes = true;
    checkPage("storage-confirm", computer, renderComputerScreen);
    computer.contactConfirmOpen = false;
    computer.contactActionOpen = false;
    computer.state = &state;
    assert(ContactRoster::teamSlotForContact(state, 0) >= 0);
    int capacityPixels = 0;
    for (int y = 22; y < 54; ++y) {
        for (int x = 144; x < 224; ++x) {
            if (canvas.readPixel(x, y) == PixelRenderer::rgb(126, 175, 175)) {
                ++capacityPixels;
            }
        }
    }
    assert(capacityPixels > 0);
    assert(computerItemAt(200, 166, ComputerViewModel::Page::STORAGE,
                          CONTACT_ROW_HEIGHT, 20) == 2);
    checkPage("settings", SettingsViewModel{}, renderSettingsScreen);
    ProgressionViewModel progression{};
    progression.state = &state;
    progression.mode = ProgressionViewModel::Mode::LEVEL_UP;
    progression.teamSlot = 0;
    progression.oldLevel = 4;
    progression.level = 5;
    progression.levelUpElapsedMs = 0;
    checkPage("progression-level-up-entrance", progression,
              renderProgressionScreen);
    progression.levelUpElapsedMs = 800;
    checkPage("progression-level-up-ready", progression,
              renderProgressionScreen);
    assert(progressionItemAt(0, 0,
                             ProgressionViewModel::Mode::LEVEL_UP) == 0);
    assert(progressionItemAt(367, 447,
                             ProgressionViewModel::Mode::LEVEL_UP) == 0);
    assert(progressionItemAt(368, 447,
                             ProgressionViewModel::Mode::LEVEL_UP) == -1);
    assert(progressionItemAt(0, 448,
                             ProgressionViewModel::Mode::LEVEL_UP) == -1);
    progression.mode = ProgressionViewModel::Mode::EVOLUTION;
    progression.teamSlot = 0;
    progression.fromSpeciesId = 1;
    progression.toSpeciesId = 2;
    progression.evolutionPhase = Game::EvolutionSequence::Phase::INTRO;
    checkPage("progression-evolution-intro", progression,
              renderProgressionScreen);
    progression.evolutionCancelHoldProgress = 50;
    checkPage("progression-evolution-hold", progression,
              renderProgressionScreen);
    progression.evolutionCancelHoldProgress = 0;
    progression.evolutionPhase = Game::EvolutionSequence::Phase::MORPH;
    progression.evolutionElapsedMs =
        Game::EvolutionSequence::INTRO_MS + 500;
    checkPage("progression-evolution-morph", progression,
              renderProgressionScreen);
    progression.evolutionPhase = Game::EvolutionSequence::Phase::COMPLETE;
    progression.evolutionElapsedMs = Game::EvolutionSequence::COMPLETE_MS;
    progression.evolutionReady = true;
    checkPage("progression-evolution-complete", progression,
              renderProgressionScreen);
    progression.evolutionPhase = Game::EvolutionSequence::Phase::CANCELLED;
    progression.evolutionElapsedMs =
        Game::EvolutionSequence::CANCEL_COMPLETE_MS;
    progression.evolutionReady = false;
    checkPage("progression-evolution-cancelled", progression,
              renderProgressionScreen);
    progression.mode = ProgressionViewModel::Mode::MOVE_REPLACE;
    progression.moveId = 22;
    progression.oldMove2 = 33;
    progression.oldMove3 = 44;
    checkPage("progression-move-list", progression,
              renderProgressionScreen);
    assert(PixelRenderer::canvas().readPixel(328, 38) ==
           UiCommon::rgb(27, 43, 51));
    assert(progressionReplaceItemAt(20, 76, 0, 0xFF, 0) == 0);
    assert(progressionReplaceItemAt(20, 142, 0, 0xFF, 0) == 1);
    assert(progressionReplaceItemAt(20, 208, 0, 0xFF, 0) == 2);
    assert(progressionReplaceItemAt(328, 38, 0, 0xFF, 0) == -1);
    progression.selectedItem = 1;
    progression.detailProgress = 0.5f;
    checkPage("progression-move-detail-opening", progression,
              renderProgressionScreen);
    progression.detailProgress = 1.0f;
    checkPage("progression-move-detail-open", progression,
              renderProgressionScreen);
    assert(PixelRenderer::canvas().readPixel(328, 38) ==
           UiCommon::rgb(27, 62, 53));
    assert(progressionReplaceItemAt(20, 320, 0, 1, 1.0f) == -1);
    progression.selectedItem = 2;
    progression.scrollOffsetY = 50;
    checkPage("progression-move-last-detail", progression,
              renderProgressionScreen);
    assert(progressionReplaceItemAt(20, 158, 50, 2, 1.0f) == 2);
    assert(progressionItemAt(40, 348,
                             ProgressionViewModel::Mode::EVOLUTION) == -1);
    assert(progressionItemAt(184, 388,
                             ProgressionViewModel::Mode::EVOLUTION) == -1);
    assert(progressionItemAt(367, 447,
                             ProgressionViewModel::Mode::EVOLUTION) == -1);
    ShowerViewModel shower{}; shower.state = &state;
    checkPage("shower", shower, renderShowerScreen);
    BattleViewModel battle{}; battle.state = &state;
    battle.playerSpeciesId = 1; battle.wildSpeciesId = 4;
    checkPage("battle", battle, renderBattleScreenCached);
    battle.showPlayerExperience = true;
    battle.playerExperience = 50;
    checkPage("battle-exp", battle, renderBattleScreenCached);
    assert(canvas.readPixel(240, 294) == PixelRenderer::rgb(35, 118, 184));
    battle.showPlayerExperience = false;
    battle.wildSpeciesId = 278;
    battle.playerSpeciesId = 380;
    checkPage("battle-floating", battle, renderBattleScreenCached);
    ExploreViewModel explore{};
    explore.previewPool.count = 1;
    explore.previewFrames[0] = PokemonSprites::findSpeciesSprite(
        1, PokemonSprites::SpriteKind::FRONT);
    checkPage("explore", explore, renderExploreScreenCached);
    ExploreMapGenerator::Map map{};
    assert(ExploreMapGenerator::generate(42, ExploreMapGenerator::Edge::BOTTOM, 0, map));
    ExploreRouteViewModel route{};
    route.state = &state; route.map = &map;
    route.worldX = 92; route.worldY = 110;
    route.cameraX = 7; route.cameraY = 11;
    checkPage("route", route, renderExploreRouteScreenCached);

    room.toast = "Test";
    checkPage("room-toast", room, renderRoomMenuScreen);
    SettingsViewModel settings{};
    settings.state = &state;
    settings.pressedItem = 0;
    settings.toast = "Test";
    checkPage("settings-toast", settings, renderSettingsScreen);
    team.actionPopupOpen = true;
    checkPage("team-popup", team, renderTeamScreen);
    team.actionPopupSlot = 1;
    checkPage("team-popup-second", team, renderTeamScreen);
    TeamStatusViewModel status{};
    status.state = &state;
    checkPage("team-status", status, renderTeamStatusScreen);
    status.page = 2;
    checkPage("team-status-moves", status, renderTeamStatusScreen);
    route.toast = "Test";
    checkPage("route-toast", route, renderExploreRouteScreenCached);

    // Large cache allocations fail while normal resource reads remain available.
    auto& original = desktop.serviceBundle();
    CacheFailAllocator failing(original.memory);
    Platform::Services limited{
        original.lifecycle, original.clock, original.logger, original.input,
        original.display, original.audio, original.imu, original.power,
        original.blobs, original.resources, failing, original.peers};
    {
        RenderCaches emptyCaches;
        renderExploreRouteScreenCached(canvas, route, 0, AmoledUi::HEIGHT);
        const auto cachedRoute = pixels;
        Platform::bind(limited);
        checkPage("route-no-cache", route,
            [&](Canvas565& target, const ExploreRouteViewModel& model, uint16_t top, uint16_t bottom) {
                renderExploreRouteScreen(target, model, emptyCaches.exploreWorld, top, bottom);
            });
        assert(pixels == cachedRoute);
        checkPage("explore-no-cache", explore,
            [&](Canvas565& target, const ExploreViewModel& model, uint16_t top, uint16_t bottom) {
                renderExploreScreen(target, model, emptyCaches.exploreBackground, top, bottom);
            });
        checkPage("battle-no-cache", battle,
            [&](Canvas565& target, const BattleViewModel& model, uint16_t top, uint16_t bottom) {
                renderBattleScreen(target, model, emptyCaches.battleBackground, top, bottom);
            });
        assert(failing.failures > 0);
        assert(emptyCaches.exploreWorld.allocatedBytes() == 0);
        assert(emptyCaches.exploreBackground.allocatedBytes() == 0);
        assert(emptyCaches.battleBackground.allocatedBytes() == 0);
        Platform::bind(original);
    }

    assert(mainMenuItemAt(100, 350, 0) == 4);
    assert(roomMenuBackAt(40, 38));
    assert(roomMenuItemAt(200, 350) == -1);
    assert(roomFoodItemAt(200, 350) == 5);
    assert(showerMenuItemAt(320, 410) == 2);
    assert(showerMenuItemAt(364, 410) == -1);
    assert(teamMovesItemAt(200, 240, TeamMovesViewModel::Mode::MANAGE, 0) == 2);
    assert(teamMovesItemAt(200, 267, TeamMovesViewModel::Mode::MANAGE, 0) == 2);
    assert(teamMovesItemAt(200, 274, TeamMovesViewModel::Mode::MANAGE, 0) == -1);
    assert(teamMovesBackAt(79, 38));
    assert(!teamMovesBackAt(80, 38));
    assert(computerItemAt(200, 90, ComputerViewModel::Page::MENU) == 0);
    assert(computerItemAt(200, 176, ComputerViewModel::Page::MENU) == 1);
    assert(computerItemAt(200, 262, ComputerViewModel::Page::MENU) == -1);
    assert(computerItemAt(200, 348, ComputerViewModel::Page::MENU) == -1);
    assert(settingsItemAt(200, 90) == 0);
    assert(settingsItemAt(200, 338) == 4);
    assert(settingsItemAt(200, 400) == -1);
    assert(computerItemAt(0, 165, ComputerViewModel::Page::STORAGE, 0, 20) == 0);
    assert(computerItemAt(367, 166, ComputerViewModel::Page::STORAGE, 0, 20) == 1);
    assert(computerItemAt(200, 255, ComputerViewModel::Page::STORAGE, 0, 20) == 1);
    assert(computerItemAt(200, 256, ComputerViewModel::Page::STORAGE, 0, 20) == 2);
    std::puts("Native geometry and framebuffer checks passed");
}
