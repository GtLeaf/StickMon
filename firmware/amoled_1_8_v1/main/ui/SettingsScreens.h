#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/SettingsLayout.h"

class Canvas565;

namespace AmoledV1 {

struct SettingsViewModel;

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool settingsBackAt(int x, int y);
int settingsItemAt(int x, int y, bool systemUpdate = false);
int settingsResetChoiceAt(int x, int y);

}  // namespace AmoledV1
