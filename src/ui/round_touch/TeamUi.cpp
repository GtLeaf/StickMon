#include "ui/common/TeamUi.h"
#include "ui/PageProfile.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/RoundPage.h"

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
int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

MenuGrid::Layout touchTeamLayout() {
    return MenuGrid::fit(MenuGrid::Rect{0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT}, Ui::Page::WIDTH - 16, 76, 0, 8, 8, 4);
}

MenuGrid::Rect touchTeamPopup(uint8_t slot, uint8_t count) {
    const int height = 8 + count * 40;
    const int desired = touchTeamLayout().cell(slot).y + 24;
    return {Ui::Page::WIDTH - 8 - 88,
        std::min(desired, Ui::Page::HEIGHT - height - 16), 88, height};
}

}  // namespace

void TeamUi::renderWithToast(const char* toastValue) {
    auto& c = PixelRenderer::canvas();
    const auto& state = host.pageState();
    const bool egg = !state.oobeDone;
    const uint8_t count = egg ? 1 : Features::Team::memberCount(state);
    if (teamCursor > count) teamCursor = 0;
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    for (uint8_t slot = 0; slot < count; ++slot) {
        const auto rect = touchTeamLayout().cell(slot);
        MenuGrid::card(c, rect, gridTouch.pressed() == slot, teamCursor == slot);
        if (egg) {
            GameAssets::drawCentered(GameAssets::Kind::EGG, rect.x + 32, rect.y + 38);
            PixelRenderer::text(rect.x + 72, rect.y + 14, Ui::Status::EGG_TITLE, 0xffff);
            PixelRenderer::text(rect.x + 72, rect.y + 44, Ui::Status::EGG_STATE, 0xffe0);
            continue;
        }
        const auto& mon = state.team[slot];
        const auto& species = speciesFor(mon);
        const auto* frame = PokemonSprites::findSpeciesSprite(mon.speciesId, PokemonSprites::SpriteKind::FRONT);
        bool drawn = false;
        if (frame) {
            const int width = FlashStorage::readByte(&frame->width);
            const int height = FlashStorage::readByte(&frame->height);
            const float scale = std::min(1.0f, std::min(56.0f / std::max(1, width), 60.0f / std::max(1, height)));
            drawn = PokemonSprites::drawFrameScaled(frame,
                rect.x + 32 - int(std::lround(width * scale)) / 2,
                rect.y + 38 - int(std::lround(height * scale)) / 2, scale);
        }
        if (!drawn) c.fillCircle(rect.x + 32, rect.y + 38, 20, PixelRenderer::rgb(42, 61, 68));
        MenuGrid::label(c, rect.x + 72 + (rect.width - 112) / 2, rect.y + 8, species.name, rect.width - 112, 0xffff);
        if (slot == 0) TeamFlag::draw(c, rect.x + rect.width - 22, rect.y + 8);
        const auto status = GameAssets::statusKind(mon.majorStatus);
        if (status != GameAssets::Kind::COUNT)
            GameAssets::drawCentered(status, rect.x + rect.width - 40, rect.y + 18);
        char value[24];
        std::snprintf(value, sizeof(value), "Lv%u", mon.level);
        PixelRenderer::text(rect.x + 72, rect.y + 30, value, PixelRenderer::rgb(126, 175, 175));
        const uint16_t displayedHp = host.teamItemHp(slot);
        std::snprintf(value, sizeof(value), "%u/%u", displayedHp, mon.hpMax);
        PixelRenderer::text(rect.x + rect.width - 12 - textPixelWidth(value), rect.y + 30, value, 0xffff);
        const int hp = std::min(100, int(uint32_t(displayedHp) * 100 / std::max<uint16_t>(1, mon.hpMax)));
        const int barWidth = rect.width - 84;
        c.fillRoundRect(rect.x + 72, rect.y + 56, barWidth, 8, 4, PixelRenderer::rgb(48, 63, 70));
        if (hp) c.fillRoundRect(rect.x + 72, rect.y + 56, barWidth * hp / 100, 8, 4,
            hp > 50 ? PixelRenderer::rgb(83, 184, 157) : hp > 20 ? 0xffe0 : 0xfaaa);
    }
    Ui::SceneSurface::pageHeader(Ui::TEAM, gridTouch.pressed() == 230);
    if (host.selectingTeamItem() && !toastValue)
        renderToast(Ui::Amoled::ITEM_TARGET);
    renderToast(toastValue);
    if (!egg && teamActionOpen && teamCursor < count) {
        const auto rect = touchTeamPopup(teamCursor, teamActionCount());
        c.fillRoundRect(rect.x, rect.y, rect.width, rect.height, 6, PixelRenderer::rgb(17, 27, 34));
        c.drawRoundRect(rect.x, rect.y, rect.width, rect.height, 6, PixelRenderer::rgb(115, 226, 183));
        for (uint8_t item = 0; item < teamActionCount(); ++item) {
            const int top = rect.y + 4 + item * 40;
            if (gridTouch.pressed() == 128 + item)
                c.fillRect(rect.x + 2, top, rect.width - 4, 40, PixelRenderer::rgb(42, 61, 68));
            PixelRenderer::text(rect.x + 12, top + 12, teamActionLabel(item), 0xffff);
            if (item + 1 < teamActionCount())
                c.drawFastHLine(rect.x + 12, top + 39, rect.width - 24, PixelRenderer::rgb(82, 117, 117));
        }
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
        } else if (host.selectingTeamItem()) {
            host.useTeamItemOn(teamCursor);
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
    uint8_t count = 0;
    while (teamActionAt(count) != TeamAction::BACK) ++count;
    return count;
}

TeamAction TeamUi::teamActionAt(uint8_t index) const {
    const auto capabilities = Features::Team::capabilities(
        host.pageState(), teamCursor, host.pageExploreContext());
    if (!capabilities.member) return TeamAction::BACK;
    uint8_t action = 0;
    if (capabilities.first && index == action++) return TeamAction::FIRST;
    if (index == action++) return TeamAction::STATUS;
    if (index == action++) return TeamAction::MOVES;
    if (teamCursor > 0 && capabilities.leave && index == action) return TeamAction::LEAVE;
    return TeamAction::BACK;
}

void TeamUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool TeamUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    float scroll = 0;
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), scroll, 0, false);
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int TeamUi::touchTarget(int16_t x, int16_t y) {
    if (teamActionOpen) {
        const auto rect = touchTeamPopup(teamCursor, teamActionCount());
        const int item = (y - rect.y - 4) / 40;
        if (rect.contains(x, y) && y >= rect.y + 4 && y < rect.y + rect.height - 4)
            return 128 + item;
        return 229;
    }
    if (RoundPage::NATIVE_BACK.contains(x + 60, y + 60)) return 230;
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const auto& state = host.pageState();
    return touchTeamLayout().itemAt(x, y, 0,
        state.oobeDone ? Features::Team::memberCount(state) : 1);
}

bool TeamUi::onTap(int target) {
    if (target < 0) return false;
    if (teamActionOpen) {
        if (target == 229) {
            teamActionOpen = false;
            cancelTouch();
            return true;
        }
        if (target < 128 || target >= 128 + teamActionCount()) return false;
    }
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
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
    const auto body = MenuGrid::Rect{0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT};
    c.fillRoundRect(8, body.y + body.height - 72, body.width - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
    MenuGrid::label(c, body.width / 2, body.y + body.height - 67, value, body.width - 32, 0xffff);
}

}  // namespace Ui
