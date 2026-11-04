#include "ui/common/ContactsUiContent.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TeamFlag.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
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

const char* bondLevelName(Game::Bond::Value value) {
    uint8_t level = static_cast<uint8_t>(Game::Bond::levelFor(value));
    return Ui::Storage::BOND_LEVELS[level - 1];
}

uint16_t bondLevelColor(Game::Bond::Value value) {
    switch (Game::Bond::levelFor(value)) {
    case Game::Bond::Level::AVERSE:
        return PixelRenderer::rgb(210, 82, 92);
    case Game::Bond::Level::DISTANT:
        return PixelRenderer::rgb(136, 144, 158);
    case Game::Bond::Level::ACQUAINTED:
        return PixelRenderer::rgb(150, 164, 178);
    case Game::Bond::Level::FAMILIAR:
        return PixelRenderer::rgb(105, 210, 150);
    case Game::Bond::Level::TRUSTED:
        return PixelRenderer::rgb(255, 216, 72);
    case Game::Bond::Level::CLOSE:
        return PixelRenderer::rgb(255, 126, 168);
    }
    return PixelRenderer::rgb(150, 164, 178);
}
const Species& speciesFor(const Game::MonsterRuntime& monster) {
    const Species* species = findSpecies(monster.speciesId);
    return species ? *species : starterSpecies();
}
}  // namespace

static void renderStorageInviteConfirmPopup(ContactsUiBase& page, int offsetY);
static void renderStorageActionPopup(ContactsUiBase& page, int offsetY);
static void renderStorageReleaseConfirmPopup(ContactsUiBase& page, int offsetY);

