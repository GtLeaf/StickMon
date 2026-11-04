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
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "presentation/PixelRenderer.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#define protected public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "core/RoomResource.h"
#include "scenes/HatchScene.h"
#include "scenes/MenuScene.h"
#undef protected
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if STICKMON_HATCH_UI_BASELINE
#define VISUAL(page) (page)
#else
#define VISUAL(page) ((page).ui)
#endif

class Input final : public Platform::IInputDevice {
public:
    bool touch = STICKMON_UI_COMPACT_TOUCH;
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return touch; }
};
class Power final : public Platform::IPowerDevice {
public:
    int batteryLevel() override { return 100; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 1234; }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return 7 * 1024 * 1024; }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
};
class Blobs final : public Platform::IBlobStore {
public:
    Platform::IBlobStore& delegate;
    bool fail = false;
    unsigned attempts = 0, removals = 0;
    explicit Blobs(Platform::IBlobStore& delegate) : delegate(delegate) {}
    bool initialize() override { return delegate.initialize(); }
    size_t blobSize(const char* ns, const char* key) override { return delegate.blobSize(ns, key); }
    bool readBlob(const char* ns, const char* key, void* out, size_t n) override {
        return delegate.readBlob(ns, key, out, n);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t n) override {
        if (std::strcmp(key, "hatch") == 0) { ++attempts; if (fail) return false; }
        return delegate.writeBlob(ns, key, data, n);
    }
    bool removeBlob(const char* ns, const char* key) override {
        if (std::strcmp(key, "hatch") == 0) ++removals;
        return delegate.removeBlob(ns, key);
    }
    bool clearNamespace(const char* ns) override { return delegate.clearNamespace(ns); }
};
class Resources final : public Platform::IResourceStore {
public:
    Platform::IResourceStore& delegate;
    bool fallback;
    unsigned denied = 0;
    explicit Resources(Platform::IResourceStore& delegate, bool fallback) : delegate(delegate), fallback(fallback) {}
    bool mount() override { return delegate.mount(); }
    size_t totalBytes() const override { return delegate.totalBytes(); }
    size_t usedBytes() const override { return delegate.usedBytes(); }
    Platform::ResourceFile open(const char* path) override {
        if (fallback && std::strstr(path, "hatch.smonfx")) { ++denied; return {}; }
        return delegate.open(path);
    }
};

