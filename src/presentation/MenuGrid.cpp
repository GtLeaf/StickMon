#include "presentation/MenuGrid.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "assets/MenuAssets.h"
#include "core/AppSceneFlow.h"
#include "core/BuildConfig.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"

namespace MenuGrid {
Rect Layout::cell(int index, float scroll) const {
    return {viewport.x + left + (index % columns) * (cellWidth + gapX),
            viewport.y + top + (index / columns) * (cellHeight + gapY) - int(std::lround(scroll)),
            cellWidth, cellHeight};
}

int Layout::itemAt(int x, int y, float scroll, int count) const {
    if (!viewport.contains(x, y)) return -1;
    const int localX = x - viewport.x - left;
    const int localY = y - viewport.y - top + int(std::lround(scroll));
    if (localX < 0 || localY < 0) return -1;
    const int column = localX / (cellWidth + gapX);
    const int row = localY / (cellHeight + gapY);
    if (column >= columns || localX % (cellWidth + gapX) >= cellWidth ||
        localY % (cellHeight + gapY) >= cellHeight) return -1;
    const int index = row * columns + column;
    return index < count ? index : -1;
}

float Layout::maxScroll(int count) const {
    const int rows = (std::max(0, count) + columns - 1) / columns;
    return float(std::max(0, top + rows * (cellHeight + gapY) - viewport.height));
}

float Layout::reveal(int index, float scroll, int count) const {
    const auto rect = cell(index, scroll);
    if (rect.y < viewport.y) scroll -= viewport.y - rect.y;
    if (rect.y + rect.height > viewport.y + viewport.height)
        scroll += rect.y + rect.height - viewport.y - viewport.height;
    return std::clamp(scroll, 0.0f, maxScroll(count));
}

Layout fit(Rect viewport, int minimumWidth, int cellHeight,
           int gapX, int gapY, int padding, int top, int maximumWidth) {
    const int available = std::max(1, viewport.width - padding * 2);
    const int columns = std::max(1, (available + gapX) / (minimumWidth + gapX));
    int cellWidth = std::max(1, (available - (columns - 1) * gapX) / columns);
    if (maximumWidth > 0) cellWidth = std::min(cellWidth, maximumWidth);
    return {viewport, columns, cellWidth, cellHeight, gapX, gapY, padding, top};
}

Layout mainLayout(int width, int height, int textScale) {
    auto layout = fit({0, 0, width, height}, 82 * textScale,
        textScale == 2 ? 152 : 72, 8 * textScale,
        textScale == 2 ? 12 : 6, 8, textScale == 2 ? 0 : 6,
        textScale == 2 ? 164 : 0);
    layout.textScale = textScale;
    // Preserve the existing AMOLED drawing and hit-test profile.
    layout.drawOffsetY = textScale == 2 ? 2 : 0;
    return layout;
}

void card(Canvas565& canvas, Rect rect, bool pressed, bool selected) {
    const int radius = std::clamp(rect.height / 18, 4, 8);
    canvas.fillRoundRect(rect.x, rect.y, rect.width, rect.height, radius,
        PixelRenderer::rgb(pressed ? 42 : 24, pressed ? 61 : 34, pressed ? 68 : 42));
    const auto border = pressed ? PixelRenderer::rgb(115, 226, 183)
        : selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(56, 75, 84);
    canvas.drawRoundRect(rect.x, rect.y, rect.width, rect.height, radius, border);
}

void label(Canvas565& canvas, int centerX, int y, const char* text,
           int maxWidth, uint16_t color, int textScale) {
    char buffer[96] = {};
    int bytes = 0, width = 0;
    const auto* p = reinterpret_cast<const uint8_t*>(text);
    while (p && *p) {
        const int length = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : 4;
        const int glyphWidth = (*p < 0x80 ? 8 : 16) * textScale;
        if (width + glyphWidth > maxWidth || bytes + length >= int(sizeof(buffer))) break;
        std::memcpy(buffer + bytes, p, length);
        bytes += length; width += glyphWidth; p += length;
    }
    if (p && *p && width + 8 * textScale <= maxWidth && bytes + 1 < int(sizeof(buffer))) {
        buffer[bytes++] = '.'; width += 8 * textScale;
    }
    PixelRenderer::text(canvas, centerX - width / 2, y, buffer, color);
}

void mainMenu(Canvas565& canvas, const Layout& layout, int count,
              float scroll, int pressed, int selected) {
    for (int i = 0; i < count; ++i) {
        auto rect = layout.cell(i, scroll);
        if (rect.y + rect.height <= layout.viewport.y || rect.y >= layout.viewport.y + layout.viewport.height) continue;
        rect.y += layout.drawOffsetY;
        card(canvas, rect, i == pressed, i == selected);
        const auto entry = AppSceneFlow::mainMenuEntry(i, STICKMON_ENABLE_DEBUG_FEATURES != 0);
        if (entry.iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            const auto& frame = MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex];
            PixelRenderer::drawRgb565RleScaled(
                rect.x + (rect.width - MenuAssets::FRAME_W * layout.textScale) / 2,
                rect.y + (layout.textScale == 2 ? 12 : 4),
                MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                MenuAssets::MAIN_ICON_RLE, FlashStorage::readWord(&frame.offset),
                FlashStorage::readWord(&frame.length), float(layout.textScale));
        }
        label(canvas, rect.x + rect.width / 2,
              rect.y + (layout.textScale == 2 ? 106 : 50), entry.shortLabel,
              rect.width - 8, PixelRenderer::rgb(226, 238, 233), layout.textScale);
    }
}
}  // namespace MenuGrid
