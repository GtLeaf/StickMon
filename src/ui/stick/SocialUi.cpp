#include "ui/stick/SocialUi.h"

#include "hardware/Hal.h"
#include "ui/common/SocialUiContent.h"

namespace Ui {
SocialLayout SocialUi::layout() const { return {Hal::DISPLAY_W, Hal::DISPLAY_H}; }

void SocialUi::render() {
    SocialFrame frame = host.socialFrame();
    frame.visuals = &state;
    SocialUiContent::draw(frame, layout());
}
}  // namespace Ui
