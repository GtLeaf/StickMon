#pragma once

#include <cstdint>

#include "AmoledGeometry.h"

class Canvas565;

namespace AmoledV1 {

struct RoomMenuViewModel;
struct RoomFoodViewModel;

void renderRoomMenuScreen(Canvas565& canvas, const RoomMenuViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool roomMenuBackAt(int x, int y);
int roomMenuItemAt(int x, int y);
void renderRoomFoodScreen(Canvas565& canvas, const RoomFoodViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool roomFoodBackAt(int x, int y);
int roomFoodItemAt(int x, int y);

}  // namespace AmoledV1
