#pragma once
#include "ui/common/ProgressionUiBase.h"
namespace Ui {
class ProgressionUi : public ProgressionUiBase {
public:
    explicit ProgressionUi(ProgressionEffects& effects) : ProgressionUiBase(effects) {}
    ProgressionLayout layout() const override;
};
}  // namespace Ui
