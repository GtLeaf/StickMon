#include "ui/compact_touch/SocialUi.h"

#include "ui/PageProfile.h"

#include "hardware/Hal.h"
#include "ui/common/SocialUiContent.h"

namespace Ui {
SocialLayout SocialUi::layout() const { return {Ui::Page::WIDTH, Ui::Page::HEIGHT}; }

void SocialUi::render() {
    SocialFrame frame = host.socialFrame();
    frame.visuals = &state;
    SocialUiContent::draw(frame, layout());
}
}  // namespace Ui
