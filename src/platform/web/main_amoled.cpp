#include "AmoledApp.h"
#include "AmoledGeometry.h"
#include "platform/web/WebPlatform.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"

#include <algorithm>
#include <emscripten.h>

namespace {
WebPlatform platform(AmoledUi::WIDTH, AmoledUi::HEIGHT,
    "stickmon_sim:stickmon:waveshare-amoled-1-8-v2:");
AmoledV1::AmoledApp app;
Canvas565 canvas;
bool ready = false;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void stickmon_web_tick(uint32_t nowMs) {
    if (!ready) return;
    platform.setTime(nowMs);
    app.update(nowMs);
    // The browser lifecycle owns suspension; no hardware sleep is entered.
    if (app.consumeLockRequest()) {
        EM_ASM({ if (Module['onScreenLock']) Module['onScreenLock'](); });
    }
    if (!app.needsRender()) return;
    const int left = app.renderColumnBegin(), right = app.renderColumnEnd();
    const int top = app.renderRowBegin(), bottom = app.renderRowEnd();
    canvas.setClipRect(left, top, right - left, bottom - top);
    app.render(canvas);
    canvas.clearClipRect();
    platform.presentRegion(left, top, right, bottom);
    app.markRendered();
}

EMSCRIPTEN_KEEPALIVE void stickmon_web_touch(int type, int x, int y) {
    if (!ready || type < 0 || type > 3) return;
    if (type == 3) { app.cancelTouch(); return; }
    AmoledV1::TouchEvent event;
    event.type = static_cast<AmoledV1::TouchEventType>(type);
    event.x = event.rawX = std::clamp(x, 0, AmoledUi::WIDTH - 1);
    event.y = event.rawY = std::clamp(y, 0, AmoledUi::HEIGHT - 1);
    event.timestampMs = platform.millis();
    app.handleTouch(event);
}

EMSCRIPTEN_KEEPALIVE int stickmon_web_save() {
    return ready && app.flushSave() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void stickmon_web_pause() { platform.stop(); }
EMSCRIPTEN_KEEPALIVE void stickmon_web_resume() {
    if (ready) app.onWake(platform.millis());
}
}

int main() {
    Platform::bind(platform.serviceBundle());
    platform.setTime(0);
    platform.begin();
    const auto buffer = platform.frameBuffer();
    canvas.attach(buffer);
    canvas.setNativeText(true);
    PixelRenderer::bind(buffer);
    PixelRenderer::canvas().setNativeText(true);
    app.begin(0);
    ready = true;
    stickmon_web_tick(0);
    EM_ASM({
        if (Module['onGameReady']) Module['onGameReady']($0, $1);
    }, app.startupStateLoaded() ? 1 : 0, app.saveWriteBlocked() ? 1 : 0);
    return 0;
}
