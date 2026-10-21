#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "core/SaveManager.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/FontResource.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"
#include "presentation/PixelRenderer.h"
#define private public
#include "scenes/ExploreScene.h"
#include "scenes/ShowerScene.h"
#include "scenes/MainScene.h"
#include "scenes/HatchScene.h"
#include "scenes/MenuScene.h"
#include "scenes/SettingsScene.h"
#include "scenes/ShopScene.h"
#undef private
#include "PowerButton.h"
#include "TouchCalibration.h"
#include "presentation/TouchUi.h"
#include "presentation/MenuGrid.h"
#include "core/TouchScroll.h"
#include "../firmware/ostb_3st/main/OstbBattery.h"

class TestTouchInput final : public Platform::IInputDevice {
public:
    explicit TestTouchInput(DesktopPlatform& input) : physical(input) {}
    void update() override {}
    bool pressed(Platform::InputButton b) const override { return physical.pressed(b); }
    bool touchAvailable() const override { return available; }
    bool available = true;
    bool pollTouch(TouchEvent& e) override {
        if (events.empty()) return false;
        e = events.front(); events.pop_front(); return true;
    }
    DesktopPlatform& physical;
    std::deque<TouchEvent> events;
};

void touchMathTests() {
    using Cal = Stickmon::TouchCalibration;
    for (bool swap : {false,true}) for (bool mirror : {false,true}) {
        Cal c;
        for (int i = 0; i < 4; ++i) {
            const int x = Cal::TARGETS[i][0], y = Cal::TARGETS[i][1];
            c.raw[i][0] = swap ? 2*y+11 : 2*x+11;
            c.raw[i][1] = swap ? (mirror ? 700-2*x : 2*x+17) : (mirror ? 700-2*y : 2*y+17);
        }
        assert(c.fit());
        const int rx = swap ? 251 : 307;
        const int ry = swap ? (mirror ? 404 : 313) : (mirror ? 460 : 257);
        assert(c.error(rx,ry,4) < 0.01);
        uint8_t bytes[17]; c.encode(bytes); Cal decoded;
        assert(decoded.decode(bytes)); assert(decoded.error(rx,ry,4) < 0.01);
    }
    Cal bad; assert(!bad.fit());
    for (int i = 0; i < 4; ++i) { bad.raw[i][0] = i*10; bad.raw[i][1] = i*20; }
    assert(!bad.fit());
    using Setup = Stickmon::TouchCalibrationSetup;
    using Result = Setup::Result;
    Setup setup;
    setup.beginContact();
    for (int i = 0; i < 8; ++i) setup.sample(i == 0 ? 200 : 100, i == 0 ? 300 : 120);
    assert(setup.coordinate(0) == 100 && setup.coordinate(1) == 120);
    setup.beginContact(); setup.sample(33, 37);
    assert(setup.coordinate(0) == 33 && setup.coordinate(1) == 37);
    // Reproduce the device's rejected center without silently finishing setup.
    Cal deviceCal;
    const uint16_t deviceRaw[4][2] = {{33,37},{282,51},{260,232},{31,231}};
    for (int i = 0; i < 4; ++i)
        assert(setup.accept(deviceCal, deviceRaw[i][0], deviceRaw[i][1]) == Result::NEXT);
    assert(setup.target() == 4);
    assert(setup.accept(deviceCal, 160, 120) == Result::RETRY_CENTER);
    assert(setup.target() == 4);
    assert(setup.accept(deviceCal, 151, 141) == Result::COMPLETE);
    uint8_t persisted[17]; deviceCal.encode(persisted);
    Cal reloaded; assert(reloaded.decode(persisted));
    assert(reloaded.error(151, 141, 4) < 12);
    Setup retry;
    for (int i = 0; i < 4; ++i) retry.accept(deviceCal, deviceRaw[i][0], deviceRaw[i][1]);
    assert(retry.accept(deviceCal, 160, 120) == Result::RETRY_CENTER);
    assert(retry.accept(deviceCal, 160, 120) == Result::RETRY_CENTER);
    assert(retry.accept(deviceCal, 160, 120) == Result::RESTART && retry.target() == 0);
    TouchTap tap;
    using Type = Platform::IInputDevice::TouchType;
    assert(tap.update({Type::DOWN,100,100,0xffffff00U},3) == -1);
    assert(tap.update({Type::UP,102,100,0x10U},3) == 3);
    tap.update({Type::DOWN,100,100,1000},3);
    tap.update({Type::MOVE,115,100,1050},3);
    assert(tap.update({Type::UP,100,100,1100},3) == -1);
    tap.update({Type::DOWN,100,100,1200},3);
    assert(tap.update({Type::UP,100,100,1300},4) == -1);
    tap.update({Type::DOWN,100,100,1400},3);
    tap.update({Type::CANCEL,100,100,1450},3);
    assert(tap.update({Type::UP,100,100,1500},3) == -1);
}

void gridTests() {
    const auto compact = MenuGrid::mainLayout(296, 240);
    assert(compact.columns == 3 && compact.cellWidth == 88);
    assert(compact.maxScroll(8) == 0);
    for (int i = 0; i < 8; ++i) {
        const auto cell = compact.cell(i);
        assert(cell.x >= 0 && cell.y >= 0 && cell.x + cell.width <= 296 && cell.y + cell.height <= 240);
        assert(compact.itemAt(cell.x + cell.width / 2, cell.y + cell.height / 2, 0, 8) == i);
        assert(compact.itemAt(cell.x + cell.width, cell.y + 12, 0, 8) == -1);
    }
    assert(compact.itemAt(244, 198, 0, 8) == -1);
    assert(MenuGrid::mainLayout(180, 240).columns == 1);
    assert(MenuGrid::mainLayout(368, 240).columns == 4);
    const auto amoled = MenuGrid::mainLayout(368, 448, 2);
    assert(amoled.columns == 2 && amoled.cellWidth == 164);
    assert(amoled.maxScroll(8) == 208 && amoled.maxScroll(9) == 372);
    assert(amoled.cell(1).x == 188);
    using Type = Platform::IInputDevice::TouchType;
    TouchScroll pointer;
    float scroll = 0;
    assert(pointer.update({Type::DOWN,40,100,100}, 3, scroll, 200, true).changed);
    assert(pointer.pressed() == 3);
    auto release = pointer.update({Type::UP,40,100,150}, 3, scroll, 200, true);
    assert(release.tap == 3 && pointer.pressed() == -1);
    pointer.update({Type::DOWN,40,100,200}, 3, scroll, 200, true);
    pointer.update({Type::MOVE,40,50,230}, 3, scroll, 200, true);
    assert(scroll == 50 && pointer.pressed() == -1);
    assert(pointer.update({Type::UP,40,50,260}, 3, scroll, 200, true).tap == -1);
    pointer.update({Type::DOWN,40,100,300}, 3, scroll, 200, true);
    pointer.cancel();
    assert(pointer.update({Type::UP,40,100,340}, 3, scroll, 200, true).tap == -1);
    pointer.update({Type::DOWN,40,100,400}, 3, scroll, 200, false);
    pointer.update({Type::MOVE,60,100,430}, 4, scroll, 200, false);
    assert(pointer.update({Type::UP,40,100,460}, 3, scroll, 200, false).tap == -1);
}

