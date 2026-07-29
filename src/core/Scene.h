#pragma once

#include <cstdint>
#include <climits>
#include "core/ButtonDispatcher.h"

enum class SceneID : uint8_t {
    MAIN = 0,
    MENU,
    SOCIAL,
    SHOP,
    EXPLORE,
    SETTINGS,
    HATCH,
    SHOWER,
};

struct SceneUpdateResult {
    static constexpr uint32_t NO_UPDATE = UINT32_MAX;

    // NO_UPDATE parks the scene until input, a scene switch, or an external
    // game-state mutation wakes it. Redraw never implies another update.
    bool redraw;
    uint32_t nextUpdateDelayMs;

    SceneUpdateResult(bool shouldRedraw = false,
                      uint32_t delayMs = NO_UPDATE)
        : redraw(shouldRedraw), nextUpdateDelayMs(delayMs) {}

    static SceneUpdateResult idle(bool redraw = false) {
        return SceneUpdateResult(redraw, NO_UPDATE);
    }

    static SceneUpdateResult after(uint32_t delayMs, bool redraw = true) {
        return SceneUpdateResult(redraw, delayMs);
    }
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onBeforeSave() {}
    virtual SceneUpdateResult update(uint32_t nowMs, float dtSeconds) = 0;
    virtual void render() = 0;
    virtual bool onButton(const ButtonEvent& event) { (void)event; return false; }
};
