#pragma once

#include "ui/SceneSurface.h"

#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"

namespace Ui::HudControls {
inline MenuGrid::Rect menuBounds() {
    return {Ui::SceneSurface::width() - 60, Ui::SceneSurface::height() - 48, 36, 32};
}

inline void drawMenu() {
    auto& canvas = PixelRenderer::canvas();
    const auto rect = menuBounds();
    canvas.fillRoundRect(rect.x, rect.y, rect.width, rect.height, 6,
        PixelRenderer::rgb(27, 43, 51));
    canvas.drawRoundRect(rect.x, rect.y, rect.width, rect.height, 6,
        PixelRenderer::rgb(67, 97, 101));
    for (int row = 0; row < 3; ++row)
        canvas.fillRoundRect(rect.x + 9, rect.y + 10 + row * 5, 18, 2, 1,
            PixelRenderer::rgb(222, 234, 229));
}
}  // namespace Ui::HudControls
