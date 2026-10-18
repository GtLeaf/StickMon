#pragma once

#include <cstdint>

#include "game/GameState.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Page-local gestures and decisions. The application executes lifecycle actions
// and applies returned setting changes to the physical display/audio devices.
class SettingsController {
public:
    enum class Change : uint8_t { NONE, BRIGHTNESS, VOLUME };
    enum class Action : uint8_t {
        NONE, REDRAW, CLOSE, BRIGHTNESS, VOLUME, SPEED, POWER_SAVE,
        RESET_GAME, SYSTEM_UPDATE, BUSY,
    };

    void reset();
    void cancelTouch();
    void invalidatePress() { pressedItem_ = 0xFF; }
    void showResetConfirmation();
    bool resetConfirm() const { return resetConfirm_; }
    uint8_t pressedItem() const { return pressedItem_; }
    bool sliderDragging() const { return sliderDragging_; }
    bool sliderChanged() const { return sliderChanged_; }

    Change beginTouch(int x, int y, bool systemUpdate,
                      Game::PlayerSettings& settings);
    Change dragSlider(int x, Game::PlayerSettings& settings);
    Action tap(int x, int y, bool systemUpdate, bool busy,
               Game::PlayerSettings& settings);
    SettingsViewModel model(const Game::GameState& state, bool systemUpdate,
                            const char* toast) const;

private:
    Change setSlider(uint8_t item, int x, Game::PlayerSettings& settings);
    uint8_t pressedItem_ = 0xFF;
    bool resetConfirm_ = false;
    bool sliderDragging_ = false;
    bool sliderChanged_ = false;
};

}  // namespace AmoledV1