class TestDisplay final : public Platform::IDisplayDevice {
public:
    static constexpr int W = Platform::LOGICAL_DISPLAY_W, H = Platform::LOGICAL_DISPLAY_H;
    std::vector<uint16_t> pixels = std::vector<uint16_t>(W * H + 32, 0xa55a);
    bool isLocked = false;
    unsigned frames = 0;
    uint8_t light = 128;
    Platform::FrameBuffer565 frameBuffer() override { return {pixels.data() + 16, W, H, false}; }
    void present() override {
        ++frames;
        assert(std::all_of(pixels.begin(), pixels.begin() + 16, [](auto x) { return x == 0xa55a; }));
        assert(std::all_of(pixels.end() - 16, pixels.end(), [](auto x) { return x == 0xa55a; }));
    }
    void setBrightness(uint8_t v) override { light = v; }
    uint8_t brightness() const override { return light; }
    void sleep() override { light = 0; }
    bool locked() const override { return isLocked; }
    void capture(const std::string& path) {
        present();
        FILE* file = std::fopen(path.c_str(), "wb"); assert(file);
        std::fprintf(file, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; ++i) {
            const uint16_t c = pixels[i + 16];
            const uint8_t rgb[] = {uint8_t((c >> 11) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, file);
        }
        std::fclose(file);
    }
};
class NoMotion final : public Platform::IImuDevice {
    bool readAcceleration(float& x, float& y, float& z) override { x=y=z=0; return false; }
};
class TestPower final : public Platform::IPowerDevice {
public:
    int percent = -1;
    int batteryLevel() override { return percent; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 1234; }
    size_t externalMemorySize() const override { return 8*1024*1024; }
    size_t externalMemoryFree() const override { return 7*1024*1024; }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
};
void powerTests() {
    using Button = Stickmon::PowerButton;
    using A = Button::Action;
    Button b; b.begin(false, 0);
    assert(b.update(true,10)==A::NONE); assert(b.update(false,20)==A::NONE);
    assert(b.update(true,30)==A::NONE); assert(b.update(true,60)==A::PRESSED);
    assert(b.down());
    assert(b.update(true,100)==A::NONE);
    assert(b.update(false,150)==A::NONE); assert(b.update(false,180)==A::TOGGLE_LOCK);
    assert(!b.down());
    assert(b.update(true,200)==A::NONE); assert(b.update(true,230)==A::PRESSED);
    // A hardware controller may remove supply before the release edge.
    // Persistence was already requested at 230ms, not after the hold timeout.
    assert(b.update(true,2200)==A::NONE);
    assert(b.down());
    assert(b.update(false,2230)==A::NONE); assert(b.update(false,2260)==A::SHUTDOWN);
    assert(b.update(false,2400)==A::NONE);
    b.begin(true,2500); assert(b.update(false,5000)==A::NONE); assert(b.update(false,5030)==A::NONE);
    b.begin(false,0xffffff00U); b.update(true,0xffffff10U); b.update(true,0xffffff30U);
    b.update(false,0x800U); assert(b.update(false,0x820U)==A::SHUTDOWN);
}
void roomShadowTests(TestDisplay& display, const std::string& output) {
    MainScene room;
    room.onEnter();
    room.cameraX = 0;
    room.cameraY = 0;
    room.mainActor.x = 120.25f;
    room.mainActor.y = 92.5f;
    room.visitor.active = true;
    room.visitor.x = room.mainActor.x;
    room.visitor.y = room.mainActor.y;
    room.visitor.task = Home::Task::WALK;
    room.pmdAction = MainScene::PmdAction::WALKING;
    // Compare actual shadow pixels with registration in the sprite canvas.
    // Match Stick sprite registration, translated by the centered room origin.
    auto check = [&](const PokemonSprites::SpriteFrame* frame, bool flip,
                     int offsetX, int offsetY) {
        assert(frame && frame->shadowRadiusX && frame->shadowRadiusY);
        const int expectedX = 28 + int(std::lround(room.mainActor.x)) +
            offsetX - frame->width / 2 +
            (flip ? frame->width - 1 - frame->shadowX : frame->shadowX);
        const int expectedY = 39 + int(std::lround(room.mainActor.y)) +
            offsetY - frame->height / 2 + frame->shadowY;
        int left = TestDisplay::W, right = -1, top = TestDisplay::H, bottom = -1;
        for (int y = 0; y < TestDisplay::H; ++y) {
            for (int x = 0; x < TestDisplay::W; ++x) {
                if (display.pixels[16 + y * TestDisplay::W + x] == 0xffff) continue;
                left = std::min(left, x); right = std::max(right, x);
                top = std::min(top, y); bottom = std::max(bottom, y);
            }
        }
        assert(right > left && bottom > top);
        assert(left + right == 2 * expectedX);
        assert(top + bottom == 2 * expectedY);
    };
    for (uint16_t species : {1, 4, 92, 147}) {
        assert(PokemonSprites::syncTeamCache(&species, 1));
        room.active = findSpecies(species);
        room.visitor.speciesId = species;
        for (bool flip : {false, true}) {
            room.pmdDirection = flip ? MainScene::PmdDirection::RIGHT : MainScene::PmdDirection::LEFT;
            room.visitor.direction = flip ? PokemonSprites::WalkDirection::RIGHT : PokemonSprites::WalkDirection::LEFT;
            for (int poseY : {0, 6}) {
                room.pmdRenderOffsetX = room.visitor.renderOffsetX = 3;
                room.pmdRenderOffsetY = room.visitor.renderOffsetY = poseY;
                PixelRenderer::clear(0xffff);
                room.drawShadow();
                check(room.currentMonsterFrame(), room.pmdDirectionFlipX(), 3, poseY);
                if (!flip && poseY == 0) {
                    room.drawMonster();
                    display.capture(output + "/shadow-" + std::to_string(species) + ".ppm");
                }
                PixelRenderer::clear(0xffff);
                room.drawVisitorShadow();
                bool visitorFlip = false;
                const auto* frame = room.visitorCurrentFrame(visitorFlip);
                check(frame, visitorFlip, 3, poseY);
                // Dropping/hopping bodies must leave their shadow on the floor.
                const auto grounded = display.pixels;
                room.visitor.dropOffsetY = 20;
                PixelRenderer::clear(0xffff);
                room.drawVisitorShadow();
                assert(display.pixels == grounded);
                room.visitor.dropOffsetY = 0;
            }
        }
    }
}
void roomViewportTests(TestDisplay& display, const std::string& output) {
    const auto& resource = RoomResource::ins();
    assert(RoomRenderer::PIXEL_SCALE == 1);
    assert(resource.width() == 240 && resource.height() == 161);
    assert(resource.artWidth() == 480 && resource.artHeight() == 322);
    const auto view = RoomRenderer::viewport();
    assert(view.x == 28 && view.y == 39 && view.width == 240 && view.height == 161);
    assert(RoomRenderer::clampCameraX(-100) == 0);
    assert(RoomRenderer::clampCameraX(1000) == 0);
    assert(RoomRenderer::clampCameraY(-100) == 0);
    assert(RoomRenderer::clampCameraY(1000) == 0);
    assert(RoomRenderer::screenX(0) == 28);
    assert(RoomRenderer::screenX(240) == 268);
    assert(RoomRenderer::screenY(0, 0) == 39);
    assert(RoomRenderer::screenY(161, 0) == 200);
    // Compare every centered pixel to the same complete room sampled at the
    // Stick 1x scale, and assert that none of the margin pixels repeat edges.
    PixelRenderer::clear(0xf81f);
    assert(RoomRenderer::drawViewport(0, 0, 240, 161, 0, 0, false, 1));
    const auto reference = display.pixels;
    assert(RoomRenderer::draw(0, false, 0));
    const auto centered = display.pixels;
    for (int y = 0; y < TestDisplay::H; ++y) {
        for (int x = 0; x < TestDisplay::W; ++x) {
            const bool inRoom = x >= 28 && x < 268 && y >= 39 && y < 200;
            const uint16_t expected = inRoom
                ? reference[16 + (y - 39) * TestDisplay::W + x - 28] : 0;
            assert(display.pixels[16 + y * TestDisplay::W + x] == expected);
        }
    }
    assert(RoomRenderer::draw(1000, false, 1000));
    assert(display.pixels == centered);
    assert(RoomRenderer::draw(-1000, false, -1000));
    assert(display.pixels == centered);
    assert(RoomRenderer::screenX(100, 46.6f) == 128);
    assert(RoomRenderer::screenY(100, 20.6f) == 139);

    uint16_t species = 1;
    assert(PokemonSprites::syncTeamCache(&species, 1));
    MainScene scene; scene.onEnter();
    scene.active = findSpecies(species);
    scene.pmdAction = MainScene::PmdAction::IDLE;
    scene.pmdDirection = MainScene::PmdDirection::FRONT;
    scene.pmdFrame = 0;
    scene.pmdRenderOffsetX = scene.pmdRenderOffsetY = 0;
    scene.visitor.active = false;
    scene.doorMainHidden = false;
    scene.pairInteraction = MainScene::PairInteraction::NONE;
    scene.roomAction = MainScene::RoomAction::NONE;
    const float positions[][2] = {{20, 35}, {120, 80}, {220, 140}};
    for (const auto& position : positions) {
        scene.mainActor.x = position[0]; scene.mainActor.y = position[1];
        scene.updateCamera();
        assert(scene.cameraX == 0 && scene.cameraY == 0);
        assert(scene.mainActor.x == position[0] && scene.mainActor.y == position[1]);
    }
    // Capture valid walking positions at the left, center and right of the
    // room, separately from the out-of-bounds camera clamp probes above.
    int index = 0;
    for (float desiredX : {40.0f, 120.0f, 200.0f}) {
        float best = 1e9f;
        for (int y = 0; y < resource.height(); ++y) {
            for (int x = 0; x < resource.width(); ++x) {
                if (!scene.monsterFootprintInsideWalkArea(x, y)) continue;
                const float distance = (x - desiredX) * (x - desiredX) +
                    (y - 100) * (y - 100);
                if (distance < best) {
                    best = distance; scene.mainActor.x = x; scene.mainActor.y = y;
                }
            }
        }
        assert(best < 1e9f);
        scene.updateCamera();
        scene.drawBackground(); scene.drawShadow(); scene.drawMonster(); scene.drawHud();
        display.capture(output + "/room-camera-" + std::to_string(index++) + ".ppm");
    }
    scene.mainActor.x = 120; scene.mainActor.y = 85; scene.updateCamera();
    // Real main/visitor bodies must keep the exact 1x Stick dimensions and
    // frame registration, including animation frames with different padding.
    for (bool visitor : {false, true}) {
        for (uint8_t pose : {uint8_t(0), uint8_t(1)}) {
            scene.pmdFrame = pose;
            scene.visitor.active = true; scene.visitor.speciesId = species;
            scene.visitor.x = scene.mainActor.x; scene.visitor.y = scene.mainActor.y;
            scene.visitor.task = Home::Task::IDLE;
            scene.visitor.direction = PokemonSprites::WalkDirection::DOWN;
            scene.visitor.frameIndex = pose;
            bool flip = false;
            const auto* frame = visitor ? scene.visitorCurrentFrame(flip) : scene.currentMonsterFrame();
            assert(frame);
            const auto bounds = PokemonSprites::frameVisibleBounds(frame);
            PixelRenderer::clear(0xffff);
            if (visitor) scene.drawVisitor(); else scene.drawMonster();
            int left = TestDisplay::W, right = -1, top = TestDisplay::H, bottom = -1;
            for (int y = 0; y < TestDisplay::H; ++y)
                for (int x = 0; x < TestDisplay::W; ++x)
                    if (display.pixels[16 + y * TestDisplay::W + x] != 0xffff) {
                        left = std::min(left, x); right = std::max(right, x);
                        top = std::min(top, y); bottom = std::max(bottom, y);
                    }
            assert(right - left + 1 == bounds.width);
            assert(bottom - top + 1 == bounds.height);
            assert(left == 28 + int(std::lround(scene.mainActor.x)) -
                frame->width / 2 + bounds.left);
            assert(top == 39 + int(std::lround(scene.mainActor.y)) -
                frame->height / 2 + bounds.top);
            display.present();
        }
    }
    // One or two status rows and the clock must use only the top margin.
    auto& gameState = GameEngine::ins().gameState();
    const auto savedState = gameState;
    for (uint8_t teamCount : {uint8_t(1), uint8_t(2)}) {
        gameState.teamCount = teamCount;
        gameState.team[1] = Game::MonsterFactory::create(1, 5);
        scene.visitor.active = teamCount == 2;
        scene.drawBackground();
        const auto beforeHud = display.pixels;
        scene.drawHud();
        for (int y = 39; y < TestDisplay::H; ++y)
            for (int x = 0; x < TestDisplay::W; ++x)
                assert(display.pixels[16 + y * TestDisplay::W + x] ==
                       beforeHud[16 + y * TestDisplay::W + x]);
        display.capture(output + "/room-hud-" + std::to_string(teamCount) + ".ppm");
    }
    auto& power = static_cast<TestPower&>(Platform::power());
    for (int percent : {65, 10, -1, 100}) {
        power.percent = percent;
        scene.drawBackground(); scene.drawHud();
        const auto at = [&](int x, int y) { return display.pixels[16 + y*296 + x]; };
        const auto green = PixelRenderer::rgb(92, 213, 139);
        const auto red = PixelRenderer::rgb(238, 91, 91);
        const auto unknown = PixelRenderer::rgb(142, 157, 155);
        assert((at(231,20) == green) == (percent >= 20));
        assert((at(231,20) == red) == (percent == 10));
        assert((at(239,22) == unknown) == (percent < 0));
        display.capture(output + "/room-battery-" + std::to_string(percent) + ".ppm");
    }
    power.percent = -1;
    gameState = savedState;
    scene.visitor.active = false;
    const auto savedBowl = GameEngine::ins().gameState().room;
    auto& bowl = GameEngine::ins().gameState().room;
    bowl.bowlFood = 0; bowl.bowlCount = 1; bowl.bowlBitesRemaining = 3;
    scene.mainActor.x = resource.foodX() + 12;
    scene.mainActor.y = resource.foodY() + 4 - scene.mainGeometry().groundOffsetY;
    scene.updateCamera();
    scene.drawBackground(); scene.drawFood(); scene.drawShadow(); scene.drawMonster();
    display.capture(output + "/room-food.ppm");
    bowl = savedBowl;
    scene.mainActor.x = resource.bedX();
    scene.mainActor.y = resource.bedY() - scene.mainGeometry().groundOffsetY;
    scene.pmdAction = MainScene::PmdAction::SLEEPING; scene.pmdFrame = 0;
    scene.updateCamera(); scene.render();
    display.capture(output + "/room-sleep.ppm");
    scene.pmdAction = MainScene::PmdAction::IDLE;
    scene.mainActor.x = 95; scene.mainActor.y = 90;
    scene.visitor.active = true; scene.visitor.x = 155; scene.visitor.y = 90;
    scene.pairInteraction = MainScene::PairInteraction::TALK;
    scene.updateCamera();
    assert(scene.cameraX == 0 && scene.cameraY == 0);
    scene.drawBackground(); scene.drawShadow(); scene.drawVisitorShadow();
    scene.drawMonster(); scene.drawVisitor(); scene.drawHud();
    display.capture(output + "/room-pair.ppm");
    scene.pairInteraction = MainScene::PairInteraction::NONE;
    scene.mainActor.x = -2; scene.mainActor.y = 110;
    scene.visitor.x = 242; scene.visitor.y = 110;
    scene.render();
    for (int y = 39; y < 200; ++y)
        for (int x = 0; x < TestDisplay::W; ++x)
            if (x < 28 || x >= 268)
                assert(display.pixels[16 + y * TestDisplay::W + x] == 0);
    display.present();

}

void touchSceneTests(TestDisplay& display, const std::string& output) {
    using Type = Platform::IInputDevice::TouchType;
    auto tap = [](Scene& scene, int x, int y) {
        scene.onTouch({Type::DOWN,int16_t(x),int16_t(y),100});
        return scene.onTouch({Type::UP,int16_t(x),int16_t(y),160});
    };
    auto parked = [](Scene& scene) {
        const auto result = scene.update(Hal::ins().millis(), 0.066f);
        assert(!result.redraw && result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    };
    auto& game = GameEngine::ins();
    auto& state = game.gameState();
    MainScene room; room.onEnter();
    state.team[0].petCountToday = 0;
    state.team[0].fainted = false; state.team[0].hpCur = state.team[0].hpMax;
    int petX = -1, petY = -1;
    for (int y = 39; y < 200 && petX < 0; ++y) for (int x = 28; x < 268; ++x) {
        if (room.touchTarget(x,y) == 0) { petX=x; petY=y; break; }
    }
    assert(petX >= 0);
    room.onTouch({Type::DOWN,int16_t(petX),int16_t(petY),100});
    assert(state.team[0].petCountToday == 0);
    room.onTouch({Type::MOVE,int16_t(petX+20),int16_t(petY),130});
    assert(!room.onTouch({Type::UP,int16_t(petX),int16_t(petY),160}));
    assert(state.team[0].petCountToday == 0);
    assert(tap(room,petX,petY)); assert(state.team[0].petCountToday == 1);
    const int count = state.room.bowlCount;
    int bowlX=-1, bowlY=-1;
    for (int y=39;y<200 && bowlX<0;++y) for(int x=28;x<268;++x) if(room.touchTarget(x,y)==1) {bowlX=x;bowlY=y;break;}
    assert(room.touchTarget(bowlX,bowlY) == 1);
    assert(tap(room,bowlX,bowlY)); assert(state.room.bowlCount >= count);
    assert(room.touchTarget(8,100) == -1);
    assert(room.touchTarget(287,100) == -1);
    room.render(); display.capture(output + "/touch-home.ppm");

    MenuScene menu; menu.onEnter(); menu.cursor=1; menu.animCursor=1;
    assert(!menu.touchNavigation());
    parked(menu);
    menu.render(); display.capture(output + "/touch-menu.ppm");
    assert(menu.onTouch({Type::DOWN,148,42,100}));
    assert(menu.viewMode == MenuScene::ViewMode::MENU && menu.gridTouch.pressed() == 1);
    menu.render(); display.capture(output + "/touch-menu-pressed.ppm");
    menu.cancelTouch();
    assert(!menu.onTouch({Type::UP,148,42,160}));
    assert(tap(menu,148,42));
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(!menu.touchNavigation()); parked(menu);
    menu.render(); display.capture(output + "/touch-team.ppm");
    assert(menu.touchTarget(80,144) == -1);
    assert(menu.onTouch({Type::DOWN,50,85,100}));
    assert(!menu.teamActionOpen);
    menu.render(); display.capture(output + "/touch-team-pressed.ppm");
    menu.cancelTouch();
    assert(!menu.onTouch({Type::UP,50,85,160}));
    assert(!menu.teamActionOpen);
    menu.onTouch({Type::DOWN,50,85,100});
    menu.onTouch({Type::MOVE,75,85,130});
    menu.onTouch({Type::UP,50,85,160});
    assert(!menu.teamActionOpen);
    assert(tap(menu,50,85)); assert(menu.teamActionOpen);
    assert(menu.teamActionCount() == 2);
    assert(menu.teamActionAt(0) == MenuScene::TeamAction::STATUS);
    assert(menu.teamActionAt(1) == MenuScene::TeamAction::MOVES);
    menu.render(); display.capture(output + "/touch-team-actions.ppm"); parked(menu);
    // An outside release dismisses the popup without activating the row.
    assert(tap(menu,50,85)); assert(!menu.teamActionOpen);
    assert(tap(menu,50,85));
    menu.onTouch({Type::DOWN,170,112,100}); menu.cancelTouch();
    assert(!menu.onTouch({Type::UP,170,112,160}));
    assert(menu.teamActionOpen && menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(tap(menu,170,112)); assert(menu.viewMode == MenuScene::ViewMode::STATUS);
    assert(!menu.touchNavigation());
    menu.render(); display.capture(output + "/touch-team-status.ppm");
    assert(tap(menu,48,24)); assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(tap(menu,50,85)); assert(tap(menu,170,152));
    assert(menu.viewMode == MenuScene::ViewMode::MOVES && !menu.touchNavigation());
    menu.render(); display.capture(output + "/touch-team-moves.ppm");
    assert(tap(menu,48,24)); assert(menu.viewMode == MenuScene::ViewMode::TEAM);

    auto safePage = [&]() {
        menu.render(); display.present();
        const auto bounds = TouchUi::safeHeaderBounds();
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 296; ++x) {
            if (y < bounds.y + bounds.height && !bounds.contains(x,y)) {
                assert(display.pixels[16 + y*296 + x] == 0);
                assert(menu.touchTarget(x,y) == -1);
            }
        }
        if (menu.viewMode != MenuScene::ViewMode::MOVES) parked(menu);
    };
    auto originalBody = [&]() {
        auto& touchInput = static_cast<TestTouchInput&>(Platform::input());
        touchInput.available = false;
        menu.render();
        const auto original = display.pixels;
        touchInput.available = true;
        menu.render();
        // Original text, stat order, sprite size, list, divider and bars remain
        // pixel-identical across the full width; only the header translates y.
        // Legacy page dots remain at the side; touch dots overlay the bottom.
        for (int y = 0; y < 129; ++y) for (int x = 0; x < 296; ++x) {
            if (menu.viewMode == MenuScene::ViewMode::STATUS && x >= 282) continue;
            assert(display.pixels[16 + (y+48)*296 + x] == original[16 + y*296 + x]);
        }
    };
    auto swipeStatus = [&](bool next) {
        menu.onTouch({Type::DOWN,int16_t(next ? 210 : 80),140,200});
        menu.onTouch({Type::MOVE,int16_t(next ? 80 : 210),140,230});
        return menu.onTouch({Type::UP,int16_t(next ? 80 : 210),140,260});
    };
    safePage();
    // Both former side margins render and activate the same full-width card.
    assert(display.pixels[16 + 90*296 + 12] != 0);
    assert(display.pixels[16 + 90*296 + 284] != 0);
    assert(menu.touchTarget(12,90) == menu.touchTarget(148,90));
    assert(menu.touchTarget(284,90) == menu.touchTarget(148,90));
    assert(tap(menu,12,90) && menu.teamActionOpen);
    assert(tap(menu,12,235) && !menu.teamActionOpen);
    assert(tap(menu,284,90) && menu.teamActionOpen);
    assert(tap(menu,12,235) && !menu.teamActionOpen);
    assert(tap(menu,50,85)); safePage();
    assert(tap(menu,170,112));
    // Original status pages retain their composition and use no bottom bar.
    for (int page = 0; page < 5; ++page) {
        assert(menu.statusPage == page); safePage();
        assert(menu.touchTarget(210,204) == -1);
        for (int dot = 0; dot < 5; ++dot) {
            const auto color = dot == page ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(92, 98, 110);
            assert(display.pixels[16 + 222*296 + 130 + dot*8] == color);
        }
        assert(menu.touchTarget(130,222) == -1);
        if (page == 1 || page == 3 || page == 4) originalBody();
        display.capture(output + "/touch-status-" + std::to_string(page) + ".ppm");
        const int height = menu.touchStatusContentHeight();
        menu.onTouch({Type::DOWN,148,150,200});
        menu.onTouch({Type::MOVE,148,-1000,230});
        menu.onTouch({Type::UP,148,-1000,260});
        assert(menu.statusScroll == std::max(0,height-176));
        safePage();
        display.capture(output + "/touch-status-" + std::to_string(page) + "-bottom.ppm");
        assert(swipeStatus(true)); assert(menu.statusScroll == 0);
    }
    assert(menu.statusPage == 0);
    // A swipe may start in the former left margin.
    menu.onTouch({Type::DOWN,4,140,200});
    menu.onTouch({Type::MOVE,90,140,230});
    assert(menu.onTouch({Type::UP,90,140,260}) && menu.statusPage == 4);
    assert(swipeStatus(true) && menu.statusPage == 0);
    assert(swipeStatus(false)); assert(menu.statusPage == 4);
    assert(tap(menu,230,8)); assert(menu.statusPage == 0); // Header title also cycles pages.
    menu.onTouch({Type::DOWN,210,140,200}); menu.cancelTouch();
    assert(!menu.onTouch({Type::UP,80,140,260}) && menu.statusPage == 0);
    assert(tap(menu,48,24)); assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    assert(tap(menu,50,85)); assert(tap(menu,170,152)); safePage();
    const auto moveState = state;
    state.team[0] = Game::MonsterFactory::create(1,50);
    menu.moveCursor = 0; menu.moveListScroll = 0; menu.descScroll = 0;
    safePage(); originalBody();
    display.capture(output + "/touch-moves-original.ppm");
    assert(menu.touchMoveInfo(0));
    const auto moveLayout = MenuGrid::fit({0,54,90,164},90,24,0,0,0);
    for (int i = 0; i < menu.touchMoveRowCount(); ++i) {
        assert(menu.onButton({1,BtnAction::PRESSED}));
        const auto selected = moveLayout.cell(menu.moveCursor, menu.moveListScroll);
        assert(selected.y >= 54 && selected.y + selected.height <= 218);
        safePage();
    }
    assert(menu.moveCursor == 0);
    // The old bottom control areas cannot activate hidden Select/Confirm.
    assert(menu.touchTarget(210,204) == -1 && menu.touchTarget(80,204) == -1);
    assert(!tap(menu,210,204) && !menu.moveForgetConfirmOpen);
    assert(!tap(menu,80,204) && menu.moveCursor == 0);
    menu.onTouch({Type::DOWN,80,90,200});
    menu.onTouch({Type::MOVE,110,90,230});
    menu.onTouch({Type::UP,80,90,260});
    assert(menu.moveCursor == 0 && !menu.moveForgetConfirmOpen);
    assert(tap(menu,80,66)); assert(!menu.moveForgetConfirmOpen); // Basic move locked.
    menu.toast = nullptr;
    assert(tap(menu,4,90)); assert(menu.moveCursor == 1 && !menu.moveForgetConfirmOpen);
    menu.render(); display.capture(output + "/touch-moves-selected.ppm");
    assert(tap(menu,80,90)); assert(menu.moveForgetConfirmOpen && !menu.moveForgetConfirmYes);
    safePage(); display.capture(output + "/touch-move-forget.ppm");
    assert(menu.touchTarget(210,204) == -1);
    assert(!tap(menu,80,66)); assert(menu.moveForgetConfirmOpen);
    assert(tap(menu,195,148)); assert(!menu.moveForgetConfirmOpen); // No.
    assert(tap(menu,80,90)); assert(menu.moveForgetConfirmOpen);
    assert(tap(menu,48,24)); assert(!menu.moveForgetConfirmOpen && menu.viewMode == MenuScene::ViewMode::MOVES);
    assert(tap(menu,80,90));
    const auto forgotten = menu.touchMoveInfo(menu.moveCursor)->id;
    assert(tap(menu,125,148)); assert(!menu.moveForgetConfirmOpen);
    assert(menu.touchMoveInfo(menu.moveCursor) == nullptr || menu.touchMoveInfo(menu.moveCursor)->id != forgotten);
    menu.toast = nullptr;
    menu.recallMoveIds[0] = forgotten; menu.recallMoveCount = 1;
    menu.movePageMode = MenuScene::MovePageMode::RECALL_SELECT;
    menu.moveCursor = 0; menu.moveListScroll = 0; menu.descScroll = 0;
    safePage(); display.capture(output + "/touch-move-recall.ppm");
    assert(tap(menu,80,66));
    assert(menu.movePageMode == MenuScene::MovePageMode::RECALL_REPLACE);
    safePage(); display.capture(output + "/touch-move-replace.ppm");
    assert(tap(menu,48,24)); assert(menu.movePageMode == MenuScene::MovePageMode::RECALL_SELECT);
    assert(tap(menu,80,66));
    assert(game.addItem(Game::ItemId::HEART_SCALE,1));
    const auto scales = game.itemCount(Game::ItemId::HEART_SCALE);
    assert(tap(menu,80,66));
    assert(game.itemCount(Game::ItemId::HEART_SCALE) == scales-1);
    assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    state = moveState;
    menu.movePageMode = MenuScene::MovePageMode::MANAGE;
    menu.toast = nullptr; menu.teamCursor = 0;

    const auto beforeTeam = state;
    state.team[1] = Game::MonsterFactory::create(4, 5);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    state.teamCount = 2;
    menu.toast = nullptr;
    safePage(); display.capture(output + "/touch-team-pair.ppm");
    assert(tap(menu,50,180)); assert(menu.teamCursor == 1 && menu.teamActionOpen);
    assert(menu.teamActionCount() == 4 && menu.teamActionAt(0) == MenuScene::TeamAction::FIRST);
    assert(menu.teamActionAt(3) == MenuScene::TeamAction::LEAVE);
    safePage(); display.capture(output + "/touch-team-partner-actions.ppm");
    assert(tap(menu,170,120));
    assert(menu.viewMode == MenuScene::ViewMode::STATUS && menu.statusMonsterIndex == 1);
    assert(tap(menu,48,24)); assert(menu.teamCursor == 1 && !menu.teamActionOpen);
    assert(tap(menu,50,180)); assert(tap(menu,170,160));
    assert(menu.viewMode == MenuScene::ViewMode::MOVES && menu.moveMonsterIndex == 1);
    assert(tap(menu,48,24)); assert(menu.teamCursor == 1 && !menu.teamActionOpen);
    assert(tap(menu,50,180));
    // Dragging over First cannot change the permanent team order.
    const auto oldFirst = state.team[0].speciesId;
    menu.onTouch({Type::DOWN,170,80,100});
    menu.onTouch({Type::MOVE,200,80,130});
    menu.onTouch({Type::UP,170,80,160});
    assert(state.team[0].speciesId == oldFirst && menu.teamActionOpen);
    assert(tap(menu,170,80));
    assert(!menu.teamActionOpen && menu.teamCursor == 0 && state.team[0].speciesId == 4);
    assert(state.team[1].speciesId == oldFirst && state.activeSlot == 0);
    // Duplicate release cannot trigger an action on a newly exposed card.
    assert(!menu.onTouch({Type::UP,170,80,170}));
    menu.toast = nullptr;
    assert(tap(menu,50,180));
    const auto oldStorage = state.storageCount;
    const auto oldSecond = state.team[1].speciesId;
    assert(tap(menu,170,200));
    assert(!menu.teamActionOpen && state.teamCount == 1);
    assert(state.storageCount == oldStorage + 1 && state.storage[oldStorage].speciesId == oldSecond);
    state = beforeTeam;
    state.teamCount = 2;
    state.team[1] = Game::MonsterFactory::create(4, 5);
    state.team[1].origin = Game::Origin::VISITOR;
    menu.teamCursor = 1;
    assert(menu.teamActionCount() == 2 && menu.teamActionAt(0) == MenuScene::TeamAction::STATUS);
    assert(menu.teamActionAt(1) == MenuScene::TeamAction::MOVES);
    state.team[1].origin = Game::Origin::BEFRIENDED;
    menu.exploreContextMode = true;
    assert(menu.teamActionCount() == 3);
    assert(menu.teamActionAt(3) == MenuScene::TeamAction::BACK);
    menu.exploreContextMode = false;
    state = beforeTeam; menu.teamCursor = 0; menu.toast = nullptr;
    assert(menu.onButton({1,BtnAction::PRESSED}));
    assert(menu.teamCursor == state.teamCount);
    assert(menu.onButton({0,BtnAction::PRESSED}));
    assert(menu.viewMode == MenuScene::ViewMode::MENU);
    assert(tap(menu,148,42));
    assert(menu.onButton({0,BtnAction::PRESSED}) && menu.teamActionOpen);
    assert(menu.onButton({1,BtnAction::PRESSED}) && menu.teamActionCursor == 1);
    assert(menu.onButton({1,BtnAction::LONG_PRESS}) && !menu.teamActionOpen);
    state.oobeDone = false;
    assert(tap(menu,50,85)); assert(menu.viewMode == MenuScene::ViewMode::STATUS);
    safePage(); display.capture(output + "/touch-egg-status.ppm");
    assert(tap(menu,48,24)); assert(menu.viewMode == MenuScene::ViewMode::TEAM);
    state = beforeTeam;
    assert(tap(menu,48,24)); assert(menu.viewMode == MenuScene::ViewMode::MENU);
    menu.openExploreTeamView();
    assert(!menu.touchNavigation()); assert(tap(menu,48,24));
    assert(menu.exploreViewClosed()); menu.exploreContextMode = false;
    menu.viewMode=MenuScene::ViewMode::BAG; menu.bagConfirmOpen=true; menu.bagConfirmYes=true;
    assert(menu.touchTarget(20,15) == -1);
    assert(tap(menu,215,88)); assert(!menu.bagConfirmOpen);
    game.addItem(Game::ItemId::POTION, 3);
    menu.bagDetailOpen = false; menu.bagCursor = 0;
    assert(tap(menu,52,74)); assert(menu.bagDetailOpen && !menu.bagConfirmOpen);
    menu.render(); display.capture(output + "/touch-bag-detail.ppm");
    assert(tap(menu,220,218)); assert(menu.bagConfirmOpen);
    assert(menu.touchTarget(52,74) == -1);
    assert(tap(menu,215,88)); assert(!menu.bagConfirmOpen);
    assert(tap(menu,28,18)); assert(!menu.bagDetailOpen && menu.viewMode == MenuScene::ViewMode::BAG);
    menu.render(); display.capture(output + "/touch-bag.ppm");
    parked(menu);
    const Game::ItemId stocked[] = {Game::ItemId::SUPER_POTION, Game::ItemId::ANTIDOTE,
        Game::ItemId::PARALYZE_HEAL, Game::ItemId::AWAKENING, Game::ItemId::BURN_HEAL,
        Game::ItemId::ICE_HEAL, Game::ItemId::MAX_POTION, Game::ItemId::FULL_RESTORE,
        Game::ItemId::FULL_HEAL, Game::ItemId::REVIVE};
    for (auto item : stocked) assert(game.addItem(item, 1));
    menu.onTouch({Type::DOWN,148,160,200});
    assert(menu.onTouch({Type::MOVE,148,60,230}));
    menu.onTouch({Type::UP,148,60,260});
    assert(menu.bagGridScroll > 0 && !menu.bagDetailOpen && !menu.bagConfirmOpen);
    menu.render(); display.capture(output + "/touch-bag-scrolled.ppm");
    assert(tap(menu,148,60) && menu.bagDetailOpen);
    assert(tap(menu,28,18) && !menu.bagDetailOpen);
    // Physical selection keeps off-screen cards visible.
    for (int i=0; i<9; ++i) assert(menu.onButton({1,BtnAction::PRESSED}));
    const auto bagLayout = MenuGrid::fit({0,38,296,202});
    const auto selected = bagLayout.cell(menu.bagCursor,menu.bagGridScroll);
    assert(selected.y >= 38 && selected.y + selected.height <= 240);
    const auto potions = game.itemCount(Game::ItemId::POTION);
    assert(potions > 1 && game.removeItem(Game::ItemId::POTION,potions-1));
    state.team[0].hpCur = 1;
    menu.bagCursor=0; menu.bagGridScroll=0;
    assert(tap(menu,52,74) && menu.bagDetailOpen);
    assert(game.itemCount(Game::ItemId::POTION)==1);
    assert(tap(menu,220,218) && menu.bagConfirmOpen);
    assert(game.itemCount(Game::ItemId::POTION)==1);
    assert(tap(menu,125,88));
    assert(game.itemCount(Game::ItemId::POTION)==0 && state.team[0].hpCur>1);
    assert(!menu.bagDetailOpen && !menu.bagConfirmOpen);
    menu.resetNavigation(); menu.toast=nullptr;
    assert(tap(menu,244,120)); assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
    assert(!menu.touchNavigation()); parked(menu);
    menu.render(); display.capture(output + "/touch-computer.ppm");
    assert(tap(menu,90,154)); assert(menu.viewMode == MenuScene::ViewMode::STORAGE);
    assert(menu.touchNavigation());
    assert(menu.onButton({1,BtnAction::LONG_PRESS}));
    assert(menu.viewMode == MenuScene::ViewMode::COMPUTER);
    assert(tap(menu,22,19)); assert(menu.viewMode == MenuScene::ViewMode::MENU);
    assert(tap(menu,244,42)); assert(menu.viewMode == MenuScene::ViewMode::ROOM);
    assert(!menu.touchNavigation()); parked(menu);
    menu.render(); display.capture(output + "/touch-room-menu.ppm");
    assert(menu.touchTarget(100,164) == int(MenuScene::ViewMode::ROOM)*256+1);
    assert(tap(menu,90,82)); assert(menu.viewMode == MenuScene::ViewMode::FOOD);
    assert(!menu.touchNavigation()); parked(menu);
    menu.render(); display.capture(output + "/touch-food-menu.ppm");
    const auto selectedFood = game.selectedFoodIndex();
    menu.onTouch({Type::DOWN,148,160,400});
    menu.onTouch({Type::MOVE,148,60,430});
    menu.onTouch({Type::UP,148,60,460});
    assert(menu.foodScroll > 0 && game.selectedFoodIndex() == selectedFood);
    menu.render(); display.capture(output + "/touch-food-scrolled.ppm");
    for (int i=0; i<6; ++i) assert(menu.onButton({1,BtnAction::PRESSED}));
    assert(game.addFoodStock(6,1));
    menu.foodCursor=6;
    const auto foodLayout = TouchUi::listLayout();
    menu.foodScroll=foodLayout.reveal(6,menu.foodScroll,Game::ROOM_FOOD_COUNT);
    const auto foodRect=foodLayout.cell(6,menu.foodScroll);
    assert(foodRect.y>=38 && foodRect.y+foodRect.height<=240);
    const auto foodStock=game.foodCount(6);
    assert(tap(menu,100,foodRect.y+20));
    assert(game.selectedFoodIndex()==6 && game.foodCount(6)==foodStock);
    state.room.food[2]=0; menu.foodScroll=0;
    assert(tap(menu,100,164)); assert(game.selectedFoodIndex()==6);
    assert(tap(menu,22,19)); assert(menu.viewMode==MenuScene::ViewMode::ROOM);
    assert(tap(menu,22,19)); assert(menu.viewMode==MenuScene::ViewMode::MENU);
    menu.viewMode=MenuScene::ViewMode::MENU; menu.cursor=0; menu.animCursor=0;
    assert(menu.touchTarget(148,198)==7 && menu.touchTarget(244,198)==-1);
    menu.render(); display.capture(output + "/touch-menu.ppm");

    SettingsScene settings; settings.onEnter();
    assert(!settings.touchNavigation()); parked(settings);
    settings.render(); display.capture(output + "/touch-settings.ppm");
    assert(tap(settings,272,64)); assert(Platform::display().brightness()==255 && state.settings.brightness==255);
    assert(tap(settings,120,64)); assert(Platform::display().brightness()==32 && state.settings.brightness==32);
    const auto oldVolume=state.settings.volume;
    settings.onTouch({Type::DOWN,180,114,500});
    settings.onTouch({Type::MOVE,180,60,530});
    settings.onTouch({Type::UP,180,60,560});
    assert(settings.touchScroll>0 && state.settings.brightness==32 && state.settings.volume==oldVolume);
    settings.touchScroll=0;
    settings.onTouch({Type::DOWN,180,114,600});
    settings.onTouch({Type::MOVE,400,114,630});
    settings.onTouch({Type::UP,400,114,660});
    assert(state.settings.volume==100);
    settings.onTouch({Type::DOWN,180,114,700});
    settings.onTouch({Type::MOVE,0,114,730});
    settings.onTouch({Type::UP,0,114,760});
    assert(state.settings.volume==0);
    settings.onTouch({Type::DOWN,180,114,800});
    Scene* settingsBase=&settings; settingsBase->cancelTouch();
    settings.onTouch({Type::MOVE,270,114,830});
    settings.onTouch({Type::UP,270,114,860});
    assert(state.settings.volume==0);
    assert(tap(settings,176,114)); assert(state.settings.volume==37);
    settings.onEnter(); assert(state.settings.volume==37); parked(settings);
    assert(settings.onButton({1,BtnAction::PRESSED})); assert(settings.cursor==SettingsScene::VOLUME);
    assert(settings.onButton({0,BtnAction::PRESSED})); assert(state.settings.volume==47);
    for (int i=0;i<4;++i) settings.onButton({1,BtnAction::PRESSED});
    assert(settings.cursor==SettingsScene::RESET_GAME);
    const auto resetRow=TouchUi::listLayout().cell(5,settings.touchScroll);
    assert(resetRow.y>=38 && resetRow.y+resetRow.height<=240);
    settings.render(); display.capture(output + "/touch-settings-bottom.ppm");
    assert(tap(settings,100,resetRow.y+20)); assert(settings.viewMode==SettingsScene::ViewMode::RESET_CONFIRM);
    assert(!settings.resetConfirmYes && settings.touchTarget(100,64)==-1);
    settings.render(); display.capture(output + "/touch-settings-reset.ppm");
    settings.onTouch({Type::DOWN,220,218,900});
    settings.onTouch({Type::CANCEL,220,218,930});
    assert(!settings.onTouch({Type::UP,220,218,960}));
    assert(state.oobeDone && settings.viewMode==SettingsScene::ViewMode::RESET_CONFIRM);
    assert(tap(settings,60,218)); assert(settings.viewMode==SettingsScene::ViewMode::MENU && state.oobeDone);
    assert(settings.onTap(SettingsScene::HELP)); assert(settings.viewMode==SettingsScene::ViewMode::HELP);
    settings.render(); display.capture(output + "/touch-settings-help.ppm"); parked(settings);
    assert(tap(settings,220,218) && settings.helpPage==1);
    assert(tap(settings,60,218) && settings.helpPage==0);
    for(int i=0;i<5;++i) assert(tap(settings,220,218));
    assert(settings.helpPage==5);
    settings.render(); display.capture(output + "/touch-settings-help-last.ppm");
    assert(tap(settings,22,19)); assert(settings.viewMode==SettingsScene::ViewMode::MENU);
    for (auto& page : Ui::Settings::TOUCH_HELP_PAGE_LINES) for (const char* line : page) {
        for (const auto* p=reinterpret_cast<const uint8_t*>(line); *p;) {
            uint32_t cp=*p++;
            if(cp<0x80) continue;
            const int more=cp<0xe0 ? 1 : cp<0xf0 ? 2 : 3;
            cp &= more==1 ? 0x1f : more==2 ? 0x0f : 7;
            for(int i=0;i<more;++i) cp=(cp<<6)|(*p++ & 0x3f);
            if (!FontResource::ins().findGlyphBitmap(cp,FontFace::SARASA_CJK)) {
                std::fprintf(stderr,"Missing touch UI glyph U+%04X\n",unsigned(cp));
                assert(false);
            }
        }
    }
    ShopScene shop; shop.onEnter(); shop.itemColumnX=128;
    assert(!shop.touchNavigation());
    parked(shop);
    shop.render(); display.capture(output + "/touch-shop-categories.ppm");
    assert(tap(shop,52,74));
    assert(shop.viewMode==ShopScene::ViewMode::DAILY);
    shop.itemColumnX=28;
    const auto coins=state.coins;
    shop.render(); display.capture(output + "/touch-shop.ppm");
    assert(tap(shop,52,74)); assert(shop.touchDetailOpen && state.coins==coins);
    shop.render(); display.capture(output + "/touch-shop-detail.ppm");
    parked(shop);
    const auto stock = game.itemCount(Game::ItemId::NORMAL_FOOD);
    // Dragging across the purchase action cannot spend coins.
    shop.onTouch({Type::DOWN,220,218,300});
    shop.onTouch({Type::MOVE,245,218,330});
    shop.onTouch({Type::UP,220,218,360});
    assert(state.coins == coins && game.itemCount(Game::ItemId::NORMAL_FOOD) == stock);
    assert(tap(shop,220,218));
    assert(state.coins == coins - ShopScene::priceFor(ShopScene::FOOD));
    assert(game.itemCount(Game::ItemId::NORMAL_FOOD) == stock + 1);
    shop.onTouch({Type::UP,220,218,400});
    assert(game.itemCount(Game::ItemId::NORMAL_FOOD) == stock + 1);
    assert(tap(shop,28,18)); assert(!shop.touchDetailOpen && shop.viewMode==ShopScene::ViewMode::DAILY);
    assert(tap(shop,28,18)); assert(shop.viewMode==ShopScene::ViewMode::CATEGORY);
    assert(tap(shop,244,74)); assert(shop.viewMode==ShopScene::ViewMode::SELL);
    const auto beforeDragCoins = state.coins;
    shop.onTouch({Type::DOWN,148,160,500});
    assert(shop.onTouch({Type::MOVE,148,60,530}));
    shop.onTouch({Type::UP,148,60,560});
    assert(shop.touchGridScroll > 0 && !shop.touchDetailOpen && state.coins == beforeDragCoins);
    shop.render(); display.capture(output + "/touch-shop-scrolled.ppm");
    // Selling the final copy returns to the grid instead of silently selecting another product.
    shop.touchGridScroll=0;
    int sellIndex=0;
    while (shop.itemAtIndex(sellIndex) != ShopScene::SUPER_POTION) { ++sellIndex; assert(sellIndex < shop.currentItemCount()); }
    auto saleRect = bagLayout.cell(sellIndex);
    assert(tap(shop,saleRect.x+20,saleRect.y+20) && shop.touchDetailOpen);
    assert(tap(shop,220,218));
    assert(game.itemCount(Game::ItemId::SUPER_POTION) == 0 && !shop.touchDetailOpen);
    // Egg taps follow the existing warm interaction without requiring a reset.
    HatchScene hatch; hatch.onEnter();
    int eggX=-1, eggY=-1;
    for(int y=39;y<200 && eggX<0;++y) for(int x=28;x<268;++x) if(hatch.touchTarget(x,y)==0) {eggX=x;eggY=y;break;}
    const auto pokes=hatch.pokeCount;
    assert(tap(hatch,eggX,eggY)); assert(hatch.pokeCount==pokes+1);
}

int main(int argc, char** argv) {
    assert(argc == 3); powerTests(); touchMathTests(); gridTests();
    assert(Ostb::batteryPercentFromRaw(1890) == 0);
    assert(Ostb::batteryPercentFromRaw(1920) == 5);
    assert(Ostb::batteryPercentFromRaw(2190) == 50);
    assert(Ostb::batteryPercentFromRaw(2489) == 100);
    assert(Ostb::batteryPercentFromRaw(2700) == 100);
    assert(Ostb::batteryPercentFromRaw(0) == -1 && Ostb::batteryPercentFromRaw(4095) == -1);
    DesktopPlatform desktop(argv[1]);
    TestDisplay display; TestPower power; NoMotion imu;
    auto services = desktop.serviceBundle();
    TestTouchInput input(desktop);
    Platform::Services bundle{services.lifecycle, services.clock, services.logger, input,
        display, services.audio, imu, power, services.blobs, services.resources, services.memory, services.peers};
    Platform::bind(bundle); assert(desktop.begin()); GameRandom::seed(1234);
    auto& game = GameEngine::ins(); assert(game.begin());
    auto tick = [&](int count) { while(count--) { desktop.advanceMs(20); game.run(); } };
    auto capture = [&](const char* name) { display.capture(std::string(argv[2])+"/"+name+".ppm"); };
    tick(10); capture("hatch");
    // Complete the real hatch transition and use the resulting monster.
    game.gameState().hatchSeconds = 1; tick(100);
    assert(game.gameState().oobeDone); capture("home");
    assert(RoomRenderer::screenX(240) == 268);
    game.requestScene(SceneID::MENU); tick(15); capture("menu");
    // A press interrupted by a screen lock must never become a menu click.
    desktop.setPressed(Platform::InputButton::PRIMARY,true); tick(2);
    display.isLocked=true; tick(4);
    const auto frameCount=display.frames;
    desktop.setPressed(Platform::InputButton::PRIMARY,false); tick(4);
    assert(display.frames==frameCount);
    ButtonEvent events[6];
    assert(ButtonDispatcher::ins().poll(events,6)==0);
    display.isLocked=false; game.invalidateScene(); tick(4);
    using Type = Platform::IInputDevice::TouchType;
    // A touch interrupted by lock and an orphan release cannot activate a row.
    input.events.push_back({Type::DOWN,148,42,desktop.millis()}); tick(1);
    display.isLocked=true; tick(2);
    input.events.push_back({Type::UP,148,42,desktop.millis()}); tick(1);
    display.isLocked=false; game.invalidateScene(); tick(2);
    game.requestScene(SceneID::EXPLORE); tick(15); capture("explore-select");
    ExploreScene explore; explore.onEnter();
    explore.areaCursor=0; explore.resetWalk(); explore.beginAutoWalk();
    assert(explore.generatedMap.paths[0].pointCount > 0);
    explore.render(); capture("explore-map");
    explore.wild=findSpecies(25); assert(explore.wild);
    explore.wildRuntime=Game::MonsterFactory::create(25,5);
    explore.wildHp=explore.wildHpMax=explore.wildRuntime.hpMax;
    explore.phase=ExploreScene::Phase::ENCOUNTER;
    explore.renderEncounter(); capture("battle");
    ShowerScene shower; shower.onEnter();
    shower.mode=ShowerScene::Mode::SOAPING;
    desktop.setPressed(Platform::InputButton::PRIMARY,true);
    for (int i=0;i<150;++i) { desktop.advanceMs(20); shower.update(desktop.millis(),0.02f); }
    assert(shower.anyFoam());
    shower.mode=ShowerScene::Mode::BRUSHING;
    for (int i=0;i<400;++i) { desktop.advanceMs(20); shower.update(desktop.millis(),0.02f); }
    shower.render(); capture("bath");
    desktop.setPressed(Platform::InputButton::PRIMARY,false);
    game.requestScene(SceneID::MAIN); tick(5);
    assert(game.saveNow());
    SaveManager saves; assert(saves.begin()); Game::GameState loaded{};
    MainSceneViewState view{}; assert(saves.load(loaded,view)); assert(loaded.oobeDone);
    assert(loaded.team[0].speciesId==game.gameState().team[0].speciesId);
    // Loading the shared room snapshot must restore the saved world position
    // before autonomous movement resumes, regardless of display dimensions.
    assert(view.valid);
    MainScene restoredHome; restoredHome.onEnter();
    assert(restoredHome.mainActor.x==view.monsterX);
    assert(restoredHome.mainActor.y==view.monsterY);
    restoredHome.onBeforeSave();
    assert(game.mainSceneViewState().monsterX==view.monsterX);
    assert(game.mainSceneViewState().monsterY==view.monsterY);
    roomShadowTests(display, argv[2]);
    roomViewportTests(display, argv[2]);
    touchSceneTests(display, argv[2]);
    std::puts(desktop.logs().c_str());
    std::puts("OSTB native render, power debounce, lock input, no-IMU bath and save checks passed");
}
