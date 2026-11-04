#pragma once

#include "ui/common/ShopUiBase.h"
#include "core/TouchScroll.h"
#include "ui/compact_touch/ItemTouchUi.h"

namespace Ui {

class ShopUi : public ShopUiBase {
public:
    explicit ShopUi(ShopHost& host) : ShopUiBase(host) {}
    void onEnter() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
private:
    float touchGridScroll = 0.0f;
    bool touchDetailOpen = false;
    TouchScroll gridTouch;
    void returnToCategories() override;
    ItemTouchUi::Grid touchShopGrid() const;
    void touchSelect(int index);
    int touchSelectedIndex() const;
    void renderToast();
};

}  // namespace Ui
