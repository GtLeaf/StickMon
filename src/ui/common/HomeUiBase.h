#pragma once

#include "ui/common/HomeUiContent.h"

namespace Ui {

class HomeUiBase : public Scene {
public:
    explicit HomeUiBase(HomeHost& host) : host(host) {}
    HomeUiContent::Camera camera;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    uint32_t hungerAnimStartedMs = 0, hungerAnimUntilMs = 0;
    uint8_t hungerAnimFrom = 0, hungerAnimTo = 0;

    void enter();
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool onTap(int target) override;
    void onExit() override { cancelTouch(); }
    uint8_t displayedHunger(uint8_t slot, uint8_t hunger, uint32_t nowMs) const;
    void animateHunger(uint8_t from, uint8_t to, uint32_t nowMs);
    void drawLayer(HomeLayer layer);

protected:
    HomeHost& host;
    virtual void drawHud(const HomeFrame& frame) = 0;
    virtual void drawToast() = 0;
    virtual void drawHomeMenu(const HomeFrame& frame) = 0;
    virtual void drawContact(const HomeFrame& frame) { HomeUiContent::drawContact(frame); }
    void focusCamera(float x, float y, float focusY);
#if STICKMON_ENABLE_DEBUG_FEATURES
    uint32_t comboStartMs = 0;
    bool comboSaved = false;
#endif
};

}  // namespace Ui
