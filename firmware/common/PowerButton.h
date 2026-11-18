#pragma once

#include <cstdint>

namespace Stickmon {

// Persist on a debounced press before a hardware power controller can cut
// supply. Confirm shutdown while held, but cut power only after release so the
// same held key cannot immediately wake the board. Ignore a held boot/wake key.
class PowerButton {
public:
    enum class Action { NONE, PRESSED, TOGGLE_LOCK, SHUTDOWN_HELD, SHUTDOWN };
    void begin(bool down, uint32_t now) {
        raw_ = stable_ = down;
        changedAt_ = pressedAt_ = now;
        armed_ = !down;
        shutdownHeld_ = false;
    }
    bool down() const { return stable_; }
    Action update(bool down, uint32_t now) {
        if (down != raw_) { raw_ = down; changedAt_ = now; }
        if (raw_ == stable_ || now - changedAt_ < debounceMs) {
            if (raw_ && stable_ && armed_ && !shutdownHeld_ && now - pressedAt_ >= holdMs) {
                shutdownHeld_ = true;
                return Action::SHUTDOWN_HELD;
            }
            return Action::NONE;
        }
        stable_ = raw_;
        if (stable_) {
            pressedAt_ = changedAt_;
            shutdownHeld_ = false;
            return armed_ ? Action::PRESSED : Action::NONE;
        }
        if (!armed_) { armed_ = true; return Action::NONE; }
        // Debounce processing can be late; classify by the observed edge times.
        return shutdownHeld_ || changedAt_ - pressedAt_ >= holdMs ? Action::SHUTDOWN : Action::TOGGLE_LOCK;
    }
    static constexpr uint32_t debounceMs = 30;
    static constexpr uint32_t holdMs = 2000;
private:
    bool raw_ = false, stable_ = false, armed_ = true;
    bool shutdownHeld_ = false;
    uint32_t changedAt_ = 0, pressedAt_ = 0;
};

}  // namespace Stickmon
