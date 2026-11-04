#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "core/AudioManager.h"
#include "core/CryPlayer.h"
#include "core/UiStrings.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/SettingsScene.h"
#undef private

#if STICKMON_SETTINGS_UI_BASELINE
#define SETTINGS(scene) (scene)
using SettingsPage = SettingsScene;
#else
#define SETTINGS(scene) ((scene).ui)
using SettingsPage = Ui::SettingsUiBase;
#endif
using Mode = SettingsPage::ViewMode;
using Type = Platform::IInputDevice::TouchType;

class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

class AudioProbe final : public Platform::IAudioDevice {
public:
    explicit AudioProbe(Platform::IAudioDevice& device) : device(device) {}
    unsigned previews = 0, stops = 0;
    void setVolume(uint8_t value) override { device.setVolume(value); }
    uint8_t volume() const override { return device.volume(); }
    bool playPcmU8(const uint8_t* data, size_t count, uint32_t rate) override {
        return device.playPcmU8(data, count, rate);
    }
    bool playPcmU8Channel(const uint8_t* data, size_t count, uint32_t rate,
                          uint8_t channel, bool stopCurrent) override {
        if (channel == AudioManager::CRY_CHANNEL) ++previews;
        return device.playPcmU8Channel(data, count, rate, channel, stopCurrent);
    }
    void setChannelVolume(uint8_t channel, uint8_t value) override { device.setChannelVolume(channel, value); }
    bool playing() const override { return device.playing(); }
    uint8_t queuedPcm(uint8_t channel) const override { return device.queuedPcm(channel); }
    void stop() override { device.stop(); }
    void stopChannel(uint8_t channel) override {
        if (channel == AudioManager::CRY_CHANNEL) ++stops;
        device.stopChannel(channel);
    }
    bool beginMicrophone() override { return device.beginMicrophone(); }
    void endMicrophone() override { device.endMicrophone(); }
    bool recordMicrophone(int16_t* data, size_t count, uint32_t rate) override {
        return device.recordMicrophone(data, count, rate);
    }
    bool microphoneRecording() const override { return device.microphoneRecording(); }
    bool microphoneActive() const override { return device.microphoneActive(); }
private:
    Platform::IAudioDevice& device;
};

