#include "ui/common/TeamUi.h"
#include "core/UiStrings.h"

namespace Ui {

void TeamUi::reset() {
    teamCursor = teamActionCursor = 0;
    teamActionOpen = false;
    cancelTouch();
}

const char* TeamUi::teamActionLabel(uint8_t index) const {
    switch (teamActionAt(index)) {
    case TeamAction::STATUS: return Ui::Team::ACTION_STATUS;
    case TeamAction::FIRST: return Ui::Team::ACTION_FIRST;
    case TeamAction::MOVES: return Ui::Team::ACTION_MOVES;
    case TeamAction::LEAVE: return Ui::Team::ACTION_LEAVE;
    default: return Ui::Team::ACTION_BACK;
    }
}

void TeamUi::activateAction(TeamAction action) {
    teamActionOpen = false;
    if (action == TeamAction::STATUS) host.openTeamStatus(teamCursor);
    else if (action == TeamAction::MOVES) host.openTeamMoves(teamCursor);
    else if (action == TeamAction::FIRST) {
        if (host.makeTeamFirst(teamCursor)) {
            teamCursor = 0;
            host.pageNotice(Ui::Menu::SWITCH_TOAST);
        }
    } else if (action == TeamAction::LEAVE) {
        const bool contactsFull = host.pageState().storageCount >= Game::STORAGE_CAP;
        const bool lastMonster = host.pageState().teamCount <= 1;
        if (host.leaveTeam(teamCursor)) {
            const auto count = host.pageState().teamCount;
            if (count == 0) teamCursor = 0;
            else if (teamCursor >= count) teamCursor = count - 1;
            host.pageNotice(Ui::Team::LEAVE_TOAST);
        } else if (contactsFull) host.pageNotice(Ui::Team::CONTACTS_FULL_TOAST);
        else if (lastMonster) host.pageNotice(Ui::Team::LEAVE_LAST_TOAST);
    }
}

}  // namespace Ui
