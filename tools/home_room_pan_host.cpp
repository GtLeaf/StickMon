// Real AmoledApp journey for the home room look-around drag.
#include "AmoledApp.h"
#include "core/FontResource.h"
#include "core/RoomResource.h"
#include "game/MonsterFactory.h"
#include "platform/common/PosixUserFiles.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/FurnitureArt.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

struct PanPower final : Platform::IPowerDevice {
    explicit PanPower(Platform::IPowerDevice& power) : power(power) {}
    Platform::IPowerDevice& power;
    int batteryLevel() override { return power.batteryLevel(); }
    Platform::WakeReason wakeReason() const override { return power.wakeReason(); }
    uint32_t hardwareRandom() override { return power.hardwareRandom(); }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return externalMemorySize(); }
    void restart() override { power.restart(); }
    void enterDeepSleep(uint64_t duration, bool external) override { power.enterDeepSleep(duration, external); }
};

namespace AmoledV1 {
struct AmoledAppTestAccess {
static void run(DesktopPlatform& desktop, const char* output) {
    using S = AppSceneFlow::Scene;
    using P = HomePresenter;
    desktop.sleepMs(1000);
    AmoledApp app;
    app.begin(desktop.millis());
    app.gameState.oobeDone = true;
    app.gameState.teamCount = 1;
    app.gameState.team[0] = Game::MonsterFactory::create(25, 20);
    app.gameState.team[0].satiety = 100;
    app.gameState.room.ownedFurniture |= 1;
    app.gameState.room.placedFurniture |= 1;
    app.exploreTransition_.reset();
    app.exploreSessionActive = false;
    app.sceneFlow.enter(S::HOME);
    auto& room = RoomResource::ins();
    assert(room.available() && room.width() == 240);

    // Park the pet near the bed side: the follow camera keeps the frame
    // (logical x 193..214) outside the 184-px view.
    auto pin = [&](float x) {
        app.homeMainActor.x = app.homeMainActor.targetX = x;
        app.homeMainActor.y = app.homeMainActor.targetY = room.roomY() + 108;
        app.homeMainActor.motion = Home::MotionPhase::STILL;
        app.homeMainActor.nextDecisionMs = desktop.millis() + 100000000U;
        app.updateCamera();
    };
    pin(70);
    const float follow = app.homePresenter_.cameraX();
    auto frameX = [&]() { return app.worldToScreenX(FurnitureArt::FRAME_WORLD_X + 10); };
    auto frameY = [&]() { return app.worldToScreenY(room.roomY() + FurnitureArt::FRAME_ART_Y + 10); };
    assert(frameX() >= AmoledUi::WIDTH && "frame starts outside the follow view");

    uint32_t time = desktop.millis();
    auto event = [&](TouchEventType type, int x, int y) {
        desktop.sleepMs(16);
        time = desktop.millis();
        app.handleTouch({type, int16_t(x), int16_t(y), int16_t(x), int16_t(y), time});
    };
    auto step = [&](uint32_t ms) {
        for (uint32_t done = 0; done < ms; done += 33) {
            desktop.sleepMs(33);
            app.homeMainActor.nextDecisionMs = desktop.millis() + 100000000U;
            app.update(desktop.millis());
        }
    };
    auto& canvas = PixelRenderer::canvas();
    auto snapshot = [&]() {
        std::vector<uint16_t> pixels(AmoledUi::WIDTH * AmoledUi::HEIGHT);
        for (int y = 0; y < AmoledUi::HEIGHT; ++y)
            for (int x = 0; x < AmoledUi::WIDTH; ++x)
                pixels[y * AmoledUi::WIDTH + x] = canvas.readPixel(x, y);
        return pixels;
    };
    auto shot = [&](const char* name) {
        const auto path = std::string(output) + "/" + name + ".ppm";
        FILE* f = std::fopen(path.c_str(), "wb");
        assert(f);
        std::fprintf(f, "P6\n368 448\n255\n");
        for (int y = 0; y < 448; ++y) for (int x = 0; x < 368; ++x) {
            const auto p = canvas.readPixel(x, y);
            const unsigned char rgb[] = {uint8_t(((p >> 11) & 31) * 255 / 31),
                uint8_t(((p >> 5) & 63) * 255 / 63), uint8_t((p & 31) * 255 / 31)};
            std::fwrite(rgb, 1, 3, f);
        }
        std::fclose(f);
    };
    // The damage requested by the pan must reproduce a full repaint.
    auto damageMatchesFull = [&]() {
        app.render(canvas);
        app.markRendered();
        const auto partial = snapshot();
        app.forceFullRender();
        app.render(canvas);
        app.markRendered();
        const auto full = snapshot();
        // Only the room band; the header clock follows its own minute cadence.
        for (int y = HOME_ROOM_TOP; y < HOME_STATUS_TOP; ++y)
            for (int x = 0; x < AmoledUi::WIDTH; ++x)
                assert(partial[y * AmoledUi::WIDTH + x] == full[y * AmoledUi::WIDTH + x] &&
                       "pan damage must cover the moved room");
    };
    app.forceFullRender(); app.render(canvas); app.markRendered();
    shot("01-follow");

    // A short horizontal wobble stays a tap candidate; the drag then pans
    // 1:1 after the 10px slop and never activates a room target.
    const uint32_t hearts = app.heartsUntil;
    event(TouchEventType::DOWN, 300, 200);
    event(TouchEventType::MOVE, 294, 200);
    assert(!app.homePresenter_.panning() && app.homePresenter_.cameraX() == follow);
    event(TouchEventType::MOVE, 280, 201);
    assert(app.homePresenter_.panning());
    assert(app.homePresenter_.cameraX() == follow + 5);
    event(TouchEventType::MOVE, 200, 203);
    event(TouchEventType::MOVE, 40, 205);
    event(TouchEventType::UP, 40, 205);
    assert(!app.homePresenter_.panning());
    assert(app.homePresenter_.cameraX() == 56 && "clamped at the right wall");
    assert(app.heartsUntil == hearts && !app.homePhotoFrame_.active());
    assert(app.sceneFlow.current() == S::HOME);
    step(33);
    damageMatchesFull();
    assert(frameX() > 0 && frameX() < AmoledUi::WIDTH && "frame visible after looking around");
    shot("02-looked-right");

    // The visible frame still opens with a tap at its new position.
    event(TouchEventType::DOWN, frameX(), frameY());
    event(TouchEventType::UP, frameX(), frameY());
    assert(app.homePhotoFrame_.active());
    app.homePhotoFrame_.close();
    app.forceFullRender();

    // A sideways drag that starts on the frame pans instead of opening it.
    event(TouchEventType::DOWN, frameX(), frameY());
    const int startX = frameX();
    event(TouchEventType::MOVE, startX + 12, frameY());
    event(TouchEventType::MOVE, startX + 30, frameY());
    event(TouchEventType::UP, startX + 30, frameY());
    assert(!app.homePhotoFrame_.active());
    assert(app.homePresenter_.cameraX() == 46);
    step(33);
    damageMatchesFull();

    // Vertical drags and drags from the status bar do not move the camera.
    event(TouchEventType::DOWN, 200, 120);
    event(TouchEventType::MOVE, 200, 200);
    event(TouchEventType::MOVE, 120, 205);
    event(TouchEventType::UP, 120, 205);
    assert(app.homePresenter_.cameraX() == 46);
    event(TouchEventType::DOWN, 300, HOME_STATUS_TOP + 20);
    event(TouchEventType::MOVE, 120, HOME_STATUS_TOP + 20);
    event(TouchEventType::UP, 120, HOME_STATUS_TOP + 20);
    assert(app.homePresenter_.cameraX() == 46);

    // The view holds while idle and eases back after the idle timer.
    step(P::PAN_IDLE_RETURN_MS - 2000);
    assert(app.homePresenter_.cameraX() == 46 && !app.homePresenter_.panReturning());
    step(2000 + 66);
    assert(app.homePresenter_.panReturning());
    assert(app.homePresenter_.cameraX() < 46);
    damageMatchesFull();
    step(P::PAN_RETURN_MS + 66);
    assert(app.homePresenter_.cameraX() == follow && app.homePresenter_.panOffset() == 0);
    damageMatchesFull();

    // Cancelling mid-drag keeps the view; walking toward the edge returns it.
    event(TouchEventType::DOWN, 300, 200);
    event(TouchEventType::MOVE, 200, 200);
    app.cancelTouch();
    event(TouchEventType::UP, 200, 200);
    assert(!app.homePresenter_.panning() && app.homePresenter_.cameraX() == follow + 45);
    step(500);
    assert(app.homePresenter_.cameraX() == follow + 45);
    pin(app.homePresenter_.cameraX() + 6);
    step(P::PAN_RETURN_MS + 99);
    assert(app.homePresenter_.panOffset() == 0);

    // Leaving the room drops the look-around immediately.
    pin(70);
    event(TouchEventType::DOWN, 300, 200);
    event(TouchEventType::MOVE, 150, 200);
    event(TouchEventType::UP, 150, 200);
    assert(app.homePresenter_.panOffset() > 0);
    app.sceneFlow.openMenu();
    step(33);
    assert(app.homePresenter_.panOffset() == 0 && app.homePresenter_.cameraX() == follow);
    app.sceneFlow.goHome();

    // The locked display also clears a held pan.
    event(TouchEventType::DOWN, 300, 200);
    event(TouchEventType::MOVE, 150, 200);
    app.setDisplayLocked(true);
    step(33);
    assert(!app.homePresenter_.panning() && app.homePresenter_.panOffset() == 0);
    app.setDisplayLocked(false);
    event(TouchEventType::UP, 150, 200);
    assert(app.homePresenter_.panOffset() == 0);

    std::puts("home room pan: slop/1:1 drag/clamp/frame visible+tap/frame-start drag/vertical+status ignored/"
              "idle hold+eased return/cancel/walk-off/leave/lock and damage bands OK");
}
};
}

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    auto& base = desktop.serviceBundle();
    PanPower power(base.power);
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input, base.display,
        base.audio, base.imu, power, base.blobs, base.resources, base.memory, base.peers};
    Platform::PosixUserFiles files(std::string(argv[2]) + "/photos");
    services.userFiles = &files;
    Platform::bind(services);
    assert(desktop.begin() && Platform::resources().mount());
    assert(GameAssets::begin() && FontResource::ins().begin());
    std::vector<uint16_t> pixels(368 * 448);
    auto& c = PixelRenderer::canvas();
    c.attach({pixels.data(), 368, 448, false});
    c.setCoordinateScale(1); c.setLayoutScale(1); c.setAssetScale(1); c.setNativeText(true);
    AmoledV1::AmoledAppTestAccess::run(desktop, argv[2]);
}
