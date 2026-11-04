// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/TouchTutorial.h"

#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"

namespace Ui::RoundTutorial {
void explore(const ExploreTutorialFrame& frame) {
    if (!frame.text) return;
    // The round map has no walking caption, including first-run onboarding.
    if (frame.kind == ExploreTutorialKind::WALK) return;
    const char* text = frame.kind == ExploreTutorialKind::BATTLE ? "点击下方指令行动" :
        "点击下方打开菜单";
    // Keep map controls and battle commands visible and tappable.
    auto& c = PixelRenderer::canvas();
    const bool battle = frame.kind == ExploreTutorialKind::BATTLE;
    const int x = battle ? 44 : 68, y = battle ? 90 : 262;
    const int width = battle ? 144 : 224;
    c.fillRoundRect(x, y, width, 26, 5, PixelRenderer::rgb(23, 37, 49));
    if (battle) PixelRenderer::textSmooth(x + 8, y + 5, text, 0xffe0, 16);
    else MenuGrid::label(c, x + width / 2, y + 5, text, width - 16, 0xffe0);
}
}  // namespace Ui::RoundTutorial