int main(int argc, char** argv) {
    assert(argc == 4);
    const bool fallback = std::strcmp(argv[3], "fallback") == 0;
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle();
    Input input; Power power; Blobs blobs(bundle.blobs); Resources resources(bundle.resources, fallback);
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, power,
        blobs, resources, bundle.memory, bundle.peers};
    Platform::bind(services); assert(desktop.begin());
    auto& engine = GameEngine::ins(); assert(engine.begin());
    auto& game = engine.gameState();
    auto& room = RoomResource::ins(); assert(room.available());
    std::unique_ptr<HatchScene> hatch;
    std::unique_ptr<MenuScene> menu;
    unsigned checks = 0;
    const ButtonEvent a{0, BtnAction::PRESSED}, b{1, BtnAction::PRESSED};
    auto reset = [&]() {
        hatch.reset(); menu.reset(); engine.currentScene.reset();
        engine.sceneFade = GameEngine::SceneFadePhase::NONE;
        engine.contactVisit = {}; engine.exploreTravel = ExploreTravelPhase::NONE;
        engine.evolutionEventHead = engine.evolutionEventCount = 0;
        engine.moveReplacementEventHead = engine.moveReplacementEventCount = 0;
        engine.clearMainSceneViewState();
        game = Game::GameState{}; game.teamCount = 0; game.oobeDone = false;
        game.hatchSeconds = 180;
        engine.currentId = SceneID::HATCH; engine.prevId = SceneID::MAIN;
        engine.saveCoordinator.reset(desktop.millis());
        engine.clearHatchProgress(); HatchScene::clearRuntimeProgress();
        blobs.fail = false; blobs.attempts = blobs.removals = 0;
        input.touch = STICKMON_UI_COMPACT_TOUCH;
        engine.gameClock.set(desktop.millis(), 420);
        GameRandom::seed(1234);
    };
    auto setup = [&]() -> HatchScene& {
        reset(); hatch = std::make_unique<HatchScene>(); hatch->onEnter(); return *hatch;
    };
    auto clock = [&](uint32_t now) { desktop.advanceMs(now - desktop.millis()); };
    auto pixels = [&](const std::string& name) {
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        assert(std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file) == size_t(fb.width) * fb.height);
        std::fclose(file);
    };
    auto capture = [&](const std::string& name) { PixelRenderer::clear(0x39e7); hatch->render(); pixels(name); };
    for (bool missingRoom : {false, true})
        for (int minute : {359, 360, 1079, 1080})
            for (float elapsed : {0.0f, 60.25f, 179.1f})
                for (uint32_t phase : {0u, 45u, 46u, 184u, 230u, 231u}) {
                    auto& h = setup(); h.elapsed = elapsed;
                    const auto interval = 2400 - int(elapsed * 100 / 180) * 19;
                    clock(uint32_t(interval * 10) + phase);
                    engine.gameClock.set(desktop.millis(), minute);
                    const bool loaded = room.loaded_; room.loaded_ = !missingRoom;
                    capture("hatch-room-" + std::to_string(!missingRoom) + "-minute-" + std::to_string(minute) + "-elapsed-" + std::to_string(int(elapsed)) + "-phase-" + std::to_string(phase));
                    room.loaded_ = loaded;
                }
    for (int button : {0, 1, 2}) {
        setup(); hatch->onButton({uint8_t(button), BtnAction::PRESSED});
        capture("hatch-toast-" + std::to_string(button));
        assert(VISUAL(*hatch).toastUntil == desktop.millis() + 700);
        assert(hatch->wipeCount == (button == 0) && hatch->pokeCount == (button != 0)); ++checks;
    }
    assert(fallback == (resources.denied > 0));
    for (uint16_t total : {uint16_t(0), uint16_t(1), uint16_t(180), uint16_t(3600), uint16_t(65535)}) {
        setup(); game.hatchSeconds = total;
        capture("hatch-countdown-" + std::to_string(total));
    }
    setup();
    for (const auto action : {BtnAction::DOWN, BtnAction::UP, BtnAction::LONG_PRESS}) {
        assert(!hatch->onButton({1, action}));
        assert(hatch->pokeCount == 0 && hatch->wipeCount == 0);
    }
    assert(!hatch->onTap(-1) && !hatch->onTap(2)); ++checks;
    setup(); hatch->elapsed = 14.9f; hatch->onButton(a); hatch->onButton(b);
    hatch->update(desktop.millis(), 0); assert(blobs.attempts == 0);
    hatch->update(desktop.millis(), 0.2f); assert(blobs.attempts == 1 && hatch->lastSavedSecond == 15);
    Game::HatchProgress progress; assert(engine.loadHatchProgress(progress));
    assert(progress.elapsedSeconds == 15 && progress.pokeCount == 1 && progress.wipeCount == 1); ++checks;
    hatch->update(desktop.millis(), 14.8f); assert(blobs.attempts == 1);
    blobs.fail = true; hatch->update(desktop.millis(), 0.2f);
    assert(blobs.attempts == 2 && hatch->lastSavedSecond == 15);
    hatch->update(desktop.millis(), 0); assert(blobs.attempts == 3 && hatch->lastSavedSecond == 15);
    blobs.fail = false; hatch->update(desktop.millis(), 0);
    assert(blobs.attempts == 4 && hatch->lastSavedSecond == 30); ++checks;
    hatch->elapsed = 37.75f; hatch->onExit(); assert(blobs.attempts == 5);
    hatch = std::make_unique<HatchScene>(); hatch->onEnter();
    assert(hatch->elapsed == 37 && hatch->pokeCount == 1 && hatch->wipeCount == 1); ++checks;
    setup(); hatch->elapsed = 23.25f; hatch->pokeCount = 4; hatch->wipeCount = 8;
    blobs.fail = true; hatch->onExit(); engine.clearHatchProgress();
    hatch = std::make_unique<HatchScene>(); hatch->onEnter();
    assert(hatch->elapsed == 23.25f && hatch->pokeCount == 4 && hatch->wipeCount == 8); ++checks;
    HatchScene::clearRuntimeProgress(); hatch = std::make_unique<HatchScene>(); hatch->onEnter();
    assert(hatch->elapsed == 0 && hatch->pokeCount == 0 && hatch->wipeCount == 0); ++checks;
    setup(); game.oobeDone = true; hatch->persistProgress(true); assert(blobs.attempts == 0); ++checks;
    for (uint32_t now : {1000u, 0xffffff00u}) {
        setup(); clock(now); hatch->onButton(a);
        hatch->update(now + 699, 0); assert(VISUAL(*hatch).toast);
        const auto expired = hatch->update(now + 700, 0);
        assert(!VISUAL(*hatch).toast && expired.redraw); ++checks;
    }
    // Observe scheduling from production, including shake, seconds, night and speed.
    for (int speed = 0; speed < 4; ++speed) {
        setup(); game.settings.speedIndex = speed;
        for (uint32_t now : {0u, 45u, 46u, 183u, 184u, 229u, 230u, 231u, 2399u, 2400u}) {
            clock(now); const auto result = hatch->update(now, 0.037f);
            std::printf("demand %d %u %d %u\n", speed, now, result.redraw, result.nextUpdateDelayMs);
        }
        ++checks;
    }
    setup(); clock(231); hatch->elapsed = 179.999f;
    const auto imminent = hatch->update(desktop.millis(), 0);
    assert(imminent.nextUpdateDelayMs == 1); ++checks;
    setup(); clock(231); hatch->onEnter();
    auto idle = hatch->update(desktop.millis(), 0); assert(!idle.redraw);
    engine.gameClock.set(desktop.millis(), 1080);
    idle = hatch->update(desktop.millis(), 0); assert(idle.redraw);
    idle = hatch->update(desktop.millis(), 0); assert(!idle.redraw); ++checks;
    using Touch = Platform::IInputDevice::TouchType;
    auto touch = [&](Scene& page, Touch type, int x, int y) {
        desktop.advanceMs(10); return page.onTouch({type, int16_t(x), int16_t(y), desktop.millis()});
    };
    for (bool missingRoom : {false, true}) {
        setup(); const bool loaded = room.loaded_; room.loaded_ = !missingRoom;
        int left = Hal::DISPLAY_W, right = -1, top = Hal::DISPLAY_H, bottom = -1;
        for (int y = 0; y < Hal::DISPLAY_H; ++y) for (int x = 0; x < Hal::DISPLAY_W; ++x)
            if (hatch->touchTarget(x, y) == 0) { left = std::min(left, x); right = std::max(right, x); top = std::min(top, y); bottom = std::max(bottom, y); }
        assert(right - left == 48 && bottom - top == 54);
        const int x = (left + right) / 2, y = (top + bottom) / 2;
        assert(!touch(*hatch, Touch::DOWN, x, y)); assert(touch(*hatch, Touch::UP, x, y));
        assert(hatch->pokeCount == 1 && hatch->wipeCount == 0);
        assert(!touch(*hatch, Touch::UP, x, y) && hatch->pokeCount == 1);
        touch(*hatch, Touch::DOWN, x, y); touch(*hatch, Touch::MOVE, x + 30, y);
        assert(!touch(*hatch, Touch::UP, x, y) && hatch->pokeCount == 1);
        touch(*hatch, Touch::DOWN, x, y); hatch->cancelTouch();
        assert(!touch(*hatch, Touch::UP, x, y) && hatch->pokeCount == 1);
        touch(*hatch, Touch::DOWN, x, y); touch(*hatch, Touch::CANCEL, x, y);
        assert(!touch(*hatch, Touch::UP, x, y) && hatch->pokeCount == 1);
        room.loaded_ = loaded; ++checks;
    }
    // Own the scene through the real engine so completion destroys the old UI.
    for (const auto counts : {std::array<int, 3>{0, 0, 1}, {9, 0, 1}, {10, 0, 4}, {0, 10, 7}, {5, 5, 1}}) {
        reset(); engine.currentScene = std::make_unique<HatchScene>();
        auto* h = static_cast<HatchScene*>(engine.currentScene.get()); h->onEnter();
        h->pokeCount = counts[0]; h->wipeCount = counts[1]; h->elapsed = 179.5f;
        const auto result = h->update(desktop.millis(), 0.5f);
        assert(game.oobeDone && game.teamCount == 1 && game.team[0].speciesId == counts[2]);
        assert(engine.currentId == SceneID::MAIN && !result.redraw);
        assert(!engine.loadHatchProgress(progress) && blobs.removals == 1); ++checks;
    }
    for (bool tapping : {false, true}) {
        reset(); engine.currentScene = std::make_unique<HatchScene>();
        auto* h = static_cast<HatchScene*>(engine.currentScene.get()); h->onEnter(); h->elapsed = 29.5f;
        if (tapping) assert(h->onTap(1)); else assert(h->onButton({0, BtnAction::LONG_PRESS}));
        assert(engine.currentId == SceneID::MENU && engine.loadHatchProgress(progress));
        assert(progress.elapsedSeconds == 29 && !game.oobeDone); ++checks;
    }
