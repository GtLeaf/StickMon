#pragma once

#include "core/AudioCues.h"

namespace Ui {
// One semantic sound per completed action. A save outcome replaces any tap
// feedback before the application consumes it; rendering never plays audio.
class MarketUiFeedback {
public:
    bool takeSfx(SfxCue& cue) {
        if (!pending_) return false;
        cue = cue_; pending_ = false; return true;
    }
    void clear() { pending_ = false; }
    void set(SfxCue cue) { cue_ = cue; pending_ = true; }
private:
    bool pending_ = false;
    SfxCue cue_ = SfxCue::UI_CONFIRM;
};
} // namespace Ui
