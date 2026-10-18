/**
 * Web entry point for StickMon simulator.
 * Uses Emscripten main loop driven by requestAnimationFrame.
 */

#include "core/GameEngine.h"
#include "game/GameRandom.h"
#include "platform/api/PlatformServices.h"
#include "platform/web/WebPlatform.h"

#include <cstdio>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static WebPlatform webPlatform;
static bool gameReady = false;

static void gameLoopIteration() {
    if (gameReady) {
        GameEngine::ins().run();
    }
}

// Expose input control to JS
#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
void stickmon_button_down(int button) {
    if (button == 0) webPlatform.setPressed(Platform::InputButton::PRIMARY, true);
    else if (button == 1) webPlatform.setPressed(Platform::InputButton::SECONDARY, true);
}

EMSCRIPTEN_KEEPALIVE
void stickmon_button_up(int button) {
    if (button == 0) webPlatform.setPressed(Platform::InputButton::PRIMARY, false);
    else if (button == 1) webPlatform.setPressed(Platform::InputButton::SECONDARY, false);
}

EMSCRIPTEN_KEEPALIVE
void stickmon_set_acceleration(float x, float y, float z) {
    webPlatform.setAcceleration(x, y, z);
}

} // extern "C"
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("[StickMon Web] Starting...\n");

    Platform::bind(webPlatform.serviceBundle());
    if (!Platform::services().lifecycle.begin()) {
        printf("[StickMon Web] ERROR: Platform init failed\n");
        return 1;
    }

    const uint32_t seed = Platform::power().hardwareRandom();
    GameRandom::seed(seed);

    gameReady = GameEngine::ins().begin();
    if (!gameReady) {
        printf("[StickMon Web] ERROR: GameEngine init failed\n");
        return 1;
    }

    printf("[StickMon Web] Init done, starting main loop\n");

#ifdef __EMSCRIPTEN__
    // 0 = browser decides via requestAnimationFrame (typically 60fps)
    emscripten_set_main_loop(gameLoopIteration, 0, 1);
#else
    // Non-Emscripten fallback: simple loop for testing
    while (true) {
        gameLoopIteration();
    }
#endif

    return 0;
}
