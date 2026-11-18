// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "PowerButton.h"
#include "DisplayLockRequest.h"
#include "Cst816TouchState.h"
#include "StandbyTouch.h"

namespace Sdgoods {
// Owns native input routing independently of GPIO/I2C and panel side effects.
class PowerInputState {
public:
    using Action = Stickmon::PowerButton::Action;
    using Event = Platform::IInputDevice::TouchEvent;
    using Type = Platform::IInputDevice::TouchType;
    void begin(bool down, uint32_t now) { key_.begin(down, now); cancelTouch(now); }
    Action pollKey(bool down, uint32_t now, bool allowLock = true) {
        const Action action = key_.update(down, now);
        switch (action) {
        case Action::PRESSED:
            save_ = true; cancelTouch(now); break;
        case Action::TOGGLE_LOCK:
            if (allowLock && !shutdownPending_) {
                if (locked_) wake(now);
                else { lockRequest_.toggle(now); cancelTouch(now); }
            }
            break;
        case Action::SHUTDOWN_HELD:
            shutdownPending_ = true; save_ = true; setLocked(true, now); break;
        case Action::SHUTDOWN:
            shutdownPending_ = shutdown_ = true; setLocked(true, now); break;
        default: break;
        }
        return action;
    }
    bool needsTouchSample() const {
        return !key_.down() && !shutdownPending_ && !lockRequest_.pending();
    }
    bool sampleTouch(const uint8_t* data, uint32_t now) {
        if (!needsTouchSample()) { cancelTouch(now); return false; }
        if (locked_) {
            touch_.sample(nullptr, now);
            if (standby_.sample(data, now)) { wake(now); return true; }
            return false;
        }
        if (data && (data[0] & 15) == 0) touchBlocked_ = false;
        touch_.sample(touchBlocked_ ? nullptr : data, now);
        return false;
    }
    bool pollTouch(Event& event, uint32_t now) {
        if (cancelPending_) {
            cancelPending_ = false; event = {Type::CANCEL, 0, 0, now}; return true;
        }
        if (locked_ || !needsTouchSample()) return false;
        return touch_.poll(event);
    }
    void cancelTouch(uint32_t now) {
        touchBlocked_ = cancelPending_ = true;
        touch_.sample(nullptr, now); standby_.reset();
    }
    void setLocked(bool locked, uint32_t now) {
        locked_ = locked;
        lockRequest_.cancel();
        if (locked) unlocked_ = false;
        cancelTouch(now);
    }
    void requestLock(uint32_t now) { lockRequest_.request(now); cancelTouch(now); }
    bool lockRequestReady(uint32_t now) const { return lockRequest_.ready(now); }
    void completeLockRequest(uint32_t now) { setLocked(true, now); }
    bool locked() const { return locked_; }
    bool powerKeyHeld() const { return key_.down(); }
    bool shutdownPending() const { return shutdownPending_; }
    bool takeSaveRequest() { const bool result = save_; save_ = false; return result; }
    bool takeUnlockRequest() { const bool result = unlocked_; unlocked_ = false; return result; }
    bool takeShutdownRequest() { const bool result = shutdown_; shutdown_ = false; return result; }
    Cst816TouchState& touchDecoder() { return touch_; }
private:
    void wake(uint32_t now) {
        if (!locked_ || shutdownPending_) return;
        setLocked(false, now); unlocked_ = true;
    }
    Stickmon::PowerButton key_;
    DisplayLockRequest lockRequest_;
    Cst816TouchState touch_;
    StandbyTouch standby_;
    bool locked_ = false, shutdownPending_ = false;
    bool save_ = false, unlocked_ = false, shutdown_ = false;
    bool touchBlocked_ = true, cancelPending_ = false;
};
}  // namespace Sdgoods
