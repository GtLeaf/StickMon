// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/RoomControls.h"
#include "presentation/PixelRenderer.h"

namespace Ui::RoundRoom {
void drawMenuButton() {
    auto& canvas = PixelRenderer::canvas();
    const auto& rect = HOME_MENU_RECT;
    canvas.fillRoundRect(rect.x, rect.y, rect.width, rect.height, 6,
        PixelRenderer::rgb(27, 43, 51));
    canvas.drawRoundRect(rect.x, rect.y, rect.width, rect.height, 6,
        PixelRenderer::rgb(67, 97, 101));
    for (int row = 0; row < 3; ++row)
        canvas.fillRoundRect(rect.x + 9, rect.y + 11 + row * 6, 20, 3, 1,
            PixelRenderer::rgb(222, 234, 229));
}
}  // namespace Ui::RoundRoom
