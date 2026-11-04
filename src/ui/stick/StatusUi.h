#pragma once

#include "core/Scene.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {
class StatusUi : public Scene {
public:
    explicit StatusUi(MenuPageHost& host) : host(host) {}
    ~StatusUi() override;
    void reset();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return true; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    void changeStatusPage(int direction);
    static constexpr uint8_t STATUS_PAGE_COUNT = 5;
    uint8_t statusPage = 0;
    uint8_t statusMonsterIndex = 0;
    bool statusFromStorage = false;
    int statusScrollKey = -1;
    float statusScroll = 0.0f;
    uint32_t statusScrollLastMs = 0;

private:
    MenuPageHost& host;
    void renderStatusPage();
    void renderEggStatusPage();
    void renderPageIndicator(uint8_t page, uint8_t count);
    void renderStatusOverlay();
    bool updateStatusScroll(int scrollKey, int maxScroll);
};
}  // namespace Ui
