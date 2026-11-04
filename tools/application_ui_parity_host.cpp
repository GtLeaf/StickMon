#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "assets/PokemonSprites.h"
#include "game/MonsterFactory.h"
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "core/GameEngine.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "scenes/MenuScene.h"
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

class Input final : public Platform::IInputDevice {
public:
    bool touch = true, down = false;
    std::deque<TouchEvent> events;
    void update() override {}
    bool pressed(Platform::InputButton button) const override {
        return down && button == Platform::InputButton::PRIMARY;
    }
    bool touchAvailable() const override { return touch; }
    bool pollTouch(TouchEvent& event) override {
        if (events.empty()) return false;
        event = events.front(); events.pop_front(); return true;
    }
};
class Display final : public Platform::IDisplayDevice {
public:
    Platform::IDisplayDevice& delegate;
    bool lock = false;
    explicit Display(Platform::IDisplayDevice& delegate) : delegate(delegate) {}
    Platform::FrameBuffer565 frameBuffer() override { return delegate.frameBuffer(); }
    void present() override { delegate.present(); }
    void setBrightness(uint8_t value) override { delegate.setBrightness(value); }
    uint8_t brightness() const override { return delegate.brightness(); }
    void sleep() override { delegate.sleep(); }
    bool locked() const override { return lock; }
};
struct ProbeState {
    std::vector<ButtonEvent> buttons;
    std::vector<int> taps;
    unsigned targets = 0, renders = 0, cancels = 0, destroyed = 0;
    bool navigation = true, clip = false, changeScene = false;
    uint8_t spriteMissing = 0;
    uint16_t firstMissing = 0;
};
class Probe final : public Scene {
public:
    ProbeState& state;
    explicit Probe(ProbeState& state) : state(state) {}
    ~Probe() override { ++state.destroyed; }
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    bool touchNavigation() const override { return state.navigation; }
    void render() override {
        ++state.renders;
        auto& canvas = PixelRenderer::canvas();
        canvas.clearClipRect(); PixelRenderer::clear(PixelRenderer::rgb(80, 160, 240));
        canvas.fillRect(3, 5, 60, 40, 0xf800);
        canvas.fillRect(70, 20, 20, 70, 0x07e0);
        canvas.fillRect(120, 60, 50, 35, 0x001f);
        if (state.clip) canvas.setClipRect(11, 13, 91, 77);
        auto& stats = const_cast<PokemonSprites::SpriteCacheStats&>(PokemonSprites::cacheStats());
        stats.missingSpecies = state.spriteMissing;
        stats.firstMissingSpecies = state.firstMissing;
    }
    int touchTarget(int16_t x, int16_t y) override {
        ++state.targets;
        return x < 100 && y < 100 ? 3 : 4;
    }
    bool onTap(int target) override {
        state.taps.push_back(target);
        if (state.changeScene) { GameEngine::ins().requestScene(SceneID::MENU); return true; }
        return true;
    }
    bool onButton(const ButtonEvent& event) override {
        state.buttons.push_back(event);
        if (state.changeScene) { GameEngine::ins().requestScene(SceneID::MENU); return true; }
        return true;
    }
    void cancelTouch() override { ++state.cancels; Scene::cancelTouch(); }
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]); const auto bundle = desktop.serviceBundle();
    Input input; Display display(bundle.display);
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger, input,
        display, bundle.audio, bundle.imu, bundle.power, bundle.blobs, bundle.resources,
        bundle.memory, bundle.peers};
    Platform::bind(services); assert(desktop.begin());
    auto& engine = GameEngine::ins(); assert(engine.begin());
    auto& room = RoomResource::ins(); auto& font = FontResource::ins();
    const bool roomLoaded = room.loaded_, sarasaLoaded = font.sarasaCjk_.loaded;
    const bool asciiLoaded = font.unsciiAscii_.loaded;
    assert(roomLoaded && sarasaLoaded && asciiLoaded);
    ProbeState state;
    using Fade = GameEngine::SceneFadePhase;
    using Touch = Platform::IInputDevice::TouchType;
    unsigned checks = 0;
    auto stats = [&]() -> PokemonSprites::SpriteCacheStats& {
        return const_cast<PokemonSprites::SpriteCacheStats&>(PokemonSprites::cacheStats());
    };
    auto reset = [&]() -> Probe& {
        engine.currentScene.reset(); state = {};
        engine.currentId = SceneID::MENU; engine.prevId = SceneID::MAIN;
        engine.sceneFade = Fade::NONE; engine.sceneFadeProgressMs = 0;
        engine.sceneFadeDurationMs = 300; engine.sceneFadeStartedMs = desktop.millis();
        engine.clearMainSceneViewState(); engine.contactVisit = {};
        engine.exploreTravel = ExploreTravelPhase::NONE;
        engine.gameState() = Game::GameState{}; engine.gameState().oobeDone = true;
        engine.gameState().teamCount = 1;
        engine.gameState().team[0] = Game::MonsterFactory::create(1, 5);
        engine.gameState().tutorialFlags = 0xff;
        engine.saveCoordinator.reset(desktop.millis());
        engine.sceneDirty = false; engine.sceneUpdateScheduled = false;
        room.loaded_ = roomLoaded; font.sarasaCjk_.loaded = sarasaLoaded;
        font.unsciiAscii_.loaded = asciiLoaded;
        stats().missingSpecies = 0; stats().firstMissingSpecies = 0;
        input.touch = true; input.down = false; input.events.clear(); display.lock = false;
        ButtonDispatcher::ins().reset(); PixelRenderer::canvas().clearClipRect();
        engine.currentScene = std::make_unique<Probe>(state);
        return *static_cast<Probe*>(engine.currentScene.get());
    };
    auto pixels = [&](const std::string& name) {
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        assert(std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file) == size_t(fb.width) * fb.height);
        std::fclose(file);
    };
    for (bool touch : {false, true}) for (bool navigation : {false, true})
        for (bool clip : {false, true}) {
            reset(); input.touch = touch; state.navigation = navigation; state.clip = clip;
            engine.render(desktop.millis());
            pixels("navigation-touch-" + std::to_string(touch) + "-enabled-" + std::to_string(navigation) + "-clip-" + std::to_string(clip));
        }
    for (bool fontMissing : {false, true}) for (bool roomMissing : {false, true})
        for (int species : {0, 1, 25, 999, 65535}) for (bool clip : {false, true}) {
            reset(); font.sarasaCjk_.loaded = !fontMissing; room.loaded_ = !roomMissing;
            stats().missingSpecies = species ? 2 : 0; stats().firstMissingSpecies = species;
            PixelRenderer::clear(0x39e7);
            if (clip) PixelRenderer::canvas().setClipRect(11, 13, 91, 77);
            engine.renderResourceAlert();
            assert(engine.resourceAlertVisible() == (fontMissing || roomMissing || species));
            pixels("alert-font-" + std::to_string(fontMissing) + "-room-" + std::to_string(roomMissing) + "-species-" + std::to_string(species) + "-clip-" + std::to_string(clip));
        }
    reset(); stats().missingSpecies = 2; stats().firstMissingSpecies = 0;
    PixelRenderer::clear(0x39e7); engine.renderResourceAlert(); pixels("alert-species-without-id");
    for (int phase = 0; phase < 4; ++phase) for (uint16_t duration : {uint16_t(1), uint16_t(300), uint16_t(65535)})
        for (int progress : {0, 1, 149, 150, 299, 300, 65535}) {
            reset(); state.navigation = true; engine.currentScene->render();
            engine.sceneFade = Fade(phase); engine.sceneFadeDurationMs = duration;
            engine.sceneFadeProgressMs = progress; engine.renderSceneFade(desktop.millis());
            pixels("fade-phase-" + std::to_string(phase) + "-duration-" + std::to_string(duration) + "-progress-" + std::to_string(progress));
        }
    for (int phase = 0; phase < 4; ++phase) for (bool touch : {false, true})
        for (bool warning : {false, true}) for (bool clip : {false, true}) {
            reset(); input.touch = touch; state.clip = clip;
            state.spriteMissing = warning ? 2 : 0; state.firstMissing = 25;
            engine.sceneFade = Fade(phase); engine.sceneFadeProgressMs = 150;
            engine.render(desktop.millis());
            pixels("stack-phase-" + std::to_string(phase) + "-touch-" + std::to_string(touch) + "-warning-" + std::to_string(warning) + "-clip-" + std::to_string(clip));
        }
    auto send = [&](Scene& scene, Touch type, int x, int y, uint32_t at) {
        return scene.onTouch({type, int16_t(x), int16_t(y), at});
    };
    const int width = Hal::DISPLAY_W, height = Hal::DISPLAY_H;
    for (bool nav : {false, true}) {
        auto& probe = reset(); state.navigation = nav;
        for (int y : {-1, 0, height - 33, height - 32, height - 1, height})
            for (int x : {-1, 0, (width - 1) / 3, width / 3, (width + 2) / 3,
                          2 * width / 3, (2 * width + 2) / 3, width - 1, width}) {
                const auto beforeButtons = state.buttons.size(), beforeTaps = state.taps.size();
                const unsigned targets = state.targets;
                assert(!send(probe, Touch::DOWN, x, y, 100));
                const bool hit = send(probe, Touch::UP, x, y, 120);
                const bool valid = x >= 0 && x < width && y >= 0 && y < height;
                assert(hit == valid);
                if (valid && nav && y >= height - 32) {
                    assert(state.targets == targets && state.buttons.size() == beforeButtons + 1);
                    const int column = x * 3 / width;
                    assert(state.buttons.back().btn == (column == 2 ? 0 : 1));
                    assert(state.buttons.back().action == (column == 0 ? BtnAction::LONG_PRESS : BtnAction::PRESSED));
                } else if (valid) assert(state.taps.size() == beforeTaps + 1);
                else assert(state.targets == targets);
                assert(!send(probe, Touch::UP, x, y, 121));
            }
        ++checks;
    }
    for (int column = 0; column < 3; ++column) for (int method = 0; method < 6; ++method) {
        auto& probe = reset(); const int x = (2 * column + 1) * width / 6, y = height - 16;
        send(probe, Touch::DOWN, x, y, 100);
        if (method == 0) send(probe, Touch::MOVE, x + 11, y, 120);
        if (method == 1) send(probe, Touch::CANCEL, x, y, 120);
        if (method == 2) probe.cancelTouch();
        if (method == 3) send(probe, Touch::MOVE, -1, y, 120);
        if (method == 4) send(probe, Touch::MOVE, x, y - 33, 120);
        const bool hit = send(probe, Touch::UP, x, y, method == 5 ? 1101 : 140);
        assert(!hit && state.buttons.empty()); ++checks;
    }
    for (uint32_t start : {0u, 0xffffff00u}) {
        auto& probe = reset(); send(probe, Touch::DOWN, 10, height - 16, start);
        assert(send(probe, Touch::UP, 20, height - 16, start + 1000));
        assert(state.buttons.size() == 1); ++checks;
    }
    // Engine input blocking and cancellation retain their existing precedence.
    for (int block = 0; block < 4; ++block) {
        auto& probe = reset(); const int x = width / 2, y = height - 16;
        send(probe, Touch::DOWN, x, y, desktop.millis());
        if (block == 0) display.lock = true;
        if (block == 1) input.down = true;
        if (block == 2) engine.sceneFade = Fade::OUT;
        if (block == 3) room.loaded_ = false;
        input.events.push_back({Touch::UP, int16_t(x), int16_t(y), desktop.millis() + 10});
        engine.processInput(desktop.millis());
        assert(state.buttons.empty() && state.taps.empty() && state.cancels > 0);
        display.lock = false; input.down = false; engine.sceneFade = Fade::NONE; room.loaded_ = true;
        input.events.push_back({Touch::UP, int16_t(x), int16_t(y), desktop.millis() + 11});
        engine.processInput(desktop.millis()); assert(state.buttons.empty()); ++checks;
    }
    reset();
    for (int i = 0; i < 10; ++i) input.events.push_back({Touch::MOVE, 10, 10, desktop.millis()});
    engine.processInput(desktop.millis()); assert(input.events.size() == 2); ++checks;
    for (int column = 0; column < 3; ++column) {
        auto& probe = reset(); state.changeScene = true;
        const int x = (2 * column + 1) * width / 6, y = height - 16;
        engine.currentId = SceneID::SOCIAL;
        send(probe, Touch::DOWN, x, y, 100);
        assert(send(probe, Touch::UP, x, y, 120));
        assert(state.destroyed == 1 && engine.currentId == SceneID::MENU); ++checks;
    }
    {
        auto& probe = reset(); state.changeScene = true; engine.currentId = SceneID::SOCIAL;
        input.events.push_back({Touch::DOWN, 10, 10, desktop.millis()});
        input.events.push_back({Touch::UP, 10, 10, desktop.millis() + 10});
        engine.processInput(desktop.millis());
        assert(state.destroyed == 1 && engine.currentId == SceneID::MENU && engine.sceneDirty && engine.sceneUpdateScheduled); ++checks;
        (void)probe;
    }
    // A real contact submenu uses the same virtual select/back actions.
    reset(); engine.currentScene = std::make_unique<MenuScene>();
    auto* menu = static_cast<MenuScene*>(engine.currentScene.get()); menu->onEnter();
    menu->pushView(MenuScene::ViewMode::STORAGE);
    assert(!send(*menu, Touch::DOWN, width / 2, height - 16, 100));
    assert(send(*menu, Touch::UP, width / 2, height - 16, 120));
    assert(!send(*menu, Touch::DOWN, 10, height - 16, 200));
    assert(send(*menu, Touch::UP, 10, height - 16, 220));
    assert(menu->viewMode == MenuScene::ViewMode::MENU); ++checks;
    reset(); engine.currentId = SceneID::SOCIAL;
    assert(!engine.fadeToScene(SceneID::SOCIAL));
    assert(engine.fadeToScene(SceneID::MENU, 0)); assert(engine.sceneFadeDurationMs == 1);
    assert(!engine.fadeToScene(SceneID::SETTINGS));
    engine.updateSceneFade(desktop.millis() + 1); assert(engine.sceneFade == Fade::HOLD);
    const auto hold = engine.sceneFadeStartedMs;
    engine.updateSceneFade(hold + 499); assert(engine.sceneFade == Fade::HOLD);
    engine.updateSceneFade(hold + 500); assert(engine.sceneFade == Fade::IN && engine.currentId == SceneID::MENU && state.destroyed == 1);
    engine.updateSceneFade(desktop.millis() + 1); assert(engine.sceneFade == Fade::NONE); ++checks;
    reset(); assert(engine.fadeToScene(SceneID::SETTINGS, 300));
    const auto start = engine.sceneFadeLastStepMs;
    engine.updateSceneFade(start + 1000); assert(engine.sceneFadeProgressMs == GameEngine::FRAME_MS); ++checks;
    reset(); state.navigation = false;
    const auto frameCount = desktop.presentCount(); engine.render(desktop.millis());
    assert(desktop.presentCount() == frameCount + 1 && state.renders == 1); ++checks;
    const auto idle = engine.currentScene->update(desktop.millis(), 0);
    assert(!idle.redraw && idle.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE); ++checks;
    room.loaded_ = roomLoaded; font.sarasaCjk_.loaded = sarasaLoaded;
    font.unsciiAscii_.loaded = asciiLoaded;
    std::printf("application-ui-checkpoints=%u\n", checks);
}
