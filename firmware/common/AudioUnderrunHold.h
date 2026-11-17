#pragma once

#include <cstdint>

namespace Stickmon {
// Keeps the codec/I2S open for a short while after the mixer drains, so a
// streaming underrun plays silence instead of an output close/reopen gap.
// A channel stopped on purpose (mute, lock, standby) cancels the hold.
class AudioUnderrunHold {
public:
    // About 300 ms at 256 samples per block and 16 kHz.
    static constexpr uint8_t HOLD_BLOCKS = 19;
    void played() { idleBlocks_ = 0; }
    void cancel() { idleBlocks_ = HOLD_BLOCKS; }
    // True when one more silent block should be written.
    bool holdNext() {
        if (idleBlocks_ >= HOLD_BLOCKS) return false;
        ++idleBlocks_;
        return true;
    }
private:
    uint8_t idleBlocks_ = HOLD_BLOCKS;
};
}  // namespace Stickmon
