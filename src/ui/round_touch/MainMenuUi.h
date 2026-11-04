// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/TouchScroll.h"
#include "presentation/MenuGrid.h"
#include "ui/common/MainMenuUiBase.h"

namespace Ui {
class MainMenuUi final : public MainMenuUiBase {
public:
    explicit MainMenuUi(MainMenuHost& host) : MainMenuUiBase(host) {}
    static constexpr uint8_t ROUND_ITEM_COUNT = ITEM_COUNT;
    static MenuGrid::Layout layout();
    static MenuGrid::Rect backBounds() { return {142, 12, 40, 40}; }
    static MenuGrid::Rect itemBounds(int index, float scroll);
    static float maximumScroll();
    void enter(bool fromHome);
    void resetNavigation();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
private:
    TouchScroll touch;
    float scroll = 0;
    void revealCursor();
};
}  // namespace Ui
