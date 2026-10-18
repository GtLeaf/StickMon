#include "HomeScreen.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "core/RoomRenderer.h"
#include "core/UiStrings.h"
#include "core/SaveManager.h"
#include "game/ContactRoster.h"
#include "game/MonsterFactory.h"
#include "game/ItemInventory.h"
#include "game/ExploreTileSemantics.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/RenderCaches.h"
#include "ui/ExploreMapRenderer.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace AmoledV1;

void checkShowerFlow(DesktopPlatform& desktop, Canvas565& canvas,
                     const Game::GameState& state);
void checkShowerGrowthRoundTrip(DesktopPlatform& desktop, Canvas565& canvas);

void checkLockFocusPixels(DesktopPlatform& desktop, Canvas565& canvas,
                         const Game::GameState& state);
void checkExpeditionTransitions(DesktopPlatform& desktop, Canvas565& canvas,
                               const Game::GameState& state);
void checkExploreReserveEncounters(DesktopPlatform& desktop,
                                   const Game::GameState& state);
void checkExploreMotionDamage(DesktopPlatform& desktop, Canvas565& canvas,
                             const Game::GameState& state);
void checkExploreParkRoutes(DesktopPlatform& desktop, const Game::GameState& state);
void checkExploreSaveBoundaries(DesktopPlatform& desktop,
                                const Game::GameState& state);
void checkProgressionReplacement(DesktopPlatform& desktop,
                                 const Game::GameState& state);
void checkSettingsReset(DesktopPlatform& desktop);
void checkBatteryDisplay(DesktopPlatform& desktop, Canvas565& canvas);
void checkExploreEcology(DesktopPlatform& desktop, Canvas565& canvas);
void checkExploreWater(DesktopPlatform& desktop, Canvas565& canvas);
void checkHatchFlow(DesktopPlatform& desktop, Canvas565& canvas);
void checkBathHatchControllerEffects();
void checkBattleAssetActions(DesktopPlatform& desktop, const Game::GameState& state);
void checkSharedBattleExecution(DesktopPlatform& desktop, const Game::GameState& state);
void checkMedicineFlow(DesktopPlatform& desktop, Canvas565& canvas,
                        const Game::GameState& state);
#if STICKMON_ENABLE_DEBUG_FEATURES
void checkBattleDiagnosticsSaveIsolation(DesktopPlatform& desktop,
                                         const Game::GameState& state);
void checkVisitorInvalidHostEntry(DesktopPlatform& desktop,
                                  const Game::GameState& state);
void checkVisitorDiagnostics(DesktopPlatform& desktop,
                             const Game::GameState& state);
#endif

bool runUiBehaviorCase(const char* name, Canvas565& canvas, Game::GameState& state);

static std::vector<uint16_t> pixels(AmoledUi::WIDTH * AmoledUi::HEIGHT);
static const char* outputDirectory = nullptr;
void checkHomeCareRewards(DesktopPlatform& desktop, Canvas565& canvas);
void checkHomeRoomAuthority(DesktopPlatform& desktop, Canvas565& canvas);
void checkHomeAutomaticFoodTurns(DesktopPlatform& desktop, Canvas565& canvas);
void checkHomeFoodExitNoStall(DesktopPlatform& desktop, Canvas565& canvas);
void checkHomeFaintRecoveryAndSleep(DesktopPlatform& desktop, Canvas565& canvas);
void checkHomeSavedTurns(DesktopPlatform& desktop);
void checkHomeYieldAndActions(DesktopPlatform& desktop, Canvas565& canvas);
void checkLockedRoomPower(DesktopPlatform& desktop, Canvas565& canvas);

class RoomRenderPower final : public Platform::IPowerDevice {
public:
    explicit RoomRenderPower(Platform::IPowerDevice& backing) : backing_(backing) {}
    int batteryLevel() override { return backing_.batteryLevel(); }
    Platform::WakeReason wakeReason() const override { return backing_.wakeReason(); }
    uint32_t hardwareRandom() override { return backing_.hardwareRandom(); }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return externalMemorySize(); }
    void restart() override { backing_.restart(); }
    void enterDeepSleep(uint64_t timer, bool secondary) override {
        backing_.enterDeepSleep(timer, secondary);
    }
private:
    Platform::IPowerDevice& backing_;
};

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

