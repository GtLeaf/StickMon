#include "core/ProgressionUi.h"

#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace ProgressionUi {

void renderLevelUp(uint8_t level) {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t background = PixelRenderer::rgb(18, 24, 32);
    const uint16_t panel = PixelRenderer::rgb(35, 42, 50);
    const uint16_t border = PixelRenderer::rgb(241, 242, 232);
    const uint16_t accent = PixelRenderer::rgb(255, 216, 72);
    const uint16_t hint = PixelRenderer::rgb(135, 214, 238);

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    canvas.fillRect(24, 26, 192, 86, panel);
    canvas.drawRect(24, 26, 192, 86, border);
    PixelRenderer::text(96, 40, Ui::Common::LEVEL_UP_TITLE, accent, 1);

    char line[32];
    snprintf(line, sizeof(line), Ui::Common::LEVEL_UP_FMT, level);
    PixelRenderer::text(82, 64, line, border, 1);
    PixelRenderer::text(92, 92, Ui::Common::A_CONTINUE, hint, 1);
}

void renderMoveLearn(uint8_t cursor) {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(18, 24, 32));
    canvas.fillRect(24, 26, 192, 86, PixelRenderer::rgb(35, 42, 50));
    canvas.drawRect(24, 26, 192, 86, PixelRenderer::rgb(241, 242, 232));

    const MoveInfo* move = findMove(GameEngine::ins().pendingMoveLearnId());
    PixelRenderer::text(46, 38, Ui::Explore::LEARN_TITLE, PixelRenderer::rgb(255, 216, 72), 1);

    char line[64];
    snprintf(line, sizeof(line), Ui::Explore::LEARN_MOVE_FMT,
             move ? move->name : Ui::Status::MOVE_UNKNOWN);
    PixelRenderer::text(36, 58, line, PixelRenderer::rgb(241, 242, 232), 1);

    const auto& state = GameEngine::ins().gameState();
    uint8_t slot = GameEngine::ins().pendingMoveLearnSlot();
    const MoveInfo* oldMove = nullptr;
    bool fillsSecondSpecialSlot = false;
    if (slot < state.teamCount && slot < Game::TEAM_CAP) {
        const Game::MonsterRuntime& mon = state.team[slot];
        if (mon.move2Id != 0 && mon.move3Id == 0) {
            fillsSecondSpecialSlot = true;
        } else if (mon.move2Id != 0 && mon.move3Id != 0) {
            oldMove = findMove(mon.move3Id);
        }
    }
    if (oldMove) {
        snprintf(line, sizeof(line), Ui::Explore::LEARN_REPLACE_FMT, oldMove->name);
        PixelRenderer::text(48, 76, line, PixelRenderer::rgb(135, 214, 238), 1);
    } else {
        PixelRenderer::text(64, 76,
                            fillsSecondSpecialSlot ? Ui::Explore::LEARN_EMPTY_SLOT_2
                                                   : Ui::Explore::LEARN_EMPTY_SLOT,
                            PixelRenderer::rgb(135, 214, 238), 1);
    }

    uint16_t yesColor = cursor == 0 ? PixelRenderer::rgb(255, 216, 72)
                                    : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = cursor == 1 ? PixelRenderer::rgb(255, 216, 72)
                                   : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(72, 96, Ui::Bag::YES, yesColor, 1);
    PixelRenderer::text(142, 96, Ui::Bag::NO, noColor, 1);
}

} // namespace ProgressionUi
