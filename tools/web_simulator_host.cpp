#include "AmoledApp.h"
#include "platform/web/WebPlatform.h"
#include <cassert>
#include <cstdio>

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void cancelledHatchTap(AmoledApp& app) {
        app.hatchController_.eggHitBounds() = {100, 100, 40, 40};
        const auto bounds = app.hatchController_.eggHitBounds();
        TouchEvent event;
        event.x = bounds.x + bounds.width / 2;
        event.y = bounds.y + bounds.height / 2;
        event.type = TouchEventType::DOWN;
        app.handleTouch(event);
        assert(app.pointerDown);
        assert(app.hatchController_.state().pressedItem == 0);
        const auto pokes = app.hatchController_.state().pokeCount;
        app.cancelTouch();
        event.type = TouchEventType::UP;
        app.handleTouch(event);
        assert(!app.pointerDown && app.hatchController_.state().pokeCount == pokes);
    }
};
}

int main() {
    WebPlatform platform(368, 448, "test:amoled:");
    Platform::bind(platform.serviceBundle());
    assert(platform.begin());
    assert(platform.frameBuffer().width == 368 && platform.frameBuffer().height == 448);
    platform.setTime(100);
    assert(platform.millis() == 100);
    platform.sleepMs(1000);
    assert(platform.millis() == 100); // A paused web clock cannot advance itself.
    assert(!platform.enable()); // No simulated ESP-NOW session that can hang.
    AmoledV1::AmoledApp app;
    app.begin(100);
    AmoledV1::AmoledAppTestAccess::cancelledHatchTap(app);
    assert(app.flushSave());
    const uint8_t marker = 42;
    assert(platform.writeBlob("other", "keep", &marker, 1));
    assert(platform.clearNamespace("stickmon"));
    uint8_t recovered = 0;
    assert(platform.readBlob("other", "keep", &recovered, 1) && recovered == marker);
    std::puts("Web native framebuffer, paused clock, cancelled touch and scoped reset passed");
}
