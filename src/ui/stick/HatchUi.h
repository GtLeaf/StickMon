#pragma once

#include "ui/common/HatchUiBase.h"

namespace Ui {
class HatchUi : public HatchUiBase {
public:
    HatchLayout layout() const override;
    void render(const HatchFrame& frame) override;
};
}  // namespace Ui