class BlobProbe final : public Platform::IBlobStore {
public:
    explicit BlobProbe(Platform::IBlobStore& store) : store(store) {}
    bool failClear = false;
    unsigned clears = 0, writes = 0;
    bool initialize() override { return store.initialize(); }
    size_t blobSize(const char* ns, const char* key) override { return store.blobSize(ns, key); }
    bool readBlob(const char* ns, const char* key, void* output, size_t length) override {
        return store.readBlob(ns, key, output, length);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t length) override {
        ++writes; return store.writeBlob(ns, key, data, length);
    }
    bool removeBlob(const char* ns, const char* key) override { return store.removeBlob(ns, key); }
    bool clearNamespace(const char* ns) override {
        ++clears; return !failClear && store.clearNamespace(ns);
    }
private:
    Platform::IBlobStore& store;
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    const auto bundle = desktop.serviceBundle();
    TouchInput input;
    AudioProbe audio(bundle.audio);
    BlobProbe blobs(bundle.blobs);
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, audio, bundle.imu, bundle.power, blobs,
        bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    SettingsScene settings;
    unsigned checkpoints = 0, resets = 0;
    constexpr unsigned helpCount = sizeof(Ui::Settings::HELP_PAGE_TITLES) /
        sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
    const ButtonEvent confirm{0, BtnAction::PRESSED};
    const ButtonEvent next{1, BtnAction::PRESSED};
    const ButtonEvent back{1, BtnAction::LONG_PRESS};
    auto setup = [&]() {
        CryPlayer::ins().stop(); CryPlayer::ins().update();
        GameRandom::seed(1234);
        state = Game::GameState{};
        state.oobeDone = true;
        state.tutorialFlags = 0xff;
        state.teamCount = 1;
        state.team[0] = Game::MonsterFactory::create(1, 25);
        state.team[0].origin = Game::Origin::STARTER;
        state.settings.brightness = 128;
        state.settings.volume = 50;
        state.settings.speedIndex = 0;
        state.settings.idleTimeoutIndex = 0;
        Hal::ins().setBrightness(128);
        Hal::ins().setAudioVolume(50);
        SETTINGS(settings).toast = nullptr;
        SETTINGS(settings).toastUntil = 0;
        settings.onEnter();
        engine.saveCoordinator.reset(desktop.millis());
        blobs.failClear = false;
        audio.previews = audio.stops = 0;
    };
    auto settle = [&]() {
        for (int i = 0; i < 100; ++i) {
            desktop.advanceMs(20);
            settings.update(desktop.millis(), .02f);
        }
    };
    auto capture = [&](const std::string& name) {
        PixelRenderer::clear(0);
        settings.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        assert(std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file) == fb.width * fb.height);
        std::fclose(file);
    };
    auto press = [&](ButtonEvent event) { assert(settings.onButton(event)); };
    auto select = [&](SettingsPage::Item item) {
        assert(SETTINGS(settings).viewMode == Mode::MENU);
        for (int i = 0; SETTINGS(settings).cursor != item; ++i) {
            assert(i < SettingsPage::COUNT); press(next);
        }
    };
    auto open = [&](SettingsPage::Item item) { select(item); press(confirm); };
    auto touch = [&](Scene& scene, Type type, int x, int y) {
        desktop.advanceMs(10);
        return scene.onTouch({type, int16_t(x), int16_t(y), desktop.millis()});
    };
    auto tap = [&](Scene& scene, int x, int y) {
        touch(scene, Type::DOWN, x, y);
        return touch(scene, Type::UP, x, y);
    };
    auto sameState = [&](const Game::GameState& before) {
        assert(std::memcmp(&state, &before, sizeof(state)) == 0);
    };
    auto soon = [&]() {
        assert(engine.saveCoordinator.dirty());
        assert(engine.saveCoordinator.soon_);
    };
    auto parked = [&]() {
        settle();
        const auto result = settings.update(desktop.millis(), .02f);
        assert(!result.redraw && result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    };

    // These visual fixtures are generated only from the frozen legacy tree.
    // State injection covers displays; real behavior uses scene entry points below.
    setup();
    for (int row = 0; row < SettingsPage::COUNT; ++row) {
        select(static_cast<SettingsPage::Item>(row)); settle();
        capture("menu-" + std::to_string(row));
    }
    setup();
    for (int level : {32,64,128,192,255}) {
        state.settings.brightness = level; Hal::ins().setBrightness(level);
        capture("brightness-" + std::to_string(level));
    }
    select(SettingsPage::VOLUME); settle();
    for (int volume : {0,37,100}) {
        state.settings.volume = volume;
        capture("volume-" + std::to_string(volume));
    }
    select(SettingsPage::GAME_SPEED); settle();
    for (int speed = 0; speed < 4; ++speed) {
        state.settings.speedIndex = speed;
        capture("speed-" + std::to_string(speed));
    }
    select(SettingsPage::POWER_SAVE); settle();
    for (int power = 0; power < 5; ++power) {
        state.settings.idleTimeoutIndex = power;
        capture("power-" + std::to_string(power));
    }
    setup(); open(SettingsPage::HELP);
    for (unsigned page = 0; page < helpCount; ++page) {
        SETTINGS(settings).helpPage = page;
        capture("help-" + std::to_string(page));
    }
    setup(); open(SettingsPage::RESET_GAME); settle();
    for (bool yes : {false,true}) {
        SETTINGS(settings).resetConfirmYes = yes;
        capture(yes ? "reset-yes" : "reset-no");
    }
    for (Mode mode : {Mode::MENU,Mode::HELP,Mode::RESET_CONFIRM}) {
        SETTINGS(settings).viewMode = mode;
        SETTINGS(settings).toast = Ui::Settings::RESET_FAILED;
        SETTINGS(settings).toastUntil = desktop.millis() + 1500;
        capture("notice-" + std::to_string(static_cast<int>(mode)));
    }
    setup();
#if STICKMON_UI_COMPACT_TOUCH
    for (int offset : {25,75,104}) {
        SETTINGS(settings).touchScroll = offset;
        capture("scroll-" + std::to_string(offset));
    }
    touch(settings, Type::DOWN, 180, 64);
    capture("slider-pressed");
    settings.cancelTouch();
#else
    for (int offset : {5,10,21}) {
        SETTINGS(settings).menuScroll = offset;
        capture("scroll-" + std::to_string(offset));
    }
#endif

    setup();
    state.settings.volume = 37;
    settings.onEnter();
#if STICKMON_UI_COMPACT_TOUCH
    assert(state.settings.volume == 37 && !engine.saveCoordinator.dirty());
#else
    assert(state.settings.volume == 30); soon();
#endif
    assert(Hal::ins().getAudioVolume() == state.settings.volume);
    ++checkpoints;

    setup(); state.settings.volume = 250;
    settings.onEnter();
    assert(state.settings.volume == 100 && audio.volume() == 100); soon();
    ++checkpoints;

    setup(); Hal::ins().setBrightness(32); state.settings.brightness = 77;
    for (int expected : {64,128,192,255,32}) {
        open(SettingsPage::BRIGHTNESS);
        assert(Hal::ins().getBrightness() == expected && state.settings.brightness == expected);
        soon(); engine.saveCoordinator.reset(desktop.millis());
    }
    ++checkpoints;

    setup(); state.settings.volume = 90; Hal::ins().setAudioVolume(90);
    open(SettingsPage::VOLUME);
    assert(state.settings.volume == 100 && audio.previews == 1); soon();
    press(confirm); assert(state.settings.volume == 0 && audio.stops == 1);
    press(confirm); assert(state.settings.volume == 10 && audio.previews == 2);
    state.teamCount = 0;
    press(confirm); assert(state.settings.volume == 20 && audio.previews == 2);
    ++checkpoints;

    setup();
    for (int i = 1; i <= 4; ++i) {
        open(SettingsPage::GAME_SPEED);
        assert(state.settings.speedIndex == i % 4); soon();
        engine.saveCoordinator.reset(desktop.millis());
    }
    ++checkpoints;

    setup();
    for (int i = 1; i <= 5; ++i) {
        open(SettingsPage::POWER_SAVE);
        assert(state.settings.idleTimeoutIndex == i % 5);
        assert(engine.idleTimeoutLabel()[0]); soon();
        engine.saveCoordinator.reset(desktop.millis());
    }
    ++checkpoints;

    setup(); const auto beforeNavigation = state;
#if STICKMON_UI_COMPACT_TOUCH
    const int order[] = {SettingsPage::VOLUME,SettingsPage::GAME_SPEED,SettingsPage::POWER_SAVE,
        SettingsPage::HELP,SettingsPage::RESET_GAME,SettingsPage::BRIGHTNESS};
#else
    const int order[] = {SettingsPage::GAME_SPEED,SettingsPage::VOLUME,SettingsPage::POWER_SAVE,
        SettingsPage::HELP,SettingsPage::RESET_GAME,SettingsPage::BRIGHTNESS};
#endif
    for (int item : order) { press(next); assert(SETTINGS(settings).cursor == item); }
    sameState(beforeNavigation); assert(!engine.saveCoordinator.dirty());
    ++checkpoints;

    setup(); open(SettingsPage::HELP); const auto beforeHelp = state;
    assert(SETTINGS(settings).viewMode == Mode::HELP && SETTINGS(settings).helpPage == 0);
    press(confirm); sameState(beforeHelp);
    assert(!settings.onButton({0, BtnAction::LONG_PRESS}));
    for (unsigned i = 1; i <= helpCount; ++i) {
        press(next); assert(SETTINGS(settings).helpPage == i % helpCount);
    }
    press(back); assert(SETTINGS(settings).viewMode == Mode::MENU); sameState(beforeHelp);
    ++checkpoints;

    setup(); const auto beforeReset = state; const auto writesBefore = blobs.writes;
    open(SettingsPage::RESET_GAME);
    assert(SETTINGS(settings).viewMode == Mode::RESET_CONFIRM && !SETTINGS(settings).resetConfirmYes);
    press(confirm); assert(SETTINGS(settings).viewMode == Mode::MENU); sameState(beforeReset);
    open(SettingsPage::RESET_GAME); press(next);
    assert(SETTINGS(settings).resetConfirmYes);
    press({0,BtnAction::LONG_PRESS});
    assert(SETTINGS(settings).viewMode == Mode::MENU && !SETTINGS(settings).resetConfirmYes);
    open(SettingsPage::RESET_GAME); press(back); sameState(beforeReset);
    assert(blobs.writes == writesBefore);
    ++checkpoints;

    setup(); blobs.failClear = true; const auto beforeFailedReset = state;
    open(SettingsPage::RESET_GAME); press(next); press(confirm);
    sameState(beforeFailedReset);
    assert(SETTINGS(settings).viewMode == Mode::MENU);
    assert(SETTINGS(settings).toast && std::strcmp(SETTINGS(settings).toast, Ui::Settings::RESET_FAILED) == 0);
    assert(SETTINGS(settings).toastUntil == desktop.millis() + 1500);
    settle(); assert(!SETTINGS(settings).toast);
    ++checkpoints;

    setup(); open(SettingsPage::HELP);
    SETTINGS(settings).toast = Ui::Settings::RESET_FAILED;
    const auto deadline = SETTINGS(settings).toastUntil = desktop.millis() + 1500;
    settings.onEnter();
    assert(SETTINGS(settings).toast && SETTINGS(settings).toastUntil == deadline);
    assert(SETTINGS(settings).cursor == 0 && SETTINGS(settings).viewMode == Mode::MENU);
    press(confirm); assert(!SETTINGS(settings).toast && SETTINGS(settings).toastUntil == 0);
    ++checkpoints;

    setup(); const auto beforeInvalid = state;
    assert(!settings.onButton({2, BtnAction::PRESSED}));
    assert(!settings.onButton({0, BtnAction::UP}));
    assert(!settings.onTap(-1) && !settings.onTap(SettingsPage::COUNT));
    assert(settings.touchTarget(-1,-1) == -1);
    sameState(beforeInvalid);
    ++checkpoints;

    setup();
#if STICKMON_UI_COMPACT_TOUCH
    assert(!settings.touchNavigation());
    assert(tap(settings,272,64)); assert(state.settings.brightness == 255);
    assert(tap(settings,120,64)); assert(state.settings.brightness == 32);
    assert(tap(settings,176,114)); assert(state.settings.volume == 37 && audio.previews == 1);
    const auto oldPreviews = audio.previews;
    assert(tap(settings,176,114)); assert(audio.previews == oldPreviews);
    assert(!touch(settings,Type::UP,176,114));
#else
    assert(settings.touchNavigation());
    assert(tap(settings,40,12)); assert(state.settings.brightness == 192);
    assert(tap(settings,120,134)); assert(SETTINGS(settings).cursor == SettingsPage::GAME_SPEED);
    assert(tap(settings,220,134)); assert(state.settings.speedIndex == 1);
#endif
    ++checkpoints;

    setup();
#if STICKMON_UI_COMPACT_TOUCH
    touch(settings,Type::DOWN,180,114);
    touch(settings,Type::MOVE,400,114);
    assert(state.settings.volume == 100 && audio.previews == 0);
    touch(settings,Type::UP,400,114);
    assert(audio.previews == 1);
    assert(!touch(settings,Type::UP,400,114) && audio.previews == 1);
    touch(settings,Type::DOWN,180,114);
    touch(settings,Type::MOVE,0,114);
    assert(state.settings.volume == 0 && audio.stops == 1);
    touch(settings,Type::UP,0,114); assert(audio.previews == 1);
    touch(settings,Type::DOWN,180,114);
    touch(settings,Type::MOVE,220,114);
    const auto canceledVolume = state.settings.volume;
    touch(settings,Type::CANCEL,220,114);
    assert(!touch(settings,Type::UP,220,114));
    assert(state.settings.volume == canceledVolume && audio.previews == 1);
    touch(settings,Type::DOWN,180,114);
    static_cast<Scene*>(&settings)->cancelTouch();
    touch(settings,Type::MOVE,270,114); touch(settings,Type::UP,270,114);
    assert(state.settings.volume == canceledVolume && audio.previews == 1);
    touch(settings,Type::DOWN,180,64); touch(settings,Type::MOVE,400,64);
    assert(state.settings.brightness == 255);
    touch(settings,Type::UP,400,64);
    touch(settings,Type::DOWN,180,64); touch(settings,Type::MOVE,0,64);
    touch(settings,Type::UP,0,64); assert(state.settings.brightness == 32);
#else
    touch(settings,Type::DOWN,40,12); settings.cancelTouch();
    assert(!touch(settings,Type::UP,40,12));
    assert(state.settings.brightness == 128);
    touch(settings,Type::DOWN,40,12); touch(settings,Type::CANCEL,40,12);
    assert(!touch(settings,Type::UP,40,12));
#endif
    ++checkpoints;

    setup();
#if STICKMON_UI_COMPACT_TOUCH
    touch(settings,Type::DOWN,180,114); touch(settings,Type::MOVE,180,60);
    touch(settings,Type::UP,180,60);
    assert(SETTINGS(settings).touchScroll > 0);
    assert(state.settings.volume == 50 && state.settings.brightness == 128 && audio.previews == 0);
    select(SettingsPage::RESET_GAME);
    const auto row = TouchUi::listLayout().cell(5,SETTINGS(settings).touchScroll);
    assert(row.y >= 38 && row.y + row.height <= Hal::DISPLAY_H);
    touch(settings,Type::DOWN,100,row.y+20); touch(settings,Type::CANCEL,100,row.y+20);
    touch(settings,Type::UP,100,row.y+20);
    assert(SETTINGS(settings).viewMode == Mode::MENU);
    assert(tap(settings,100,row.y+20)); assert(SETTINGS(settings).viewMode == Mode::RESET_CONFIRM);
    assert(settings.touchTarget(100,64) == -1);
#else
    const auto beforeTouchDrag = state;
    touch(settings,Type::DOWN,40,12); touch(settings,Type::MOVE,40,60);
    assert(!touch(settings,Type::UP,40,60)); sameState(beforeTouchDrag);
#endif
    ++checkpoints;

    setup(); open(SettingsPage::HELP);
#if STICKMON_UI_COMPACT_TOUCH
    assert(tap(settings,60,218) && SETTINGS(settings).helpPage == helpCount-1);
    assert(tap(settings,60,218) && SETTINGS(settings).helpPage == helpCount-2);
    assert(tap(settings,220,218) && SETTINGS(settings).helpPage == helpCount-1);
    assert(tap(settings,48,24) && SETTINGS(settings).viewMode == Mode::MENU);
    open(SettingsPage::RESET_GAME);
    touch(settings,Type::DOWN,220,218); touch(settings,Type::CANCEL,220,218);
    assert(!touch(settings,Type::UP,220,218) && state.oobeDone);
    assert(tap(settings,60,218) && SETTINGS(settings).viewMode == Mode::MENU);
#else
    assert(tap(settings,120,134) && SETTINGS(settings).helpPage == 1);
    assert(tap(settings,40,134) && SETTINGS(settings).viewMode == Mode::MENU);
#endif
    ++checkpoints;

    setup(); parked(); open(SettingsPage::HELP); parked();
    press(back); open(SettingsPage::RESET_GAME); parked(); press(back); settle();
    SETTINGS(settings).toast = Ui::Settings::RESET_FAILED;
    SETTINGS(settings).toastUntil = desktop.millis() + 1500;
    auto demand = settings.update(desktop.millis(),.02f);
    assert(!demand.redraw && demand.nextUpdateDelayMs == 1500);
    desktop.advanceMs(1499); demand = settings.update(desktop.millis(),.02f);
    assert(!demand.redraw && demand.nextUpdateDelayMs == 1 && SETTINGS(settings).toast);
    desktop.advanceMs(1); demand = settings.update(desktop.millis(),.02f);
    assert(demand.redraw && demand.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE && !SETTINGS(settings).toast);
    parked();
    ++checkpoints;

    // The following operations synchronously destroy their engine-owned scene.
    // Touch and button paths must return without accessing that old allocation.
    auto ownedSettings = [&]() {
        setup();
        engine.requestScene(SceneID::MAIN,false);
        engine.requestScene(SceneID::SETTINGS,false);
        assert(engine.currentId == SceneID::SETTINGS);
        return engine.currentScene.get();
    };
    auto ownedSelect = [&](Scene* owned, SettingsPage::Item item) {
        const auto* scene = static_cast<SettingsScene*>(owned);
        for (int i = 0; SETTINGS(*scene).cursor != item; ++i) {
            assert(i < SettingsPage::COUNT); assert(owned->onButton(next));
        }
    };
    for (bool useTouch : {false,true}) {
        Scene* owned = ownedSettings();
        if (!useTouch) assert(owned->onButton(back));
        else {
#if STICKMON_UI_COMPACT_TOUCH
            assert(tap(*owned,48,24));
#else
            assert(tap(*owned,40,134));
#endif
        }
        assert(engine.currentId == SceneID::MENU && state.oobeDone);
    }
    ++checkpoints;

    for (bool useTouch : {false,true}) {
        Scene* owned = ownedSettings();
        ownedSelect(owned,SettingsPage::HELP); assert(owned->onButton(confirm));
        for (unsigned i = 1; i < helpCount; ++i) assert(owned->onButton(next));
        if (!useTouch) assert(owned->onButton(confirm));
        else {
#if STICKMON_UI_COMPACT_TOUCH
            assert(tap(*owned,220,218));
#else
            assert(tap(*owned,220,134));
#endif
        }
        assert(engine.currentId == SceneID::MAIN && state.tutorialFlags == 0);
    }
    ++checkpoints;

    for (bool useTouch : {false,true}) {
        Scene* owned = ownedSettings();
        ownedSelect(owned,SettingsPage::RESET_GAME); assert(owned->onButton(confirm));
        assert(!SETTINGS(*static_cast<SettingsScene*>(owned)).resetConfirmYes);
        if (!useTouch) { assert(owned->onButton(next)); assert(owned->onButton(confirm)); }
        else {
#if STICKMON_UI_COMPACT_TOUCH
            assert(tap(*owned,220,218));
#else
            assert(tap(*owned,120,134)); assert(tap(*owned,220,134));
#endif
        }
        assert(engine.currentId == SceneID::HATCH && !state.oobeDone && state.teamCount == 0);
        ++resets;
    }
    ++checkpoints;
    std::printf("settings-checkpoints=%u reset=%u tutorial=%u\n",checkpoints,resets,state.tutorialFlags);
    engine.currentScene.reset();
    CryPlayer::ins().stop(); CryPlayer::ins().update();
}
