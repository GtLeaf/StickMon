#include "ui/stick/DebugUi.h"
#if STICKMON_ENABLE_DEBUG_FEATURES
#include "hardware/Hal.h"
#include "presentation/TouchUi.h"
namespace Ui {
DebugLayout DebugUi::layout() const { return {Hal::DISPLAY_W, Hal::DISPLAY_H, TouchUi::CONTENT_H}; }
}  // namespace Ui
#endif
