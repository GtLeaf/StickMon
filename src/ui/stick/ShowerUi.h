#pragma once

#include "ui/common/ShowerUiBase.h"

namespace Ui {

class ShowerUi : public ShowerUiBase {
public:
    explicit ShowerUi(ShowerHost& host) : ShowerUiBase(host) {}
    ShowerLayout layout() const override;
    void render() override;
};

}  // namespace Ui
