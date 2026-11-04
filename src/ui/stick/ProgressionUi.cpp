#include "ui/stick/ProgressionUi.h"
#include "hardware/Hal.h"
namespace Ui {
ProgressionLayout ProgressionUi::layout() const { return {Hal::DISPLAY_W, Hal::DISPLAY_H}; }
}  // namespace Ui
