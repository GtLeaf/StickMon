#pragma once

#include "ui/common/MenuOverlayUiModel.h"

namespace Ui {
class MenuOverlayUiBase {
public:
    virtual ~MenuOverlayUiBase() = default;
    virtual MenuOverlayLayout layout() const = 0;
    const char* noticeText(const char* text) const { return text; }
    void renderBackground() const;
    void renderToast(const MenuOverlayFrame& frame) const;
    void renderTutorial(const MenuOverlayFrame& frame) const;
    bool touchNavigation(bool directTouchPage) const;
};
}  // namespace Ui
