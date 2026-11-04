#pragma once
#include "ui/common/DebugUiBase.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
namespace Ui {
class DebugUi : public DebugUiBase {
public:
    explicit DebugUi(DebugHost& host) : DebugUiBase(host) {}
    DebugLayout layout() const override;
    void render() override { renderDebugPage(); }
};
}  // namespace Ui
#endif
