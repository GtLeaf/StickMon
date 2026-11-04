#include "ui/compact_touch/ApplicationUi.h"

#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/common/ApplicationUiContent.h"

namespace Ui {
ApplicationLayout ApplicationUi::layout() const {
    return {PixelRenderer::canvas().width(), PixelRenderer::canvas().height(), TouchUi::NAV_H};
}
void ApplicationUi::renderNavigation() const { ApplicationUiContent::drawNavigation(layout()); }
void ApplicationUi::renderResourceAlert(const ApplicationResourceAlertFrame& frame) const {
    ApplicationUiContent::drawResourceAlert(frame, layout());
}
void ApplicationUi::renderSceneFade(const ApplicationFadeFrame& frame) const {
    ApplicationUiContent::drawSceneFade(frame);
}
}  // namespace Ui
