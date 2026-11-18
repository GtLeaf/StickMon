#pragma once

#include "core/Scene.h"
#include "ui/common/ExploreRouteUiModel.h"
#include "ui/common/ExploreBattleUiModel.h"

namespace Ui {

enum class ExplorePhase : uint8_t {
    SELECT, WALKING, ENCOUNTER, LEVEL_UP, EVOLUTION, EVOLUTION_CANCELLED,
    LEARN_MOVE, MOVE_REPLACED, FRIENDSHIP, SPECIAL_PROMPT, PICKUP, RESULT,
    EXITING, ENDING
};

enum class ExploreAction : uint8_t {
    RETURN_HOME, RESUME_WALK, FRIENDSHIP_CONFIRM, SPECIAL_CONFIRM,
    MENU_ITEM, OPEN_MENU, EXIT, AREA_BACK, DEPART, AREA_CHANGED, WALK,
    BATTLE_DEFEAT, BATTLE_COMMAND, BATTLE_CURSOR, RESULT_CONTINUE, DEBUG_RETURN,
    MENU_RETURN_PAUSE
};

struct ExploreUiState {
    uint8_t areaCursor = 0;
    float areaAnimCursor = 0;
    uint32_t areaPreviewVisualCycle = UINT32_MAX;
    uint8_t battleCursor = 0, exploreMenuCursor = 0;
    bool friendshipConfirmYes = true, specialChallengeYes = true;
};

struct ExploreInputFrame {
    ExplorePhase phase = ExplorePhase::SELECT;
    uint8_t areaCount = 0, visibleAreaCount = 0;
    bool friendshipConfirmation = false;
    bool iceSliding = false, menuOpen = false, routeBlocked = false;
    bool battleBusy = false, defeatAwaitInput = false, debugBattle = false;
};

enum class ExploreTutorialKind : uint8_t { WALK, MENU, BATTLE };
struct ExploreTutorialFrame {
    const char* text = nullptr;
    uint8_t button = 0;
    ExploreTutorialKind kind = ExploreTutorialKind::WALK;
};

struct ExploreCursorUpdate {
    bool moved = false, animating = false, redraw = false;
};

class ExploreHost {
public:
    virtual ~ExploreHost() = default;
    virtual ExploreInputFrame exploreInputFrame() const = 0;
    virtual ExploreRouteFrame exploreRouteFrame() const = 0;
    virtual ExploreBattleFrame exploreBattleFrame() const = 0;
    virtual ExploreTutorialFrame exploreTutorialFrame() const = 0;
    virtual bool activateExplore(ExploreAction action, uint8_t value = 0) = 0;
    virtual bool handleExploreProgressionButton(const ButtonEvent& event) = 0;
    virtual void renderExploreProgression() = 0;
};

}  // namespace Ui
