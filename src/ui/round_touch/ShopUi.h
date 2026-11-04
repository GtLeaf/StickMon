// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/common/ShopUiBase.h"
#include "core/TouchScroll.h"
#include "ui/compact_touch/ItemTouchUi.h"

namespace Ui {
class ShopUi final : public ShopUiBase {
public:
    explicit ShopUi(ShopHost& host) : ShopUiBase(host) {}
    void onEnter() override;
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    static MenuGrid::Rect coinBounds() { return {206, 56, 88, 36}; }
    static MenuGrid::Rect actionBounds() { return {70, 268, 104, 36}; }
    static MenuGrid::Rect detailBackBounds() { return {186, 268, 104, 36}; }
    MenuGrid::Rect tabBounds(int index) const;
    MenuGrid::Rect tabHitBounds(int index) const;
private:
    float touchGridScroll = 0;
    bool touchDetailOpen = false;
    TouchScroll gridTouch;
    char exactBalance[24] = {};
    float rememberedScroll[2] = {};
    int rememberedIndex[2] = {};
    void switchMode(bool sell);
    void returnToCategories() override;
    ItemTouchUi::Grid touchShopGrid() const;
    void touchSelect(int index);
    int touchSelectedIndex() const;
    void renderTabs();
    static int formatBalance(uint32_t coins, char (&text)[24]);
    void renderCoins();
    void renderDetail();
    void renderToast();
};
}  // namespace Ui
