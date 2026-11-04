#include "ui/stick/ShowerUi.h"

#include "hardware/Hal.h"
#include "ui/common/ShowerUiContent.h"

namespace Ui {

ShowerLayout ShowerUi::layout() const {
    return ShowerLayout{Hal::DISPLAY_W, Hal::DISPLAY_H};
}

void ShowerUi::render() {
    ShowerFrame frame = host.showerFrame();
    frame.visuals = &state;
    ShowerUiContent::draw(frame, layout());
}

}  // namespace Ui