#if STICKMON_UI_COMPACT_TOUCH
    reset(); engine.currentScene = std::make_unique<HatchScene>();
    auto* owned = static_cast<HatchScene*>(engine.currentScene.get()); owned->onEnter();
    assert(!touch(*owned, Touch::DOWN, Hal::DISPLAY_W - 30, Hal::DISPLAY_H - 20));
    assert(touch(*owned, Touch::UP, Hal::DISPLAY_W - 30, Hal::DISPLAY_H - 20));
    assert(engine.currentId == SceneID::MENU); ++checks;
#endif
    auto menuSetup = [&]() -> MenuScene& {
        reset(); game.oobeDone = true; game.teamCount = 1; game.tutorialFlags = 0xff;
        game.team[0] = Game::MonsterFactory::create(1, 5);
        engine.currentId = SceneID::MENU;
        menu = std::make_unique<MenuScene>(); menu->onEnter(); return *menu;
    };
    const int pages = 9 + STICKMON_ENABLE_DEBUG_FEATURES;
    for (bool touchOn : {false, true}) for (int page = 0; page < pages; ++page) {
        for (int notice = 0; notice < 3; ++notice) {
            auto& m = menuSetup(); input.touch = touchOn; m.pushView(MenuScene::ViewMode(page));
            m.toast = notice == 0 ? nullptr : notice == 1 ? "OK" : "A LONG NOTICE FOR WRAPPING";
            PixelRenderer::clear(0x39e7); m.renderToast();
            pixels("menu-toast-touch-" + std::to_string(touchOn) + "-page-" + std::to_string(page) + "-notice-" + std::to_string(notice));
            PixelRenderer::clear(0x39e7); m.render();
            pixels("menu-full-touch-" + std::to_string(touchOn) + "-page-" + std::to_string(page) + "-notice-" + std::to_string(notice));
        }
        auto& m = menuSetup(); input.touch = touchOn; m.pushView(MenuScene::ViewMode(page));
        m.pageNotice("TEMP", 1100); m.update(desktop.millis() + 1099, 0);
        assert(m.toast); const auto expired = m.update(desktop.millis() + 1100, 0);
        assert(!m.toast && expired.redraw); ++checks;
    }
    for (int flags = 0; flags < 4; ++flags) for (bool hatched : {false, true})
        for (bool explore : {false, true}) for (bool touchOn : {false, true})
            for (bool main : {false, true}) {
                auto& m = menuSetup(); input.touch = touchOn; game.oobeDone = hatched;
                game.tutorialFlags = (flags & 1 ? uint8_t(Game::TutorialStep::MENU_NAV) : 0) |
                    (flags & 2 ? uint8_t(Game::TutorialStep::MENU_BACK) : 0);
                m.exploreContextMode = explore;
                if (!main) m.pushView(MenuScene::ViewMode::ROOM);
                PixelRenderer::clear(0x39e7); m.renderTutorial();
                pixels("tutorial-" + std::to_string(flags) + "-hatched-" + std::to_string(hatched) + "-explore-" + std::to_string(explore) + "-touch-" + std::to_string(touchOn) + "-main-" + std::to_string(main));
            }
    for (uint32_t now : {2000u, 0xffffff00u}) {
        auto& m = menuSetup(); clock(now); m.pageNotice("OLD", 1400);
        assert(m.toastUntil == now + 1400); m.update(now + 1399, 0); assert(m.toast);
        auto result = m.update(now + 1400, 0); assert(!m.toast && result.redraw); ++checks;
        m.pageNotice("NEW", 1100); assert(m.toast && std::strcmp(m.toast, "NEW") == 0);
        result = m.update(desktop.millis() + 1100, 0); assert(!m.toast && result.redraw);
    }
    menuSetup(); game.tutorialFlags = 0; menu->onButton(b);
    assert(engine.tutorialComplete(Game::TutorialStep::MENU_NAV)); ++checks;
    menu->pushView(MenuScene::ViewMode::ROOM); menu->onButton({1, BtnAction::LONG_PRESS});
    assert(menu->viewMode == MenuScene::ViewMode::MENU);
    assert(engine.tutorialComplete(Game::TutorialStep::MENU_BACK)); ++checks;
    std::printf("hatch-overlay-checkpoints=%u\n", checks);
}
