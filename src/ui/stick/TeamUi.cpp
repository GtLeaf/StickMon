#include "ui/common/TeamUi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/UiStrings.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "presentation/MenuGrid.h"
#include "presentation/TeamFlag.h"
#include "platform/api/FlashStorage.h"

namespace Ui {
namespace {
const Species& speciesFor(const Game::MonsterRuntime& monster) {
    const Species* species = findSpecies(monster.speciesId);
    return species ? *species : starterSpecies();
}
}  // namespace

void TeamUi::renderWithToast(const char* value) {
    auto& c = PixelRenderer::canvas();
    const auto& state = host.pageState();
    bool showingEgg = !state.oobeDone;
    uint8_t teamCount = showingEgg ? 1 : state.teamCount;
    uint8_t rowCount = teamCount + 1;
    if (teamCursor >= rowCount) teamCursor = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(8, 6, Ui::TEAM, PixelRenderer::rgb(67, 213, 224), 1);

    static constexpr int ROW_X = 8;
    static constexpr int ROW_Y = 24;
    static constexpr int ROW_W = Hal::DISPLAY_W - ROW_X * 2;
    static constexpr int ROW_H = 34;
    static constexpr int ROW_GAP = 5;
    for (uint8_t i = 0; i < rowCount; ++i) {
        int y = ROW_Y + i * (ROW_H + ROW_GAP);
        bool selected = i == teamCursor;
        if (i >= teamCount) {
            uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
            if (selected) c.fillRect(ROW_X, y + 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
            PixelRenderer::text(ROW_X + 16, y + 3, Ui::BACK, color, 1);
            continue;
        }

        if (showingEgg) {
            uint16_t border = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(72, 83, 98);
            c.drawRect(ROW_X, y, ROW_W, ROW_H, border);
            if (selected) c.fillRect(ROW_X, y, 4, ROW_H, PixelRenderer::rgb(255, 216, 72));
            if (!GameAssets::drawCentered(GameAssets::Kind::EGG, ROW_X + 25, y + ROW_H / 2)) {
                c.fillEllipse(ROW_X + 25, y + ROW_H / 2, 11, 15, PixelRenderer::rgb(240, 232, 184));
            }
            PixelRenderer::text(ROW_X + 50, y + 1, Ui::Status::EGG_TITLE,
                                PixelRenderer::rgb(241, 242, 232), 1);
            PixelRenderer::text(ROW_X + 50, y + 18, Ui::Status::EGG_STATE,
                                PixelRenderer::rgb(255, 216, 72), 1);
            continue;
        }

        const Game::MonsterRuntime& mon = state.team[i];
        const Species& species = speciesFor(mon);
        uint16_t border = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(72, 83, 98);
        c.drawRect(ROW_X, y, ROW_W, ROW_H, border);
        if (selected) c.fillRect(ROW_X, y, 4, ROW_H, PixelRenderer::rgb(255, 216, 72));

        PixelRenderer::text(ROW_X + 10, y + 4, species.name, PixelRenderer::rgb(241, 242, 232), 1);

        uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
        uint16_t hpColor = hpPct > 50 ? PixelRenderer::rgb(92, 222, 112) :
                           (hpPct > 20 ? PixelRenderer::rgb(255, 216, 72) :
                            PixelRenderer::rgb(239, 85, 85));
        int barX = ROW_X + 112;
        int barY = y + 7;
        int barW = ROW_W - 124;
        GameAssets::Kind statusIcon = GameAssets::statusKind(mon.majorStatus);
        if (statusIcon != GameAssets::Kind::COUNT) {
            GameAssets::drawCentered(statusIcon, barX - 8, barY + 4);
        }
        c.fillRect(barX, barY, barW, 8, PixelRenderer::rgb(82, 87, 95));
        int fillW = (barW * hpPct) / 100;
        if (fillW > 0) c.fillRect(barX, barY, fillW, 8, hpColor);
        c.drawRect(barX, barY, barW, 8, PixelRenderer::rgb(45, 48, 56));

        char buf[24];
        snprintf(buf, sizeof(buf), Ui::Status::LEVEL_FMT, mon.level);
        PixelRenderer::text(ROW_X + 10, y + 19, buf, PixelRenderer::rgb(135, 214, 238), 1);

        snprintf(buf, sizeof(buf), "%u/%u", mon.hpCur, mon.hpMax);
        PixelRenderer::text(ROW_X + 112, y + 19, buf, PixelRenderer::rgb(241, 242, 232), 1);
    }

    if (!showingEgg && teamActionOpen && teamCursor < teamCount) {
        renderActionPopup();
    }
    renderToast(value);
}

void TeamUi::renderActionPopup() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 142;
    static constexpr int POP_Y = 8;
    static constexpr int POP_W = 78;
    uint8_t actionCount = teamActionCount();
    int popH = 10 + actionCount * 22;
    c.fillRect(POP_X, POP_Y, POP_W, popH, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, popH, PixelRenderer::rgb(241, 242, 232));

    for (uint8_t i = 0; i < actionCount; ++i) {
        int y = POP_Y + 6 + i * 22;
        bool selected = i == teamActionCursor;
        if (selected) c.fillRect(POP_X + 5, y - 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(POP_X + 16, y, teamActionLabel(i),
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
}



bool TeamUi::onButton(const ButtonEvent& event) {
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        if (teamActionOpen) teamActionOpen = false;
        else host.pageBack();
        return true;
    }
    cancelTouch();
    const auto& state = host.pageState();
    const bool egg = !state.oobeDone;
    const uint8_t count = egg ? 1 : state.teamCount;
    const uint8_t rows = count + 1;
    if (teamCursor >= rows) teamCursor = 0;
    if (teamActionOpen && (egg || teamCursor >= count)) teamActionOpen = false;
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (teamActionOpen) teamActionCursor = (teamActionCursor + 1) % teamActionCount();
        else teamCursor = (teamCursor + 1) % rows;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        if (teamCursor >= count) {
            teamActionOpen = false;
            host.pageBack();
        } else if (egg) {
            teamActionOpen = false;
            host.openTeamStatus(0);
        } else if (!teamActionOpen) {
            teamActionOpen = true;
            teamActionCursor = 0;
        } else activateAction(teamActionAt(teamActionCursor));
        return true;
    }
    return false;
}

uint8_t TeamUi::teamActionCount() const {
    const auto& state = host.pageState();
    const bool first = teamCursor > 0 && teamCursor < state.teamCount;
    const bool leave = !host.pageExploreContext() && Features::Team::formalCount(state) > 1;
    return 3 + (first ? 1 : 0) + (leave ? 1 : 0);
}

TeamAction TeamUi::teamActionAt(uint8_t index) const {
    const auto& state = host.pageState();
    const bool first = teamCursor > 0 && teamCursor < state.teamCount;
    const bool leave = !host.pageExploreContext() && Features::Team::formalCount(state) > 1;
    uint8_t action = 0;
    if (index == action++) return TeamAction::STATUS;
    if (first && index == action++) return TeamAction::FIRST;
    if (index == action++) return TeamAction::MOVES;
    if (leave && index == action) return TeamAction::LEAVE;
    return TeamAction::BACK;
}

void TeamUi::cancelTouch() { Scene::cancelTouch(); }

bool TeamUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    return Scene::onTouch(event);
}

int TeamUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 0 || y >= TouchUi::CONTENT_H) return -1;
    if (teamActionOpen) {
        const int count = teamActionCount();
        if (x < 142 || x >= 220 || y < 12 || y >= 12 + count * 22) return -1;
        return 128 + (y - 12) / 22;
    }
    const int rows = (host.pageState().oobeDone ? host.pageState().teamCount : 1) + 1;
    const int row = (y - 24) / 39;
    return y >= 24 && row >= 0 && row < rows ? row : -1;
}

bool TeamUi::onTap(int target) {
    if (target < 0) return false;
    if (target >= 200) return false;
    if (target >= 128) {
        if (!teamActionOpen) return false;
        teamActionCursor = target - 128;
    } else teamCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

void TeamUi::renderToast(const char* value) {
    if (!value) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, value, PixelRenderer::rgb(255, 255, 255), 1);
}

}  // namespace Ui
