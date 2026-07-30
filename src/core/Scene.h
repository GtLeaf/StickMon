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

    static SceneUpdateResult parked() {
        return SceneUpdateResult(false, NO_UPDATE);
    }

    static SceneUpdateResult frame() {
        return SceneUpdateResult(true, NO_UPDATE);
    }

    static SceneUpdateResult animate(uint32_t delayMs) {
        return SceneUpdateResult(true, delayMs);
    }

    static SceneUpdateResult pollAfter(uint32_t delayMs) {
        return SceneUpdateResult(false, delayMs);
    }

private:
    friend class RenderDemand;

    SceneUpdateResult(bool shouldRedraw, uint32_t delayMs)
        : redraw(shouldRedraw), nextUpdateDelayMs(delayMs) {}
};

// Collects every visual change and wake-up source for one scene update.
// animate() redraws and schedules another frame; wakeIn() only schedules work.
class RenderDemand {
public:
    void redraw() { redrawRequested = true; }

    void changed(bool didChange) {
        if (didChange) redraw();
    }

    void animate(bool active, uint32_t delayMs = 66) {
        if (!active) return;
        redraw();
        wakeIn(delayMs);
    }

    void wakeIn(uint32_t delayMs) {
        if (delayMs == SceneUpdateResult::NO_UPDATE) return;
        if (delayMs == 0) delayMs = 1;
        if (nextDelayMs == SceneUpdateResult::NO_UPDATE ||
            delayMs < nextDelayMs) {
            nextDelayMs = delayMs;
        }
    }

    void wakeAt(uint32_t nowMs, uint32_t deadlineMs) {
        int32_t remaining = static_cast<int32_t>(deadlineMs - nowMs);
        wakeIn(remaining > 0 ? static_cast<uint32_t>(remaining) : 1);
    }

    bool expired(bool active, uint32_t nowMs, uint32_t deadlineMs) {
        if (!active) return false;
        if (static_cast<int32_t>(nowMs - deadlineMs) >= 0) {
            redraw();
            return true;
        }
        wakeAt(nowMs, deadlineMs);
        return false;
    }

    SceneUpdateResult result() const {
        return SceneUpdateResult(redrawRequested, nextDelayMs);
    }

private:
    bool redrawRequested = false;
    uint32_t nextDelayMs = SceneUpdateResult::NO_UPDATE;
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
