#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

class PixelCache565;

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
void renderShowerScreenCached(Canvas565& canvas, const ShowerViewModel& model,
                              PixelCache565& backgroundCache,
                              uint16_t rowBegin, uint16_t rowEnd,
                              PixelCache565* petCache = nullptr);
int showerPetGroundY(uint16_t speciesId);
bool showerBackAt(int x, int y);
int showerMenuItemAt(int x, int y);
int showerSoapItemAt(int x, int y);
int showerExitChoiceAt(int x, int y);
bool showerToolAt(int x, int y, int toolX, int toolY);

}  // namespace AmoledV1
