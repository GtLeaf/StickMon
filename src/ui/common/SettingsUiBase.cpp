#include "ui/common/SettingsUiBase.h"

#include "core/UiStrings.h"
#include "hardware/Hal.h"

namespace Ui {

void SettingsUiBase::resetState() {
    cursor = 0;
    helpPage = 0;
    viewMode = ViewMode::MENU;
    resetConfirmYes = false;
}

SceneUpdateResult SettingsUiBase::updateNotice(uint32_t nowMs) {
    RenderDemand demand;
    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool SettingsUiBase::handleButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::RESET_CONFIRM) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
            resetConfirmYes = false;
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            resetConfirmYes = !resetConfirmYes;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (!resetConfirmYes) {
                viewMode = ViewMode::MENU;
                return true;
            }
            if (!host.resetSettingsGame()) {
                viewMode = ViewMode::MENU;
                toast = Ui::Settings::RESET_FAILED;
                toastUntil = Hal::ins().millis() + 1500;
                return true;
            }
            host.finishSettingsReset();
            return true;
        }
        return false;
    }

    if (viewMode == ViewMode::HELP) {
        static constexpr uint8_t HELP_PAGE_COUNT =
            sizeof(Ui::Settings::HELP_PAGE_TITLES) /
            sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            helpPage = (helpPage + 1) % HELP_PAGE_COUNT;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (helpPage + 1 == HELP_PAGE_COUNT) {
                host.replaySettingsTutorial();
                return true;
            }
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
            return true;
        }
        return false;
    }

    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        host.leaveSettings();
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateCurrent();
        return true;
    }
    return false;
}

void SettingsUiBase::activateCurrent() {
    toast = nullptr;
    toastUntil = 0;
    switch (cursor) {
    case BRIGHTNESS:
    case GAME_SPEED:
    case VOLUME:
    case POWER_SAVE:
        host.cycleSetting(static_cast<Item>(cursor));
        break;
    case HELP:
        viewMode = ViewMode::HELP;
        helpPage = 0;
        toast = nullptr;
        return;
    case RESET_GAME:
        viewMode = ViewMode::RESET_CONFIRM;
        resetConfirmYes = false;
        toast = nullptr;
        return;
    default:
        break;
    }
}

}  // namespace Ui
