#pragma once
#include <cstdint>
#include "game/GameState.h"
#include "game/Species.h"
namespace Ui {
enum class MoveLearnStage : uint8_t {
    PROMPT,
    REPLACEMENT,
};

struct MoveLearnState {
    MoveLearnStage stage = MoveLearnStage::PROMPT;
    uint8_t promptCursor = 0;
    uint8_t replacementSlot = 1;
    bool confirmOpen = false;
    bool confirmYes = false;
};

struct MoveLearnFrame {
    bool pending = false;
    bool needsReplacement = false;
    Game::MoveId moveId = 0;
    const Game::MonsterRuntime* monster = nullptr;
    const Species* species = nullptr;
};
enum class MoveLearnAction : uint8_t { NONE, COMPLETE, ACCEPT, DECLINE, REPLACE };
class ProgressionEffects {
public:
    virtual ~ProgressionEffects() = default;
    virtual void stopProgressionCry() = 0;
    virtual void playProgressionCry(uint16_t speciesId) = 0;
};
}  // namespace Ui
