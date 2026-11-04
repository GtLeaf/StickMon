// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/MenuOverlayUi.h"

#include <cstring>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "presentation/MenuGrid.h"

namespace Ui {
const char* MenuOverlayUi::noticeText(const char* text) const {
    return text && std::strcmp(text, Ui::Menu::SWITCH_TOAST) == 0 ? nullptr : text;
}

void MenuOverlayUi::renderTutorial(const MenuOverlayFrame& frame) const {
    if (frame.exploreContext || !frame.hatched || frame.toast) return;
    const char* hint = nullptr;
    if (frame.menuPage && !frame.navigationComplete) hint = "点选功能，上下滑动";
    else if (!frame.menuPage && frame.navigationComplete && !frame.backComplete)
        hint = "点击左上角返回";
    if (!hint) return;
    auto& c = PixelRenderer::canvas();
    const bool native = c.width() == 360;
    const int x = native ? 68 : 8, y = native ? 284 : 208;
    c.fillRoundRect(x, y, 224, 28, 5, PixelRenderer::rgb(23, 37, 49));
    MenuGrid::label(c, x + 112, y + 6, hint, 208, 0xffe0);
}
}  // namespace Ui
