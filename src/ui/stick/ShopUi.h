#pragma once

#include "ui/common/ShopUiBase.h"

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
private:
    float itemColumnX = 128.0f;
    float itemAnimCursor = 0.0f;
    uint16_t columnTraceMask = 0;
    void activateCategory();
    void returnToCategories() override;
    bool itemColumnSettled() const;
    void renderList();
    void renderCategoryList();
    void renderSubmenu();
    void renderIconColumn(int centerX, bool dimmed, bool selectable);
    void renderItemDetail();
    void renderToast();
};

}  // namespace Ui
