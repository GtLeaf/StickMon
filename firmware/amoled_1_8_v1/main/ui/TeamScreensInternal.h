#pragma once

#include <cstdint>

#include "game/Species.h"

class Canvas565;

namespace AmoledV1 {

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY);
void drawTeamMoveTypeBadge(Canvas565& canvas, int x, int y, TypeId type);
void drawMoveDetailContent(Canvas565& canvas, const MoveInfo& move,
                           int detailBase, uint8_t fade);

}  // namespace AmoledV1
