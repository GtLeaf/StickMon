#include "ui/stick/HatchUi.h"

#include "hardware/Hal.h"
#include "ui/common/HatchUiContent.h"

namespace Ui {
HatchLayout HatchUi::layout() const { return {Hal::DISPLAY_W, Hal::DISPLAY_H}; }
void HatchUi::render(const HatchFrame& frame) {
    HatchUiContent::draw(frame, layout(), notice());
}
}  // namespace Ui
