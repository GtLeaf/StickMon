#pragma once
#include "ui/common/ProgressionUiBase.h"
namespace Ui {
class ProgressionUi : public ProgressionUiBase {
public:
    explicit ProgressionUi(ProgressionEffects& effects) : ProgressionUiBase(effects) {}
    ProgressionLayout layout() const override;
    MoveLearnAction handleMoveLearnTouch(MoveLearnState& state, const TouchTap::Event& event,
                                        const MoveLearnFrame& frame) override;
    bool handleLevelUpTouch(const TouchTap::Event& event) override;
    bool handleMoveReplacementTouch(const TouchTap::Event& event) override;
    void cancelMoveLearnTouch() override;
private:
    TouchTap learnTap;
    const Game::MonsterRuntime* touchMonster = nullptr;
    Game::MoveId touchMove = 0;
    uint16_t touchState = 0;
};
}  // namespace Ui
