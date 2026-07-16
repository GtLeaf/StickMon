#pragma once

#include <cstdint>
#include "core/ButtonDispatcher.h"

enum class SceneID : uint8_t {
    MAIN = 0,
    MENU,
    SOCIAL,
    SHOP,
    EXPLORE,
    SETTINGS,
    HATCH,
};

class Scene {
public:
    virtual ~Scene() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void onBeforeSave() {}
    virtual void update(uint32_t nowMs, float dtSeconds) = 0;
    virtual void render() = 0;
    virtual bool onButton(const ButtonEvent& event) { (void)event; return false; }
};
