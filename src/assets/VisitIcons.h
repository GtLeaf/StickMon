#pragma once
#include <cstdint>
class Canvas565;
namespace VisitIcons {
enum class Kind : uint8_t { DOOR, PAWS, VISITOR, WARNING };
void draw(Canvas565 &canvas, Kind kind, int centerX, int top);
} // namespace VisitIcons
