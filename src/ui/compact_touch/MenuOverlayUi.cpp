#include "ui/compact_touch/MenuOverlayUi.h"

#include "ui/PageProfile.h"

#include "hardware/Hal.h"
#include "presentation/TouchUi.h"

namespace Ui {
MenuOverlayLayout MenuOverlayUi::layout() const {
    return {Ui::Page::WIDTH, Ui::Page::HEIGHT, TouchUi::CONTENT_H};
}
}  // namespace Ui
