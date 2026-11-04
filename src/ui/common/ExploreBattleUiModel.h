#pragma once

#include "assets/GameAssets.h"
#include "core/BuildConfig.h"
#include "game/BattleSystem.h"
#include "presentation/ExploreBattleTiming.h"
#include "presentation/MenuGrid.h"

namespace Ui {

enum class BattleSwitchVisualStage : uint8_t { NONE, RETREATING, ENTERING };

// Read-only battle data; action and reward timelines remain scene-owned.
struct ExploreBattleFrame {
    uint32_t nowMs = 0;
    GameAssets::Kind background = GameAssets::Kind::COUNT;
    const Species* wildSpecies = nullptr;
    const Species* playerSpecies = nullptr;
    uint16_t wildHp = 0, wildHpMax = 0;
    uint16_t playerHp = 0, playerHpMax = 0;
    uint8_t wildLevel = 0, playerLevel = 0;
    uint32_t playerExp = 0;
    Game::MajorStatus wildStatus = Game::MajorStatus::NONE;
    Game::MajorStatus playerStatus = Game::MajorStatus::NONE;
    BattleSystem::BattleActorState wildBattleState{}, playerBattleState{};
    bool battleTurnAnimating = false, battleTurnIdle = true;
    bool battleActionSelfHit = false, battleActionAttackerWild = false;
    Game::MoveId battleMoveId = 0;
    bool battleDefenderStatChanged = false;
    uint16_t battleHpFrom = 0, battleHpTo = 0;
    uint32_t battleActionStarted = 0;
    BattleSwitchVisualStage battleSwitchStage = BattleSwitchVisualStage::NONE;
    uint32_t battleSwitchStarted = 0;
    bool battleResultPending = false, battleExpVisible = false;
    bool expAnimationPending = false, expAnimationActive = false;
    uint32_t expAnimationFrom = 0, expAnimationTo = 0, expAnimationStarted = 0;
    bool foodThrowActive = false;
    uint8_t foodThrowIndex = 0;
    uint32_t foodThrowStarted = 0;
    bool battleLogActive = false;
    uint8_t battleLogVisibleCount = 0;
    const char* battleLogVisible[2] = {};
    bool encounterPhase = false, defeatAwaitInput = false;
    uint8_t battleCursor = 0;
    bool friendshipPromptVisible = false, friendshipConfirmYes = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool drawBounds = false;
#endif
};

struct ExploreBattleHudLayout {
    int nameX, nameY, levelRight;
    int statusX, statusY, hpX, hpY, hpWidth;
    MenuGrid::Rect panel{};
};

struct ExploreBattleLayout {
    int width, height;
    int playerNameY;
    int wildX, wildGroundY, wildMaxW, wildMaxH;
    int playerX, playerGroundY, playerMaxW, playerMaxH;
    int footerY, footerH;
    int commandInset;
    bool centeredPrompts;
    bool moveEffects;
    int expLabelX, expLabelVisualH, expBarX, expBarY, expBarW, expBarH, expLabelY;

    ExploreBattleHudLayout wildHud, playerHud;
    bool floatingControls = false;
    MenuGrid::Rect commands[4]{};
    MenuGrid::Rect message{};
    int friendshipRowHeight = 28;
    int foodStartX, foodStartY, foodEndX, foodEndY;

    constexpr ExploreBattleLayout(int width, int height, int commandInset = 0,
                                 bool centeredPrompts = false, int footerExtraHeight = 0,
                                 bool moveEffects = false)
        : width(width), height(height),
          playerNameY(66 + (height - 135) - footerExtraHeight),
          wildX(178 + (width - 240)), wildGroundY(66 + (height - 135) / 2),
          wildMaxW(116), wildMaxH(wildGroundY),
          playerX(52 + (width - 240) / 3),
          playerGroundY(footerExtraHeight > 0 ? height - 36 - footerExtraHeight
                                            : 107 + (height - 135)),
          playerMaxW(105), playerMaxH(65),
          footerY(height - 36 - footerExtraHeight), footerH(height - footerY),
          commandInset(commandInset), centeredPrompts(centeredPrompts), moveEffects(moveEffects),
          expLabelX(118 + (width - 240)), expLabelVisualH(9),
          expBarX(149 + (width - 240)), expBarY(85 + (height - 135) - footerExtraHeight),
          expBarW(81), expBarH(6),
          expLabelY(expBarY + expBarH / 2 - expLabelVisualH / 2),
          wildHud{width > 240 ? 16 : 6, height > 135 ? 14 : 5, width > 240 ? 124 : 114,
                  width > 240 ? 22 : 12, height > 135 ? 29 : 20,
                  width > 240 ? 38 : 28, height > 135 ? 32 : 23, 74, {}},
          playerHud{120 + width - 240, playerNameY, 228 + width - 240,
                    128 + width - 240, playerNameY + 16,
                    144 + width - 240, playerNameY + 19, 86, {}},
          foodStartX(78 + (width - 240) / 3), foodStartY(playerNameY + 17),
          foodEndX(166 + width - 240), foodEndY(48 + (height - 135) / 2) {}

    int commandTextX(int index) const {
        if (commandInset > 0) {
            const int cellWidth = (width - 2 * commandInset) / 4;
            return commandInset + cellWidth * index + (cellWidth - 32) / 2;
        }
        const int xs[] = {18, 74 + (width - 240) / 3,
                         126 + (width - 240) * 2 / 3, 190 + width - 240};
        return xs[index];
    }
    MenuGrid::Rect commandTarget(int index) const {
        if (floatingControls) return commands[index];
        const int cellWidth = (width - 2 * commandInset) / 4;
        return {commandInset + index * cellWidth + 2, footerY,
                cellWidth - 4, footerH};
    }
    MenuGrid::Rect friendshipPrompt() const {
        if (floatingControls) return {(width - 88) / 2, 106, 88, 99};
        return {centeredPrompts ? (width - 60) / 2 : width - 60,
                centeredPrompts ? (height - 67) / 2 : 34, 60, 67};
    }
};

}  // namespace Ui
