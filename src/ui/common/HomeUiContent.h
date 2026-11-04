#pragma once

#include "core/RoomRenderer.h"
#include "ui/common/HomeUiModel.h"

namespace Ui::HomeUiContent {

struct Camera { float x = 0, y = 0; };

int16_t screenX(float x, const Camera& camera);
int16_t screenY(float y, const Camera& camera);
int16_t spriteGroundY(float y, float groundOffset,
                     const PokemonSprites::SpriteFrame* frame, const Camera& camera);
void drawLayer(HomeLayer layer, const HomeFrame& frame, const Camera& camera);
int touchTarget(int16_t x, int16_t y, const HomeFrame& frame,
                const Camera& camera, bool menuHit);
void fillRoundRectAlpha(int x, int y, int w, int h, int radius,
                        uint16_t color, uint8_t alpha);
void drawHudRow(const HomeHudRow& row, int statusX, int rowY);
void drawTutorial(const HomeFrame& frame);
void drawContact(const HomeFrame& frame, int x = 5, int bottom = 131, int width = 230);

}  // namespace Ui::HomeUiContent
