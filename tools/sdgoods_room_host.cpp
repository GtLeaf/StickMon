// SPDX-License-Identifier: Apache-2.0
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/MenuScene.h"
#include "scenes/SettingsScene.h"
#include "scenes/ShopScene.h"
#include "scenes/ExploreScene.h"
#include "scenes/ShowerScene.h"
#include "scenes/HatchScene.h"
#include "scenes/MainScene.h"
#undef private
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/FontResource.h"
#include "core/SaveCodec.h"
#include "core/SaveManager.h"
#include "game/GameRandom.h"
#include "game/ItemInventory.h"
#include "game/HatchService.h"
#include "game/MonsterFactory.h"
#include "game/ExploreDestinations.h"
#include "game/Tutorial.h"
#include "ui/SceneSurface.h"
#include "presentation/PixelRenderer.h"
#include "ResourceArchive.h"
#include "RoomSession.h"
#include "Cst816TouchState.h"
#include "TouchCalibrationStore.h"
#include "ui/round_touch/RoomControls.h"
#include "ui/round_touch/RoundPage.h"
#include "ui/round_touch/ExploreSelectUi.h"
#include "ui/round_touch/ExploreRouteUi.h"
#include "ui/round_touch/ExploreBattleUi.h"
#include "ui/common/ExploreBattleUiContent.h"
#include "ui/round_touch/TouchTutorial.h"

namespace fs = std::filesystem;
using Type = Platform::IInputDevice::TouchType;
using Event = Platform::IInputDevice::TouchEvent;

std::vector<uint8_t> readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void writeFile(const fs::path& path, const void* data, size_t length) {
    std::ofstream output(path, std::ios::binary);
    output.write(static_cast<const char*>(data), length);
    assert(output.good());
}

class DiskBlobs final : public Platform::IBlobStore {
public:
    explicit DiskBlobs(fs::path root) : root_(std::move(root)) { fs::create_directories(root_); }
    bool initialize() override { return true; }
    fs::path path(const char* ns, const char* key) const { return root_ / (std::string(ns) + "--" + key); }
    size_t blobSize(const char* ns, const char* key) override {
        const auto file = path(ns, key);
        return fs::exists(file) ? fs::file_size(file) : 0;
    }
    bool readBlob(const char* ns, const char* key, void* output, size_t size) override {
        const auto bytes = readFile(path(ns, key));
        if (!output || bytes.size() != size) return false;
        std::memcpy(output, bytes.data(), size);
        return true;
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t size) override {
        if (failWrites) return false;
        const auto file = path(ns, key);
        const auto pending = fs::path(file.string() + ".tmp");
        writeFile(pending, data, size);
        fs::rename(pending, file);
        return true;
    }
    bool removeBlob(const char* ns, const char* key) override { fs::remove(path(ns, key)); return true; }
    bool clearNamespace(const char* ns) override {
        const std::string prefix = std::string(ns) + "--";
        for (const auto& entry : fs::directory_iterator(root_))
            if (entry.path().filename().string().rfind(prefix, 0) == 0) fs::remove(entry.path());
        return true;
    }
    bool failWrites = false;
private:
    fs::path root_;
};

class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return true; }
    bool touchCalibrationSupported() const override { return true; }
    void bypassTouchCalibration(bool value) override {
        bypass = value;
        events.push_back({Type::CANCEL, 0, 0, 0});
    }
    bool saveTouchCalibration(const Platform::TouchTransform& candidate) override {
        if (!Sdgoods::TouchCalibrationStore::save(*store, candidate)) return false;
        transform = candidate;
        bypassTouchCalibration(false);
        return true;
    }
    bool pollTouch(Event& event) override {
        if (events.empty()) return false;
        event = events.front(); events.pop_front();
        if (!bypass && event.type != Type::CANCEL) {
            const int x = event.x, y = event.y;
            event.x = std::clamp(int(std::lround(transform.x(x,y))), 0, 359);
            event.y = std::clamp(int(std::lround(transform.y(x,y))), 0, 359);
        }
        return true;
    }
    std::deque<Event> events;
    Platform::IBlobStore* store = nullptr;
    Platform::TouchTransform transform;
    bool bypass = false;
};

class NoPeers final : public Platform::IPeerTransport {
public:
    bool enable() override { return false; }
    void end() override {}
    bool active() const override { return false; }
    bool send(const uint8_t[6], const void*, size_t) override { return false; }
    bool receive(Platform::PeerPacket&) override { return false; }
};

class PageMemory final : public Platform::IMemoryAllocator {
public:
    explicit PageMemory(Platform::IMemoryAllocator& base) : base(base) {}
    void* allocate(size_t size, bool external) override {
        if (failPage && size==240*240*sizeof(uint16_t)) return nullptr;
        if (failStatusSlide && size==360*308*2*sizeof(uint16_t)) return nullptr;
        return base.allocate(size, external);
    }
    void release(void* data) override { base.release(data); }
    size_t externalFree() const override { return base.externalFree(); }
    bool failPage=false;
    bool failStatusSlide=false;
private:
    Platform::IMemoryAllocator& base;
};

class TestPower final : public Platform::IPowerDevice {
public:
    int batteryLevel() override { return level; }
    bool batteryCharging() override { return charging; }
    int level = 73;
    bool charging = false;
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 42; }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return 7 * 1024 * 1024; }
    void restart() override { ++restarts; }
    void enterDeepSleep(uint64_t, bool) override { assert(false); }
    int restarts = 0;
};

void capture(const fs::path& path) {
    std::ofstream image(path, std::ios::binary);
    image << "P6\n360 360\n255\n";
    auto& canvas = PixelRenderer::canvas();
    for (int y = 0; y < 360; ++y) for (int x = 0; x < 360; ++x) {
        uint16_t pixel = canvas.readPixel(x, y);
        if ((x - 180) * (x - 180) + (y - 180) * (y - 180) >= 180 * 180) pixel = 0;
        const char rgb[] = {char(((pixel >> 11) & 31) * 255 / 31),
            char(((pixel >> 5) & 63) * 255 / 63), char((pixel & 31) * 255 / 31)};
        image.write(rgb, 3);
    }
}

void testNativeTouch() {
    Sdgoods::Cst816TouchState state;
    Event e;
    const uint8_t a[]{1, 0, 0, 1, 103}; // raw (0,359) => logical (359,359)
    const uint8_t b[]{1, 1, 103, 0, 0}; // raw (359,0) => logical (0,0)
    const uint8_t up[5]{};
    state.sample(a, 20); assert(state.poll(e) && e.type == Type::DOWN && e.x == 359 && e.y == 359);
    assert(!state.poll(e));
    state.sample(b, 40); assert(state.poll(e) && e.type == Type::MOVE && e.x == 0 && e.y == 0);
    state.sample(nullptr, 60); assert(state.poll(e) && e.type == Type::CANCEL);
    state.sample(a, 80); assert(!state.poll(e));
    state.sample(up, 100); assert(!state.poll(e));
    state.sample(b, 120); assert(state.poll(e) && e.type == Type::DOWN);
    state.sample(up, 140); assert(state.poll(e) && e.type == Type::UP && e.x == 0 && e.y == 0);
    assert(!state.poll(e));
    const uint8_t invalid[]{1, 1, 104, 0, 0};
    state.sample(a, 160); assert(state.poll(e));
    state.sample(invalid, 180); assert(state.poll(e) && e.type == Type::CANCEL);
    std::puts("PASS native sensor axes, single-delivery events and I2C/invalid sample cancellation");
}

