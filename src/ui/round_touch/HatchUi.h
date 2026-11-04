// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/common/HatchUiBase.h"

namespace Ui {
class HatchUi : public HatchUiBase {
public:
    HatchLayout layout() const override { return {360, 360}; }
    void render(const HatchFrame& frame) override;
    int touchTarget(int16_t x, int16_t y) const;
    HatchAction onTap(int target, uint32_t nowMs);
};
}  // namespace Ui