template<class Render>
void checkBattleFxRendering(const Game::GameState& state, Render render) {
    Canvas565& canvas = PixelRenderer::canvas();
    static constexpr const char* FX_NAMES[] = {"hit", "fire", "water", "electric",
        "scratch", "bite", "leer", "growl", "sand"};
    static constexpr Game::MoveId MOVE_IDS[] = {33, 52, 55, 84, 10, 44, 43, 45, 28};
    static constexpr uint16_t ANIMATION_ROW_END = 384;
    for (uint8_t fxIndex = 0; fxIndex < static_cast<uint8_t>(GameAssets::BattleFx::COUNT); ++fxIndex) {
        const auto fx = static_cast<GameAssets::BattleFx>(fxIndex);
        for (uint8_t fxFrame = 0; fxFrame < 4; ++fxFrame) {
            canvas.clearClipRect();
            std::fill(pixels.begin(), pixels.end(), 0xF81F);
            assert(GameAssets::drawCentered(
                GameAssets::battleFxKind(fx, fxFrame), 180, 180));
            const auto visiblePixels = std::count_if(
                pixels.begin(), pixels.end(), [](uint16_t pixel) {
                    return pixel != 0xF81F;
                });
            assert(visiblePixels > 0);
            std::printf("battle-fx-%s asset %u: %td visible pixels\n",
                        FX_NAMES[fxIndex], fxFrame, visiblePixels);
        }
        for (bool attackerWild : {false, true}) {
            BattleViewModel model{};
            model.state = &state;
            model.playerSpeciesId = state.team[0].speciesId;
            model.wildSpeciesId = state.team[1].speciesId;
            model.playerLevel = model.wildLevel = 5;
            model.playerHp = model.wildHp = 100;
            model.animationAttackerWild = attackerWild;
            model.animationHit = !GameAssets::battleFxIsStatus(fx);
            model.animationStatusEffect = GameAssets::battleFxIsStatus(fx);
            model.animationFx = fx;
            const Species* attacker = findSpecies(
                attackerWild ? model.wildSpeciesId : model.playerSpeciesId);
            const MoveInfo* move = findMove(MOVE_IDS[fxIndex]);
            assert(attacker && move);
            assert(GameAssets::battleFxForMove(move->id, move->type) == fx);
            char attackLog[64];
            std::snprintf(attackLog, sizeof(attackLog), "%s / %s",
                          attacker->name, move->name);
            model.logLines[0] = attackLog;
            model.logCount = 1;
            std::vector<uint16_t> idle;
            std::vector<uint16_t> previous;
            for (uint8_t frame = 0; frame <= 7; ++frame) {
                model.animationActive = frame > 0 && frame < 7;
                model.animationFrame = frame < 7 ? frame : 0;
                char name[80];
                std::snprintf(name, sizeof(name), "battle-fx-%s-%s-%u",
                              FX_NAMES[fxIndex], attackerWild ? "wild" : "player",
                              frame);
                checkPage(name, model, render);
                const auto expected = pixels;
                if (frame == 0) idle = expected;
                if (frame == 7) assert(expected == idle);
                if (!previous.empty()) {
                    pixels = previous;
                    render(canvas, model, 0, ANIMATION_ROW_END);
                    if (pixels != expected) {
                        for (size_t index = 0; index < pixels.size(); ++index) {
                            if (pixels[index] != expected[index]) {
                                std::fprintf(stderr,
                                    "%s animation dirty rows mismatch at (%zu,%zu)\n",
                                    name, index % AmoledUi::WIDTH,
                                    index / AmoledUi::WIDTH);
                                std::exit(1);
                            }
                        }
                    }
                    assert(std::equal(
                        previous.begin() + ANIMATION_ROW_END * AmoledUi::WIDTH,
                        previous.end(), pixels.begin() +
                            ANIMATION_ROW_END * AmoledUi::WIDTH));
                }
                if (frame >= 2 && frame <= 5) {
                    auto withoutFx = model;
                    withoutFx.animationHit = false;
                    withoutFx.animationStatusEffect = false;
                    render(canvas, withoutFx, 0, AmoledUi::HEIGHT);
                    assert(pixels != expected);
                    if (GameAssets::battleFxIsStatus(fx)) assert(pixels == idle);
                    pixels = expected;
                }
                previous = expected;
            }
        }
    }
    std::puts("battle-fx: 72 key frames and 126 animation dirty-row transitions agree");
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
        if (argc < 4 || argc > 5) return 2;
        selectedCase = argv[3];
        if (argc == 5) outputDirectory = argv[4];
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
    if (selectedCase && std::strcmp(selectedCase, "hatch-flow") == 0) {
        checkHatchFlow(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "bath-hatch-controller-effects") == 0) {
        checkBathHatchControllerEffects();
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "hatch-render") == 0) {
        auto& original = desktop.serviceBundle();
        RoomRenderPower power(original.power);
        Platform::Services withPsram{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, power, original.blobs,
            original.resources, original.memory, original.peers};
        Platform::bind(withPsram);
        if (!RoomResource::ins().begin()) {
            std::fputs(desktop.logs().c_str(), stderr);
            assert(false && "hatch render needs the real room resource");
        }
        assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, false, 2));
        assert(hatchMenuAt(80, 344) && hatchMenuAt(159, 447));
        assert(!hatchMenuAt(79, 344) && !hatchMenuAt(160, 396));
        auto renderEgg = [](Canvas565& target, const HatchViewModel& model,
                            uint16_t begin, uint16_t end) {
            renderHatchScreen(target, model, begin, end);
        };
        AmoledUi::Rect bounds{};
        HatchViewModel baseline;
        renderHatchScreen(canvas, baseline, 0, 448, &bounds);
        assert(bounds.contains(184, 200));
        assert(!bounds.contains(120, 396));
        for (bool night : {false, true}) {
            for (int progress : {0, 50, 99, 100}) {
                HatchViewModel egg;
                egg.night = night;
                egg.remainingSeconds = 180 * (100 - progress) / 100;
                egg.shakeOffset = progress % 7 - 3;
                egg.showHeart = progress == 50 || progress == 99;
                char name[48];
                std::snprintf(name, sizeof(name), "hatch-%s-%d", night ? "night" : "day", progress);
                checkPage(name, egg, renderEgg);
            }
        }
        HatchViewModel error;
        error.remainingSeconds = 0;
        error.toast = Ui::Hatch::SAVE_FAILED;
        checkPage("hatch-save-failed", error, renderEgg);
        Platform::bind(original);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "pet-heart-layout") == 0) {
        auto& original = desktop.serviceBundle();
        RoomRenderPower power(original.power);
        Platform::Services withPsram{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, power, original.blobs,
            original.resources, original.memory, original.peers};
        Platform::bind(withPsram);
        assert(RoomResource::ins().begin());
        for (uint16_t species : {1, 4, 7, 6, 143, 92}) {
            PokemonSprites::PetAnimationProfile profile;
            assert(PokemonSprites::petAnimationProfile(species, profile));
            const auto* frame = PokemonSprites::findSpeciesSprite(species, profile.idleBase);
            assert(frame);
            const auto bounds = PokemonSprites::frameVisibleBounds(frame);
            // Compare the metadata query with the actual decoded opaque pixels.
            canvas.fillSprite(0);
            assert(PokemonSprites::drawFrameSilhouette(frame, 0, 0, 0xFFFF));
            int left = 368, top = 448, right = -1, bottom = -1;
            for (int y = 0; y < 448; ++y) {
                for (int x = 0; x < 368; ++x) {
                    if (canvas.readPixel(x, y) != 0xFFFF) continue;
                    left = std::min(left, x); top = std::min(top, y);
                    right = std::max(right, x); bottom = std::max(bottom, y);
                }
            }
            assert(bounds.left == left && bounds.top == top);
            assert(bounds.width == right - left + 1 && bounds.height == bottom - top + 1);
            assert(PokemonSprites::frameVisibleWidth(frame) == bounds.width);
            for (auto direction : {PokemonSprites::WalkDirection::DOWN,
                                   PokemonSprites::WalkDirection::RIGHT}) {
                int firstHeartTop = 0;
                for (int hop : {0, -14}) {
                    HomeViewModel model;
                    model.speciesId = species;
                    model.petDirection = direction;
                    model.petRenderOffsetY = hop;
                    model.petVisible = false;
                    renderHomeScreen(canvas, model);
                    const auto emptyRoom = pixels;
                    model.petVisible = true;
                    renderHomeScreen(canvas, model);
                    const auto petOnly = pixels;
                    int petTop = 448;
                    for (size_t i = 0; i < pixels.size(); ++i) {
                        if (pixels[i] != emptyRoom[i]) petTop = std::min(petTop, int(i / 368));
                    }
                    model.showHearts = true;
                    renderHomeScreen(canvas, model);
                    left = 368; top = 448; right = bottom = -1;
                    for (size_t i = 0; i < pixels.size(); ++i) {
                        if (pixels[i] == petOnly[i]) continue;
                        const int x = i % 368, y = i / 368;
                        left = std::min(left, x); top = std::min(top, y);
                        right = std::max(right, x); bottom = std::max(bottom, y);
                    }
                    assert(right >= left && right - left < 60);
                    assert(bottom < petTop && petTop - bottom <= 14);
                    assert(std::abs((left + right) / 2 - model.petCenterX) <= 20);
                    if (hop == 0) firstHeartTop = top;
                    else assert(top == firstHeartTop + hop);
                    char name[72];
                    std::snprintf(name, sizeof(name), "pet-hearts-%u-%s-hop%d", species,
                        direction == PokemonSprites::WalkDirection::DOWN ? "down" : "right", hop);
                    checkPage(name, model, renderHomeScreen);
                }
            }
        }
        HomeViewModel hidden;
        hidden.petVisible = false;
        renderHomeScreen(canvas, hidden);
        const auto noPet = pixels;
        hidden.showHearts = true;
        renderHomeScreen(canvas, hidden);
        assert(pixels == noPet);
        Platform::bind(original);
        return 0;
    }
    // Gameplay cases start with a completed save. Hatch cases above use empty NVS.
    SaveManager fixtureSave;
    assert(fixtureSave.begin());
    Game::GameState fixture;
    fixture.oobeDone = true;
    fixture.team[0] = Game::MonsterFactory::create(1, 5);
    assert(fixtureSave.saveSnapshot(fixture, MainSceneViewState{}));
    Game::GameState state{};
    state.oobeDone = true;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    state.team[1] = Game::MonsterFactory::create(4, 5);
    state.storageCount = 2;
    state.storage[0] = state.team[0];
    state.storage[1] = state.team[1];

    if (selectedCase && std::strcmp(selectedCase, "battle-fx") == 0) {
        checkBattleFxRendering(state, renderBattleScreenCached);
        return 0;
    }

    if (selectedCase && std::strcmp(selectedCase, "battle-asset-actions") == 0) {
        checkBattleAssetActions(desktop, state);
        return 0;
    }

    if (selectedCase && std::strcmp(selectedCase, "battle-shared-execution") == 0) {
        checkSharedBattleExecution(desktop, state);
        return 0;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (selectedCase && std::strcmp(selectedCase, "battle-diagnostic-save") == 0) {
        checkBattleDiagnosticsSaveIsolation(desktop, state);
        return 0;
    }
#endif

    if (selectedCase && std::strcmp(selectedCase, "medicine-flow") == 0) {
        checkMedicineFlow(desktop, canvas, state);
        return 0;
    }

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

    if (selectedCase && std::strcmp(selectedCase, "explore-pickup") == 0) {
        assert(GameAssets::drawCentered(GameAssets::Kind::EXPLORE_PICKUP_BALL,
                                        208, 232, 2.0f));
        ExploreMapGenerator::Map map{};
        map.seed = 12345;
        map.pathCount = 1;
        map.paths[0].pointCount = 3;
        map.paths[0].points[0] = {3, 3};
        map.paths[0].points[1] = {3, 4};
        map.paths[0].points[2] = {3, 5};
        ExploreRouteViewModel model;
        model.state = &state;
        model.map = &map;
        model.worldX = model.worldY = -1000;  // Keep actors away from the marker.
        model.pickupIndex = 1;
        renderExploreRouteScreenCached(canvas, model, 0, AmoledUi::HEIGHT);
        const auto empty = pixels;
        model.pickupAvailable = true;
        checkPage("explore-pickup", model, renderExploreRouteScreenCached);
        size_t changed = 0, red = 0;
        for (int y = 208; y < 256; ++y) {
            for (int x = 184; x < 232; ++x) {
                const size_t i = y * AmoledUi::WIDTH + x;
                if (pixels[i] == empty[i]) continue;
                ++changed;
                const auto c = pixels[i];
                if (((c >> 11) & 31) > 18 && ((c >> 5) & 63) < 35) ++red;
            }
        }
        std::printf("explore pickup: %zu changed pixels, %zu red pixels\n", changed, red);
        assert(changed > 200 && red > 30);
        return 0;
    }

    if (selectedCase && std::strcmp(selectedCase, "explore-vegetation") == 0) {
        using ExploreTileSemantics::ActorDepth;
        using ExploreTileSemantics::ActorRole;
        ActorDepth order[] = {
            {ActorRole::COMPANION, 130.0f}, {ActorRole::LEADER, 100.0f},
            {ActorRole::BOSS, 117.0f}};
        ExploreTileSemantics::sortActorsByGroundDepth(order, 3);
        assert(order[0].role == ActorRole::LEADER);
        assert(order[1].role == ActorRole::BOSS);
        assert(order[2].role == ActorRole::COMPANION);
        for (ActorDepth& actor : order) actor.groundWorldY = 120.0f;
        ExploreTileSemantics::sortActorsByGroundDepth(order, 3);
        assert(order[0].role == ActorRole::BOSS);
        assert(order[1].role == ActorRole::COMPANION);
        assert(order[2].role == ActorRole::LEADER);
        ExploreMapGenerator::Map map{};
        map.seed = 54321;
        map.areaIndex = ExploreMapGenerator::TALL_GRASS_PARK_AREA;
        ExploreRouteViewModel model{};
        model.state = &state;
        model.map = &map;
        model.area = ExploreMapGenerator::TALL_GRASS_PARK_AREA;
        model.worldX = model.worldY = -1000;
        constexpr int tileSize = 52;
        constexpr int tileX = 3;
        constexpr uint16_t emptyColor = 0xF81F;
        constexpr uint16_t actorColor = 0x07FF;
        struct VegetationCase { uint16_t tileId; int tileY; float groundDepth; };
        // Each row of a complete tree must share the same ground depth.
        for (const VegetationCase& item : {
                VegetationCase{391, 3, 130.0f},
                VegetationCase{804, 3, 156.0f},
                VegetationCase{812, 4, 156.0f},
                VegetationCase{820, 5, 156.0f},
                VegetationCase{824, 3, 156.0f},
                VegetationCase{832, 4, 156.0f},
                VegetationCase{840, 5, 156.0f}}) {
            std::fill(std::begin(map.layers[1]), std::end(map.layers[1]), 0);
            map.layers[1][item.tileY * ExploreMapGenerator::WIDTH + tileX] = item.tileId;
            const int x = tileX * tileSize;
            const int y = item.tileY * tileSize;
            canvas.fillSprite(emptyColor);
            assert(GameAssets::drawExploreTileTo(canvas, item.tileId, x, y, 0, 2));
            const auto vegetation = pixels;
            size_t restored = 0;
            for (float actorDepth : {item.groundDepth - 0.01f, item.groundDepth}) {
                canvas.fillSprite(actorColor);
                drawExploreRouteMapVegetationForActor(
                    canvas, model, actorDepth, x + 4, y + 4,
                    x + tileSize - 4, y + tileSize - 4,
                    0, EXPLORE_ROUTE_MAP_BOTTOM);
                for (int py = y; py < y + tileSize; ++py) {
                    for (int px = x; px < x + tileSize; ++px) {
                        const size_t index = py * AmoledUi::WIDTH + px;
                        const bool shouldRestore = actorDepth < item.groundDepth &&
                            px >= x + 4 && px < x + tileSize - 4 &&
                            py >= y + 4 && py < y + tileSize - 4 &&
                            vegetation[index] != emptyColor;
                        assert(pixels[index] == (shouldRestore
                            ? vegetation[index] : actorColor));
                        if (shouldRestore) ++restored;
                    }
                }
                // The local actor clip must not affect the next actor.
                canvas.drawPixel(1, 1, emptyColor);
                assert(canvas.readPixel(1, 1) == emptyColor);
            }
            assert(restored > 0);
        }
        std::fill(std::begin(map.layers[0]), std::end(map.layers[0]), 384);
        std::fill(std::begin(map.layers[1]), std::end(map.layers[1]), 0);
        map.layers[1][3 * ExploreMapGenerator::WIDTH + tileX] = 391;
        canvas.fillSprite(emptyColor);
        assert(GameAssets::drawExploreTileTo(
            canvas, 391, 3 * tileSize, 3 * tileSize, 0, 2));
        const auto grass = pixels;
        renderExploreRouteScreenCached(canvas, model, 0, AmoledUi::HEIGHT);
        const auto empty = pixels;
        model.worldX = 91;
        model.worldY = 110;
        model.speciesId = 1;
        checkPage("explore-vegetation-behind", model, renderExploreRouteScreenCached);
        size_t hiddenPixels = 0;
        for (int y = 3 * tileSize; y < 4 * tileSize; ++y) {
            for (int x = 3 * tileSize; x < 4 * tileSize; ++x) {
                const size_t index = y * AmoledUi::WIDTH + x;
                if (grass[index] == emptyColor) continue;
                assert(pixels[index] == empty[index]);
                ++hiddenPixels;
            }
        }
        assert(hiddenPixels > 0);
        model.worldY = 130;  // On the grass depth boundary: the actor is in front.
        checkPage("explore-vegetation-front", model, renderExploreRouteScreenCached);
        const auto frontActor = pixels;
        model.companionVisible = true;
        model.companionSpeciesId = 25;
        model.companionWorldX = 91;
        model.companionWorldY = 110;
        checkPage("explore-vegetation-dual", model, renderExploreRouteScreenCached);
        size_t visibleFrontPixels = 0;
        for (int y = 3 * tileSize; y < 4 * tileSize; ++y) {
            for (int x = 3 * tileSize; x < 4 * tileSize; ++x) {
                const size_t index = y * AmoledUi::WIDTH + x;
                if (frontActor[index] == empty[index]) continue;
                assert(pixels[index] == frontActor[index]);
                ++visibleFrontPixels;
            }
        }
        assert(visibleFrontPixels > 0);
        // A boss between the two team members must not overpaint the nearer
        // companion. Sorting only against the leader used to draw it last.
        model.companionVisible = false;
        model.speciesId = 25;
        checkPage("explore-vegetation-companion-alone", model, renderExploreRouteScreenCached);
        const auto frontCompanion = pixels;
        model.speciesId = 1;
        model.worldY = 100;
        model.companionVisible = true;
        model.companionWorldY = 130;
        map.pathCount = 1;
        map.paths[0].pointCount = 1;
        map.paths[0].points[0] = {3, 4};
        model.bossPending = true;
        model.bossSpeciesId = 4;
        checkPage("explore-vegetation-three-actors", model, renderExploreRouteScreenCached);
        visibleFrontPixels = 0;
        for (int y = 3 * tileSize; y < 4 * tileSize; ++y) {
            for (int x = 3 * tileSize; x < 4 * tileSize; ++x) {
                const size_t index = y * AmoledUi::WIDTH + x;
                if (frontCompanion[index] == empty[index]) continue;
                assert(pixels[index] == frontCompanion[index]);
                ++visibleFrontPixels;
            }
        }
        assert(visibleFrontPixels > 0);
        // The boss patrol reaches Y=122 from its Y=117 anchor. It must now
        // cover a leader at Y=120 even though its original path point is behind.
        model.worldX = model.worldY = -1000;
        model.companionVisible = false;
        model.animationNowMs = 2300;
        checkPage("explore-vegetation-patrol-alone", model, renderExploreRouteScreenCached);
        const auto frontBoss = pixels;
        model.worldX = 91;
        model.worldY = 120;
        model.companionVisible = true;
        model.companionWorldY = 110;
        checkPage("explore-vegetation-patrol-three", model, renderExploreRouteScreenCached);
        size_t visibleBossPixels = 0;
        for (int y = 4 * tileSize; y < 238; ++y) {
            for (int x = 3 * tileSize; x < 4 * tileSize; ++x) {
                const size_t index = y * AmoledUi::WIDTH + x;
                if (frontBoss[index] == empty[index]) continue;
                assert(pixels[index] == frontBoss[index]);
                ++visibleBossPixels;
            }
        }
        assert(visibleBossPixels > 0);
        // The same occlusion must be drawn when the large world cache is absent.
        const auto cached = pixels;
        auto& original = desktop.serviceBundle();
        CacheFailAllocator failing(original.memory);
        Platform::Services limited{
            original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, original.power,
            original.blobs, original.resources, failing, original.peers};
        RenderCaches emptyCaches;
        Platform::bind(limited);
        checkPage("explore-vegetation-no-cache", model,
            [&](Canvas565& target, const ExploreRouteViewModel& view,
                uint16_t top, uint16_t bottom) {
                renderExploreRouteScreen(target, view, emptyCaches.exploreWorld, top, bottom);
            });
        assert(pixels == cached);
        Platform::bind(original);
        std::puts("explore vegetation: grass/tree ground depths, actor clips, three actor depths and boss patrol, dirty rows and cache fallback agree");
        return 0;
    }

    if (selectedCase && std::strcmp(selectedCase, "lock-focus-pixels") == 0) {
        checkLockFocusPixels(desktop, canvas, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "explore-reserve-encounters") == 0) {
        checkExploreReserveEncounters(desktop, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "explore-motion-damage") == 0) {
        checkExploreMotionDamage(desktop, canvas, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "explore-park-routes") == 0) {
        checkExploreParkRoutes(desktop, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "explore-save-boundaries") == 0) {
        checkExploreSaveBoundaries(desktop, state);
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
    if (selectedCase && std::strcmp(selectedCase, "settings-reset") == 0) {
        checkSettingsReset(desktop);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "settings-reset-render") == 0) {
        SettingsViewModel reset{};
        reset.state = &state;
        reset.brightness = state.settings.brightness;
        reset.volume = state.settings.volume;
        checkPage("settings-reset-menu", reset, renderSettingsScreen);
        const auto withoutUpdate = pixels;
        reset.systemUpdate = true;
        checkPage("settings-reset-menu-v2", reset, renderSettingsScreen);
#if STICKMON_ENABLE_DEBUG_FEATURES
        assert(pixels != withoutUpdate);
#else
        assert(pixels == withoutUpdate);
#endif
        reset.systemUpdate = false;
        reset.resetConfirm = true;
        for (int pressed : {-1, 0, 1}) {
            reset.pressedItem = pressed;
            char name[64];
            std::snprintf(name, sizeof(name), "settings-reset-confirm-%d", pressed);
            checkPage(name, reset, renderSettingsScreen);
            assert(canvas.readPixel(106, 418) == PixelRenderer::rgb(
                pressed == 0 ? 53 : 36, pressed == 0 ? 76 : 54,
                pressed == 0 ? 83 : 61));
            assert(canvas.readPixel(262, 418) == PixelRenderer::rgb(
                pressed == 1 ? 129 : 91, pressed == 1 ? 59 : 49,
                pressed == 1 ? 68 : 55));
        }
        reset.toast = Ui::Settings::RESET_FAILED;
        checkPage("settings-reset-failed-toast", reset, renderSettingsScreen);
        assert(settingsResetChoiceAt(36, 354) == 0);
        assert(settingsResetChoiceAt(175, 425) == 0);
        assert(settingsResetChoiceAt(192, 354) == 1);
        assert(settingsResetChoiceAt(331, 425) == 1);
        for (int x : {35, 176, 184, 191, 332}) {
            assert(settingsResetChoiceAt(x, 390) == -1);
        }
        assert(settingsResetChoiceAt(262, 353) == -1);
        assert(settingsResetChoiceAt(262, 426) == -1);
        assert(settingsResetChoiceAt(40, 38) == -1);
        assert(settingsResetChoiceAt(184, 200) == -1);
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
    if (selectedCase && std::strcmp(selectedCase, "locked-room-power") == 0) {
        checkLockedRoomPower(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-care-rewards") == 0) {
        checkHomeCareRewards(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-room-authority") == 0) {
        checkHomeRoomAuthority(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-food-turns") == 0) {
        checkHomeAutomaticFoodTurns(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-food-exit-stall") == 0) {
        checkHomeFoodExitNoStall(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-faint-sleep-wake") == 0) {
        checkHomeFaintRecoveryAndSleep(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-saved-turns") == 0) {
        checkHomeSavedTurns(desktop);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "home-yield-actions") == 0) {
        checkHomeYieldAndActions(desktop, canvas);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "shower-render") == 0) {
        PixelCache565 cache;
        auto render = [&](Canvas565& target, const ShowerViewModel& model, uint16_t top, uint16_t bottom) {
            renderShowerScreenCached(target, model, cache, top, bottom);
        };
        ShowerFoamState foam[8] = {};
        ShowerWaterState water[4] = {};
        ShowerFloatState floats[2] = {};
        ShowerViewModel shower;
        shower.state = &state; shower.speciesId = 8;
        shower.groundY = showerPetGroundY(8);
        shower.mode = ShowerMode::SOAPING;
        shower.toolX = 184; shower.toolY = 212; shower.toolDragging = true;
        shower.foam = foam; shower.foamCount = 8;
        for (int i = 0; i < 8; ++i) {
            foam[i].active = true; foam[i].stage = i % 2;
            foam[i].x = 140 + (i % 4) * 30; foam[i].y = 164 + (i / 4) * 96;
        }
        checkPage("shower-soap", shower, render);
        shower.mode = ShowerMode::BRUSHING;
        for (int i = 0; i < 4; ++i) {
            foam[i].stage = 2; foam[i].y = shower.groundY - 24 - i * 4;
        }
        shower.petFlip = true; shower.petOffsetY = -12;
        checkPage("shower-brush", shower, render);
        shower.foamCount = 0; shower.atmosphereAlpha = 200;
        checkPage("shower-full-foam", shower, render);
        shower.mode = ShowerMode::RINSING; shower.toolDragging = false;
        shower.petOffsetY = 0; shower.petFlip = false;
        shower.rinseProgress = 35; shower.elapsedMs = 630;
        shower.water = water; shower.waterCount = 4;
        for (int i = 0; i < 4; ++i) water[i] = {120.0f + i * 40, 200.0f + i * 20, 0, -90, true};
        checkPage("shower-rinse", shower, render);
        shower.mode = ShowerMode::COMPLETE; shower.completionHearts = 3;
        shower.atmosphereAlpha = 0; shower.nowMs = 1200;
        floats[0] = {1000, 5, true, 0}; floats[1] = {1000, 0, true, 3};
        shower.floats = floats; shower.floatCount = 2;
        checkPage("shower-complete", shower, render);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "shower-flow") == 0) {
        checkShowerFlow(desktop, canvas, state);
        return 0;
    }
    if (selectedCase && std::strcmp(selectedCase, "shower-growth-roundtrip") == 0) {
        checkShowerGrowthRoundTrip(desktop, canvas);
        return 0;
    }
    if (selectedCase) {
        if (std::strcmp(selectedCase, "explore-water") == 0) {
            checkExploreWater(desktop, canvas);
            return 0;
        }
        if (std::strcmp(selectedCase, "explore-ecology") == 0) {
            checkExploreEcology(desktop, canvas);
            return 0;
        }
        if (std::strcmp(selectedCase, "battery-display") == 0) {
            checkBatteryDisplay(desktop, canvas);
            return 0;
        }
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
    SettingsViewModel defaultSettings{};
    defaultSettings.state = &state;
    defaultSettings.brightness = state.settings.brightness;
    defaultSettings.volume = state.settings.volume;
    checkPage("settings", defaultSettings, renderSettingsScreen);
    SettingsViewModel otaSettings{};
    otaSettings.state = &state;
    otaSettings.brightness = state.settings.brightness;
    otaSettings.volume = state.settings.volume;
    otaSettings.systemUpdate = true;
    checkPage("settings-v2-update", otaSettings, renderSettingsScreen);
    SettingsViewModel resetSettings{};
    resetSettings.resetConfirm = true;
    checkPage("settings-reset-confirm", resetSettings, renderSettingsScreen);
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
    battle.thrownItem = Game::ItemId::SWEET_FOOD;
    for (int frame = 0; frame <= 4; ++frame) {
        battle.itemThrowProgress = frame / 4.0f;
        const auto label = std::string("battle-fruit-throw-") + std::to_string(frame);
        checkPage(label.c_str(), battle, renderBattleScreenCached);
    }
    battle.thrownItem = Game::ItemId::COUNT;
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
        checkPage("shower-no-cache", shower,
            [&](Canvas565& target, const ShowerViewModel& model, uint16_t top, uint16_t bottom) {
                renderShowerScreenCached(target, model, emptyCaches.showerBackground, top, bottom,
                                        &emptyCaches.showerPet);
            });
        assert(emptyCaches.showerBackground.allocatedBytes() == 0);
        assert(emptyCaches.showerPet.allocatedBytes() == 0);
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
    assert(settingsItemAt(200, 323) == SETTINGS_POWER_SAVE_ITEM);
    assert(settingsItemAt(200, 324) == SETTINGS_RESET_GAME_ITEM);
    assert(settingsItemAt(200, 383) == SETTINGS_RESET_GAME_ITEM);
    assert(settingsItemAt(200, 384) == -1);
    assert(settingsItemAt(200, 400) == -1);
    assert(settingsItemAt(200, 324, true) == SETTINGS_RESET_GAME_ITEM);
    assert(settingsItemAt(200, 383, true) == SETTINGS_RESET_GAME_ITEM);
    const int updateItem = STICKMON_ENABLE_DEBUG_FEATURES
        ? SETTINGS_SYSTEM_UPDATE_ITEM : -1;
    assert(settingsItemAt(200, 384, true) == updateItem);
    assert(settingsItemAt(200, 443, true) == updateItem);
    assert(settingsItemAt(200, 444, true) == -1);
    assert(settingsItemAt(200, 400, true) == updateItem);
    assert(settingsItemAt(200, 447, true) == -1);
    assert(settingsItemAt(200, 448, true) == -1);
    assert(computerItemAt(0, 165, ComputerViewModel::Page::STORAGE, 0, 20) == 0);
    assert(computerItemAt(367, 166, ComputerViewModel::Page::STORAGE, 0, 20) == 1);
    assert(computerItemAt(200, 255, ComputerViewModel::Page::STORAGE, 0, 20) == 1);
    assert(computerItemAt(200, 256, ComputerViewModel::Page::STORAGE, 0, 20) == 2);
    std::puts("Native geometry and framebuffer checks passed");
}
