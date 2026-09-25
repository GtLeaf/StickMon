#include <cstdint>

#include "game/EvolutionSequence.h"

int main() {
    using Phase = Game::EvolutionSequence::Phase;

    Game::EvolutionSequence sequence;
    if (sequence.initialized() || sequence.phase(0) != Phase::IDLE) return 1;

    constexpr uint32_t start = 100;
    sequence.begin(25, 26, start);
    if (!sequence.initialized() || !sequence.matches(25, 26)) return 2;
    if (sequence.phase(start) != Phase::INTRO) return 3;
    if (sequence.phase(start + Game::EvolutionSequence::INTRO_MS) !=
        Phase::MORPH) return 4;
    if (sequence.phase(start + Game::EvolutionSequence::MORPH_END_MS) !=
        Phase::FLASH) return 5;
    if (sequence.phase(start + Game::EvolutionSequence::FLASH_END_MS) !=
        Phase::REVEAL) return 6;
    if (sequence.animationComplete(
            start + Game::EvolutionSequence::COMPLETE_MS - 1)) return 7;
    if (!sequence.animationComplete(
            start + Game::EvolutionSequence::COMPLETE_MS)) return 8;

    sequence.begin(25, 26, start);
    constexpr uint32_t cancelStart = 900;
    if (!sequence.beginCancellation(cancelStart)) return 9;
    if (sequence.beginCancellation(cancelStart + 1)) return 10;
    if (sequence.phase(cancelStart) != Phase::CANCEL_MORPH) return 11;
    if (sequence.phase(cancelStart +
                       Game::EvolutionSequence::CANCEL_MORPH_MS) !=
        Phase::CANCEL_REVEAL) return 12;
    if (sequence.cancellationComplete(
            cancelStart + Game::EvolutionSequence::CANCEL_COMPLETE_MS - 1)) {
        return 13;
    }
    if (!sequence.cancellationComplete(
            cancelStart + Game::EvolutionSequence::CANCEL_COMPLETE_MS)) {
        return 14;
    }
    if (sequence.phase(cancelStart +
                       Game::EvolutionSequence::CANCEL_COMPLETE_MS) !=
        Phase::CANCELLED) return 15;
    if (sequence.animationComplete(cancelStart + 10000)) return 16;

    sequence.reset();
    if (sequence.initialized() || sequence.phase(10000) != Phase::IDLE) {
        return 17;
    }
    return 0;
}
