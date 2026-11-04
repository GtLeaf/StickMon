#pragma once

#include "ui/common/HomeUiBase.h"

namespace Ui {

class HomeUi : public HomeUiBase {
public:
    explicit HomeUi(HomeHost& host) : HomeUiBase(host) {}
    void focus(float x, float y);
    int touchTarget(int16_t x, int16_t y) override;
protected:
    void drawHud(const HomeFrame& frame) override;
    void drawToast() override;
    void drawHomeMenu(const HomeFrame& frame) override;
    void drawContact(const HomeFrame& frame) override;
};

}  // namespace Ui
