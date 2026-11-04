#include "ui/compact_touch/ExploreUi.h"

#include <algorithm>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "ui/common/HomeUiContent.h"
#include "ui/compact_touch/ExploreHudUi.h"
#include "ui/compact_touch/HudControls.h"
#if STICKMON_UI_ROUND_TOUCH
#include "ui/round_touch/ExploreRouteUi.h"
#endif

namespace Ui {
bool ExploreUi::drawHud(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout) const {
#if STICKMON_UI_ROUND_TOUCH
    (void)layout;
    RoundExplore::drawRouteHud(frame);
    return true;
#else
    auto& canvas = PixelRenderer::canvas();
    const auto panelColor = PixelRenderer::rgb(8, 10, 14);
    const auto border = PixelRenderer::rgb(72, 83, 98);
    const auto text = PixelRenderer::rgb(245, 246, 232);
    const uint8_t count = std::min<uint8_t>(frame.teamHudCount, 2);
    if (count) {
        const int height = count > 1 ? 42 : 22;
        HomeUiContent::fillRoundRectAlpha(36, 12, 88, height, 5, panelColor, 150);
        canvas.drawRoundRect(36, 12, 88, height, 5, border);
    }
    for (uint8_t i = 0; i < count; ++i) {
        const auto& member = frame.teamHud[i];
        const unsigned percent = member.hpMax == 0 ? 0 :
            std::min<unsigned>(100, unsigned(member.hpCur) * 100 / member.hpMax);
        const auto color = member.fainted ? PixelRenderer::rgb(156, 174, 181) :
            percent > 50 ? PixelRenderer::rgb(92, 222, 112) :
            percent > 20 ? PixelRenderer::rgb(246, 204, 72) : PixelRenderer::rgb(232, 80, 84);
        const int rowY = 14 + i * 20;
        canvas.drawRect(44, rowY + 6, 52, 6, PixelRenderer::rgb(220, 224, 218));
        canvas.fillRect(45, rowY + 7, 50, 4, PixelRenderer::rgb(39, 45, 50));
        const int fill = int(50 * percent / 100);
        if (fill) canvas.fillRect(45, rowY + 7, fill, 4, color);
        const auto status = GameAssets::statusKind(member.status);
        if (status != GameAssets::Kind::COUNT) GameAssets::draw(status, 104, rowY + 2);
    }

    const int mapX = layout.width - 100;
    HomeUiContent::fillRoundRectAlpha(mapX, 12, 72, 22, 5, panelColor, 150);
    canvas.drawRoundRect(mapX, 12, 72, 22, 5, border);
    MenuGrid::label(canvas, mapX + 36, 15, frame.mapName, 60, text);
    if (frame.menuAvailable && Platform::input().touchAvailable()) HudControls::drawMenu();

    if (frame.exploreMenuOpen) {
        const auto panel = ExploreHudUi::menuBounds();
        canvas.fillRoundRect(panel.x, panel.y, panel.width, panel.height, 6,
            PixelRenderer::rgb(20, 25, 32));
        canvas.drawRoundRect(panel.x, panel.y, panel.width, panel.height, 6,
            PixelRenderer::rgb(190, 200, 205));
        for (uint8_t i = 0; i < AppSceneFlow::exploreMenuItemCount(); ++i) {
            const auto row = ExploreHudUi::menuItem(i);
            const bool selected = i == frame.exploreMenuCursor;
            const auto color = selected ? PixelRenderer::rgb(255, 216, 72) : text;
            if (selected) canvas.fillRoundRect(row.x + 2, row.y + 7, 3, 18, 1, color);
            MenuGrid::label(canvas, row.x + row.width / 2 + 3, row.y + 8,
                Ui::Explore::SIDE_MENU_ITEMS[i], row.width - 16, color);
        }
    }
    return true;
#endif
}
}  // namespace Ui
