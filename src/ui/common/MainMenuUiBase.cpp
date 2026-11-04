#include "ui/common/MainMenuUiBase.h"

namespace Ui {

int8_t MainMenuUiBase::lastCursor = 0;

void MainMenuUiBase::resetState(bool fromHome) {
    cursor = fromHome ? 0 : lastCursor;
}

bool MainMenuUiBase::advanceCursor() {
    if (!host.pageExploreContext()) host.pageNavigationTutorial();
    ++cursor;
    if (cursor >= ITEM_COUNT) {
        cursor = 0;
        return true;
    }
    return false;
}

bool MainMenuUiBase::handleButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        host.leaveMainMenu();
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED)
        return host.activateMainMenu(static_cast<uint8_t>(cursor));
    return false;
}

}  // namespace Ui
