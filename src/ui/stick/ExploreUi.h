#pragma once

#include "ui/common/ExploreUiBase.h"

namespace Ui {
class ExploreUi : public ExploreUiBase {
public:
    explicit ExploreUi(ExploreHost& host) : ExploreUiBase(host) {}
    void render() override;
};
}  // namespace Ui
