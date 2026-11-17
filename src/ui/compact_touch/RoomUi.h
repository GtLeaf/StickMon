#pragma once

#include "core/TouchScroll.h"
#include "ui/common/RoomUiBase.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/common/HomeLibraryUi.h"
#endif

namespace Ui {

class RoomUi : public RoomUiBase {
public:
    explicit RoomUi(RoomHost& host) : RoomUiBase(host) {}
    uint8_t roomCursor = 0;
    float roomScroll = 0;
    TouchScroll gridTouch;
#if STICKMON_UI_ROUND_TOUCH
    HomeLibraryUi homeLibrary{{360,360,true}};
    void handleHomeLibraryRequest(HomeLibraryUi::Request request);
#endif

    static constexpr bool directTouch() { return true; }
    bool keepsDisplayAwake() const override {
#if STICKMON_UI_ROUND_TOUCH
        return homeLibrary.keepsDisplayAwake();
#else
        return false;
#endif
    }
    bool showsNotices() const {
#if STICKMON_UI_ROUND_TOUCH
        return !homeLibrary.active();
#else
        return true;
#endif
    }
    void reset();
    void onExit() override { cancelTouch(); }
#if STICKMON_UI_ROUND_TOUCH
    SceneUpdateResult update(uint32_t,float) override;
#else
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
#endif
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
};

}  // namespace Ui
