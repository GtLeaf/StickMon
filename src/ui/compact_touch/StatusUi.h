#pragma once

#include "core/Scene.h"
#include "ui/common/MenuPageHost.h"
#include "core/TouchScroll.h"

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
    bool touchNavigation() const override { return false; }
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
    bool statusSwipeActive = false;
    int statusSwipeAxis = 0;
    bool statusSwipePreviewFailed = false;
    int16_t statusSwipeX = 0, statusSwipeY = 0;
    TouchScroll gridTouch;
    uint16_t* statusSlideFrames = nullptr;
    bool statusSlideCapturing = false;
    bool statusSlideDragging = false;
    uint8_t statusSlideNextPage = 0;
    uint32_t statusSlideStartedMs = 0;
    int statusSlideDirection = 1;
    int statusSlidePixels = 0;
    int statusSlideFromPixels = 0, statusSlideToPixels = 0;
    uint32_t statusSlideDurationMs = 220;
    bool prepareStatusSlide(int direction);
    void trackStatusSwipe(int dx);
    void settleStatusSwipe(bool commit);
    void stopStatusSlide();
    int touchStatusContentHeight();

private:
    MenuPageHost& host;
    void renderStatusPage();
    void renderEggStatusPage();
    void renderPageIndicator(uint8_t page, uint8_t count);
    void renderStatusOverlay();
    int renderTouchStatusProfile(bool draw);
};
}  // namespace Ui
