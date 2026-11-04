#pragma once

#include "ui/common/MenuOverlayUiBase.h"

namespace Ui {
class MenuOverlayUi : public MenuOverlayUiBase {
public:
    MenuOverlayLayout layout() const override;
};
}  // namespace Ui
