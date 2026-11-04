#pragma once

#include "ui/common/ApplicationUiModel.h"

namespace Ui {
class ApplicationUi {
public:
    ApplicationLayout layout() const;
    void renderNavigation() const;
    void renderResourceAlert(const ApplicationResourceAlertFrame& frame) const;
    void renderSceneFade(const ApplicationFadeFrame& frame) const;
};
}  // namespace Ui
