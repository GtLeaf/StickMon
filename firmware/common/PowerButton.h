#pragma once

#include <cstdint>

namespace Stickmon {

// Persist on a debounced press before a hardware power controller can cut
// supply. Lock/sleep actions occur on release; a held wake key is ignored.
class PowerButton {
public:
    enum class Action { NONE, PRESSED, TOGGLE_LOCK, SHUTDOWN };
    void begin(bool down, uint32_t now) {
        raw_ = stable_ = down;
        changedAt_ = pressedAt_ = now;
        armed_ = !down;
    }
    bool down() const { return stable_; }
    Action update(bool down, uint32_t now) {
        if (down != raw_) { raw_ = down; changedAt_ = now; }
        if (raw_ == stable_ || now - changedAt_ < debounceMs) return Action::NONE;
        stable_ = raw_;
        if (stable_) {
            pressedAt_ = now;
            return armed_ ? Action::PRESSED : Action::NONE;
        }
        if (!armed_) { armed_ = true; return Action::NONE; }
        return now - pressedAt_ >= holdMs ? Action::SHUTDOWN : Action::TOGGLE_LOCK;
    }
    static constexpr uint32_t debounceMs = 30;
    static constexpr uint32_t holdMs = 2000;
private:
    bool raw_ = false, stable_ = false, armed_ = true;
    uint32_t changedAt_ = 0, pressedAt_ = 0;
};

}  // namespace Stickmon
