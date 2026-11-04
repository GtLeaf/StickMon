#pragma once

#include "ui/common/ApplicationUiModel.h"

namespace Ui::ApplicationUiContent {
void drawNavigation(const ApplicationLayout& layout);
void drawResourceAlert(const ApplicationResourceAlertFrame& frame, const ApplicationLayout& layout);
void drawSceneFade(const ApplicationFadeFrame& frame);
}  // namespace Ui::ApplicationUiContent
