#pragma once

#include <cstdint>

namespace Game {

class EvolutionSequence {
public:
    static constexpr uint32_t INTRO_MS = 600;
    static constexpr uint32_t MORPH_END_MS = 2840;
    static constexpr uint32_t FLASH_END_MS = 3180;
    static constexpr uint32_t REVEAL_END_MS = 3580;
    static constexpr uint32_t COMPLETE_MS = 4000;
    static constexpr uint32_t CANCEL_MORPH_MS = 650;
    static constexpr uint32_t CANCEL_COMPLETE_MS = 1000;

    enum class Phase : uint8_t {
        IDLE = 0,
        INTRO,
        MORPH,
        FLASH,
        REVEAL,
        COMPLETE,
        CANCEL_MORPH,
        CANCEL_REVEAL,
        CANCELLED,
    };

    void begin(uint16_t fromSpeciesId, uint16_t toSpeciesId,
               uint32_t nowMs) {
        fromSpeciesId_ = fromSpeciesId;
        toSpeciesId_ = toSpeciesId;
        startedAt_ = nowMs;
        cancellationStartedAt_ = 0;
        initialized_ = fromSpeciesId != 0 && toSpeciesId != 0 &&
                       fromSpeciesId != toSpeciesId;
        cancelling_ = false;
        cryPlayed_ = false;
    }

    void reset() { *this = EvolutionSequence{}; }

    bool matches(uint16_t fromSpeciesId, uint16_t toSpeciesId) const {
        return initialized_ && fromSpeciesId_ == fromSpeciesId &&
               toSpeciesId_ == toSpeciesId;
    }

    bool beginCancellation(uint32_t nowMs) {
        if (!initialized_ || cancelling_) return false;
        cancelling_ = true;
        cancellationStartedAt_ = nowMs;
        return true;
    }

    bool initialized() const { return initialized_; }
    bool cancelling() const { return cancelling_; }
    bool cryPlayed() const { return cryPlayed_; }
    void markCryPlayed() { cryPlayed_ = true; }
    uint16_t fromSpeciesId() const { return fromSpeciesId_; }
    uint16_t toSpeciesId() const { return toSpeciesId_; }

    uint32_t elapsed(uint32_t nowMs) const {
        return initialized_ ? nowMs - startedAt_ : 0;
    }

    uint32_t cancellationElapsed(uint32_t nowMs) const {
        return initialized_ && cancelling_ ? nowMs - cancellationStartedAt_ : 0;
    }

    bool animationComplete(uint32_t nowMs) const {
        return initialized_ && !cancelling_ && elapsed(nowMs) >= COMPLETE_MS;
    }

    bool cancellationComplete(uint32_t nowMs) const {
        return initialized_ && cancelling_ &&
               cancellationElapsed(nowMs) >= CANCEL_COMPLETE_MS;
    }

    Phase phase(uint32_t nowMs) const {
        if (!initialized_) return Phase::IDLE;
        if (cancelling_) {
            const uint32_t value = cancellationElapsed(nowMs);
            if (value < CANCEL_MORPH_MS) return Phase::CANCEL_MORPH;
            if (value < CANCEL_COMPLETE_MS) return Phase::CANCEL_REVEAL;
            return Phase::CANCELLED;
        }
        const uint32_t value = elapsed(nowMs);
        if (value < INTRO_MS) return Phase::INTRO;
        if (value < MORPH_END_MS) return Phase::MORPH;
        if (value < FLASH_END_MS) return Phase::FLASH;
        if (value < REVEAL_END_MS) return Phase::REVEAL;
        return Phase::COMPLETE;
    }

private:
    uint16_t fromSpeciesId_ = 0;
    uint16_t toSpeciesId_ = 0;
    uint32_t startedAt_ = 0;
    uint32_t cancellationStartedAt_ = 0;
    bool initialized_ = false;
    bool cancelling_ = false;
    bool cryPlayed_ = false;
};

}  // namespace Game
