#include "ui/stick/ApplicationUi.h"

#include "hardware/Hal.h"
#include "presentation/TouchUi.h"
#include "ui/common/ApplicationUiContent.h"

namespace Ui {
ApplicationLayout ApplicationUi::layout() const {
    return {Hal::DISPLAY_W, Hal::DISPLAY_H, TouchUi::NAV_H};
}
void ApplicationUi::renderNavigation() const { ApplicationUiContent::drawNavigation(layout()); }
void ApplicationUi::renderResourceAlert(const ApplicationResourceAlertFrame& frame) const {
    ApplicationUiContent::drawResourceAlert(frame, layout());
}
void ApplicationUi::renderSceneFade(const ApplicationFadeFrame& frame) const {
    ApplicationUiContent::drawSceneFade(frame);
}
}  // namespace Ui
