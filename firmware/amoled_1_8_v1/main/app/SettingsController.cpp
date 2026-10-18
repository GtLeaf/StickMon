#include "app/SettingsController.h"

#include <algorithm>

#include "ui/SettingsScreens.h"

namespace AmoledV1 {

void SettingsController::reset() {
    resetConfirm_ = false;
    cancelTouch();
}

void SettingsController::cancelTouch() {
    pressedItem_ = 0xFF;
    sliderDragging_ = false;
    sliderChanged_ = false;
}

void SettingsController::showResetConfirmation() {
    cancelTouch();
    resetConfirm_ = true;
}

SettingsController::Change SettingsController::beginTouch(
    int x, int y, bool systemUpdate, Game::PlayerSettings& settings) {
    cancelTouch();
    const int item = resetConfirm_ ? settingsResetChoiceAt(x, y)
                                  : settingsItemAt(x, y, systemUpdate);
    pressedItem_ = item < 0 ? 0xFF : static_cast<uint8_t>(item);
    constexpr int TAP_SLOP = 12;
    sliderDragging_ = !resetConfirm_ && (item == 0 || item == 1) &&
        x >= SETTINGS_SLIDER_LEFT - TAP_SLOP &&
        x <= SETTINGS_SLIDER_RIGHT + TAP_SLOP;
    return sliderDragging_ ? setSlider(pressedItem_, x, settings) : Change::NONE;
}

SettingsController::Change SettingsController::setSlider(
    uint8_t item, int x, Game::PlayerSettings& settings) {
    if (item > SETTINGS_VOLUME_ITEM) return Change::NONE;
    const int position = std::clamp(x, SETTINGS_SLIDER_LEFT, SETTINGS_SLIDER_RIGHT);
    const int width = SETTINGS_SLIDER_RIGHT - SETTINGS_SLIDER_LEFT;
    const int minimum = item == SETTINGS_BRIGHTNESS_ITEM ? 32 : 0;
    const int maximum = item == SETTINGS_BRIGHTNESS_ITEM ? 255 : 100;
    const auto value = static_cast<uint8_t>(minimum +
        (position - SETTINGS_SLIDER_LEFT) * (maximum - minimum) / std::max(1, width));
    uint8_t& current = item == SETTINGS_BRIGHTNESS_ITEM
        ? settings.brightness : settings.volume;
    if (current == value) return Change::NONE;
    current = value;
    sliderChanged_ = true;
    return item == SETTINGS_BRIGHTNESS_ITEM ? Change::BRIGHTNESS : Change::VOLUME;
}

SettingsController::Change SettingsController::dragSlider(
    int x, Game::PlayerSettings& settings) {
    return sliderDragging_ ? setSlider(pressedItem_, x, settings) : Change::NONE;
}

SettingsController::Action SettingsController::tap(
    int x, int y, bool systemUpdate, bool busy, Game::PlayerSettings& settings) {
    if (settingsBackAt(x, y)) {
        if (!resetConfirm_) return Action::CLOSE;
        reset();
        return Action::REDRAW;
    }
    if (resetConfirm_) {
        const int choice = settingsResetChoiceAt(x, y);
        if (choice < 0 || choice != pressedItem_) return Action::NONE;
        if (choice == 0) {
            reset();
            return Action::REDRAW;
        }
        return busy ? Action::BUSY : Action::RESET_GAME;
    }
    const int item = settingsItemAt(x, y, systemUpdate);
    switch (item) {
    case SETTINGS_RESET_GAME_ITEM:
        if (busy) return Action::BUSY;
        showResetConfirmation();
        return Action::REDRAW;
    case SETTINGS_SYSTEM_UPDATE_ITEM:
        return busy ? Action::BUSY : Action::SYSTEM_UPDATE;
    case SETTINGS_BRIGHTNESS_ITEM:
    case SETTINGS_VOLUME_ITEM:
        setSlider(static_cast<uint8_t>(item), x, settings);
        return item == SETTINGS_BRIGHTNESS_ITEM ? Action::BRIGHTNESS : Action::VOLUME;
    case SETTINGS_GAME_SPEED_ITEM:
        settings.speedIndex = static_cast<uint8_t>((settings.speedIndex + 1) % 4);
        return Action::SPEED;
    case SETTINGS_POWER_SAVE_ITEM:
        settings.idleTimeoutIndex = static_cast<uint8_t>((settings.idleTimeoutIndex + 1) % 5);
        return Action::POWER_SAVE;
    default: return Action::NONE;
    }
}

SettingsViewModel SettingsController::model(
    const Game::GameState& state, bool systemUpdate, const char* toast) const {
    SettingsViewModel model;
    model.state = &state;
    model.systemUpdate = systemUpdate;
    model.resetConfirm = resetConfirm_;
    model.brightness = state.settings.brightness;
    model.volume = state.settings.volume;
    model.pressedItem = pressedItem_;
    model.toast = toast;
    return model;
}

}  // namespace AmoledV1
