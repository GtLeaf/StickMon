#pragma once

#include "ui/common/MovesUiBase.h"
#include "core/TouchScroll.h"

namespace Ui {

class MovesUi : public MovesUiBase {
public:
    explicit MovesUi(MovesHost& host) : MovesUiBase(host) {}
    uint8_t moveForgetSlot = 0;
    bool moveForgetConfirmOpen = false;
    bool moveForgetConfirmYes = false;
    float moveListScroll = 0.0f;
    int descScrollKey = -1;
    float descScroll = 0.0f;
    TouchScroll gridTouch;
    TouchScroll descriptionTouch;
    void reset();
    void openManage(uint8_t slot);
    bool openRecall(uint8_t slot);
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    int touchMoveRowCount() const {
        const int count = rowCount(host.pageState());
        return count > 0 ? count - 1 : 0;
    }
    const MoveInfo* touchMoveInfo(uint8_t row, uint8_t* proficiency = nullptr) const {
        return info(host.pageState(), row, proficiency);
    }
    void renderMoveForgetConfirmPopup();
private:
    void resetDescription();
    void resetPresentation();
    bool descriptionGesture = false;
    bool syncDescription();
    float descriptionMaximum() const;
    void renderDescription(const char* text, uint16_t color);
};

}  // namespace Ui
