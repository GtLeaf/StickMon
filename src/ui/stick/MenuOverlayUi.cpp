#include "ui/stick/MenuOverlayUi.h"

#include "hardware/Hal.h"
#include "presentation/TouchUi.h"

namespace Ui {
MenuOverlayLayout MenuOverlayUi::layout() const {
    return {Hal::DISPLAY_W, Hal::DISPLAY_H, TouchUi::CONTENT_H};
}
}  // namespace Ui
