// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ExploreRouteUi.h"

#include <algorithm>
#include "core/AppSceneFlow.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/common/HomeUiContent.h"

namespace Ui::RoundExplore {
namespace {
// Q4 centerline offsets: radius 164.5px, -116 through -137 degrees.
// Mirror the left stroke for member two. No per-frame trigonometry or allocation.
struct ArcPoint { int x, y; };
constexpr int ARC_CENTER = 2872;  // 179.5 * 16
constexpr int ARC_RADIUS = 2632;  // 164.5 * 16
constexpr ArcPoint ARC_POINTS[] = {
    {-1154, -2366}, {-1208, -2339}, {-1261, -2310}, {-1314, -2281},
    {-1365, -2250}, {-1417, -2218}, {-1467, -2185}, {-1517, -2151},
    {-1566, -2116}, {-1614, -2079}, {-1661, -2042}, {-1707, -2003},
    {-1753, -1964}, {-1797, -1923}, {-1841, -1881}, {-1883, -1839},
    {-1925, -1795}
};
ArcPoint arcEnd(unsigned percent) {
    const unsigned step = percent * 16;
    const unsigned index = step / 100;
    if (index >= 16) return ARC_POINTS[16];
    const auto a = ARC_POINTS[index], b = ARC_POINTS[index + 1];
    return {a.x + (b.x - a.x) * int(step % 100) / 100,
        a.y + (b.y - a.y) * int(step % 100) / 100};
}
bool inArc(int x, int y, ArcPoint end, int halfWidth) {
    const auto start = ARC_POINTS[0];
    const int sx = x - start.x, sy = y - start.y;
    const int ex = x - end.x, ey = y - end.y;
    const int cap = halfWidth * halfWidth;
    if (sx * sx + sy * sy <= cap || ex * ex + ey * ey <= cap) return true;
    const int radius = x * x + y * y;
    return radius >= (ARC_RADIUS - halfWidth) * (ARC_RADIUS - halfWidth) &&
        radius <= (ARC_RADIUS + halfWidth) * (ARC_RADIUS + halfWidth) &&
        start.x * y - start.y * x <= 0 && x * end.y - y * end.x <= 0;
}
void drawHpArc(unsigned member, unsigned percent, uint16_t color, bool fainted) {
    auto& c = PixelRenderer::canvas();
    const auto outline = PixelRenderer::rgb(0, 0, 0);
    const auto outerEdge = PixelRenderer::rgb(255, 255, 255);
    const auto track = fainted ? PixelRenderer::rgb(65, 74, 79) :
        PixelRenderer::rgb(16, 41, 43);
    const auto end = arcEnd(percent);
    // The 7px body has a 1px black outline and a further 2px white outer edge,
    // separating HP from both light and dark terrain. Four subpixel samples
    // soften the round caps within two small corner rectangles.
    for (int y = 26; y < 74; ++y) for (int x = 53; x < 114; ++x) {
        const int px = member ? 359 - x : x;
        const auto background = c.readPixel(px, y);
        unsigned red = 0, green = 0, blue = 0;
        bool covered = false;
        for (int oy : {4, 12}) for (int ox : {4, 12}) {
            const int dx = x * 16 + ox - ARC_CENTER;
            const int dy = y * 16 + oy - ARC_CENTER;
            uint16_t sample = background;
            if (inArc(dx, dy, ARC_POINTS[16], 88)) {
                covered = true;
                sample = inArc(dx, dy, ARC_POINTS[16], 56) ? outline : outerEdge;
                if (inArc(dx, dy, ARC_POINTS[16], 40)) {
                    sample = percent && inArc(dx, dy, end, 40) ? color : track;
                }
            }
            red += (sample >> 11) & 31;
            green += (sample >> 5) & 63;
            blue += sample & 31;
        }
        if (covered) c.drawPixel(px, y,
            uint16_t(((red + 2) / 4 << 11) | ((green + 2) / 4 << 5) | ((blue + 2) / 4)));
    }
}
void panel(MenuGrid::Rect bounds) {
    HomeUiContent::fillRoundRectAlpha(bounds.x, bounds.y, bounds.width, bounds.height,
        7, PixelRenderer::rgb(23, 60, 51), 232);
    PixelRenderer::canvas().drawRoundRect(bounds.x, bounds.y, bounds.width,
        bounds.height, 7, PixelRenderer::rgb(134, 169, 148));
}
}  // namespace

bool routePoint(int x, int y) {
    const int dx = 2 * x - 359, dy = 2 * y - 359;
    return x >= 0 && x < 360 && y >= 0 && y < 360 &&
        dx * dx + dy * dy < 360 * 360;
}
bool routeHudHit(int x, int y, uint8_t members) {
    if (ROUTE_NAME.contains(x, y)) return true;
    if (!routePoint(x, y)) return false;
    for (unsigned i = 0; i < std::min<unsigned>(members, 2); ++i) {
        if (routeStatusSlot(i).contains(x, y)) return true;
        // Reserve a 28px touch band around the narrow visual stroke.
        const int px = i ? 359 - x : x;
        if (px >= 45 && px < 124 && y >= 17 && y < 82 &&
            inArc(px * 16 + 8 - ARC_CENTER, y * 16 + 8 - ARC_CENTER,
                ARC_POINTS[16], 224)) return true;
    }
    return false;
}
void drawRouteHud(const ExploreRouteFrame& frame) {
    auto& c = PixelRenderer::canvas();
    const auto foreground = PixelRenderer::rgb(247, 246, 233);
    // The native 20px subset gives each map-name glyph a fixed cell.
    unsigned count = 0;
    if (frame.mapName) for (const unsigned char* p =
        reinterpret_cast<const unsigned char*>(frame.mapName); *p; ++p)
        if ((*p & 0xc0) != 0x80) ++count;
    const int titleWidth = int(count) * 20;
    const bool showMenu = frame.menuAvailable && Platform::input().touchAvailable();
    const auto title = showMenu ? ROUTE_TITLE : ROUTE_NAME;
    panel(title);
    c.setClipRect(title.x, title.y, title.width, title.height);
    PixelRenderer::textSmooth(title.x + (title.width - titleWidth) / 2, 24,
        frame.mapName ? frame.mapName : "", foreground, 20);
    c.clearClipRect();
    const uint8_t members = std::min<uint8_t>(frame.teamHudCount, 2);
    for (uint8_t i = 0; i < members; ++i) {
        const auto& member = frame.teamHud[i];
        const unsigned percent = member.hpMax ?
            std::min<unsigned>(100, unsigned(member.hpCur) * 100 / member.hpMax) : 0;
        const auto color = member.fainted ? PixelRenderer::rgb(156, 174, 181) :
            percent > 50 ? PixelRenderer::rgb(92, 222, 112) :
            percent > 20 ? PixelRenderer::rgb(246, 204, 72) : PixelRenderer::rgb(232, 80, 84);
        drawHpArc(i, percent, color, member.fainted);
        const auto status = GameAssets::statusKind(member.status);
        if (status != GameAssets::Kind::COUNT) {
            const auto slot = routeStatusSlot(i);
            GameAssets::drawCentered(status, slot.x + slot.width / 2, slot.y + slot.height / 2);
        }
    }
    if (showMenu) {
        const auto button = ROUTE_MENU_BUTTON;
        c.fillRoundRect(button.x, button.y, button.width, button.height,
            8, PixelRenderer::rgb(23, 45, 50));
        c.drawRoundRect(button.x, button.y, button.width, button.height,
            8, PixelRenderer::rgb(107, 139, 132));
        const int x = button.x + (button.width - 18) / 2;
        const int centerY = button.y + button.height / 2;
        for (int y : {centerY - 7, centerY - 1, centerY + 5})
            c.fillRect(x, y, 18, 2, foreground);
    }
    if (!frame.exploreMenuOpen) return;
    c.fillRoundRect(ROUTE_PANEL.x, ROUTE_PANEL.y, ROUTE_PANEL.width,
        ROUTE_PANEL.height, 8, PixelRenderer::rgb(20, 25, 32));
    c.drawRoundRect(ROUTE_PANEL.x, ROUTE_PANEL.y, ROUTE_PANEL.width,
        ROUTE_PANEL.height, 8, PixelRenderer::rgb(190, 200, 205));
    for (uint8_t i = 0; i < AppSceneFlow::exploreMenuItemCount(); ++i) {
        const auto row = routeMenuItem(i);
        const bool selected = i == frame.exploreMenuCursor;
        const auto color = selected ? PixelRenderer::rgb(255, 216, 72) : foreground;
        if (selected) c.fillRect(row.x + 6, row.y + 10, 3, 24, color);
        PixelRenderer::textSmooth(row.x + 21, row.y + 11,
            Ui::Explore::SIDE_MENU_ITEMS[i], color, 20);
    }
}
}  // namespace Ui::RoundExplore