int main(int argc, char** argv) {
    testNativeTouch();
    assert(argc == 5);
    const std::string mode = argv[1];
    const fs::path output = argv[2];
    fs::create_directories(output);
    auto archiveBytes = readFile(argv[3]);
    Sdgoods::ResourceArchive archive(archiveBytes.data(), archiveBytes.size());
    assert(archive.mount());
    auto manifest = archive.open("/packs/dev/manifest.json");
    assert(manifest && manifest.size() > 0);
    assert(!manifest.seek(manifest.size() + 1));
    assert(manifest.seek(manifest.size()));
    uint8_t scratch;
    assert(manifest.read(&scratch, 1) == 0);
    Sdgoods::ResourceArchive truncated(archiveBytes.data(), 10);
    assert(!truncated.mount());

    DesktopPlatform desktop;
    TouchInput touch;
    TestPower power;
    NoPeers peers;
    DiskBlobs blobs(argv[4]);
    touch.store = &blobs;
    const auto& d = desktop.serviceBundle();
    PageMemory memory(d.memory);
    Platform::Services services{d.lifecycle, d.clock, d.logger, touch, d.display, d.audio,
        d.imu, power, blobs, archive, memory, peers};
    Platform::bind(services);
    GameRandom::seed(42);
    if (mode == "fonts" || mode == "fonts-invalid") {
        std::vector<uint16_t> guarded(360*360+32,0xa55a);
        for (bool swapped : {false,true}) {
            PixelRenderer::bind({guarded.data()+16,360,360,swapped});
            const uint16_t background=PixelRenderer::rgb(37,65,91);
            auto& c=PixelRenderer::canvas();
            for (int height : {16,20}) {
                auto* glyph=FontResource::ins().findCoverageGlyph(0x5546,height);
                if (mode == "fonts-invalid") {
                    assert(!glyph);
                    PixelRenderer::clear(background);
                    PixelRenderer::textSmooth(40,40,Ui::SHOP,0xffff,height);
                    unsigned ink=0;
                    for(int y=40;y<40+height;++y)for(int x=40;x<40+height*2;++x)
                        ink+=c.readPixel(x,y)!=background;
                    assert(ink>0);
                    continue;
                }
                assert(glyph);
                assert(!FontResource::ins().findCoverageGlyph('A',height));
                PixelRenderer::clear(background);
                PixelRenderer::textSmooth(40,40,Ui::SHOP,0xffff,height);
                unsigned solid=0,partial=0;
                for(int y=0;y<height;++y)for(int x=0;x<height;++x) {
                    const int i=y*height+x;
                    const int a=((i&1)?(glyph[i/2]&15):(glyph[i/2]>>4))*17;
                    const auto channel=[&](int shift,int mask) {
                        const int bg=((background>>shift)&mask)*255/mask;
                        return uint8_t((255*a+bg*(255-a))/255);
                    };
                    const auto expected=a==0?background:PixelRenderer::rgb(
                        channel(11,31),channel(5,63),channel(0,31));
                    assert(c.readPixel(40+x,40+y)==expected);
                    solid+=a==255;partial+=a>0&&a<255;
                }
                assert(solid>0 && partial>0);
                assert(c.readPixel(39,40)==background && c.readPixel(40,39)==background);
            }
            assert(!FontResource::ins().findCoverageGlyph(0x5546,18));
            for(int height=16;height<=20;++height) {
                PixelRenderer::clear(background);
                c.setClipRect(4,4,352,352);
                PixelRenderer::textSmooth(-10,-10,Ui::SHOP,0xffff,height);
                PixelRenderer::textSmooth(350,350,Ui::SHOP,0xffff,height);
                c.clearClipRect();
                assert(c.readPixel(0,0)==background && c.readPixel(359,359)==background);
                for(int i=0;i<16;++i)assert(guarded[i]==0xa55a && guarded[guarded.size()-1-i]==0xa55a);
            }
        }
        std::puts("PASS native A4 glyphs, coverage blending, byte order, clipping and missing-font fallback");
        return 0;
    }
    if(mode=="surface") {
        std::vector<uint16_t> guarded(360*360+32,0xa55a);
        Platform::FrameBuffer565 frame{guarded.data()+16,360,360,true};
        PixelRenderer::bind(frame);Ui::SceneSurface::select(SceneID::MENU);
        memory.failPage=true;
        assert(!Ui::SceneSurface::begin(frame));
        Event touchEvent{Type::DOWN,180,180,0};
        assert(!Ui::SceneSurface::mapTouch(touchEvent));
        assert(PixelRenderer::canvas().width()==360);
        memory.failPage=false;
        assert(Ui::SceneSurface::begin(frame));
        PixelRenderer::clear(0x001f);PixelRenderer::canvas().drawPixel(0,0,0xf800);
        PixelRenderer::canvas().drawPixel(239,239,0x07e0);Ui::SceneSurface::finish();
        assert(PixelRenderer::canvas().width()==360);
        assert(frame.pixels[60*360+60]==0x00f8 && frame.pixels[299*360+299]==0xe007);
        assert(frame.pixels[61*360+61]==0x1f00 && frame.pixels[0]==0 && frame.pixels[300*360+300]==0);
        for(int i=0;i<16;++i)assert(guarded[i]==0xa55a && guarded[guarded.size()-1-i]==0xa55a);
        { Ui::SceneSurface::InputScope scope(frame); assert(PixelRenderer::canvas().width()==240); }
        assert(PixelRenderer::canvas().width()==360);
        touchEvent={Type::DOWN,60,60,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.x==0 && touchEvent.y==0);
        touchEvent={Type::UP,299,299,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.x==239 && touchEvent.y==239);
        touchEvent={Type::MOVE,300,100,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.type==Type::CANCEL);
        assert(Ui::SceneSurface::begin(frame));
        Ui::SceneSurface::pageHeader(Ui::BAG, false);
        Ui::SceneSurface::finish();
        assert(PixelRenderer::canvas().readPixel(180,0)==PixelRenderer::rgb(7,12,17));
        touchEvent={Type::DOWN,158,32,0};
        assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.type==Type::DOWN && touchEvent.x==98 && touchEvent.y==-28);
        Ui::SceneSurface::mainMenu(false);
        touchEvent={Type::DOWN,158,32,0};
        assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.type==Type::CANCEL);
        Ui::SceneSurface::mainMenu(true);
        assert(Ui::SceneSurface::width()==360 && Ui::SceneSurface::height()==360);
        assert(Ui::SceneSurface::begin(frame));
        PixelRenderer::clear(0x07e0);Ui::SceneSurface::finish();
        assert(frame.pixels[16*360+180]==0xe007);
        { Ui::SceneSurface::InputScope scope(frame); assert(PixelRenderer::canvas().width()==360); }
        touchEvent={Type::DOWN,162,32,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.x==162 && touchEvent.y==32);
        touchEvent={Type::MOVE,20,20,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.type==Type::CANCEL);
        Ui::SceneSurface::mainMenu(false);assert(Ui::SceneSurface::width()==240);
        { Ui::SceneSurface::InputScope scope(frame); assert(PixelRenderer::canvas().width()==240); }
        Ui::SceneSurface::select(SceneID::MAIN);touchEvent={Type::DOWN,180,282,0};
        Ui::SceneSurface::mainMenu(true);assert(Ui::SceneSurface::width()==360);
        assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.x==180 && touchEvent.y==282);
        Ui::SceneSurface::select(SceneID::SHOP);assert(Ui::SceneSurface::height()==360);
        touchEvent={Type::DOWN,50,174,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.x==50);
        touchEvent={Type::MOVE,20,20,0};assert(Ui::SceneSurface::mapTouch(touchEvent) && touchEvent.type==Type::CANCEL);
        Ui::SceneSurface::select(SceneID::SETTINGS);assert(Ui::SceneSurface::height()==240);
        Ui::SceneSurface::select(SceneID::HATCH);assert(Ui::SceneSurface::height()==360);
        std::puts("PASS page allocation failure, RGB565 byte order, native copy guards, input binding and edge cancellation");
        return 0;
    }
    if (mode == "seed-hatch") {
        Game::GameState state;
        Game::HatchService::initializeNewGame(state);
        SaveManager writer;
        assert(writer.begin() && writer.saveSnapshot(state, MainSceneViewState{}));
        Game::HatchProgress progress{}; progress.elapsedSeconds = 37; progress.pokeCount = 2; progress.wipeCount = 3;
        assert(writer.saveHatchProgress(progress));
        return 0;
    }
    if (mode == "battery-hud" || mode == "bag-flow" || mode == "fresh" || mode == "reset" || mode == "hud-pair" || mode == "menu-fixed" || mode == "shop-tabs" || mode == "home-notice" || mode == "contacts-header" || mode == "settings-header" || mode == "status-round" || mode == "team-moves" || mode == "explore-select" || mode == "debug-menu" || (mode == "explore-round" || mode == "battle-round")) {
        // Existing menu journeys use an explicit established-save fixture.
        Game::GameState state;
        Game::HatchService::initializeNewGame(state);
        assert(Game::HatchService::finish(state, 0));
        state.tutorialFlags = Game::TUTORIAL_ALL_FLAGS;
        state.settings.volume = 0;
        if (mode == "home-notice") {
            state.team[0].level = 7;
            state.pendingLevelUp = true;
            state.pendingLevelUpLevel = 7;
        }
        if (mode == "bag-flow" || mode == "hud-pair" || mode == "team-moves" || (mode == "explore-round" || mode == "battle-round")) {
            state.team[1] = Game::MonsterFactory::create(25, 5);
            state.team[1].satiety = 25;
            state.team[1].hpCur = state.team[1].hpMax / 2;
            state.teamCount = 2;
        }
        SaveManager writer;
        assert(writer.begin() && writer.saveSnapshot(state, MainSceneViewState{}));
    }
    Sdgoods::RoomSession session;
    if (mode == "reject") {
        assert(!session.begin() && !session.ready());
        std::puts("PASS unreadable/newer save rejected without starting game");
        return 0;
    }
    assert(session.begin());
    auto& engine = GameEngine::ins();
    const auto tick = [&](unsigned duration) {
        const auto until = desktop.millis() + duration;
        while (desktop.millis() < until) { desktop.advanceMs(17); session.poll(); }
    };
    const auto event = [&](Type type, int x, int y) {
        touch.events.push_back({type, int16_t(x), int16_t(y), desktop.millis()});
        tick(20);
    };
    const auto tap = [&](int x, int y) { event(Type::DOWN, x, y); event(Type::UP, x, y); };
    const auto bowlPoint = [&]() {
        assert(engine.currentId == SceneID::MAIN);
        int sumX = 0, sumY = 0, hits = 0;
        const auto viewport = RoomRenderer::viewport();
        for (int y = viewport.y; y < viewport.y + viewport.height; ++y)
            for (int x = viewport.x; x < viewport.x + viewport.width; ++x)
                if (engine.currentScene->touchTarget(x, y) == 1) {
                    sumX += x; sumY += y; ++hits;
                }
        assert(hits > 0);
        return std::pair<int, int>{sumX / hits, sumY / hits};
    };
    constexpr int homeMenuX = Ui::RoundRoom::HOME_MENU_RECT.x + Ui::RoundRoom::HOME_MENU_RECT.width / 2;
    constexpr int homeMenuY = Ui::RoundRoom::HOME_MENU_RECT.y + Ui::RoundRoom::HOME_MENU_RECT.height / 2;
    if (mode == "resume-hatch") {
        assert(engine.homeScene() == SceneID::HATCH && !engine.gameState().oobeDone);
        assert(static_cast<HatchScene*>(engine.currentScene.get())->elapsed == 37);
        for (int i = 0; i < 20; ++i) { desktop.advanceMs(17); session.poll(); }
        capture(output / "hatch-restored.ppm");
        assert(session.save());
        Game::HatchProgress progress{};
        assert(engine.loadHatchProgress(progress) && progress.elapsedSeconds >= 37);
        assert(progress.pokeCount == 2 && progress.wipeCount == 3);
        std::puts("PASS unfinished hatch resumes saved time and care counters");
        return 0;
    }
    if (mode == "onboard-fresh") {
        assert(engine.currentId == SceneID::HATCH && !engine.gameState().oobeDone);
        assert(engine.gameState().teamCount == 0 && engine.gameState().coins == 0);
        assert(engine.gameState().tutorialFlags == 0 && engine.gameState().hatchSeconds == 180);
        for (auto food : engine.gameState().room.food) assert(food == 0);
        tick(500); capture(output / "onboard-hatch.ppm");
        auto& hatch = *static_cast<HatchScene*>(engine.currentScene.get());
        event(Type::DOWN, 130, 285); event(Type::CANCEL, 130, 285); event(Type::UP, 130, 285);
        assert(hatch.wipeCount == 0);
        tap(130,285); tap(230,285);
        assert(hatch.wipeCount == 1 && hatch.pokeCount == 1);
        tick(2300);
        // Force-save before the normal 15-second interval, as the power key does.
        assert(hatch.elapsed < 15 && session.save());
        Game::HatchProgress progress{};
        assert(engine.loadHatchProgress(progress) && progress.elapsedSeconds >= 2);
        assert(progress.wipeCount == 1 && progress.pokeCount == 1);
        blobs.failWrites = true;
        assert(!session.save() && !session.restart() && power.restarts == 0);
        blobs.failWrites = false;
        assert(session.save());
        std::puts("PASS empty store enters real hatch, care/cancel, immediate progress save and failed-save restart guard");
        return 0;
    }
    if (mode == "onboard-resume") {
        assert(engine.currentId == SceneID::HATCH && !engine.gameState().oobeDone);
        auto& hatch = *static_cast<HatchScene*>(engine.currentScene.get());
        assert(hatch.elapsed >= 2 && hatch.elapsed < 15);
        assert(hatch.wipeCount == 1 && hatch.pokeCount == 1);
        // Keep production duration; only this test fixture is near completion.
        for (int i = 0; i < 10; ++i) tap(130,285);
        hatch.elapsed = engine.gameState().hatchSeconds - 0.1f;
        engine.invalidateScene(); tick(1200);
        assert(engine.currentId == SceneID::MAIN && engine.gameState().oobeDone);
        assert(engine.gameState().teamCount == 1 && engine.activeMonster().speciesId == 7);
        assert(engine.gameState().coins == Game::INITIAL_COINS && engine.selectedFoodCount() > 0);
        assert(engine.gameState().tutorialFlags == 0);
        Game::HatchProgress cleared{};assert(!engine.loadHatchProgress(cleared));
        capture(output / "onboard-room-feed.ppm");
        const auto bowl = bowlPoint();
        tap(bowl.first, bowl.second); tick(1400);
        assert(engine.tutorialComplete(Game::TutorialStep::ROOM_FEED));
        capture(output / "onboard-room-pet.ppm");
        int petX=-1,petY=-1;
        for (int y=110;y<230 && petX<0;++y) for (int x=70;x<290;++x)
            if (engine.currentScene->touchTarget(x,y)==0) { petX=x;petY=y;break; }
        assert(petX>=0);tap(petX,petY);tick(1400);
        assert(engine.tutorialComplete(Game::TutorialStep::ROOM_PET));
        capture(output / "onboard-room-menu.ppm");
        power.level=12;engine.invalidateScene();tick(150);capture(output / "battery-low.ppm");
        power.level=-1;engine.invalidateScene();tick(150);capture(output / "battery-unknown.ppm");
        power.level=100;engine.invalidateScene();tick(150);capture(output / "battery-full.ppm");
        tap(homeMenuX,homeMenuY);tick(1000);
        assert(engine.currentId==SceneID::MENU && engine.tutorialComplete(Game::TutorialStep::OPEN_MENU));
        capture(output / "onboard-menu-navigation.ppm");
        // Team card is the second menu item, open it and use the visible back control.
        const auto teamCard=Ui::MainMenuUi::itemBounds(1,0);
        tap(teamCard.x+teamCard.width/2,teamCard.y+teamCard.height/2);tick(1000);
        assert(engine.tutorialComplete(Game::TutorialStep::MENU_NAV));
        capture(output / "onboard-menu-back.ppm");
        tap(158,32);tick(1000);
        assert(engine.tutorialComplete(Game::TutorialStep::MENU_BACK));
        const auto menuBack=Ui::MainMenuUi::backBounds();
        tap(menuBack.x+menuBack.width/2,menuBack.y+menuBack.height/2);tick(1000);assert(engine.currentId==SceneID::MAIN);
        assert(session.save());
        assert(engine.gameState().tutorialFlags==0x1f);
        std::puts("PASS hatch process restore/completion/starter rules, real touch feed/pet/menu/back and persisted tutorial flags");
        return 0;
    }
    if (mode == "onboard-restored") {
        assert(engine.currentId==SceneID::MAIN && engine.gameState().oobeDone);
        assert(engine.activeMonster().speciesId==7 && engine.gameState().tutorialFlags==0x1f);
        std::puts("PASS first-boot completion and tutorial progress restored across a fresh process");
        return 0;
    }
    assert(engine.homeScene() == SceneID::MAIN && engine.gameState().oobeDone);
    assert(RoomResource::ins().available());
    assert(RoomRenderer::viewport().x == 60 && RoomRenderer::viewport().y == 99);
    assert(RoomRenderer::viewport().width == 240 && RoomRenderer::viewport().height == 161);
    if (mode == "home-notice") {
        tick(500);
        assert(engine.hasPendingLevelUp());
        auto& main = *static_cast<MainScene*>(engine.currentScene.get());
        assert(main.homeFrame().touchBlocked);
        capture(output / "home-level-up.ppm");
        // Inspect real rendered frames: only the arrow moves inside the dialogue.
        desktop.advanceMs(640 - desktop.millis() % 640);
        std::vector<uint16_t> dialogue;
        int firstArrowY = -1;
        for (int phase = 0; phase <= 4; ++phase) {
            if (phase) desktop.advanceMs(160);
            main.render();
            auto& canvas = PixelRenderer::canvas();
            const auto paper = PixelRenderer::rgb(248,248,224);
            const auto red = PixelRenderer::rgb(216,56,56);
            assert(canvas.readPixel(64,306)==paper && canvas.readPixel(296,306)==paper);
            assert(canvas.readPixel(180,326)==paper); // No continue label.
            std::vector<uint16_t> current;
            int top = 360, bottom = -1, redPixels = 0;
            for (int y=286; y<360; ++y) for (int x=0; x<360; ++x) {
                const auto color = canvas.readPixel(x,y);
                if (x>=260 && x<269 && y>=326 && y<335) {
                    if (color==red) { top=std::min(top,y);bottom=std::max(bottom,y);++redPixels; }
                } else current.push_back(color);
            }
            assert(redPixels==23 && bottom-top==4);
            if (!phase) { dialogue=current;firstArrowY=top; }
            else { assert(current==dialogue); }
            const int offsets[]{0,1,2,1,0};
            assert(top==firstArrowY+offsets[phase]);
            capture(output / ("home-level-arrow-"+std::to_string(phase)+".ppm"));
        }
        assert(engine.hasPendingLevelUp());
        const auto stock = engine.selectedFoodCount();
        const auto bites = engine.bowlFoodBitesRemaining();
        tap(180,100);tap(180,180);tap(180,280);
        assert(engine.hasPendingLevelUp() && engine.currentId == SceneID::MAIN);
        event(Type::DOWN,180,300);event(Type::CANCEL,180,300);event(Type::UP,180,300);
        assert(engine.hasPendingLevelUp());
        event(Type::DOWN,180,300);event(Type::MOVE,210,300);event(Type::UP,180,300);
        assert(engine.hasPendingLevelUp());
        event(Type::DOWN,180,300);event(Type::UP,180,250);
        assert(engine.hasPendingLevelUp());
        event(Type::DOWN,64,306);assert(engine.hasPendingLevelUp());
        event(Type::UP,64,306);
        assert(!engine.hasPendingLevelUp() && !main.homeFrame().touchBlocked);
        event(Type::UP,90,280);
        engine.gameState().pendingLevelUp = true;
        engine.gameState().pendingLevelUpLevel = 7;
        tick(100);assert(engine.hasPendingLevelUp());
        tap(homeMenuX,homeMenuY);
        assert(!engine.hasPendingLevelUp() && engine.currentId==SceneID::MAIN);
        assert(engine.selectedFoodCount()==stock && engine.bowlFoodBitesRemaining()==bites);
        assert(engine.currentId==SceneID::MAIN && engine.gameState().settings.volume==0);
        assert(session.save());
        SaveManager verifier;Game::GameState saved{};MainSceneViewState view{};
        assert(verifier.begin() && verifier.load(saved,view) && !saved.pendingLevelUp);
        tick(100);capture(output / "home-level-up-dismissed.ppm");
        main.onButton({0, BtnAction::PRESSED});
        engine.invalidateScene();tick(20);capture(output / "home-interaction-toast.ppm");
        std::puts("PASS full-width pixel dialogue and 640ms arrow bob, bottom level-up notice: panel-wide release tap, old location/menu/room blocked, cancel/drag/outside release rejected, no click-through and persisted acknowledgement");
        return 0;
    }
    if (mode == "hud-pair") {
        assert(engine.gameState().teamCount == 2);
        tick(500);
        capture(output / "room-two-monsters.ppm");
        std::puts("PASS two-member room fixture rendered through the real engine");
        return 0;
    }
    const auto menu = [&]() -> MenuScene& {
        assert(engine.currentId == SceneID::MENU);
        return *static_cast<MenuScene*>(engine.currentScene.get());
    };
    const auto pageEvent = [&](Type type, int x, int y) { const int offset=Ui::SceneSurface::width()==360?0:60;event(type, x + offset, y + offset); };
    const auto pageTap = [&](int x, int y) { const int offset=Ui::SceneSurface::width()==360?0:60;tap(x + offset, y + offset); };
    const auto nativeMenu = [&]() {
        return engine.currentId == SceneID::MENU && menu().viewMode == MenuScene::ViewMode::MENU;
    };
    const auto back = [&]() {
        const auto nativePage = [](MenuScene::ViewMode view) {
            return view==MenuScene::ViewMode::BAG || view==MenuScene::ViewMode::COMPUTER ||
                view==MenuScene::ViewMode::TEAM || view==MenuScene::ViewMode::ROOM ||
                view==MenuScene::ViewMode::STORAGE || view==MenuScene::ViewMode::STATUS ||
                view==MenuScene::ViewMode::MOVES
#if STICKMON_ENABLE_DEBUG_FEATURES
                || view==MenuScene::ViewMode::DEBUG
#endif
                ;
        };
        const bool bag=(engine.currentId==SceneID::MENU && nativePage(menu().viewMode)) ||
            (engine.currentId==SceneID::EXPLORE && static_cast<ExploreScene*>(engine.currentScene.get())->exploreSubViewOpen &&
             nativePage(static_cast<ExploreScene*>(engine.currentScene.get())->exploreSubView.viewMode));
        if (nativeMenu() || bag || engine.currentId==SceneID::SHOP || engine.currentId==SceneID::SETTINGS) { const auto r=Ui::MainMenuUi::backBounds();tap(r.x+r.width/2,r.y+r.height/2); }
        else pageTap(28, 24);
        tick(1000);
    };
    const auto swipe = [&](int x, int y1, int y2) {
        pageEvent(Type::DOWN, x, y1); pageEvent(Type::MOVE, x, y2); pageEvent(Type::UP, x, y2);
    };
    const auto foodRow = [&](int index) {
        const auto rect = Ui::RoundPage::listLayout().cell(index, menu().foodUi.foodScroll);
        pageTap(rect.x + rect.width / 2, rect.y + rect.height / 2);
    };
    const auto menuItem = [&](int index) {
        assert(menu().viewMode == MenuScene::ViewMode::MENU);
        for (int tries = 0; tries < 8; ++tries) {
            const auto grid = Ui::MainMenuUi::layout();
            const auto rect = Ui::MainMenuUi::itemBounds(index, menu().mainMenuUi.scroll);
            const int top = std::max(rect.y, grid.viewport.y);
            const int bottom = std::min(rect.y + rect.height, grid.viewport.y + grid.viewport.height);
            if (bottom - top >= 24 && rect.y+rect.height/2<320) { tap(rect.x + rect.width / 2, (top + bottom) / 2); tick(1000); return; }
            swipe(120, bottom < grid.viewport.y + 24 ? 70 : 190, bottom < grid.viewport.y + 24 ? 190 : 70);
        }
        assert(false && "menu item must be reachable by scrolling");
    };
    const auto targetTap = [&](auto& page, int target) {
        const int size=Ui::SceneSurface::width();
        int left = size, top = size, right = -1, bottom = -1;
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
            if (page.touchTarget(x, y) == target) { left = std::min(left,x); right = std::max(right,x); top = std::min(top,y); bottom = std::max(bottom,y); }
        assert(right >= left && bottom >= top);
        const int x = (left + right) / 2, y = (top + bottom) / 2;
        assert(page.touchTarget(x,y) == target);
        pageTap(x,y);
    };
    const auto selectRegion = [&](ExploreScene& page,uint8_t id) {
        const auto index=ExploreDestinations::visibleIndex(id,engine.gameState());
        assert(index<ExploreDestinations::visibleCount(engine.gameState()));
        page.ui.state.areaCursor=index;
        page.activateExplore(Ui::ExploreAction::AREA_CHANGED,index);
        // Programmatic actions must wake the scene just as processInput does.
        engine.scheduleSceneUpdate(desktop.millis());
        engine.invalidateScene();tick(1000);
    };
    const auto capturePage = [&](const char* name) {
        tick(300); engine.invalidateScene(); tick(20); capture(output / (std::string(name) + ".ppm"));
    };
    if (mode == "battery-hud") {
        tick(1000);
        assert(engine.currentId==SceneID::MAIN);
        auto& home=static_cast<MainScene*>(engine.currentScene.get())->homeUi;
        Ui::HatchUi hatch;
        auto& canvas=PixelRenderer::canvas();
        const auto pixels=[&]() {
            std::vector<uint16_t> result;
            for(int y=0;y<360;++y) for(int x=0;x<360;++x) result.push_back(canvas.readPixel(x,y));
            return result;
        };
        for(const int level : {-1,0,10,73,100}) for(const bool isHatch : {false,true}) {
            power.level=level;
            const auto draw=[&]() {
                canvas.fillRect(0,0,360,360,0);
                if(isHatch) hatch.render(Ui::HatchFrame{});
                else home.drawLayer(Ui::HomeLayer::HUD);
            };
            power.charging=false; draw(); const auto before=pixels();
            const std::string name=std::string(isHatch?"battery-hatch-":"battery-home-")+std::to_string(level);
            capture(output/(name+"-off.ppm"));
            power.charging=true; draw(); const auto after=pixels();
            capture(output/(name+"-on.ppm"));
            char text[16];
            if(level<0) std::snprintf(text,sizeof(text),"--%%");
            else std::snprintf(text,sizeof(text),"%d%%",level);
            const int width=int(std::strlen(text))*8+26+(level>=0 && level<20?54:0);
            const int bx=isHatch?180-width/2:200, by=isHatch?28:20;
            int changed=0, yellow=0;
            for(int y=0;y<360;++y) for(int x=0;x<360;++x) {
                if(before[y*360+x]==after[y*360+x]) continue;
                assert(x>=bx && x<bx+18 && y>=by && y<by+8);
                ++changed;
                if(after[y*360+x]==PixelRenderer::rgb(255,220,88)) ++yellow;
            }
            assert(changed>0 && yellow>0);
        }
        power.level=73; power.charging=true;
        capturePage("battery-home-full-charging");
        power.charging=false; capturePage("battery-home-full-idle");
        std::puts("PASS real home/hatch charging HUD at unknown/empty/low/normal/full levels; changes confined to battery icon");
        return 0;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (mode == "debug-menu") {
        tap(homeMenuX,homeMenuY);tick(1000);
        assert(Ui::MainMenuUi::ROUND_ITEM_COUNT==9);
        menuItem(7);assert(menu().viewMode==MenuScene::ViewMode::DEBUG);
        capturePage("debug-menu-open");
        assert(Ui::SceneSurface::width()==360 && Ui::SceneSurface::height()==360);
        auto& debug=menu().debugUi;
        // Header pixels must match the main menu's native back button exactly.
        auto& canvas=PixelRenderer::canvas();
        const auto background=PixelRenderer::rgb(7,12,17);
        assert(canvas.readPixel(153,32)==0xffff);
        assert(canvas.readPixel(20,188)!=background && canvas.readPixel(340,188)!=background);
        assert(canvas.readPixel(180,348)==background);
        assert(debug.touchTarget(158,32)==Ui::RoundPage::BACK_TARGET);
        const auto openTouch=[&]() {
            for(int tries=0;tries<6;++tries) {
                int x=-1,y=-1;
                for(int yy=80;yy<298 && x<0;++yy)
                    if(debug.touchTarget(180,yy)==Ui::DebugUi::DEBUG_TOUCH_ROOT_INDEX) { x=180;y=yy+8; }
                if(x>=0 && debug.touchTarget(x,y)==Ui::DebugUi::DEBUG_TOUCH_ROOT_INDEX) {tap(x,y);break;}
                event(Type::DOWN,180,278);event(Type::MOVE,180,108);event(Type::UP,180,108);
            }
            assert(debug.debugCategory==Ui::DebugUi::DebugCategory::TOUCH_TEST);
            assert(Ui::SceneSurface::width()==360 && Ui::SceneSurface::height()==360);
        };
        openTouch();
        capturePage("debug-touch-native");
        assert(debug.debugTouchTest.count==0);
        // Real panel edge coordinates reach diagnostics without the old 60px offset.
        event(Type::DOWN,30,118);event(Type::MOVE,0,0);event(Type::UP,30,118);
        assert(debug.debugTouchTest.count==0 && !debug.debugTouchTest.pressed);
        event(Type::DOWN,30,118);event(Type::CANCEL,30,118);event(Type::UP,30,118);
        assert(debug.debugTouchTest.count==0);
        const auto clearTouch=[&]() {
            tap(Ui::TouchTest::CLEAR_X+Ui::TouchTest::CLEAR_W/2,
                Ui::TouchTest::CLEAR_Y+Ui::TouchTest::CLEAR_H/2);
            assert(debug.debugTouchTest.count==0);
        };
        const auto clearX=Ui::TouchTest::CLEAR_X+Ui::TouchTest::CLEAR_W/2;
        const auto clearY=Ui::TouchTest::CLEAR_Y+Ui::TouchTest::CLEAR_H/2;
        for(int i=0;i<Ui::TouchTest::COUNT;++i) {
            const auto point=Ui::TouchTest::TARGETS[i];
            const int dx=point.x-180,dy=point.y-180;
            if(i<8) assert(dx*dx+dy*dy>138*138 && dx*dx+dy*dy<142*142);
            assert(!Ui::RoundPage::NATIVE_BACK.contains(point.x,point.y));
            assert(Ui::TouchTest::controlAt(point)==-1);
            // Crosshairs and a small aiming margin must not enter header/button hit regions.
            for(int ox:{-14,0,14})for(int oy:{-14,0,14}) {
                assert(!Ui::RoundPage::NATIVE_BACK.contains(point.x+ox,point.y+oy));
                assert(Ui::TouchTest::controlAt({point.x+ox,point.y+oy})==-1);
            }
            event(Type::DOWN,point.x,point.y);
            assert(debug.debugTouchTest.last.x==point.x && debug.debugTouchTest.last.y==point.y);
            event(Type::MOVE,point.x+1,point.y+1);event(Type::UP,point.x+2,point.y+2);
            assert(debug.debugTouchTest.count==i+1);
            const auto& sample=debug.debugTouchTest.samples[i];
            assert(sample.down.x==point.x && sample.down.y==point.y);
            assert(sample.up.x==point.x+2 && sample.up.y==point.y+2);
            assert(sample.minimum.x==point.x && sample.maximum.x==point.x+2);
        }
        capturePage("debug-touch-nine");
        float dx,dy,error;debug.debugTouchTest.statistics(dx,dy,error);
        assert(dx==0 && dy==0 && error==0);
        tap(180,180);assert(debug.debugTouchTest.count==9);
        event(Type::DOWN,clearX,clearY);event(Type::UP,180,210);
        assert(debug.debugTouchTest.count==9);
        event(Type::DOWN,clearX,clearY);event(Type::CANCEL,clearX,clearY);event(Type::UP,clearX,clearY);
        assert(debug.debugTouchTest.count==9);
        clearTouch();capturePage("debug-touch-cleared");
        event(Type::DOWN,118,56);event(Type::MOVE,120,59);
        capturePage("debug-touch-live");event(Type::CANCEL,120,59);
        event(Type::DOWN,158,32);event(Type::MOVE,0,0);event(Type::UP,158,32);
        assert(debug.debugCategory==Ui::DebugUi::DebugCategory::TOUCH_TEST);
        tap(158,32);assert(debug.debugCategory==Ui::DebugUi::DebugCategory::ROOT);
        openTouch();assert(debug.debugTouchTest.count==0);
        const auto openCalibration=[&]() {
            tap(180,267);
            assert(debug.calibrationOpen && !touch.bypass);
        };
        const auto startCalibration=[&]() {
            tap(136,267);
            assert(debug.calibration.stage==Ui::TouchCalibrationSession::Stage::COLLECT);
            assert(touch.bypass);
        };
        const auto holdPoint=[&](int x,int y) {
            event(Type::DOWN,x,y);
            for(int j=0;j<7;++j) event(Type::MOVE,x,y);
            event(Type::UP,x,y);
        };
        const auto distorted=[](int x,int y) {
            return Ui::TouchTest::Point{int(std::lround(180+(x-180)*1.1+4)),
                                        int(std::lround(180+(y-180)*1.08-2))};
        };
        openCalibration();capturePage("touch-calibration-intro");
        for (char32_t ch : U"偏八参差序拟映校独约致证") {
            if (ch) assert(FontResource::ins().findGlyphBitmap(ch,FontFace::SARASA_CJK));
        }
        startCalibration();
        holdPoint(118,56);assert(debug.calibration.count==1);
        menu().cancelTouch();
        assert(!debug.calibrationOpen && !touch.bypass);
        assert(blobs.blobSize("touchcal","affine_v1")==0);
        openCalibration();startCalibration();capturePage("touch-calibration-outer");
        tap(118,56);assert(debug.calibration.count==0); // A quick tap is not stable evidence.
        event(Type::DOWN,118,56);event(Type::CANCEL,118,56);event(Type::UP,118,56);
        assert(debug.calibration.count==0 && debug.calibrationOpen);
        for(int i=0;i<8;++i) {
            const auto target=Ui::TouchTest::TARGETS[i];
            const auto raw=distorted(target.x,target.y);
            holdPoint(raw.x,raw.y);
            assert(debug.calibration.count==i+1);
        }
        capturePage("touch-calibration-center");
        holdPoint(220,210);
        assert(debug.calibration.count==8 && debug.calibration.stage==Ui::TouchCalibrationSession::Stage::COLLECT);
        capturePage("touch-calibration-center-retry");
        holdPoint(184,178);
        assert(debug.calibration.stage==Ui::TouchCalibrationSession::Stage::READY);
        assert(debug.calibration.rms<1 && debug.calibration.centerError<1);
        capturePage("touch-calibration-ready");
        blobs.failWrites=true;tap(136,267);
        assert(debug.calibrationOpen && touch.bypass && touch.transform.x(17,341)==17);
        assert(blobs.blobSize("touchcal","affine_v1")==0);
        capturePage("touch-calibration-save-failed");
        blobs.failWrites=false;tap(136,267);
        assert(!debug.calibrationOpen && !touch.bypass && debug.debugTouchTest.count==0);
        assert(blobs.blobSize("touchcal","affine_v1")==32);
        const auto reloaded=Sdgoods::TouchCalibrationStore::load(blobs);
        assert(std::abs(reloaded.x(184,178)-180)<1 && std::abs(reloaded.y(184,178)-180)<1);
        for(int i=0;i<9;++i) {
            const auto target=Ui::TouchTest::TARGETS[i];
            const auto raw=distorted(target.x,target.y);
            tap(raw.x,raw.y);
            assert(debug.debugTouchTest.count==i+1);
            const auto actual=debug.debugTouchTest.samples[i].down;
            assert(std::abs(actual.x-target.x)<=1 && std::abs(actual.y-target.y)<=1);
        }
        capturePage("touch-calibration-corrected-nine");
        const auto correctedTap=[&](int x,int y) {
            const auto raw=distorted(x,y);tap(raw.x,raw.y);
        };
        correctedTap(180,267);assert(debug.calibrationOpen && !touch.bypass);
        correctedTap(224,267); // Explicit restore to the original mapping.
        assert(!debug.calibrationOpen && !touch.bypass && debug.debugTouchTest.count==0);
        assert(touch.transform.x(17,341)==17 && touch.transform.y(17,341)==341);
        capturePage("touch-calibration-restored");
        openCalibration();startCalibration();tap(158,32);
        assert(!debug.calibrationOpen && !touch.bypass);
        assert(debug.debugCategory==Ui::DebugUi::DebugCategory::TOUCH_TEST);
        tap(Ui::TouchTest::BACK_X+Ui::TouchTest::BACK_W/2,
            Ui::TouchTest::BACK_Y+Ui::TouchTest::BACK_H/2);
        assert(debug.debugCategory==Ui::DebugUi::DebugCategory::ROOT);
        // Other debug submenus/popups also use native geometry and preserve cancel semantics.
        targetTap(debug,0);assert(debug.debugCategory==Ui::DebugUi::DebugCategory::MONSTER);
        targetTap(debug,Ui::DebugUi::DEBUG_MONSTER_SWITCH_INDEX);assert(debug.debugSwitchOpen);
        capturePage("debug-popup-native");
        back();assert(!debug.debugSwitchOpen && debug.debugCategory==Ui::DebugUi::DebugCategory::MONSTER);
        back();assert(debug.debugCategory==Ui::DebugUi::DebugCategory::ROOT);
        back();assert(menu().viewMode==MenuScene::ViewMode::MENU);
        assert(Ui::SceneSurface::width()==360);
        menuItem(8);tick(1000);assert(engine.currentId==SceneID::MAIN);
        assert(engine.gameState().settings.volume==0);
        std::puts("PASS native debug/header/popups, nine rim/center samples with panel coordinates, cancel/clear/return/reentry; volume zero");
        std::puts("PASS calibration intro/cancel/quick-tap/stable median, independent center retry, save failure/commit/reload, corrected nine-point retest and identity restore");
        return 0;
    }
#endif
    if (mode == "bag-flow") {
        using Item=Game::ItemId;
        auto& state=engine.gameState();
        for(int i=0;i<int(Item::COUNT);++i) Game::ItemInventory::add(state,Item(i),3);
        state.team[0].satiety=state.team[1].satiety=75;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(3);
        const auto openItem=[&](MenuScene& m,Item item) {
            assert(m.viewMode==MenuScene::ViewMode::BAG && !m.bagUi.detailOpen);
            int index=0;
            while(index<Game::ItemInventory::homeBagItemCount(state) &&
                  Game::ItemInventory::homeBagItemAt(state,index)!=item)++index;
            assert(index<Game::ItemInventory::homeBagItemCount(state));
            const auto grid=m.bagUi.grid(state,m.battleBagMode);
            m.bagUi.gridScroll=grid.reveal(index,m.bagUi.gridScroll);
            engine.invalidateScene();tick(20);
            const auto cell=grid.targetCell(index,m.bagUi.gridScroll);
            pageTap(cell.x+34,cell.y+34);
        };
        assert(menu().bagUi.grid(state,false).count()==Game::ItemInventory::homeBagItemCount(state));
        openItem(menu(),Item::NORMAL_FOOD);
        assert(menu().bagUi.detailOpen && menu().bagUi.target(130,270,state,menu().bagContext())==-1);
        capturePage("bag-food-readonly");back();
        openItem(menu(),Item::NUGGET);
        assert(menu().bagUi.detailOpen && menu().bagUi.target(130,270,state,menu().bagContext())==-1);back();
        openItem(menu(),Item::SOAP_0);assert(menu().bagUi.detailOpen);back();
        openItem(menu(),Item::MAX_REPEL);const auto repel=state.bag.maxRepel;
        pageTap(130,270);assert(state.bag.maxRepel==repel && engine.repelStepsRemaining()==0);back();
        state.team[0].hpCur=state.team[0].hpMax;
        state.team[1].hpCur=1;
        state.bag.potion=1;
        const auto potion=state.bag.potion;
        openItem(menu(),Item::POTION);pageTap(130,270);
        assert(menu().selectingTeamItem() && !menu().bagUi.confirmOpen);
        capturePage("bag-target-two-members");
        targetTap(menu().teamUi,0);
        assert(state.bag.potion==potion && menu().selectingTeamItem() && !menu().itemFeedback);
        pageEvent(Type::DOWN,100,125);pageEvent(Type::CANCEL,100,125);pageEvent(Type::UP,100,125);
        assert(state.bag.potion==potion);
        targetTap(menu().teamUi,1);
        assert(state.bag.potion==potion-1 && state.team[0].hpCur==state.team[0].hpMax);
        assert(menu().itemFeedback && menu().teamItemHp(1)<state.team[1].hpCur);
        pageTap(100,125);assert(state.bag.potion==potion-1);
        capturePage("bag-hp-feedback");tick(800);
        assert(menu().viewMode==MenuScene::ViewMode::BAG && !menu().bagUi.detailOpen);
        openItem(menu(),Item::SUPER_POTION);pageTap(130,270);back();
        assert(!menu().selectingTeamItem() && state.bag.superPotion>=3);
        // Revive the selected member, retaining the other fainted member.
        state.team[0].fainted=state.team[1].fainted=true;
        state.team[0].hpCur=state.team[1].hpCur=0;
        const auto revives=state.bag.revive;
        openItem(menu(),Item::REVIVE);pageTap(130,270);targetTap(menu().teamUi,1);
        assert(state.team[0].fainted && !state.team[1].fainted && state.bag.revive==revives-1);
        tick(800);assert(menu().viewMode==MenuScene::ViewMode::BAG);
        state.team[0].fainted=false;state.team[0].hpCur=state.team[0].hpMax;
        state.team[1].majorStatus=Game::MajorStatus::SLEEP;
        const auto cures=state.bag.awakening;
        openItem(menu(),Item::AWAKENING);pageTap(130,270);targetTap(menu().teamUi,0);
        assert(state.bag.awakening==cures && !menu().itemFeedback);
        targetTap(menu().teamUi,1);assert(state.team[1].majorStatus==Game::MajorStatus::NONE);
        tick(800);
        // Candy uses the same growth/evolution/move pipeline as earned EXP.
        state.team[1]=Game::MonsterFactory::create(25,5);
        const auto leaderLevel=state.team[0].level;const auto candy=state.bag.candy;
        openItem(menu(),Item::CANDY);pageTap(130,270);targetTap(menu().teamUi,1);tick(800);
        assert(state.team[1].level==6 && state.team[0].level==leaderLevel && state.bag.candy==candy-1);
        assert(menu().itemProgression==MenuScene::ItemProgression::LEVEL_UP && Ui::SceneSurface::width()==360);
        capturePage("bag-candy-level-up");
        event(Type::DOWN,180,312);event(Type::CANCEL,180,312);event(Type::UP,180,312);
        assert(menu().itemProgression==MenuScene::ItemProgression::LEVEL_UP);
        tap(180,312);tick(100);
        for(int i=0;i<12 && menu().itemProgression!=MenuScene::ItemProgression::NONE;++i) {
            menu().onButton({1,BtnAction::PRESSED});menu().onButton({0,BtnAction::PRESSED});tick(50);
        }
        assert(menu().viewMode==MenuScene::ViewMode::BAG && !state.pendingLevelUp);
        const auto stones=state.bag.thunderStone;
        openItem(menu(),Item::THUNDER_STONE);pageTap(130,270);targetTap(menu().teamUi,0);
        assert(!menu().itemFeedback && state.bag.thunderStone==stones);
        targetTap(menu().teamUi,1);tick(800);
        assert(menu().itemProgression==MenuScene::ItemProgression::EVOLUTION);
        tap(158,32);tick(2000);capturePage("bag-evolution-cancelled");pageTap(120,210);tick(100);
        assert(state.team[1].speciesId==25 && state.bag.thunderStone==stones);
        assert(menu().viewMode==MenuScene::ViewMode::BAG);
        openItem(menu(),Item::THUNDER_STONE);pageTap(130,270);targetTap(menu().teamUi,1);tick(800);
        capturePage("bag-stone-evolution");tick(8000);capturePage("bag-evolution-complete");pageTap(120,210);tick(100);
        for(int i=0;i<12 && menu().itemProgression!=MenuScene::ItemProgression::NONE;++i) {
            menu().onButton({1,BtnAction::PRESSED});menu().onButton({0,BtnAction::PRESSED});tick(50);
        }
        assert(state.team[1].speciesId==26 && state.bag.thunderStone==stones-1);
        assert(menu().viewMode==MenuScene::ViewMode::BAG && Ui::SceneSurface::width()==360);
        const auto leaderBeforeRecall=state.team[0];
        state.team[0]=Game::MonsterFactory::create(1,20);
        state.team[0].move2Id=state.team[0].move3Id=0;
        const auto scales=state.bag.heartScale;
        openItem(menu(),Item::HEART_SCALE);
        assert(menu().viewMode==MenuScene::ViewMode::MOVES && !menu().bagUi.detailOpen);
        capturePage("bag-heart-scale-direct");back();
        assert(menu().viewMode==MenuScene::ViewMode::BAG && state.bag.heartScale==scales);
        state.team[0]=leaderBeforeRecall;
        back();menuItem(0);
        selectRegion(*static_cast<ExploreScene*>(engine.currentScene.get()),0);
        targetTap(*static_cast<ExploreScene*>(engine.currentScene.get()),Ui::RoundExplore::DEPART);
        const auto departureDeadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<departureDeadline)tick(100);
        assert(engine.currentId==SceneID::EXPLORE);
        auto& explore=*static_cast<ExploreScene*>(engine.currentScene.get());
        tick(1000);
        explore.exploreSubView.openExploreBagView();explore.exploreSubViewOpen=true;
        explore.routePausedForTouch=true;engine.invalidateScene();tick(20);
        openItem(explore.exploreSubView,Item::MAX_REPEL);pageTap(130,270);
        if (!(state.bag.maxRepel==repel-1 && engine.repelStepsRemaining()==100))
            std::fprintf(stderr,"repel before=%u after=%u steps=%u scene=%u mode=%u detail=%u context=%u toast=%s\n",repel,state.bag.maxRepel,engine.repelStepsRemaining(),unsigned(engine.currentId),unsigned(explore.exploreSubView.viewMode),explore.exploreSubView.bagUi.detailOpen,explore.exploreSubView.exploreContextMode,explore.exploreSubView.toast?explore.exploreSubView.toast:"none");
        assert(state.bag.maxRepel==repel-1 && engine.repelStepsRemaining()==100);
        openItem(explore.exploreSubView,Item::MAX_REPEL);pageTap(130,270);
        assert(state.bag.maxRepel==repel-1);back();
        openItem(explore.exploreSubView,Item::HONEY);pageTap(130,270);
        assert(engine.honeyEncounterPending());back();assert(!explore.exploreSubViewOpen);
        const auto resetBattle=[&]() {
            state.team[0].hpCur=state.team[0].hpMax;
            state.team[1].hpCur=state.team[1].hpMax;
            state.team[0].fainted=state.team[1].fainted=false;
            explore.beginEncounter(*findSpecies(25),5);explore.clearBattleLogs();
            explore.phase=Ui::ExplorePhase::ENCOUNTER;
            explore.defeatAwaitInput=false;engine.invalidateScene();tick(20);
        };
        const auto openBattleBag=[&]() {
            tap(148,304);assert(explore.exploreSubViewOpen);
            assert(explore.exploreSubView.battleBagMode);
        };
        state.bag.potion=2;
        resetBattle();openBattleBag();auto& battleMenu=explore.exploreSubView;
        const auto battleCandy=state.bag.candy;
        openItem(battleMenu,Item::CANDY);
        assert(!battleMenu.bagUi.detailOpen && state.bag.candy==battleCandy);
        const auto battlePotion=state.bag.potion;
        openItem(battleMenu,Item::POTION);pageTap(130,270);targetTap(battleMenu.teamUi,0);
        assert(!battleMenu.itemFeedback && state.bag.potion==battlePotion && !explore.battleLogBusy());
        back();assert(battleMenu.viewMode==MenuScene::ViewMode::BAG);back();
        assert(!explore.exploreSubViewOpen && !explore.battleLogBusy());
        openBattleBag();state.team[1].hpCur=1;
        openItem(battleMenu,Item::POTION);pageTap(130,270);targetTap(battleMenu.teamUi,1);
        assert(battleMenu.itemFeedback && !explore.battleLogBusy());
        assert(state.bag.potion==battlePotion-1 && state.team[1].hpCur>1);
        tick(800);assert(!explore.exploreSubViewOpen && Ui::SceneSurface::width()==360);
        assert(explore.battleLogBusy() || explore.battleTurnStage!=ExploreScene::BattleTurnStage::IDLE);
        capturePage("bag-battle-after-use");tick(6000);
        resetBattle();openBattleBag();state.team[1].fainted=true;state.team[1].hpCur=0;
        const auto battleRevives=state.bag.revive;
        openItem(battleMenu,Item::REVIVE);pageTap(130,270);targetTap(battleMenu.teamUi,1);
        assert(!state.team[1].fainted && state.bag.revive==battleRevives-1);
        tick(800);assert(!explore.exploreSubViewOpen);tick(6000);
        resetBattle();state.room.food[0]=1;openBattleBag();const auto food=state.room.food[0];
        openItem(battleMenu,Item::NORMAL_FOOD);capturePage("bag-battle-feed-detail");
        pageTap(130,270);
        assert(!explore.exploreSubViewOpen && !battleMenu.bagUi.confirmOpen);
        assert(state.room.food[0]==food-1 && explore.foodThrowActive);
        assert(state.settings.volume==0);
        engine.markDirty(SaveUrgency::IMMEDIATE);tick(1000);
        SaveManager verifier;assert(verifier.begin());
        Game::GameState restored;MainSceneViewState restoredView;
        assert(verifier.load(restored,restoredView));
        assert(restored.bag.candy==state.bag.candy && restored.bag.thunderStone==state.bag.thunderStone);
        assert(restored.bag.revive==state.bag.revive && restored.room.food[0]==state.room.food[0]);
        assert(restored.team[1].speciesId==26 && restored.team[1].level==6 && restored.settings.volume==0);
        std::puts("PASS complete inventory, read-only items, context guards, target cancel/failure/choice, one-shot recovery feedback, chosen revive/cures, candy growth, stone cancel/commit, route items, battle no-effect/no-turn, delayed enemy turn and direct feeding; volume zero");
        return 0;
    }
    if (mode == "battle-round") {
        auto& state=engine.gameState();
        state.team[1].satiety=75;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(0);
        selectRegion(*static_cast<ExploreScene*>(engine.currentScene.get()),0);
        targetTap(*static_cast<ExploreScene*>(engine.currentScene.get()),Ui::RoundExplore::DEPART);
        const auto departureDeadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<departureDeadline)tick(100);
        assert(engine.currentId==SceneID::EXPLORE);
        auto& explore=*static_cast<ExploreScene*>(engine.currentScene.get());
        const auto resetBattle=[&]() {
            state.team[0].hpCur=state.team[0].hpMax;
            state.team[1].hpCur=state.team[1].hpMax;
            state.team[0].fainted=state.team[1].fainted=false;
            explore.beginEncounter(*findSpecies(25),5);explore.clearBattleLogs();
            explore.phase=Ui::ExplorePhase::ENCOUNTER;
            explore.defeatAwaitInput=false;
            capturePage("battle-round-idle");
            assert(Ui::SceneSurface::width()==360);
        };
        resetBattle();
        const auto layout=Ui::RoundExplore::battleLayout();
        for(int i=0;i<4;++i) {
            const auto r=layout.commandTarget(i);
            assert(r.width>=44 && r.height>=44);
            for(int x:{r.x,r.x+r.width-1})for(int y:{r.y,r.y+r.height-1}) {
                assert(Ui::RoundExplore::routePoint(x,y));
                assert(explore.touchTarget(x,y)==110+i);
            }
        }
        assert(explore.touchTarget(0,0)==-1 && explore.touchTarget(180,350)==-1);
        assert(explore.touchTarget(118,295)==-1);
        event(Type::DOWN,88,286);event(Type::MOVE,0,0);event(Type::UP,88,286);
        assert(!explore.battleLogBusy());
        event(Type::DOWN,88,286);event(Type::CANCEL,88,286);event(Type::UP,88,286);
        assert(!explore.battleLogBusy());
        tap(148,304);assert(explore.exploreSubViewOpen);
        capturePage("battle-round-bag");assert(Ui::SceneSurface::width()==360);
        back();assert(!explore.exploreSubViewOpen && Ui::SceneSurface::width()==360);
        const auto oldSlot=explore.battlePlayerSlot;
        tap(208,304);assert(explore.battleLogBusy());
        assert(explore.touchTarget(88,286)==-1);
        tick(6000);assert(explore.battlePlayerSlot!=oldSlot);
        capturePage("battle-round-switched");
        resetBattle();tap(88,286);
        assert(explore.battleLogBusy() && explore.touchTarget(268,286)==-1);
        tick(160);engine.invalidateScene();tick(20);capture(output/"battle-round-action.ppm");
        resetBattle();
        // Render read-only presentation frames without advancing the game clock.
        auto frame=explore.exploreBattleFrame();
        const auto savedState=state;
        const auto render=[&](const char* name) {
            Ui::ExploreBattleUiContent::drawEncounter(frame,layout);
            if(frame.friendshipPromptVisible)
                Ui::ExploreBattleUiContent::drawFriendshipPrompt(frame,layout);
            capture(output/(std::string(name)+".ppm"));
            assert(std::memcmp(&savedState,&state,sizeof(state))==0);
        };
        const auto hpColor=PixelRenderer::rgb(92,222,112);
        const auto expColor=PixelRenderer::rgb(35,118,184);
        const auto playerHpPixel=[&]() {return PixelRenderer::canvas().readPixel(layout.playerHud.hpX+8,layout.playerHud.hpY+3);};
        frame.playerHp=frame.playerHpMax;render("battle-round-hp-only");
        assert(playerHpPixel()==hpColor);
        // Battle labels must use native coverage at both sizes, while the
        // neighboring level column remains the original two-color pixel font.
        const auto smoothInk=[&](int x,int y,int height,uint16_t ink,uint16_t paper) {
            unsigned partial=0;
            for(int row=0;row<height;++row)for(int col=0;col<height;++col) {
                const auto pixel=PixelRenderer::canvas().readPixel(x+col,y+row);
                partial+=pixel!=ink && pixel!=paper;
            }
            assert(partial>0);
        };
        smoothInk(layout.wildHud.nameX,layout.wildHud.nameY,16,
                  PixelRenderer::rgb(23,45,50),PixelRenderer::rgb(243,235,207));
        const auto command=layout.commandTarget(0);
        smoothInk(command.x+(command.width-40)/2,command.y+(command.height-20)/2,20,
                  PixelRenderer::rgb(23,45,50),PixelRenderer::rgb(243,235,207));
        for(int y=layout.wildHud.nameY;y<layout.wildHud.nameY+16;++y)
            for(int x=layout.wildHud.levelRight-24;x<layout.wildHud.levelRight;++x) {
                const auto pixel=PixelRenderer::canvas().readPixel(x,y);
                assert(pixel==PixelRenderer::rgb(23,45,50) || pixel==PixelRenderer::rgb(243,235,207));
            }
        for(int y=244;y<252;++y)for(int x=180;x<288;++x)
            assert(PixelRenderer::canvas().readPixel(x,y)!=expColor);
        frame.wildStatus=Game::MajorStatus::PARALYSIS;frame.playerStatus=Game::MajorStatus::POISON;
        render("battle-round-status");
        frame.battleLogVisibleCount=2;frame.battleLogActive=true;
        frame.battleLogVisible[0]="对手使用了电光一闪!";
        frame.battleLogVisible[1]="妙蛙种子使用了飞叶快刀!";
        render("battle-round-message");
        char longMessage[64];
        snprintf(longMessage,sizeof(longMessage),Ui::Explore::RECOIL_FMT,"妙蛙种子",65535U);
        frame.battleLogVisible[1]=longMessage;
        render("battle-round-long-message");
        frame.battleLogVisibleCount=0;frame.battleLogActive=false;
        for(int species:{3,6,9,130,143,149}) {
            frame.wildSpecies=findSpecies(species);frame.playerSpecies=findSpecies(species);
            assert(frame.wildSpecies && frame.playerSpecies);
            render(("battle-round-species-"+std::to_string(species)).c_str());
        }
        frame.wildLevel=frame.playerLevel=100;
        render("battle-round-level-100");
        frame=explore.exploreBattleFrame();
        frame.foodThrowActive=true;frame.foodThrowStarted=frame.nowMs-160;
        render("battle-round-food-throw");
        frame=explore.exploreBattleFrame();
        frame.playerHp=frame.playerHpMax;
        frame.wildHp=0;frame.battleResultPending=true;
        frame.expAnimationFrom=minimumExpForLevel(frame.playerSpecies->growthRate,5)+1;
        frame.expAnimationTo=minimumExpForLevel(frame.playerSpecies->growthRate,6)+5;
        frame.expAnimationPending=true;
        render("battle-round-before-exp");assert(playerHpPixel()==hpColor);
        frame.battleExpVisible=true;frame.expAnimationPending=false;frame.expAnimationActive=true;
        frame.expAnimationStarted=frame.nowMs-300;
        render("battle-round-exp");
        bool expDrawn=false;
        for(int x=layout.expBarX+1;x<layout.expBarX+layout.expBarW-1;++x)
            expDrawn|=PixelRenderer::canvas().readPixel(x,layout.expBarY+1)==expColor;
        assert(expDrawn && playerHpPixel()!=hpColor);
        frame.nowMs=frame.expAnimationStarted+ExploreBattleTiming::EXP_ANIMATION_MS;
        render("battle-round-exp-level-up");
        frame=explore.exploreBattleFrame();frame.defeatAwaitInput=true;
        render("battle-round-defeat");
        frame.encounterPhase=false;frame.defeatAwaitInput=false;frame.friendshipPromptVisible=true;
        frame.battleLogVisibleCount=2;
        frame.battleLogVisible[0]="皮卡丘认可了你";frame.battleLogVisible[1]=Ui::Explore::FRIEND_CONTACT_QUESTION;
        render("battle-round-friendship");
        explore.phase=Ui::ExplorePhase::FRIENDSHIP;
        explore.friendshipStep=ExploreScene::FriendshipStep::CONTACT_CONFIRM;
        explore.prepareSurface();assert(Ui::SceneSurface::width()==360);
        const auto prompt=layout.friendshipPrompt();
        assert(explore.touchTarget(prompt.x+20,prompt.y+20)==132);
        assert(explore.touchTarget(prompt.x+20,prompt.y+65)==133);
        assert(explore.touchTarget(180,300)==-1);
        resetBattle();
        // Reward visibility is driven by the actual queued EXP_GAIN cue.
        explore.wildHp=0;explore.finishWildFaint();
        engine.scheduleSceneUpdate(desktop.millis());
        assert(explore.battleResultPending && !explore.battleExpVisible);
        const auto rewardDeadline=desktop.millis()+8000;
        while(!explore.battleExpVisible && desktop.millis()<rewardDeadline)tick(17);
        assert(explore.battleExpVisible && explore.expAnimationActive);
        assert(explore.touchTarget(88,286)==-1);
        engine.invalidateScene();tick(20);capture(output/"battle-round-victory-flow.ppm");
        resetBattle();explore.fleeAttempts=9;tap(268,286);assert(explore.battleLogBusy());
        tick(6000);
        assert(explore.phase==Ui::ExplorePhase::WALKING && Ui::SceneSurface::width()==360);
        assert(state.settings.volume==0);
        std::puts("PASS native battle command corners/cancel/busy, bag return, switch/attack/flee, real victory EXP cue, HP-only combat, reward animation, statuses, long logs, large sprites and friendship surfaces");
        return 0;
    }
    if (mode == "explore-round") {
        auto& state=engine.gameState();
        state.team[1].satiety=75;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(0);
        selectRegion(*static_cast<ExploreScene*>(engine.currentScene.get()),0);
        targetTap(*static_cast<ExploreScene*>(engine.currentScene.get()),Ui::RoundExplore::DEPART);
        const auto deadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<deadline)tick(100);
        assert(engine.currentId==SceneID::EXPLORE);
        auto& explore=*static_cast<ExploreScene*>(engine.currentScene.get());
        assert(explore.phase==Ui::ExplorePhase::WALKING && Ui::SceneSurface::width()==360);
        // Inspect isolated HUD rendering on a known background. The view must
        // stay inside the round display and leave the former center HUD clear.
        auto hud=explore.exploreRouteFrame();
        hud.menuAvailable=false;hud.exploreMenuOpen=false;hud.teamHudCount=0;
        auto& canvas=PixelRenderer::canvas();
        const auto background=PixelRenderer::rgb(37,65,91);
        const auto renderHud=[&]() {
            PixelRenderer::clear(background);Ui::RoundExplore::drawRouteHud(hud);
        };
        const auto pixels=[&]() {
            std::vector<uint16_t> result;
            for(int y=0;y<360;++y)for(int x=0;x<360;++x)
                result.push_back(canvas.readPixel(x,y));
            return result;
        };
        renderHud();const auto emptyHud=pixels();
        unsigned previousFill=0;
        for(unsigned hp:{0u,1u,20u,21u,50u,51u,100u,65535u}) {
            hud.teamHudCount=2;
            hud.teamHud[0]=hud.teamHud[1]={uint16_t(hp),100,Game::MajorStatus::NONE,false};
            renderHud();
            const auto color=hp>50 ? PixelRenderer::rgb(92,222,112) :
                hp>20 ? PixelRenderer::rgb(246,204,72) : PixelRenderer::rgb(232,80,84);
            unsigned fill=0,ink=0;
            for(int y=0;y<360;++y)for(int x=0;x<360;++x) {
                const auto pixel=canvas.readPixel(x,y);
                if(pixel==emptyHud[y*360+x])continue;
                ++ink;assert(Ui::RoundExplore::routePoint(x,y));
                assert(y<74 && (x<116 || x>=244));
                assert(pixel==canvas.readPixel(359-x,y));
                assert(Ui::RoundExplore::routeHudHit(x,y,2));
                fill+=pixel==color;
            }
            assert(ink>1000 && ink<1900 && fill>=previousFill);
            if(!hp)assert(fill==0);else assert(fill>0);
            previousFill=fill;
            if(hp==0 || hp==20 || hp==50 || hp==100)
                capture(output/("explore-arc-hp-"+std::to_string(hp)+".ppm"));
        }
        const auto fullPair=pixels();
        hud.teamHudCount=1;renderHud();
        for(int y=0;y<360;++y)for(int x=0;x<360;++x)
            assert(canvas.readPixel(x,y)==(x<116 ? fullPair : emptyHud)[y*360+x]);
        hud.teamHudCount=2;
        hud.teamHud[0]=hud.teamHud[1]={0,100,Game::MajorStatus::NONE,false};
        renderHud();const auto zeroHp=pixels();
        hud.teamHud[0]=hud.teamHud[1]={100,0,Game::MajorStatus::NONE,false};
        renderHud();assert(pixels()==zeroHp);
        hud.teamHud[0]=hud.teamHud[1]={0,100,Game::MajorStatus::NONE,true};
        renderHud();assert(pixels()!=zeroHp);capture(output/"explore-arc-fainted.ppm");
        hud.teamHud[0]=hud.teamHud[1]={100,100,Game::MajorStatus::NONE,false};
        renderHud();const auto normalHud=pixels();
        hud.teamHud[0].status=Game::MajorStatus::POISON;
        hud.teamHud[1].status=Game::MajorStatus::PARALYSIS;
        renderHud();unsigned statusInk[2]={};
        for(int y=0;y<360;++y)for(int x=0;x<360;++x) {
            if(canvas.readPixel(x,y)==normalHud[y*360+x])continue;
            const int member=x<180 ? 0 : 1;
            assert(Ui::RoundExplore::routeStatusSlot(member).contains(x,y));
            assert(Ui::RoundExplore::routeHudHit(x,y,2));++statusInk[member];
        }
        assert(statusInk[0]>0 && statusInk[1]>0);
        hud.teamHud[0].status=hud.teamHud[1].status=Game::MajorStatus::NONE;
        renderHud();assert(pixels()==normalHud);
        hud.menuAvailable=true;renderHud();
        // The menu joins the title. Everything below the header, including the
        // old bottom control footprint, must match the HUD without its button.
        for(int y=60;y<360;++y)for(int x=0;x<360;++x)
            assert(canvas.readPixel(x,y)==normalHud[y*360+x]);
        assert(!Ui::RoundExplore::routeHudHit(83,46,0));
        assert(Ui::RoundExplore::routeHudHit(83,46,1));
        assert(!Ui::RoundExplore::routeHudHit(276,46,1));
        assert(Ui::RoundExplore::routeHudHit(276,46,2));
        assert(!Ui::RoundExplore::routeHudHit(180,70,2));
        assert(!Ui::RoundExplore::routeHudHit(0,0,2));
        capturePage("explore-round-map");
        assert(explore.touchTarget(180,32)==-1 && explore.touchTarget(83,46)==-1);
        assert(explore.touchTarget(180,70)==100 && explore.touchTarget(100,70)==100);
        assert(explore.touchTarget(92,82)==100 && explore.touchTarget(47,80)==-1);
        assert(explore.touchTarget(0,0)==-1 && explore.touchTarget(180,180)==100);
        const auto menuHit=Ui::RoundExplore::ROUTE_MENU;
        assert(menuHit.width==44 && menuHit.height==44);
        for(int x:{menuHit.x,menuHit.x+menuHit.width-1})
            for(int y:{menuHit.y,menuHit.y+menuHit.height-1})
                assert(explore.touchTarget(x,y)==144);
        assert(explore.touchTarget(180,318)==100);
        assert(explore.touchTarget(menuHit.x-1,35)==-1);
        event(Type::DOWN,220,35);event(Type::CANCEL,220,35);event(Type::UP,220,35);
        assert(!explore.exploreMenuOpen);
        event(Type::DOWN,220,35);event(Type::MOVE,359,0);event(Type::UP,220,35);
        assert(!explore.exploreMenuOpen);
        tap(220,35);assert(explore.exploreMenuOpen);
        capturePage("explore-round-menu");
        const auto menuFrames=desktop.presentCount();tick(500);
        assert(desktop.presentCount()==menuFrames);

        for(int i=0;i<4;++i) {
            const auto r=Ui::RoundExplore::routeMenuItem(i);
            for(int x:{r.x,r.x+r.width-1})for(int y:{r.y,r.y+r.height-1}) {
                assert(Ui::RoundExplore::routePoint(x,y));
                assert(explore.touchTarget(x,y)==120+i);
            }
        }
        const auto steps=explore.steps;
        tap(180,180);tick(500);
        assert(explore.exploreMenuOpen && explore.steps==steps && !explore.exploreSubViewOpen);
        targetTap(explore,120);assert(explore.exploreSubViewOpen && Ui::SceneSurface::width()==240);
        targetTap(explore.exploreSubView.teamUi,0);
        int statusAction=-1;
        auto& team=explore.exploreSubView.teamUi;
        for(int i=0;i<team.teamActionCount();++i)
            if(team.teamActionAt(i)==Ui::TeamAction::STATUS)statusAction=i;
        assert(statusAction>=0);targetTap(team,128+statusAction);
        assert(Ui::SceneSurface::width()==360);
        back();assert(explore.exploreSubViewOpen && Ui::SceneSurface::width()==240);
        back();assert(!explore.exploreSubViewOpen && Ui::SceneSurface::width()==360 && explore.exploreMenuOpen);
        targetTap(explore,121);assert(explore.exploreSubViewOpen && Ui::SceneSurface::width()==360);
        // Wait for the newly opened page to paint its physical header before
        // tapping it; a stopped map no longer happens to redraw every poll.
        capturePage("explore-round-bag");
        back();assert(!explore.exploreSubViewOpen && Ui::SceneSurface::width()==360);
        tap(220,35);assert(!explore.exploreMenuOpen && explore.routePausedForTouch && !explore.autoWalkActive);
        tick(100);const auto pausedFrames=desktop.presentCount();
        tick(500);assert(explore.steps==steps && desktop.presentCount()==pausedFrames);
        tap(47,80);assert(explore.routePausedForTouch && !explore.autoWalkActive);
        event(Type::DOWN,83,46);event(Type::CANCEL,83,46);event(Type::UP,180,70);
        assert(explore.routePausedForTouch && !explore.autoWalkActive);
        tap(180,70);assert(explore.autoWalkActive && !explore.routePausedForTouch);
        tap(220,35);assert(explore.exploreMenuOpen);
        targetTap(explore,123);assert(!explore.exploreMenuOpen && explore.routePausedForTouch);
        state.team[0].majorStatus=Game::MajorStatus::POISON;
        state.team[1].majorStatus=Game::MajorStatus::PARALYSIS;
        capturePage("explore-round-status");
        state.teamCount=1;capturePage("explore-round-single");state.teamCount=2;
        state.team[0].majorStatus=state.team[1].majorStatus=Game::MajorStatus::NONE;
        // The first-run WALK tutorial is deliberately invisible.
        PixelRenderer::clear(0x1234);
        Ui::RoundTutorial::explore({"walk",0,Ui::ExploreTutorialKind::WALK});
        for(int y=0;y<360;++y)for(int x=0;x<360;++x)
            assert(PixelRenderer::canvas().readPixel(x,y)==0x1234);
        explore.beginEncounter(*findSpecies(25),5);explore.clearBattleLogs();
        explore.phase=Ui::ExplorePhase::ENCOUNTER;explore.battleTurnStage=ExploreScene::BattleTurnStage::IDLE;
        capturePage("explore-round-battle");assert(Ui::SceneSurface::width()==360);
        explore.phase=Ui::ExplorePhase::WALKING;capturePage("explore-round-battle-return");
        assert(Ui::SceneSurface::width()==360 && explore.touchTarget(220,35)==144);
        for(auto phase:{Ui::ExplorePhase::PICKUP,Ui::ExplorePhase::SPECIAL_PROMPT,Ui::ExplorePhase::ENDING}) {
            explore.phase=phase;explore.prepareSurface();assert(Ui::SceneSurface::width()==360);
        }
        explore.phase=Ui::ExplorePhase::RESULT;explore.prepareSurface();assert(Ui::SceneSurface::width()==240);
        assert(state.settings.volume==0);
        for(int area=0;area<6;++area) {
            explore.activeArea=static_cast<ExploreScene::Area>(area);
            explore.resetWalk();
            capturePage(("explore-round-edge-area-"+std::to_string(area)).c_str());
            // Both cap centers must contain authored tiles, not the old field fill.
            const auto field=explore.exploreRouteFrame().fieldColor;
            bool topTerrain=false,bottomTerrain=false;
            for(int y=0;y<24;++y)for(int x=130;x<230;++x) {
                topTerrain|=PixelRenderer::canvas().readPixel(x,y)!=field;
                bottomTerrain|=PixelRenderer::canvas().readPixel(x,336+y)!=field;
            }
            assert(topTerrain && bottomTerrain);
        }
        std::puts("PASS native round map/menu geometry, cancel/outside isolation, pause/resume, nested team/status/bag and battle surfaces, HP/status slots and absent walking tutorial; volume zero");
        return 0;
    }
    if (mode == "team-moves") {
        auto& state=engine.gameState();
        state.team[0].move2Id=394;
        state.team[0].move3Id=104;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(1);
        auto& team=menu().teamUi;
        targetTap(team,1);capturePage("team-narrow-actions-second");
        // The old popup's empty left area now dismisses without selecting a card.
        pageTap(110,70);
        assert(!team.teamActionOpen && team.teamCursor==1 && state.teamCount==2);
        targetTap(team,0);capturePage("team-narrow-actions-first");
        int movesAction=-1;
        for(int i=0;i<team.teamActionCount();++i)
            if(team.teamActionAt(i)==Ui::TeamAction::MOVES)movesAction=i;
        assert(movesAction>=0);targetTap(team,128+movesAction);
        auto& moves=menu().movesUi;
        assert(menu().viewMode==MenuScene::ViewMode::MOVES && Ui::SceneSurface::width()==360);
        assert(moves.touchTarget(158,32)==230 && moves.touchTarget(28,24)==-1);
        capturePage("moves-native-header");
        assert(PixelRenderer::canvas().readPixel(6,180)==PixelRenderer::rgb(10,14,20));
        assert(PixelRenderer::canvas().readPixel(180,356)==PixelRenderer::rgb(10,14,20));
        assert(FontResource::ins().findCoverageGlyph(0x6280,20));
        assert(FontResource::ins().findCoverageGlyph(0x80fd,20));
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().viewMode==MenuScene::ViewMode::MOVES);
        event(Type::DOWN,158,32);event(Type::MOVE,180,100);event(Type::UP,180,100);
        assert(menu().viewMode==MenuScene::ViewMode::MOVES && !moves.moveForgetConfirmOpen);
        targetTap(moves,1);capturePage("moves-long-description");
        const auto move2=state.team[0].move2Id;
        // Description drags cannot select or activate a move.
        swipe(200,240,150);
        assert(moves.moveCursor==1 && !moves.moveForgetConfirmOpen);
        targetTap(moves,1);
        assert(moves.moveForgetConfirmOpen && !moves.moveForgetConfirmYes);
        capturePage("moves-native-forget-confirm");
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(moves.moveForgetConfirmOpen);
        back();assert(!moves.moveForgetConfirmOpen && state.team[0].move2Id==move2);
        // The real recall catalog exercises a list taller than the native viewport.
        state.team[0].level=100;
        assert(moves.openRecall(0));tick(50);
        assert(moves.recallMoveCount>5);
        capturePage("moves-native-recall");
        targetTap(moves,0);
        assert(moves.movePageMode==Ui::MovesUi::MovePageMode::RECALL_REPLACE);
        capturePage("moves-native-replace");
        back();assert(moves.movePageMode==Ui::MovesUi::MovePageMode::RECALL_SELECT);
        swipe(80,270,100);
        assert(moves.moveListScroll>0 && moves.movePageMode==Ui::MovesUi::MovePageMode::RECALL_SELECT);
        capturePage("moves-native-recall-scroll");
        back();assert(menu().viewMode==MenuScene::ViewMode::TEAM && Ui::SceneSurface::width()==240);
        // The same detail surface must return to exploration's compact team page.
        menu().exploreContextMode=true;Ui::SceneSurface::select(SceneID::EXPLORE);
        menu().openTeamMoves(0);assert(Ui::SceneSurface::width()==360);
        menu().popView();assert(Ui::SceneSurface::width()==240);
        assert(state.team[0].move2Id==move2 && state.settings.volume==0);
        std::puts("PASS narrow team popups/outside dismissal, native move header/body, drag/cancel isolation, forget cancellation, recall/replacement/list scroll and home/exploration surface restoration; volume zero");
        return 0;
    }
    if (mode == "status-round") {
        // A supported second description makes vertical overflow deterministic.
        engine.gameState().team[0].move3Id=104;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(1);
        targetTap(menu().teamUi,0);
        int statusAction=-1;
        for(int i=0;i<menu().teamUi.teamActionCount();++i)
            if(menu().teamUi.teamActionAt(i)==Ui::TeamAction::STATUS)statusAction=i;
        assert(statusAction>=0);targetTap(menu().teamUi,128+statusAction);
        assert(menu().viewMode==MenuScene::ViewMode::STATUS && Ui::SceneSurface::width()==360);
        auto& status=menu().statusUi;
        const auto region = [&](int left,int top,int width,int height) {
            std::vector<uint16_t> pixels;
            for(int y=top;y<top+height;++y)for(int x=left;x<left+width;++x)
                pixels.push_back(PixelRenderer::canvas().readPixel(x,y));
            return pixels;
        };
        capturePage("status-round-profile");
        const auto originalNature=engine.gameState().team[0].nature;
        engine.gameState().team[0].nature=16;
        capturePage("status-round-profile-mild");
        engine.gameState().team[0].nature=0;
        capturePage("status-round-profile-neutral");
        engine.gameState().team[0].nature=originalNature;
        capturePage("status-round-profile");
        const auto header=region(0,0,360,52);
        const auto dots=region(110,330,140,24);
        assert(PixelRenderer::canvas().readPixel(6,180)==PixelRenderer::rgb(10,14,20));
        assert(PixelRenderer::canvas().readPixel(180,356)==PixelRenderer::rgb(10,14,20));
        assert(status.touchTarget(158,32)==230 && status.touchTarget(200,32)==232);
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().viewMode==MenuScene::ViewMode::STATUS);
        event(Type::DOWN,158,32);event(Type::MOVE,180,100);event(Type::UP,180,100);
        assert(menu().viewMode==MenuScene::ViewMode::STATUS && status.statusPage==0);
        // Short swipes rebound, including when reversing direction before release.
        pageEvent(Type::DOWN,270,160);pageEvent(Type::MOVE,225,160);
        assert(status.statusSlideFrames && status.statusSlideDragging && status.statusSlidePixels==45);
        capturePage("status-round-short-drag");assert(region(0,0,360,52)==header && region(110,330,140,24)==dots);
        pageEvent(Type::MOVE,290,160);assert(status.statusSlideDirection==-1 && status.statusSlidePixels==20);
        pageEvent(Type::UP,290,160);tick(350);
        assert(status.statusPage==0 && !status.statusSlideFrames);
        pageEvent(Type::DOWN,270,160);pageEvent(Type::MOVE,90,160);
        event(Type::CANCEL,90,160);event(Type::UP,90,160);
        assert(status.statusPage==0 && !status.statusSlideFrames);
        for(int i=1;i<5;++i) {
            pageEvent(Type::DOWN,270,160);pageEvent(Type::MOVE,110,160);pageEvent(Type::UP,110,160);tick(350);
            assert(status.statusPage==i && !status.statusSlideFrames);
            capturePage(("status-round-page-"+std::to_string(i)).c_str());assert(region(0,0,360,52)==header);
        }
        // Title tap wraps to the first page without a numeric header counter.
        tap(200,32);tick(350);assert(status.statusPage==0);
        // Low memory keeps release-to-switch behavior and never retains a partial capture.
        memory.failStatusSlide=true;
        pageEvent(Type::DOWN,270,160);pageEvent(Type::MOVE,110,160);
        assert(!status.statusSlideFrames);pageEvent(Type::UP,110,160);tick(350);
        assert(status.statusPage==1 && !status.statusSlideFrames);memory.failStatusSlide=false;
        tap(200,32);tick(350);assert(status.statusPage==2);
        const auto maxScroll=std::max(0,status.touchStatusContentHeight()-248);
        assert(maxScroll>0);
        pageEvent(Type::DOWN,180,270);pageEvent(Type::MOVE,180,90);pageEvent(Type::UP,180,90);tick(100);
        assert(status.statusPage==2 && status.statusScroll==std::min(180.f,float(maxScroll)));
        capturePage("status-round-moves-bottom");assert(region(0,0,360,52)==header);
        event(Type::DOWN,20,20);event(Type::UP,20,20);assert(status.statusPage==2);
        back();assert(menu().viewMode==MenuScene::ViewMode::TEAM && Ui::SceneSurface::width()==240);
        capturePage("status-round-return-team");
        // The same native status surface is used inside exploration, then restores 240px content.
        menu().exploreContextMode=true;Ui::SceneSurface::select(SceneID::EXPLORE);
        menu().openTeamStatus(0);assert(Ui::SceneSurface::width()==360);
        menu().popView();assert(Ui::SceneSurface::width()==240);
        menu().exploreContextMode=false;Ui::SceneSurface::select(SceneID::MENU);
        Ui::SceneSurface::mainMenu(false);menu().openTeamStatus(0);
        engine.gameState().oobeDone=false;capturePage("status-round-egg");
        assert(status.touchTarget(200,32)==-1);engine.gameState().oobeDone=true;
        assert(engine.gameState().settings.volume==0);
        std::puts("PASS native status background/header, five-page arc markers, cancel/reverse/rebound/swipe/title-wrap, vertical scroll, low-memory fallback, team/exploration surface restoration and egg status; muted save retained");
        return 0;
    }
    if (mode == "settings-header") {
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(6);
        assert(engine.currentId==SceneID::SETTINGS);
        auto& settings=static_cast<SettingsScene*>(engine.currentScene.get())->ui;
        assert(settings.touchTarget(98,-28)==230);
        capturePage("settings-native-header");
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(engine.currentId==SceneID::SETTINGS);
        event(Type::DOWN,158,32);event(Type::MOVE,200,100);event(Type::UP,200,100);
        assert(engine.currentId==SceneID::SETTINGS);
        // Vertical movement from a slider must scroll without changing its value.
        const auto volume=engine.gameState().settings.volume;
        pageEvent(Type::DOWN,150,76);pageEvent(Type::MOVE,150,26);pageEvent(Type::UP,150,26);
        assert(settings.touchScroll>0 && engine.gameState().settings.volume==volume);
        swipe(70,210,70);targetTap(settings,Ui::SettingsUi::HELP);
        assert(settings.viewMode==Ui::SettingsUi::ViewMode::HELP);
        assert(settings.touchTarget(98,-28)==256+230);
        capturePage("settings-help-native-header");
        tap(158,32);assert(settings.viewMode==Ui::SettingsUi::ViewMode::MENU);
        // A stale help-page target must not activate a menu setting.
        assert(!settings.onTap(256+201));
        swipe(70,210,70);targetTap(settings,Ui::SettingsUi::RESET_GAME);
        assert(settings.viewMode==Ui::SettingsUi::ViewMode::RESET_CONFIRM && !settings.resetConfirmYes);
        capturePage("settings-reset-native-header");
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(settings.viewMode==Ui::SettingsUi::ViewMode::RESET_CONFIRM);
        tap(158,32);assert(settings.viewMode==Ui::SettingsUi::ViewMode::MENU);
        assert(engine.gameState().oobeDone && engine.gameState().settings.volume==0);
        tap(158,32);assert(nativeMenu());capturePage("settings-return-menu");
        std::puts("PASS settings native header, slider/scroll axis isolation, reachable help/reset, mode-scoped targets, nested back and cancellation; volume remains zero");
        return 0;
    }
    if (mode == "explore-select") {
        auto& state=engine.gameState();
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(0);
        assert(engine.currentId==SceneID::EXPLORE && Ui::SceneSurface::width()==360);
        auto& areas=*static_cast<ExploreScene*>(engine.currentScene.get());
        assert(areas.phase==Ui::ExplorePhase::SELECT && areas.ui.state.areaCursor==0);
        assert(areas.exploreRouteFrame().marketPreview);
        assert(FontResource::ins().findCoverageGlyph(0x63a2,20));
        assert(FontResource::ins().findCoverageGlyph(0x7d22,20));
        capturePage("explore-select-first");
        assert(areas.touchTarget(67,59)==-1 && areas.touchTarget(292,100)==Ui::RoundExplore::NEXT);
        // The background occupies the entire body while the native header stays fixed.
        auto& canvas=PixelRenderer::canvas();
        assert(canvas.readPixel(180,54)!=0 && canvas.readPixel(180,355)!=0);
        assert(canvas.readPixel(110,30)==PixelRenderer::rgb(7,12,17));
        selectRegion(areas,0);
        const auto grass=areas.ui.state.areaCursor;
        // Selection now commits the whole current pool after budgeted loading.
        unsigned previewWait=0;
        while (!areas.areaPreviewReady && previewWait<3000) { tick(100); previewWait+=100; }
        if (!areas.areaPreviewReady) std::fprintf(stderr,"%s",desktop.logs().c_str());
        assert(areas.areaPreviewReady);
        std::printf("PASS current pool settled after %u ms additional wait\n",previewWait);
        // A one-member pool stays centered throughout the automatic carousel cycle.
        auto single=areas.exploreRouteFrame();
        single.areaPreviewCount=1;
        single.areaPreviewFrames[0]=single.areaPreviewFrames[0] ? single.areaPreviewFrames[0] : single.areaPreviewFrames[1];
        assert(single.areaPreviewFrames[0]);
        single.areaPreviewHasRare=false;
        single.nowMs=single.areaPreviewStartedAt+1000;
        areas.ui.selection.render(single);
        std::vector<uint16_t> singleFrame;
        for(int y=106;y<246;++y)for(int x=0;x<360;++x)singleFrame.push_back(canvas.readPixel(x,y));
        single.nowMs=single.areaPreviewStartedAt+2600;
        areas.ui.selection.render(single);
        unsigned singlePixel=0;
        for(int y=106;y<246;++y)for(int x=0;x<360;++x)assert(canvas.readPixel(x,y)==singleFrame[singlePixel++]);
        const auto pixels=[&]() {
            std::vector<uint16_t> result;
            for(int y=0;y<360;++y)for(int x=0;x<360;++x)result.push_back(canvas.readPixel(x,y));
            return result;
        };
        auto strip=areas.exploreRouteFrame();
        strip.areaPreviewHasRare=false;
        strip.toast=nullptr;
        auto empty=strip;
        empty.areaPreviewCount=0;
        areas.ui.selection.render(empty);
        const auto background=pixels();
        // Two members must repeat beyond their immediate neighbors to reach both edges.
        strip.areaPreviewCount=2;
        strip.areaPreviewFrames[0]=strip.areaPreviewFrames[1]=single.areaPreviewFrames[0];
        strip.areaPreviewHidden[0]=strip.areaPreviewHidden[1]=false;
        for(const uint32_t phase : {0u,2300u,2425u,2550u,2675u,2799u}) {
            strip.nowMs=strip.areaPreviewStartedAt+phase;
            areas.ui.selection.render(strip);
            const auto image=pixels();
            int left=0,right=0;
            for(int y=106;y<246;++y)for(int x=0;x<360;++x) {
                if(image[y*360+x]!=background[y*360+x]) {
                    if(x<60)++left;
                    if(x>=300)++right;
                }
            }
            assert(left>0 && right>0);
            for(int y=0;y<360;++y)if(y<106 || y>=246)
                for(int x=0;x<360;++x)assert(image[y*360+x]==background[y*360+x]);
        }
        // Different sizes, silhouettes and cyclic wrap must not pop at the cycle seam.
        strip=areas.exploreRouteFrame();
        const auto loaded=strip;
        strip.areaPreviewCount=0;
        for(int i=0;i<loaded.areaPreviewCount;++i)if(loaded.areaPreviewFrames[i]) {
            const int slot=strip.areaPreviewCount++;
            strip.areaPreviewFrames[slot]=loaded.areaPreviewFrames[i];
            strip.areaPreviewHidden[slot]=loaded.areaPreviewHidden[i];
        }
        assert(strip.areaPreviewCount>1);
        const auto firstHidden=strip.areaPreviewHidden[0];
        strip.areaPreviewHidden[0]=true;
        for(int cycle=0;cycle<strip.areaPreviewCount;++cycle) {
            strip.nowMs=strip.areaPreviewStartedAt+(cycle+1)*Ui::ExploreRouteTiming::PREVIEW_CYCLE_MS-1;
            areas.ui.selection.render(strip);
            const auto before=pixels();
            ++strip.nowMs;
            areas.ui.selection.render(strip);
            assert(pixels()==before);
        }
        strip.areaPreviewHidden[0]=firstHidden;
        std::vector<uint16_t> held;
        for(int phase : {0,2425,2550,2675,2800}) {
            strip.nowMs=strip.areaPreviewStartedAt+phase;
            areas.ui.selection.render(strip);
            if(phase==0)held=pixels();
            if(phase==2550)assert(pixels()!=held);
            capture(output / ("explore-carousel-wide-"+std::to_string(phase)+".ppm"));
        }
        strip.areaPreviewCount=2;
        strip.areaPreviewFrames[0]=strip.areaPreviewFrames[1]=nullptr;
        areas.ui.selection.render(strip);
        strip.areaPreviewCount=0;
        areas.ui.selection.render(strip);
        std::puts("PASS full-width carousel edges throughout motion, vertical clipping, single-member stability, cycle seams and missing frames");
        engine.invalidateScene();tick(20);
        event(Type::DOWN,272,80);event(Type::CANCEL,272,80);event(Type::UP,272,80);
        assert(areas.ui.state.areaCursor==grass);
        event(Type::DOWN,272,80);event(Type::MOVE,200,80);event(Type::UP,272,80);
        assert(areas.ui.state.areaCursor==grass);
        tap(292,100);assert(areas.ui.state.areaCursor==grass+1);
        assert(areas.touchTarget(272,80)==-1);
        capturePage("explore-select-locked");
        const auto coins=state.coins;
        tap(180,302);assert(areas.phase==Ui::ExplorePhase::SELECT && state.coins==coins);
        assert(engine.exploreTravelPhase()==ExploreTravelPhase::NONE);
        tap(67,59);assert(areas.ui.state.areaCursor==grass);
        event(Type::DOWN,250,156);event(Type::MOVE,120,156);event(Type::UP,120,156);
        assert(areas.ui.state.areaCursor==grass+1);
        event(Type::DOWN,120,156);event(Type::MOVE,250,156);event(Type::CANCEL,250,156);event(Type::UP,250,156);
        assert(areas.ui.state.areaCursor==grass+1);
        event(Type::DOWN,120,156);event(Type::MOVE,250,156);event(Type::UP,250,156);
        assert(areas.ui.state.areaCursor==grass);
        event(Type::DOWN,180,120);event(Type::MOVE,180,220);event(Type::UP,180,220);
        assert(areas.ui.state.areaCursor==grass && areas.phase==Ui::ExplorePhase::SELECT);
        // Leaving the round panel cancels the gesture even when it returns inside.
        event(Type::DOWN,272,80);event(Type::MOVE,359,0);event(Type::UP,272,80);
        assert(areas.ui.state.areaCursor==grass);
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(engine.currentId==SceneID::EXPLORE);
        state.team[0].satiety=0;
        tap(180,302);assert(areas.phase==Ui::ExplorePhase::SELECT);
        capturePage("explore-select-hungry");
        state.team[0].satiety=75;
        for(auto& cleared:state.explorePoolRerollCounts)cleared=1;
        for(int i=grass+1;i<ExploreDestinations::COUNT;++i) {
            tap(272,80);assert(areas.ui.state.areaCursor==i);
            const auto name="explore-select-area-"+std::to_string(i);
            capturePage(name.c_str());
        }
        assert(areas.touchTarget(272,80)==-1 && areas.touchTarget(88,80)==Ui::RoundExplore::PREVIOUS);
        for(auto& cleared:state.explorePoolRerollCounts)cleared=0;
        tap(158,32);assert(engine.currentId==SceneID::MENU && nativeMenu());
        menuItem(0);assert(Ui::SceneSurface::width()==360);
        auto& again=*static_cast<ExploreScene*>(engine.currentScene.get());
        selectRegion(again,0);
        targetTap(again,Ui::RoundExplore::DEPART);
        const auto deadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<deadline)tick(100);
        assert(engine.currentId==SceneID::EXPLORE && Ui::SceneSurface::width()==360);
        assert(static_cast<ExploreScene*>(engine.currentScene.get())->phase==Ui::ExplorePhase::WALKING);
        capturePage("explore-select-departed");
        assert(state.settings.volume==0);
        std::puts("PASS round explore selection, body background, arrow edges/limits, locked departure, swipe/cancel/axis isolation, hungry feedback, menu return and walking surface restoration");
        return 0;
    }
    if (mode == "contacts-header") {
        auto& state=engine.gameState();
        state.storageCount=0;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(5);
        targetTap(menu().computerUi,1);
        assert(menu().viewMode==MenuScene::ViewMode::STORAGE);
        assert(Ui::SceneSurface::width()==360);
        capturePage("contacts-empty");
        state.storage[0]=state.team[0];state.storageCount=1;
        menu().contactsUi.reset();capturePage("contacts-one");
        for (int i=1;i<20;++i) {
            state.storage[i]=Game::MonsterFactory::create(25,5);
            state.storage[i].origin=Game::Origin::BEFRIENDED;
            state.storage[i].bond=100;
        }
        state.storageCount=20;menu().contactsUi.reset();capturePage("contacts-full");
        auto& contacts=menu().contactsUi;
        assert(contacts.touchTarget(49,98)==0 && contacts.touchTarget(310,98)==0);
        assert(contacts.touchTarget(180,138)==-1 && contacts.touchTarget(180,148)==1);
        event(Type::DOWN,180,98);event(Type::CANCEL,180,98);event(Type::UP,180,98);
        assert(!contacts.storageActionOpen);
        const auto footer = [&]() {
            std::vector<uint16_t> result;
            auto& c=PixelRenderer::canvas();
            for(int y=320;y<344;++y)for(int x=140;x<220;++x)result.push_back(c.readPixel(x,y));
            return result;
        };
        const auto fixedFooter=footer();
        assert(std::any_of(fixedFooter.begin(),fixedFooter.end(),[](uint16_t p){return p!=0;}));
        for(int i=0;i<9;++i) {swipe(120,220,30);tick(100);}
        capturePage("contacts-full-bottom");
        assert(menu().contactsUi.storageScroll>1000 && footer()==fixedFooter);
        tap(180,332);assert(!menu().contactsUi.storageActionOpen);
        targetTap(menu().contactsUi,19);assert(menu().contactsUi.storageActionOpen);
        assert(menu().contactsUi.touchTarget(158,32)==230);
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().contactsUi.storageActionOpen);
        int remove=-1;
        for(int i=0;i<menu().contactsUi.actionCount();++i)
            if(menu().contactsUi.actionAt(i)==Features::Contacts::Action::DELETE)remove=i;
        assert(remove>=0);targetTap(menu().contactsUi,128+remove);
        assert(menu().contactsUi.storageReleaseConfirmOpen);
        capturePage("contacts-full-confirm");assert(footer()==fixedFooter);
        back();assert(!menu().contactsUi.storageReleaseConfirmOpen && menu().contactsUi.storageActionOpen);
        back();assert(!menu().contactsUi.storageActionOpen && menu().viewMode==MenuScene::ViewMode::STORAGE);
        assert(state.storageCount==20);
        back();assert(menu().viewMode==MenuScene::ViewMode::COMPUTER);
        capturePage("contacts-return-computer");
        const auto cleared=footer();assert(std::all_of(cleared.begin(),cleared.end(),[](uint16_t p){return p==0;}));
        std::puts("PASS contacts native header, 0/1/20 capacity, fixed footer during scrolling/modal, last row reachable, footer input isolation and staged back without stale footer");
        return 0;
    }
    if (mode == "shop-tabs") {
        const auto bottomOverflow = [&](const ItemTouchUi::Grid& grid, float& scroll, const char* name) {
            assert(Ui::SceneSurface::width()==360);
            assert(grid.layout.viewport.y+grid.layout.viewport.height==360);
            int index=-1;
            for(int i=0;i<grid.count();++i) {
                const auto r=grid.cell(i);
                if(r.x==146 && r.y>=320 && r.y-320+80<=grid.maxScroll()) {index=i;break;}
            }
            assert(index>=0);
            const float start=grid.cell(index).y-320;
            scroll=start;
            capturePage(name);
            // A real card paints through the last physical row, beyond both old clips.
            assert(PixelRenderer::canvas().readPixel(148,359)==PixelRenderer::rgb(24,34,42));
            assert(grid.itemAt(180,350,scroll)==grid.targets[index]);
            event(Type::DOWN,180,350);event(Type::CANCEL,180,350);event(Type::UP,180,350);
            assert(scroll==start);
            event(Type::DOWN,180,350);event(Type::MOVE,180,270);event(Type::UP,180,270);
            assert(scroll>start);
            scroll=start;engine.invalidateScene();tick(20);
            tap(180,350);
        };
        for (int i=0;i<int(Game::ItemId::COUNT);++i)
            Game::ItemInventory::add(engine.gameState(),Game::ItemId(i),2);
        // Exercise long buy and sell catalogues, including the narrowing rim.
        for(auto& clears:engine.gameState().explorePoolRerollCounts) clears=1;
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(3);
        auto& bag=menu().bagUi;
        const auto inventory=bag.grid(engine.gameState(),false);
        assert(inventory.layout.columns==3 && inventory.layout.cellWidth==68 && inventory.layout.cellHeight==68);
        const int base=inventory.firstCount>=4?0:inventory.firstCount;
        assert(inventory.cell(base).y==inventory.cell(base+1).y && inventory.cell(base+1).y==inventory.cell(base+2).y);
        assert(inventory.cell(base+3).y==inventory.cell(base).y+76);
        for (int i=0;i<3;++i) {
            const auto cell=inventory.cell(base+i);
            assert(inventory.itemAt(cell.x,cell.y,0)==inventory.targets[base+i]);
            assert(inventory.itemAt(cell.x+67,cell.y+67,0)==inventory.targets[base+i]);
        }
        assert(inventory.itemAt(inventory.cell(base).x+69,inventory.cell(base).y,0)==-1);
        capturePage("bag-three-columns");
        bottomOverflow(inventory,bag.gridScroll,"bag-full-height");
        assert(bag.detailOpen);back();
        assert(!bag.detailOpen && bag.target(0,359,engine.gameState(),menu().bagContext())==-1);
        swipe(120,200,80);assert(bag.gridScroll>0 && !bag.detailOpen);
        const auto last=bag.grid(engine.gameState(),false);
        bag.gridScroll=last.maxScroll();engine.invalidateScene();tick(20);
        capturePage("bag-three-columns-bottom");
        const auto lastCell=last.cell(last.count()-2,bag.gridScroll);
        assert(lastCell.y+lastCell.height<=316);
        for(int i=std::max(0,last.count()-3);i<last.count();++i) {
            const auto r=last.cell(i,last.reveal(i,0));
            assert(r.y>=last.layout.viewport.y && r.y+r.height<=316);
            for(int x:{r.x,r.x+r.width-1})for(int y:{r.y,r.y+r.height-1})
                assert(Ui::RoundExplore::routePoint(x,y));
        }
        pageTap(lastCell.x+34,lastCell.y+34);assert(bag.detailOpen);
        back();back();menuItem(4);
        assert(engine.currentId==SceneID::SHOP && Ui::SceneSurface::width()==360);
        auto& shop=static_cast<ShopScene*>(engine.currentScene.get())->ui;
        const auto tabTap=[&](int index) {const auto r=shop.tabBounds(index);tap(r.x+r.width/2,r.y+r.height/2);};
        const auto coinPixels=[&]() {
            std::vector<uint16_t> result;const auto r=Ui::ShopUi::coinBounds();auto& c=PixelRenderer::canvas();
            for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)result.push_back(c.readPixel(x,y));
            return result;
        };
        capturePage("shop-tabs-buy");const auto balance=coinPixels();
        bottomOverflow(shop.touchShopGrid(),shop.touchGridScroll,"shop-buy-full-height");
        assert(shop.touchDetailOpen);back();
        assert(!shop.touchDetailOpen && shop.touchTarget(0,359)==-1 && balance==coinPixels());
        shop.touchGridScroll=0;engine.invalidateScene();tick(20);
        const auto buy=shop.tabBounds(0),sell=shop.tabBounds(1),coin=Ui::ShopUi::coinBounds();
        assert(buy.y==sell.y && sell.y==coin.y && buy.x+buy.width<sell.x && sell.x+sell.width<coin.x);
        assert(shop.touchShopGrid().layout.columns==3);
        assert(PixelRenderer::canvas().readPixel(buy.x+20,buy.y+31)==0xffff);
        const auto coins=engine.gameState().coins;
        // Empty tab corners are clickable; cancellation and drags cannot switch.
        event(Type::DOWN,sell.x,sell.y);event(Type::CANCEL,sell.x,sell.y);event(Type::UP,sell.x,sell.y);
        assert(shop.viewMode!=Ui::ShopUi::ViewMode::SELL);
        event(Type::DOWN,sell.x+20,sell.y+10);event(Type::MOVE,180,150);event(Type::UP,180,150);
        assert(shop.viewMode!=Ui::ShopUi::ViewMode::SELL && !shop.touchDetailOpen);
        event(Type::DOWN,104,250);event(Type::MOVE,104,130);event(Type::UP,104,130);
        assert(shop.touchGridScroll>0 && shop.viewMode!=Ui::ShopUi::ViewMode::SELL && !shop.touchDetailOpen);
        const auto buyScroll=shop.touchGridScroll;
        tap(sell.x,sell.y);assert(shop.viewMode==Ui::ShopUi::ViewMode::SELL);
        assert(!shop.touchDetailOpen && balance==coinPixels());
        capturePage("shop-tabs-sell");
        bottomOverflow(shop.touchShopGrid(),shop.touchGridScroll,"shop-sell-full-height");
        assert(shop.touchDetailOpen);back();
        assert(!shop.touchDetailOpen && balance==coinPixels());
        shop.touchGridScroll=0;engine.invalidateScene();tick(20);
        event(Type::DOWN,180,260);event(Type::MOVE,180,140);event(Type::UP,180,140);
        const auto sellScroll=shop.touchGridScroll;assert(sellScroll>0 && !shop.touchDetailOpen);
        tabTap(0);assert(shop.touchGridScroll==buyScroll && balance==coinPixels());
        tabTap(1);assert(shop.touchGridScroll==sellScroll && balance==coinPixels());
        assert(coins==engine.gameState().coins);
        shop.touchGridScroll=shop.touchShopGrid().maxScroll();engine.invalidateScene();tick(20);
        const auto lastProduct=shop.touchShopGrid().cell(shop.touchShopGrid().count()-1,shop.touchGridScroll);
        assert(lastProduct.y+lastProduct.height<=316);
        capturePage("shop-tabs-sell-bottom");
        shop.touchGridScroll=0;engine.invalidateScene();tick(20);
        const auto first=shop.touchShopGrid().cell(0);tap(first.x+34,first.y+34);
        assert(shop.touchDetailOpen);capturePage("shop-tabs-sell-detail");
        const auto owned=shop.ownedCountFor(shop.selectedItem());const auto money=engine.gameState().coins;
        const auto action=Ui::ShopUi::actionBounds();tap(action.x+action.width/2,action.y+action.height/2);
        assert(shop.ownedCountFor(shop.selectedItem())==owned-1 && engine.gameState().coins>money);
        capturePage("shop-tabs-sold");
        back();tabTap(0);
        shop.touchGridScroll=0;engine.invalidateScene();tick(20);
        const auto product=shop.touchShopGrid().cell(0);tap(product.x+34,product.y+34);
        assert(shop.touchDetailOpen);capturePage("shop-tabs-buy-detail");
        const auto beforeBuy=engine.gameState().coins;tap(action.x+action.width/2,action.y+action.height/2);
        assert(engine.gameState().coins<beforeBuy);back();tick(2000);
        // A mode change while holding a product must cancel the old release.
        event(Type::DOWN,product.x+34,product.y+34);shop.switchMode(true);
        event(Type::UP,product.x+34,product.y+34);assert(!shop.touchDetailOpen);
        tabTap(0);
        const auto frames=desktop.presentCount();tick(400);assert(frames==desktop.presentCount());
        for (const auto& sample:std::vector<std::pair<uint32_t,const char*>>{
                {0,"0"},{9999,"9999"},{10000,"1万"},{10099,"1万"},
                {10100,"1.01万"},{11000,"1.1万"},{12345,"1.23万"},
                {19999,"1.99万"},{999999,"99.99万"},{1000000,"100万"},
                {UINT32_MAX,"429496万"}}) {
            char text[24];const int width=Ui::ShopUi::formatBalance(sample.first,text);
            assert(std::strcmp(text,sample.second)==0 && width+20<=coin.width);
        }
        for(const auto& sample:std::vector<std::pair<uint32_t,const char*>>{
                {9999,"shop-coins-9999"},{10000,"shop-coins-10000"},
                {12345,"shop-coins-12345"},{999999,"shop-coins-999999"},
                {UINT32_MAX,"shop-tabs-max-coins"}}) {
            engine.gameState().coins=sample.first;engine.invalidateScene();tick(20);capturePage(sample.second);
        }
        tap(coin.x+coin.width/2,coin.y+coin.height/2);
        assert(std::strcmp(shop.exactBalance,"4294967295C")==0);
        assert(engine.gameState().coins==UINT32_MAX && !shop.touchDetailOpen);
        engine.invalidateScene();tick(20);capture(output/"shop-coins-exact.ppm");
        tick(2300);
        const auto quietFrames=desktop.presentCount();tick(400);assert(quietFrames==desktop.presentCount());
        assert(shop.touchTarget(20,20)==-1);
        back();assert(nativeMenu() && engine.gameState().settings.volume==0);
        std::puts("PASS three-column bag/shop, native icon targets, fixed tabs/balance, cancel and scroll isolation, remembered scroll, real buy/sell, compact/exact money and parked rendering");
        return 0;
    }
    if (mode == "menu-fixed") {
        tap(homeMenuX,homeMenuY);tick(1000);
        assert(nativeMenu() && Ui::SceneSurface::width()==360);
        auto& ui=menu().mainMenuUi;
        const auto grid=Ui::MainMenuUi::layout();
        for (int offset=0;offset<=int(Ui::MainMenuUi::maximumScroll());++offset) {
            ui.scroll=float(offset);
            for (int i=0;i<Ui::MainMenuUi::ROUND_ITEM_COUNT;++i) {
                const auto original=grid.cell(i,ui.scroll);
                const auto r=Ui::MainMenuUi::itemBounds(i,ui.scroll);
                assert(r.width==108 && r.height==72);
                assert(r.x==original.x && r.y==original.y);
                const int cy=original.y+original.height/2;
                const int cx=r.x+r.width/2;
                if (cy>=grid.viewport.y && cy<320) assert(ui.touchTarget(cx,cy)==i);
                if (r.y>=grid.viewport.y && r.y+r.height<320)
                    assert(ui.touchTarget(r.x-1,cy)==-1);
                if (i%2==0) {
                    const auto peer=Ui::MainMenuUi::itemBounds(i+1,ui.scroll);
                    assert(peer.width==r.width && peer.height==r.height && peer.y==r.y);
                }
            }
        }
        ui.scroll=0;capturePage("menu-fixed-top");
        {
            ui.scroll=Ui::MainMenuUi::maximumScroll()/2;capturePage("menu-fixed-middle");
            ui.scroll=Ui::MainMenuUi::maximumScroll();capturePage("menu-fixed-bottom");
            const auto last=Ui::MainMenuUi::itemBounds(7,ui.scroll);
            for (int y:{last.y,last.y+last.height-1}) for (int x:{last.x,last.x+last.width-1}) {
                const int dx=2*x-359,dy=2*y-359;assert(dx*dx+dy*dy<360*360);
            }
            // Allow startup sprite warming and menu-open feedback to finish.
            ui.scroll=0;engine.invalidateScene();tick(2000);
            const auto frames=desktop.presentCount();tick(400);assert(frames==desktop.presentCount());
            event(Type::DOWN,180,250);event(Type::MOVE,180,130);event(Type::UP,180,130);
            assert(nativeMenu() && ui.scroll==Ui::MainMenuUi::maximumScroll());
            event(Type::DOWN,180,130);event(Type::MOVE,180,250);event(Type::UP,180,250);
            assert(nativeMenu() && ui.scroll==0);
            const auto first=Ui::MainMenuUi::itemBounds(0,0);
            event(Type::DOWN,first.x+first.width/2,first.y+first.height/2);
            event(Type::MOVE,20,20);event(Type::UP,first.x+first.width/2,first.y+first.height/2);
            assert(nativeMenu());
        }
        ui.scroll=0;menuItem(1);
        assert(menu().viewMode==MenuScene::ViewMode::TEAM && Ui::SceneSurface::width()==240);
        back();assert(nativeMenu() && Ui::SceneSurface::width()==360);
        back();assert(engine.currentId==SceneID::MAIN && engine.gameState().settings.volume==0);
        std::puts("PASS fixed-size menu cards, hit geometry, final row, drag cancellation, parked rendering and subpage mapping");
        return 0;
    }
    if (mode == "onboard-explore") {
        assert(engine.gameState().tutorialFlags==0x1f);
        tap(homeMenuX,homeMenuY);tick(1000);menuItem(0);
        auto& areas=*static_cast<ExploreScene*>(engine.currentScene.get());
        selectRegion(areas,0);targetTap(areas,Ui::RoundExplore::DEPART);
        const auto deadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<deadline)tick(100);
        assert(engine.currentId==SceneID::EXPLORE);tick(500);
        auto& explore=*static_cast<ExploreScene*>(engine.currentScene.get());
        assert(explore.phase==Ui::ExplorePhase::WALKING);capturePage("onboard-explore-walk");
        assert(explore.exploreTutorialFrame().kind==Ui::ExploreTutorialKind::WALK);
        targetTap(explore,100);tick(100);
        assert(engine.tutorialComplete(Game::TutorialStep::EXPLORE_WALK));
        assert(explore.exploreTutorialFrame().kind==Ui::ExploreTutorialKind::MENU);
        capturePage("onboard-explore-menu");
        targetTap(explore,144);assert(explore.exploreMenuOpen);
        assert(engine.tutorialComplete(Game::TutorialStep::EXPLORE_MENU));
        explore.exploreMenuOpen=false;explore.beginEncounter(*findSpecies(25),5);explore.clearBattleLogs();
        explore.phase=Ui::ExplorePhase::ENCOUNTER;explore.battleTurnStage=ExploreScene::BattleTurnStage::IDLE;
        assert(explore.exploreTutorialFrame().kind==Ui::ExploreTutorialKind::BATTLE);
        capturePage("onboard-battle-action");targetTap(explore,110);tick(300);
        assert(engine.gameState().tutorialFlags==0xff);
        assert(explore.exploreTutorialFrame().text==nullptr);
        assert(session.save());
        std::puts("PASS touch exploration/walk/menu/battle tutorial actions, hints and all eight completion flags");
        return 0;
    }
    if (mode == "reset") {
        tap(homeMenuX,homeMenuY); tick(1000); menuItem(6);
        assert(engine.currentId == SceneID::SETTINGS);
        auto& settings = static_cast<SettingsScene*>(engine.currentScene.get())->ui;
        swipe(80,210,70); swipe(80,210,70);
        targetTap(settings, Ui::SettingsUi::RESET_GAME);
        assert(settings.viewMode == Ui::SettingsUi::ViewMode::RESET_CONFIRM);
        capturePage("reset-confirm");
        pageTap(60,217); assert(engine.gameState().oobeDone);
        targetTap(settings, Ui::SettingsUi::RESET_GAME);
        pageTap(180,217); tick(1000);
        assert(engine.currentId == SceneID::HATCH && !engine.gameState().oobeDone);
        capturePage("hatch-after-reset");
        auto& hatch = *static_cast<HatchScene*>(engine.currentScene.get());
        tap(130,285); tap(230,285);
        assert(hatch.wipeCount == 1 && hatch.pokeCount == 1);
        tap(180,324); tick(1000); assert(engine.currentId == SceneID::MENU);
        back(); assert(engine.currentId == SceneID::HATCH);
        assert(session.save());
        // Test completion through the existing shared hatch update, with a near-complete fixture.
        static_cast<HatchScene*>(engine.currentScene.get())->elapsed = engine.gameState().hatchSeconds - 0.1f;
        engine.invalidateScene(); tick(1200);
        assert(engine.currentId == SceneID::MAIN && engine.gameState().oobeDone);
        capturePage("room-after-hatch");
        std::puts("PASS reset cancel/confirm, hatch care/menu/return and shared completion");
        return 0;
    }

    if (mode == "restore") {
        const auto expectedBytes = readFile(output / "expected.snapshot");
        SaveCodec::Snapshot expected{};
        assert(SaveCodec::decode(expectedBytes.data(), expectedBytes.size(), expected));
        const auto& state = engine.gameState();
        assert(state.teamCount == expected.state.teamCount);
        assert(state.team[0].speciesId == expected.state.team[0].speciesId);
        assert(state.team[0].ivPacked == expected.state.team[0].ivPacked);
        assert(state.team[0].satiety == expected.state.team[0].satiety);
        assert(state.team[0].hpCur == expected.state.team[0].hpCur);
        assert(state.team[0].exp == expected.state.team[0].exp);
        assert(state.coins == expected.state.coins);
        assert(state.settings.volume == 0 && expected.state.settings.volume == 0);
        assert(Platform::audio().volume() == 0);
        for (unsigned i = 0; i < Game::ROOM_FOOD_COUNT; ++i)
            assert(state.room.food[i] == expected.state.room.food[i]);
        assert(state.room.selectedFood == expected.state.room.selectedFood);
        assert(state.room.bowlFood == expected.state.room.bowlFood);
        assert(state.room.bowlCount == expected.state.room.bowlCount);
        assert(state.room.bowlBitesRemaining == expected.state.room.bowlBitesRemaining);
        assert(state.room.roomStyle == expected.state.room.roomStyle);
        assert(state.room.activeToy == expected.state.room.activeToy);
        assert(state.room.ownedToys == expected.state.room.ownedToys);
        assert(state.room.ownedFurniture == expected.state.room.ownedFurniture);
        assert(state.room.placedFurniture == expected.state.room.placedFurniture);
        const auto& view = engine.mainSceneViewState();
        assert(view.valid == expected.view.valid);
        if (std::fabs(view.monsterX - expected.view.monsterX) >= 0.001f ||
            std::fabs(view.monsterY - expected.view.monsterY) >= 0.001f) {
            std::fprintf(stderr, "restore position %.3f,%.3f expected %.3f,%.3f task=%u\n",
                view.monsterX, view.monsterY, expected.view.monsterX, expected.view.monsterY, expected.view.aiMode);
            writeFile(output / "restore-failed.log", desktop.logs().data(), desktop.logs().size());
        }
        assert(std::fabs(view.monsterX - expected.view.monsterX) < 0.001f);
        assert(std::fabs(view.monsterY - expected.view.monsterY) < 0.001f);
        tick(500);
        capture(output / "restored.ppm");
        std::printf("PASS fresh process restored species=%u stock=%u bites=%u satiety=%u position=%.2f,%.2f\n",
            state.team[0].speciesId, state.room.food[0], state.room.bowlBitesRemaining,
            state.team[0].satiety, expected.view.monsterX, expected.view.monsterY);
    } else {
        assert(mode == "fresh");
        tick(500);
        capture(output / "before.ppm");
        const uint8_t stock = engine.selectedFoodCount();
        const uint8_t newbornSatiety = engine.activeMonster().satiety;
        assert(stock > 0 && !engine.bowlHasFood());
        assert(engine.currentScene->touchTarget(180, 282) == -1);
        tap(180, 282);
        assert(engine.selectedFoodCount() == stock && !engine.bowlHasFood());
        const auto bowl = bowlPoint();
        event(Type::DOWN, bowl.first, bowl.second);
        event(Type::MOVE, bowl.first + 30, bowl.second);
        event(Type::UP, bowl.first, bowl.second);
        assert(engine.selectedFoodCount() == stock && !engine.bowlHasFood());
        event(Type::DOWN, bowl.first, bowl.second);
        event(Type::CANCEL, bowl.first, bowl.second);
        event(Type::UP, bowl.first, bowl.second);
        assert(engine.selectedFoodCount() == stock && !engine.bowlHasFood());
        event(Type::DOWN, bowl.first, bowl.second);
        assert(engine.selectedFoodCount() == stock);
        event(Type::UP, bowl.first, bowl.second);
        assert(engine.selectedFoodCount() == stock - 1);
        assert(engine.bowlHasFood() && engine.bowlFoodBitesRemaining() == 3);
        tap(bowl.first, bowl.second);
        assert(engine.selectedFoodCount() == stock - 1);
        SaveManager verifier;
        Game::GameState persisted{};
        MainSceneViewState persistedView{};
        assert(verifier.begin() && verifier.load(persisted, persistedView));
        assert(persisted.room.food[0] == stock - 1 && persisted.room.bowlBitesRemaining == 3);
        capture(output / "food-added.ppm");

        // Newborn starts at 75, above the shared feeding target (70). Keep firmware
        // rules unchanged; use a hungry test fixture to exercise the actual AI bite.
        assert(engine.activeMonster().satiety == newbornSatiety);
        const uint8_t satiety = 50;
        engine.gameState().team[0].satiety = satiety;
        // Run real room AI until it eats; no direct calls to the bite rules.
        const auto deadline = desktop.millis() + 60000;
        while (engine.activeMonster().satiety == satiety && desktop.millis() < deadline) tick(100);
        if (engine.activeMonster().satiety <= satiety) {
            writeFile(output / "feeding-failed.log", desktop.logs().data(), desktop.logs().size());
            capture(output / "feeding-failed.ppm");
        }
        assert(engine.activeMonster().satiety > satiety);
        tick(400);
        capture(output / "after-bite.ppm");
        // Shared restoration repositions feeding poses outside the walk area.
        // Let the real AI leave the bowl before checking exact walk-position recovery.
        tick(10000);
        assert(session.save());
        assert(verifier.load(persisted, persistedView));
        assert(persisted.team[0].satiety > satiety && persistedView.valid);

        // All page actions below pass through the actual engine touch dispatcher.
        const auto bowlBeforeMenu = engine.bowlFoodBitesRemaining();
        const auto foodBeforeMenu = engine.selectedFoodCount();
        event(Type::DOWN,homeMenuX,homeMenuY); event(Type::MOVE,homeMenuX+30,homeMenuY); event(Type::UP,homeMenuX,homeMenuY);
        assert(engine.currentId == SceneID::MAIN);
        event(Type::DOWN,homeMenuX,homeMenuY); event(Type::CANCEL,homeMenuX,homeMenuY); event(Type::UP,homeMenuX,homeMenuY);
        assert(engine.currentId == SceneID::MAIN);
        tap(homeMenuX,homeMenuY); tick(1500);
        assert(menu().viewMode == MenuScene::ViewMode::MENU && menu().navDepth == 0);
        capturePage("main-menu");
        const auto menuFrames = desktop.presentCount(); tick(400);
        assert(desktop.presentCount() == menuFrames);
        tap(20,20); assert(menu().viewMode == MenuScene::ViewMode::MENU);
        // Leaving the native page cancels even when the finger returns before UP.
        pageEvent(Type::DOWN,60,80); event(Type::MOVE,20,20); pageEvent(Type::UP,60,80);
        assert(menu().viewMode == MenuScene::ViewMode::MENU);
        assert(engine.selectedFoodCount() == foodBeforeMenu && engine.bowlFoodBitesRemaining() == bowlBeforeMenu);
        menuItem(2); assert(menu().viewMode == MenuScene::ViewMode::ROOM);
        assert(menu().roomUi.touchTarget(98,-28)==230);
        capturePage("room-menu");
        targetTap(menu().roomUi,0);
        assert(menu().viewMode == MenuScene::ViewMode::FOOD && menu().navDepth == 2);
        capturePage("food-top");
        const auto foodsBefore = engine.gameState().room;
        foodRow(1); assert(engine.selectedFoodIndex() == foodsBefore.selectedFood);
        for (unsigned i=0;i<Game::ROOM_FOOD_COUNT;++i) assert(engine.foodCount(i)==foodsBefore.food[i]);
        capturePage("food-empty-notice"); tick(1100);
        const auto foodFrames=desktop.presentCount();tick(400);assert(desktop.presentCount()==foodFrames);
        engine.gameState().room.food[6]=4;
        swipe(120,210,70);swipe(120,210,70);
        assert(menu().foodUi.foodScroll>200 && engine.selectedFoodIndex()==0);
        capturePage("food-bottom");
        const auto lastRect=Ui::RoundPage::listLayout().cell(6,menu().foodUi.foodScroll);
        const int lastY=lastRect.y+lastRect.height/2;
        assert(Ui::RoundPage::BODY.contains(120,lastY));
        pageEvent(Type::DOWN,120,lastY);pageEvent(Type::CANCEL,120,lastY);pageEvent(Type::UP,120,lastY);
        assert(engine.selectedFoodIndex()==0);
        pageEvent(Type::DOWN,120,lastY);pageEvent(Type::MOVE,150,lastY);pageEvent(Type::UP,120,lastY);
        assert(engine.selectedFoodIndex()==0);
        pageEvent(Type::DOWN,120,lastY);assert(engine.selectedFoodIndex()==0);pageEvent(Type::UP,120,lastY);
        assert(engine.selectedFoodIndex()==6 && engine.foodCount(6)==4);
        assert(engine.bowlFoodBitesRemaining()==bowlBeforeMenu && engine.bowlFoodIndex()==foodsBefore.bowlFood);
        assert(verifier.load(persisted,persistedView) && persisted.room.selectedFood==6 && persisted.room.food[6]==4);
        capturePage("food-selected");
        pageEvent(Type::DOWN,120,lastY);menu().pageBack();pageEvent(Type::UP,120,lastY);
        assert(menu().viewMode==MenuScene::ViewMode::ROOM && power.restarts==0);
        targetTap(menu().roomUi,0);assert(menu().foodUi.foodCursor==6 && menu().foodUi.foodScroll>0);
        back();back();assert(menu().viewMode==MenuScene::ViewMode::MENU);

        menuItem(1);assert(menu().viewMode==MenuScene::ViewMode::TEAM);capturePage("team");
        targetTap(menu().teamUi,0);assert(menu().teamUi.teamActionOpen);capturePage("team-actions");
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().teamUi.teamActionOpen);
        tap(158,32);assert(menu().viewMode==MenuScene::ViewMode::TEAM && !menu().teamUi.teamActionOpen);
        targetTap(menu().teamUi,0);assert(menu().teamUi.teamActionOpen);
        int statusAction=0,movesAction=0;
        for(int i=0;i<menu().teamUi.teamActionCount();++i) {
            if(menu().teamUi.teamActionAt(i)==Ui::TeamAction::STATUS)statusAction=i;
            if(menu().teamUi.teamActionAt(i)==Ui::TeamAction::MOVES)movesAction=i;
        }
        targetTap(menu().teamUi,128+statusAction);assert(menu().viewMode==MenuScene::ViewMode::STATUS);
        capturePage("status-0");
        for(int i=1;i<5;++i) {
            pageEvent(Type::DOWN,200,140);pageEvent(Type::MOVE,60,140);
            capturePage(("status-drag-"+std::to_string(i)).c_str());
            pageEvent(Type::UP,60,140);tick(350);
            assert(menu().statusUi.statusPage==i);capturePage(("status-"+std::to_string(i)).c_str());
        }
        back();assert(menu().viewMode==MenuScene::ViewMode::TEAM);
        targetTap(menu().teamUi,0);targetTap(menu().teamUi,128+movesAction);
        assert(menu().viewMode==MenuScene::ViewMode::MOVES);capturePage("moves");
        swipe(170,200,130);capturePage("moves-description");
        targetTap(menu().movesUi,1);targetTap(menu().movesUi,1);
        assert(menu().movesUi.moveForgetConfirmOpen);capturePage("moves-forget-confirm");
        targetTap(menu().movesUi,201);assert(!menu().movesUi.moveForgetConfirmOpen);back();back();

        menuItem(3);assert(menu().viewMode==MenuScene::ViewMode::BAG);capturePage("bag");
        assert(Ui::SceneSurface::width()==360);
        assert(FontResource::ins().findCoverageGlyph(0x80cc,20));
        assert(FontResource::ins().findCoverageGlyph(0x5305,20));
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().viewMode==MenuScene::ViewMode::BAG && !menu().bagUi.detailOpen);
        event(Type::DOWN,158,32);event(Type::MOVE,158,140);event(Type::UP,158,140);
        assert(menu().viewMode==MenuScene::ViewMode::BAG && !menu().bagUi.detailOpen);
        pageTap(28,24);assert(menu().viewMode==MenuScene::ViewMode::BAG && !menu().bagUi.detailOpen);
        int potionIndex=0;
        while(Game::ItemInventory::homeBagItemAt(engine.gameState(),potionIndex)!=Game::ItemId::POTION)++potionIndex;
        menu().bagUi.gridScroll=menu().bagUi.grid(engine.gameState(),false).reveal(potionIndex,menu().bagUi.gridScroll);
        const auto potionRect=menu().bagUi.grid(engine.gameState(),false).targetCell(potionIndex,menu().bagUi.gridScroll);
        engine.invalidateScene();tick(20);
        pageTap(potionRect.x+potionRect.width/2,potionRect.y+potionRect.height/2);
        assert(menu().bagUi.detailOpen);capturePage("bag-detail");
        const auto potions=engine.gameState().bag.potion;
        pageTap(130,270);assert(menu().selectingTeamItem() && !menu().bagUi.confirmOpen);
        capturePage("bag-target");
        back();assert(menu().viewMode==MenuScene::ViewMode::BAG && engine.gameState().bag.potion==potions);
        engine.gameState().team[0].hpCur=engine.gameState().team[0].hpMax-5;
        pageTap(potionRect.x+potionRect.width/2,potionRect.y+potionRect.height/2);
        pageTap(130,270);targetTap(menu().teamUi,0);
        assert(menu().itemFeedback && engine.gameState().bag.potion==potions-1);
        capturePage("bag-recovery");tick(800);
        assert(menu().viewMode==MenuScene::ViewMode::BAG && !menu().bagUi.detailOpen);
        assert(engine.gameState().team[0].hpCur==engine.gameState().team[0].hpMax);back();

        menuItem(4);assert(engine.currentId==SceneID::SHOP);capturePage("shop");
        auto& shop=static_cast<ShopScene*>(engine.currentScene.get())->ui;
        const auto product=shop.touchShopGrid().cell(0);
        tap(product.x+product.width/2,product.y+product.height/2);assert(shop.touchDetailOpen);capturePage("shop-detail");
        const auto coins=engine.gameState().coins;
        const auto action=Ui::ShopUi::actionBounds();tap(action.x+action.width/2,action.y+action.height/2);
        assert(engine.gameState().coins<coins);capturePage("shop-bought");
        back();const auto sellTab=shop.tabBounds(1);tap(sellTab.x+sellTab.width/2,sellTab.y+sellTab.height/2);capturePage("shop-sell");back();

        // Contacts fixture exercises detail/status and confirmation without deleting the real save.
        engine.gameState().storageCount=2;engine.gameState().storage[0]=engine.gameState().team[0];
        engine.gameState().storage[1]=Game::MonsterFactory::create(25,5);
        engine.gameState().storage[1].origin=Game::Origin::BEFRIENDED;engine.gameState().storage[1].bond=100;
        menuItem(5);assert(menu().viewMode==MenuScene::ViewMode::COMPUTER);capturePage("computer");
        assert(menu().computerUi.touchTarget(98,-28)==230);
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(menu().viewMode==MenuScene::ViewMode::COMPUTER);
        targetTap(menu().computerUi,1);assert(menu().viewMode==MenuScene::ViewMode::STORAGE);capturePage("contacts");
        targetTap(menu().contactsUi,0);assert(menu().contactsUi.storageActionOpen);capturePage("contact-actions");
        targetTap(menu().contactsUi,128);assert(menu().viewMode==MenuScene::ViewMode::STATUS);capturePage("contact-status");
        back();targetTap(menu().contactsUi,1);
        int invite=-1,remove=-1;
        for(int i=0;i<menu().contactsUi.actionCount();++i) {
            if(menu().contactsUi.actionAt(i)==Features::Contacts::Action::INVITE)invite=i;
            if(menu().contactsUi.actionAt(i)==Features::Contacts::Action::DELETE)remove=i;
        }
        assert(invite>=0 && remove>=0);targetTap(menu().contactsUi,128+invite);
        assert(menu().contactsUi.storageInviteConfirmOpen);capturePage("contact-invite");
        targetTap(menu().contactsUi,201);assert(engine.gameState().teamCount==1);
        targetTap(menu().contactsUi,128+remove);assert(menu().contactsUi.storageReleaseConfirmOpen);capturePage("contact-delete");
        targetTap(menu().contactsUi,201);assert(engine.gameState().storageCount==2);
        targetTap(menu().contactsUi,128+remove);targetTap(menu().contactsUi,200);assert(engine.gameState().storageCount==1);
        tap(158,32);assert(menu().viewMode==MenuScene::ViewMode::COMPUTER);
        targetTap(menu().computerUi,0);tick(1000);assert(engine.currentId==SceneID::SOCIAL);capturePage("social");
        pageTap(120,72);tick(200);
        assert(engine.visitService().state()==Communication::VisitSessionService::State::FAILED);
        capturePage("social-unavailable");pageTap(35,224);tick(1000);assert(engine.currentId==SceneID::MENU);

        menuItem(2);targetTap(menu().roomUi,1);tick(1000);assert(engine.currentId==SceneID::SHOWER);capturePage("shower");
        auto& shower=*static_cast<ShowerScene*>(engine.currentScene.get());
        engine.gameState().bag.soap[0]=2;
        targetTap(shower,100);assert(shower.mode==Ui::ShowerMode::SOAP_SELECT);capturePage("soap-picker");
        targetTap(shower,120);assert(shower.mode==Ui::ShowerMode::SOAPING && engine.gameState().bag.soap[0]==1);
        pageEvent(Type::DOWN,90,130);pageEvent(Type::MOVE,110,145);pageEvent(Type::UP,110,145);
        assert(!shower.ui.dragging);capturePage("shower-soaping");
        targetTap(shower,103);assert(shower.mode==Ui::ShowerMode::EXIT_CONFIRM);capturePage("shower-exit");
        targetTap(shower,110);tick(2500);assert(engine.currentId==SceneID::MENU);back();assert(engine.currentId==SceneID::MAIN);
        capturePage("room-return");tap(homeMenuX,homeMenuY);tick(1000);

        menuItem(0);assert(engine.currentId==SceneID::EXPLORE);capturePage("explore-areas");
        auto& areas=*static_cast<ExploreScene*>(engine.currentScene.get());
        selectRegion(areas,0);
        targetTap(areas,Ui::RoundExplore::DEPART);
        const auto departureDeadline=desktop.millis()+30000;
        while(engine.currentId!=SceneID::EXPLORE && desktop.millis()<departureDeadline)tick(100);
        if(engine.currentId!=SceneID::EXPLORE)writeFile(output/"departure-failed.log",desktop.logs().data(),desktop.logs().size());
        assert(engine.currentId==SceneID::EXPLORE);tick(500);
        auto& explore=*static_cast<ExploreScene*>(engine.currentScene.get());
        assert(explore.phase==Ui::ExplorePhase::WALKING);capturePage("explore-route");
        targetTap(explore,144); // TARGET_OPEN_MENU
        assert(explore.exploreMenuOpen);capturePage("explore-menu");
        targetTap(explore,120);assert(explore.exploreSubViewOpen);capturePage("explore-team");back();
        assert(!explore.exploreSubViewOpen);
        targetTap(explore,121);assert(explore.exploreSubViewOpen);
        assert(explore.exploreSubView.viewMode==MenuScene::ViewMode::BAG);
        capturePage("explore-bag");
        event(Type::DOWN,158,32);event(Type::CANCEL,158,32);event(Type::UP,158,32);
        assert(explore.exploreSubViewOpen);
        tap(158,32);assert(!explore.exploreSubViewOpen && explore.exploreMenuOpen);
        capturePage("explore-bag-return");
        // The native bag header must not intercept the restored route/menu.
        Event staleHeader{Type::DOWN,158,32,desktop.millis()};
        assert(Ui::SceneSurface::mapTouch(staleHeader) && staleHeader.type==Type::DOWN);
        assert(explore.touchTarget(staleHeader.x,staleHeader.y)==-1);
        // Use a deterministic battle fixture; actions still pass through native touch input.
        explore.exploreMenuOpen=false;explore.beginEncounter(*findSpecies(25),5);explore.clearBattleLogs();
        explore.phase=Ui::ExplorePhase::ENCOUNTER;explore.battleTurnStage=ExploreScene::BattleTurnStage::IDLE;
        engine.invalidateScene();capturePage("battle");
        targetTap(explore,110);tick(300);capturePage("battle-action");
        engine.beginExploreReturn(false);
        const auto returnDeadline=desktop.millis()+30000;
        while(engine.exploreTravelPhase()!=ExploreTravelPhase::NONE && desktop.millis()<returnDeadline)tick(100);
        assert(engine.currentId==SceneID::MAIN && engine.exploreTravelPhase()==ExploreTravelPhase::NONE);
        tap(homeMenuX,homeMenuY);tick(1000);

        menuItem(6);assert(engine.currentId==SceneID::SETTINGS);capturePage("settings");
        auto& settings=static_cast<SettingsScene*>(engine.currentScene.get())->ui;
        pageTap(200,26);assert(engine.gameState().settings.brightness>100);
        const auto brightness=engine.gameState().settings.brightness;
        pageEvent(Type::DOWN,200,26);event(Type::MOVE,20,20);pageEvent(Type::UP,140,26);
        assert(engine.gameState().settings.brightness==brightness);
        pageEvent(Type::DOWN,150,76);pageEvent(Type::MOVE,210,76);pageEvent(Type::UP,210,76);
        assert(engine.gameState().settings.volume>70);
        // The device owner keeps this setting muted; preserve it across processes.
        pageTap(120,76);
        assert(engine.gameState().settings.volume==0 && Platform::audio().volume()==0);
        swipe(70,210,70);targetTap(settings,Ui::SettingsUi::HELP);
        assert(settings.viewMode==Ui::SettingsUi::ViewMode::HELP);
        for(int i=0;i<6;++i) {capturePage(("help-"+std::to_string(i)).c_str());if(i<5)pageTap(180,217);}
        back();assert(settings.viewMode==Ui::SettingsUi::ViewMode::MENU);
        swipe(70,210,70);targetTap(settings,Ui::SettingsUi::RESET_GAME);capturePage("settings-reset");
        pageTap(60,217);assert(engine.gameState().oobeDone);back();
        assert(engine.currentId==SceneID::MENU);
        capturePage("menu-bottom");
        blobs.failWrites=true;assert(!session.save());
        assert(!session.lastSaveOk() && !Ui::RoundRoom::lastSaveOk);
        assert(!session.restart() && power.restarts==0);capturePage("menu-save-failed");
        blobs.failWrites=false;
        assert(session.save());
        swipe(120,190,70);swipe(120,190,70);
        capturePage("menu-bottom");
        assert(!menu().mainMenuUi.onTap(8) && !menu().mainMenuUi.onTap(9));
        const auto backRect=Ui::MainMenuUi::itemBounds(7,menu().mainMenuUi.scroll);
        const int backX=backRect.x+backRect.width/2,backY=backRect.y+backRect.height/2;
        event(Type::DOWN,backX,backY);event(Type::CANCEL,backX,backY);event(Type::UP,backX,backY);
        assert(engine.currentId==SceneID::MENU && power.restarts==0);
        menuItem(7);assert(engine.currentId==SceneID::MAIN && power.restarts==0);
        assert(session.restart() && session.lastSaveOk() && power.restarts==1);
        assert(verifier.load(persisted, persistedView));
        uint8_t encoded[SaveCodec::MAX_ENCODED_BYTES];
        size_t written = 0;
        assert(SaveCodec::encode(persisted, persistedView, 1, encoded, sizeof(encoded), written));
        writeFile(output / "expected.snapshot", encoded, written);
        std::printf("PASS touch release/drag/cancel/full bowl, AI feeding, automatic save, restart guard; satiety=%u->%u\n",
            satiety, persisted.team[0].satiety);
        std::puts("PASS full native menu journeys: team/status/moves, room/food/shower, bag/shop, contacts/social, explore/battle, settings/help/reset; persistence and static parking");
    }
    writeFile(output / (mode + ".log"), desktop.logs().data(), desktop.logs().size());
    return 0;
}
