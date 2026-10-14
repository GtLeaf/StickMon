#pragma once

#include <cstdint>

#include "AmoledGeometry.h"

class Canvas565;

namespace AmoledV1 {

struct SettingsViewModel;

inline constexpr int SETTINGS_BRIGHTNESS_ITEM = 0;
inline constexpr int SETTINGS_VOLUME_ITEM = 1;
inline constexpr int SETTINGS_GAME_SPEED_ITEM = 2;
inline constexpr int SETTINGS_POWER_SAVE_ITEM = 3;
inline constexpr int SETTINGS_RESET_GAME_ITEM = 4;
inline constexpr int SETTINGS_SYSTEM_UPDATE_ITEM = 5;

inline constexpr int SETTINGS_SLIDER_LEFT = 148;
inline constexpr int SETTINGS_SLIDER_RIGHT = 332;
inline constexpr int SETTINGS_SLIDER_OFFSET_Y = 48;

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool settingsBackAt(int x, int y);
int settingsItemAt(int x, int y, bool systemUpdate = false);
int settingsResetChoiceAt(int x, int y);

}  // namespace AmoledV1
