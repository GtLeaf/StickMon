#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "presentation/VisitMenu.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/SocialUi.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
struct Host final : Ui::SocialHost {
    Ui::SocialFrame frame{};
    Ui::SocialAction action = Ui::SocialAction::BACK;
    unsigned calls = 0;
    uint8_t value = 0;
    Ui::SocialFrame socialFrame() const override {
        return frame;
    }
    bool activateSocial(Ui::SocialAction a, uint8_t v = 0) override {
        action = a;
        value = v;
        ++calls;
        return true;
    }
};
int main() {
    DesktopPlatform desktop("data");
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    assert(FontResource::ins().begin());
    Host host;
    Ui::SocialUi ui(host);
    std::vector<uint16_t> pixels(360 * 360);
    Platform::FrameBuffer565 fb{pixels.data(), 360, 360, false};
    Ui::SceneSurface::select(SceneID::SOCIAL);
    assert(Ui::SceneSurface::begin(fb));
    assert(ui.layout().width == 360 && !ui.touchNavigation());
    auto tap = [&](int x, int y) {
        ui.render();
        using T = Platform::IInputDevice::TouchType;
        ui.onTouch({T::DOWN, int16_t(x), int16_t(y), 100});
        return ui.onTouch({T::UP, int16_t(x), int16_t(y), 160});
    };
    host.frame.model.state = Ui::SocialState::WAITING_HOST_DECISION;
    ui.enter(host.frame.model, 0);
    ui.render();
    int visitorPixels = 0;
    for (int y = 100; y < 148; ++y)
        for (int x = 190; x < 228; ++x)
            if (pixels[y * 360 + x] != 0)
                ++visitorPixels;
    assert(visitorPixels > 100); // the preview's visitor face must be present beside the door
    assert(tap(180, 230) && host.action == Ui::SocialAction::DECIDE && host.value == 1);
    assert(tap(180, 288) && host.action == Ui::SocialAction::DECIDE && host.value == 0);
    host.frame.model.state = Ui::SocialState::FAILED;
    host.frame.model.error = "TEAM NOT SOLO";
    ui.enter(host.frame.model, 0);
    assert(tap(180, 280) && host.action == Ui::SocialAction::ADJUST_TEAM);
    host.frame.model.state = Ui::SocialState::SEARCHING;
    ui.enter(host.frame.model, 0);
    assert(tap(180, 285) && host.action == Ui::SocialAction::STOP);
    host.frame.model.state = Ui::SocialState::ROOM_LIST;
    host.frame.model.roomCount = 4;
    for (int i = 0; i < 4; ++i) {
        host.frame.model.rooms[i].roomId = 2;
        host.frame.model.rooms[i].mac[5] = uint8_t(i + 1);
    }
    ui.enter(host.frame.model, 0);
    ui.render();
    const auto parked = pixels;
    // A continuous upward drag reveals the fourth room without joining anything.
    using Touch = Platform::IInputDevice::TouchType;
    const auto beforeScroll = host.calls;
    ui.onTouch({Touch::DOWN, 180, 220, 170});
    ui.onTouch({Touch::MOVE, 180, 140, 210});
    ui.onTouch({Touch::MOVE, 180, 70, 240});
    ui.onTouch({Touch::UP, 180, 70, 260});
    assert(host.calls == beforeScroll);
    ui.render();
    for (int y = 0; y < 360; ++y)
        if (y < 102 || y >= 252)
            for (int x = 0; x < 360; ++x)
                assert(pixels[y * 360 + x] == parked[y * 360 + x]);
    assert(tap(180, 220) && host.action == Ui::SocialAction::SELECT_ROOM && host.value == 3);
    assert(tap(180, 140) && host.value == 2);
    // Replace the selected device while the finger is held down.
    using T = Platform::IInputDevice::TouchType;
    const auto calls = host.calls;
    ui.onTouch({T::DOWN, 180, 125, 200});
    host.frame.model.rooms[2].mac[5] = 99;
    ui.onTouch({T::UP, 180, 125, 250});
    assert(host.calls == calls);
    // Cancel, drag and outside-circle release cannot activate a button.
    ui.onTouch({T::DOWN, 180, 195, 300});
    ui.onTouch({T::CANCEL, 180, 195, 350});
    ui.onTouch({T::UP, 180, 195, 360});
    assert(host.calls == calls);
    ui.onTouch({T::DOWN, 180, 195, 400});
    ui.onTouch({T::MOVE, 210, 195, 420});
    ui.onTouch({T::UP, 180, 195, 440});
    assert(host.calls == calls);
    assert(!tap(0, 0) && host.calls == calls);
    // Inertia uses the scene update cadence; the first touch brakes without joining.
    ui.enter(host.frame.model, 1000);
    ui.render();
    ui.onTouch({T::DOWN, 180, 220, 1000});
    ui.onTouch({T::MOVE, 180, 200, 1020});
    ui.onTouch({T::MOVE, 180, 180, 1040});
    ui.onTouch({T::UP, 180, 180, 1050});
    ui.render();
    const auto released = pixels;
    RenderDemand inertia;
    ui.update(host.frame.model, 1083, inertia);
    assert(inertia.result().redraw && inertia.result().nextUpdateDelayMs <= 33);
    ui.render();
    assert(pixels != released);
    assert(ui.visualFrameIntervalMs(1083) == 33);
    ui.onTouch({T::DOWN, 180, 180, 1090});
    ui.onTouch({T::UP, 180, 180, 1120});
    assert(host.calls == calls);
    ui.cancelTouch();
    assert(ui.visualFrameIntervalMs(1200) == 66);
    ui.render();
    const auto stopped = pixels;
    RenderDemand stop;
    ui.update(host.frame.model, 1200, stop);
    ui.render();
    assert(pixels == stopped);
    host.frame.model.roomCount = 0;
    RenderDemand empty;
    ui.update(host.frame.model, 1210, empty);
    ui.render();
    assert(Ui::VisitMenu::itemAt(180, 150, host.frame.model, true) == -1);
    // A requester replaced during a gesture cannot inherit its decision.
    host.frame.model.state = Ui::SocialState::WAITING_HOST_DECISION;
    host.frame.model.requestSeq = 41;
    ui.render();
    ui.onTouch({T::DOWN, 180, 230, 500});
    host.frame.model.requestSeq = 42;
    ui.onTouch({T::UP, 180, 230, 550});
    assert(host.calls == calls);
    // Same-count room identity updates request a new frame immediately.
    host.frame.model.roomCount = 4;
    host.frame.model.state = Ui::SocialState::ROOM_LIST;
    ui.enter(host.frame.model, 0);
    RenderDemand initial;
    ui.update(host.frame.model, 0, initial);
    host.frame.model.rooms[0].mac[5] ^= 1;
    RenderDemand changed;
    ui.update(host.frame.model, 1, changed);
    assert(changed.result().redraw);
    host.frame.model.state = Ui::SocialState::ENDING;
    ui.enter(host.frame.model, 0);
    RenderDemand start;
    ui.update(host.frame.model, 0, start);
    RenderDemand still;
    ui.update(host.frame.model, 100, still);
    assert(!still.result().redraw);
    RenderDemand animated;
    ui.update(host.frame.model, 500, animated);
    assert(animated.result().redraw);
    Ui::VisitMenu::Model rejected{};
    rejected.state = Ui::SocialState::ROOM_LIST;
    rejected.error = "JOIN DECLINED";
    assert(std::strcmp(Ui::VisitMenu::title(rejected), "已拒绝") == 0);
    rejected.error = "JOIN TIMEOUT";
    assert(std::strcmp(Ui::VisitMenu::title(rejected), "确认超时") == 0);
    for (const char *code : {"TEAM NOT SOLO", "LINK UNAVAILABLE", "NO PET", "SEARCH TIMEOUT",
                             "HOST TIMEOUT", "LINK LOST", "HOST TEAM BUSY", "LINK TIMEOUT"}) {
        auto e = Ui::VisitMenu::error(code);
        assert(e.title && e.action);
        assert(std::strcmp(e.title, code) != 0);
    }
    for (int state = 0; state < 12; ++state) {
        host.frame.model.state = Ui::SocialState(state);
        ui.render();
    }
    std::puts(
        "PASS native round: accept/decline, cancel, adjust team, four-room continuous scrolling, "
        "replaced room, drag/CANCEL and circle bounds");
}
