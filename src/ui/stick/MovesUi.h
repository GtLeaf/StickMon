#pragma once

#include "ui/common/MovesUiBase.h"


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
    uint32_t descScrollLastMs = 0;

    void reset();
    void openManage(uint8_t slot);
    bool openRecall(uint8_t slot);
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return true; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    int touchMoveRowCount() const { return rowCount(host.pageState()); }
    const MoveInfo* touchMoveInfo(uint8_t row, uint8_t* proficiency = nullptr) const {
        return info(host.pageState(), row, proficiency);
    }
    void renderMoveForgetConfirmPopup();
private:
    void resetDescription();
    void resetPresentation();
    bool updateDescriptionScroll(int key, int maxScroll);
    void renderDescription(const char* const* lines, int count, int x, int y, int width,
                           uint16_t color, int key);
};

}  // namespace Ui
