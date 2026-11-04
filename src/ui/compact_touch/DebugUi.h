#pragma once
#include "ui/common/DebugUiBase.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include "core/TouchScroll.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/TouchCalibrationSession.h"
#endif
namespace Ui {
class DebugUi : public DebugUiBase {
public:
    explicit DebugUi(DebugHost& host) : DebugUiBase(host) {}
#if STICKMON_UI_ROUND_TOUCH
    ~DebugUi() override { closeCalibration(); }
#endif
    DebugLayout layout() const override;
    void reset();
    void render() override;
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    void onExit() override { cancelTouch(); }
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    uint8_t visibleItemCount() const { return debugItemCount() - 1; }
private:
    TouchScroll listTouch;
    bool touchHeaderDown = false;
#if STICKMON_UI_ROUND_TOUCH
    bool calibrationOpen = false;
    bool calibrationButtonDown = false;
    int calibrationControl = -1;
    const char* calibrationNotice = nullptr;
    TouchCalibrationSession calibration;
    void closeCalibration();
    void openCalibration();
    void renderCalibration();
    void handleCalibration(const Platform::IInputDevice::TouchEvent& event);
#endif
    const char* title() const;
    void renderPopup();
    void renderTouchTest();
    void handleTouchTest(const Platform::IInputDevice::TouchEvent& event);
};
}  // namespace Ui
#endif
