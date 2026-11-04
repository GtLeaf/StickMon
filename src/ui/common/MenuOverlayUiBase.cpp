#include "ui/common/MenuOverlayUiBase.h"

#include "core/UiStrings.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "presentation/TutorialOverlay.h"

namespace Ui {
void MenuOverlayUiBase::renderBackground() const {
    const auto bounds = layout();
    PixelRenderer::canvas().fillRect(0, 0, bounds.width, bounds.contentHeight,
        PixelRenderer::rgb(7, 9, 14));
}

void MenuOverlayUiBase::renderToast(const MenuOverlayFrame& frame) const {
    const char* toast = frame.toast;
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    if (TouchUi::enabled() && frame.bodyNotice) {
        const auto body = TouchUi::pageBody();
        c.fillRoundRect(8, body.y + body.height - 72, body.width - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
        MenuGrid::label(c, body.width / 2, body.y + body.height - 67, toast, body.width - 32, 0xffff);
        return;
    }
    if (TouchUi::enabled() && frame.directTouchPage) {
        const auto bounds = layout();
        c.fillRoundRect(8, bounds.height - 72, bounds.width - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
        MenuGrid::label(c, bounds.width / 2, bounds.height - 67, toast, bounds.width - 32, 0xffff);
        return;
    }
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

void MenuOverlayUiBase::renderTutorial(const MenuOverlayFrame& frame) const {
    if (frame.touchAvailable || frame.exploreContext || !frame.hatched) return;
    if (frame.menuPage && !frame.navigationComplete) {
        TutorialOverlay::draw(TutorialOverlay::Button::B, Ui::Tutorial::MENU_NAV);
    } else if (!frame.menuPage && frame.navigationComplete && !frame.backComplete) {
        TutorialOverlay::draw(TutorialOverlay::Button::B, Ui::Tutorial::MENU_BACK, true);
    }
}

bool MenuOverlayUiBase::touchNavigation(bool directTouchPage) const {
    return !TouchUi::enabled() || !directTouchPage;
}
}  // namespace Ui
