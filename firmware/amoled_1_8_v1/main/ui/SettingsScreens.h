#pragma once

#include <cstdint>

#include "AmoledGeometry.h"

class Canvas565;

namespace AmoledV1 {

struct SettingsViewModel;

inline constexpr int SETTINGS_SLIDER_LEFT = 148;
inline constexpr int SETTINGS_SLIDER_RIGHT = 332;
inline constexpr int SETTINGS_SLIDER_OFFSET_Y = 48;

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool settingsBackAt(int x, int y);
int settingsItemAt(int x, int y);

}  // namespace AmoledV1