void renderContactsContent(ContactsUiBase& page, float scroll, const ContactsLayout& layout) {
    auto& c = PixelRenderer::canvas();
    const auto& state = page.state();
    uint8_t rowCount = state.storageCount + 1;
    if (page.storageCursor >= rowCount) page.storageCursor = rowCount - 1;

    c.fillRect(0, 0, Ui::Page::WIDTH, layout.contentHeight, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 18;
    const int LIST_Y_START = layout.listTop;
    const int LIST_BOTTOM = layout.contentHeight - 6;
    const int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 24;
    static constexpr int SEP_W = Ui::Page::WIDTH - ROW_X * 2;

    if (state.storageCount == 0) {
        PixelRenderer::text(74, 44 + layout.listTop - 6, Ui::Computer::STORAGE_EMPTY, 0x7BEF, 1);
    }

    c.setClipRect(0, LIST_Y_START, Ui::Page::WIDTH, VISIBLE_H);
    for (uint8_t i = 0; i < rowCount; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)scroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == page.storageCursor;
        bool inTeam = i < state.storageCount &&
            page.contactCapabilities(i).inTeam;
        bool unavailable = i < state.storageCount &&
            (inTeam || page.contactCapabilities(i).locked ||
             page.contactCapabilities(i).visiting);
        uint16_t color = unavailable
            ? PixelRenderer::rgb(92, 98, 108)
            : (selected ? PixelRenderer::rgb(255, 216, 72)
                        : PixelRenderer::rgb(241, 242, 232));
        if (selected) {
            c.fillRect(ROW_X, y + 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        }

        if (i < state.storageCount) {
            int8_t teamSlot = ContactRoster::teamSlotForContact(state, i);
            const Game::MonsterRuntime& mon = teamSlot >= 0
                ? state.team[teamSlot] : state.storage[i];
            const Species& species = speciesFor(mon);
            const char* bondLabel = bondLevelName(mon.bond);
            int bondX = Ui::Page::WIDTH - textPixelWidth(bondLabel) - 10;
            const int flagSpace = inTeam ? TeamFlag::width(1) + 4 : 0;
            const int nameWidth = std::min(textPixelWidth(species.name),
                std::max(0, bondX - TEXT_X - 4 - flagSpace));
            const int nameTop = std::max(y + 3, LIST_Y_START);
            const int nameBottom = std::min(y + 19, LIST_BOTTOM);
            c.setClipRect(TEXT_X, nameTop, nameWidth,
                          std::max(0, nameBottom - nameTop));
            PixelRenderer::text(TEXT_X, y + 3, species.name, color, 1);
            c.setClipRect(0, LIST_Y_START, Ui::Page::WIDTH, VISIBLE_H);
            if (inTeam) {
                TeamFlag::draw(c, TEXT_X + nameWidth + 4, y + 4);
            }
            PixelRenderer::text(MathUtil::max(TEXT_X + 82, bondX), y + 3,
                                bondLabel,
                                unavailable ? color
                                            : bondLevelColor(mon.bond),
                                1);
        } else {
            PixelRenderer::text(TEXT_X, y + 3, Ui::BACK, color, 1);
        }

        if (i + 1 < rowCount) {
            c.drawFastHLine(ROW_X, y + ROW_H - 1, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }
    c.clearClipRect();
    if (page.storageActionOpen && page.storageCursor < state.storageCount) {
        renderStorageActionPopup(page, layout.popupOffsetY);
        if (page.storageInviteConfirmOpen) renderStorageInviteConfirmPopup(page, layout.popupOffsetY);
        if (page.storageReleaseConfirmOpen) renderStorageReleaseConfirmPopup(page, layout.popupOffsetY);
    }
}

static void renderStorageInviteConfirmPopup(ContactsUiBase& page, int offsetY) {
    auto& c = PixelRenderer::canvas();
    const auto& state = page.state();
    if (page.storageCursor >= state.storageCount) return;
    const Species& species =
        speciesFor(state.storage[page.storageCursor]);

    static constexpr int POP_X = 18;
    const int POP_Y = 30 + offsetY;
    static constexpr int POP_W = 204;
    static constexpr int POP_H = 76;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H,
               PixelRenderer::rgb(241, 242, 232));

    char line[48];
    snprintf(line, sizeof(line), Ui::Storage::INVITE_CONFIRM_FMT,
             species.name);
    int lineX = POP_X + MathUtil::max(6, (POP_W - textPixelWidth(line)) / 2);
    PixelRenderer::text(lineX, POP_Y + 17, line,
                        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = page.storageInviteConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !page.storageInviteConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 62, POP_Y + 52, Ui::Storage::YES,
                        yesColor, 1);
    PixelRenderer::text(POP_X + 137, POP_Y + 52, Ui::Storage::NO,
                        noColor, 1);
}

static void renderStorageActionPopup(ContactsUiBase& page, int offsetY) {
    auto& c = PixelRenderer::canvas();
    uint8_t actionCount = page.actionCount();
    static constexpr int POP_X = 142;
    const int POP_Y = 20 + offsetY;
    static constexpr int POP_W = 78;
    int popH = 16 + actionCount * 21;
    c.fillRect(POP_X, POP_Y, POP_W, popH, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, popH, PixelRenderer::rgb(241, 242, 232));

    for (uint8_t i = 0; i < actionCount; ++i) {
        int y = POP_Y + 9 + i * 21;
        bool selected = i == page.storageActionCursor;
        if (selected) c.fillRect(POP_X + 5, y - 2, 4, 18, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(POP_X + 16, y, page.actionLabel(i),
                            selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232),
                            1);
    }
}

static void renderStorageReleaseConfirmPopup(ContactsUiBase& page, int offsetY) {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 25;
    const int POP_Y = 32 + offsetY;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 72;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    int lineW = textPixelWidth(Ui::Storage::DELETE_CONFIRM);
    int lineX = POP_X + (POP_W - lineW) / 2;
    if (lineX < POP_X + 6) lineX = POP_X + 6;
    PixelRenderer::text(lineX, POP_Y + 17, Ui::Storage::DELETE_CONFIRM,
                        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = page.storageReleaseConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !page.storageReleaseConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 60, POP_Y + 49, Ui::Storage::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 130, POP_Y + 49, Ui::Storage::NO, noColor, 1);
}

}  // namespace Ui
