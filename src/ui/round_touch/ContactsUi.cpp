#include "ui/compact_touch/ContactsUi.h"

#include "ui/PageProfile.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/RoundPage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/PokemonSprites.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/Species.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TeamFlag.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
constexpr int TARGET_BACK = 230;
constexpr int DISMISS = 231;
constexpr int ACTION_ROW_HEIGHT = 40;
constexpr int ACTION_PADDING = 6;
constexpr int WIDTH = 360;
constexpr int HEIGHT = 360;

int textWidth(const char* text) {
    int width = 0;
    for (const auto* p = reinterpret_cast<const uint8_t*>(text); p && *p; ++p)
        if ((*p & 0xc0) != 0x80) width += *p < 0x80 ? 8 : 16;
    return width;
}

uint16_t bondColor(Game::Bond::Value bond) {
    switch (Game::Bond::levelFor(bond)) {
    case Game::Bond::Level::AVERSE: return PixelRenderer::rgb(239, 143, 148);
    case Game::Bond::Level::DISTANT: return PixelRenderer::rgb(126, 145, 145);
    case Game::Bond::Level::ACQUAINTED: return PixelRenderer::rgb(135, 180, 198);
    case Game::Bond::Level::FAMILIAR: return PixelRenderer::rgb(115, 226, 183);
    case Game::Bond::Level::TRUSTED: return PixelRenderer::rgb(248, 210, 105);
    case Game::Bond::Level::CLOSE: return PixelRenderer::rgb(255, 126, 168);
    }
    return 0xffff;
}

void avatar(uint16_t speciesId, int centerX, int centerY) {
    const auto* frame = PokemonSprites::findSpeciesSprite(speciesId, PokemonSprites::SpriteKind::ICON_0);
    if (!frame) return;
    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    if (!width || !height) return;
    const float scale = std::min(4.0f, 56.0f / std::max(width, height));
    const auto bounds = PokemonSprites::frameVisibleBounds(frame);
    if (!bounds.height) return;
    // Center the visible sprite rather than its transparent canvas padding.
    const int top = int(bounds.top * scale);
    const int bottom = int(std::ceil((bounds.top + bounds.height) * scale));
    PokemonSprites::drawFrameScaled(frame, centerX - int(std::lround(width * scale)) / 2,
        centerY - (top + bottom) / 2, scale);
}
}  // namespace

MenuGrid::Layout ContactsUi::listLayout() const {
    return MenuGrid::fit({48, 62, 264, 246}, 264, 72, 0, 10, 0);
}

MenuGrid::Rect ContactsUi::actionBounds() const {
    const int height = actionCount() * ACTION_ROW_HEIGHT + ACTION_PADDING * 2;
    const auto row = listLayout().cell(storageCursor, storageScroll);
    return {180, std::clamp(row.y + row.height / 2 - height / 2, 62,
        300 - height), 132, height};
}

MenuGrid::Rect ContactsUi::confirmBounds() const {
    return {56, 116, 248, 136};
}

MenuGrid::Rect ContactsUi::confirmChoice(bool yes) const {
    const auto panel = confirmBounds();
    const int width = (panel.width - 36) / 2;
    return {panel.x + 12 + (yes ? 0 : width + 12), panel.y + 80, width, 44};
}

void ContactsUi::reset() {
    resetState();
    storageScroll = 0;
    revealCursor = true;
    cancelTouch();
}

void ContactsUi::enter() {
    cancelTouch();
    enterState();
    closePopups();
    storageScroll = 0;
    revealCursor = true;
}

SceneUpdateResult ContactsUi::update(uint32_t, float) {
    const auto layout = listLayout();
    const auto count = listItemCount();
    storageCursor = count ? std::min<uint8_t>(storageCursor, count - 1) : 0;
    const float maximum = layout.maxScroll(count);
    const float target = revealCursor && !listTouch.active()
        ? layout.reveal(storageCursor, storageScroll, count)
        : std::clamp(storageScroll, 0.0f, maximum);
    const auto step = UiMotion::lerp(storageScroll, target, 0.25f, 0.5f);
    RenderDemand demand;
    demand.changed(step.changed);
    demand.animate(step.active);
    return demand.result();
}

void ContactsUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, WIDTH, HEIGHT, PixelRenderer::rgb(8, 14, 20));
    const auto layout = listLayout();
    const auto& contacts = state();
    const auto count = listItemCount();
    storageCursor = count ? std::min<uint8_t>(storageCursor, count - 1) : 0;
    const bool modal = storageActionOpen || storageInviteConfirmOpen || storageReleaseConfirmOpen;
    c.setClipRect(layout.viewport.x, layout.viewport.y, layout.viewport.width, layout.viewport.height);
    if (!count) MenuGrid::label(c, WIDTH / 2, 170, Ui::Computer::STORAGE_EMPTY,
        layout.viewport.width, PixelRenderer::rgb(126, 145, 145));
    for (uint8_t i = 0; i < count; ++i) {
        const auto row = layout.cell(i, storageScroll);
        if (row.y + row.height <= layout.viewport.y || row.y >= layout.viewport.y + layout.viewport.height) continue;
        const auto* mon = ContactRoster::liveMonsterForContact(contacts, i);
        if (!mon) continue;
        const auto* species = findSpecies(mon->speciesId);
        if (!species) species = &starterSpecies();
        const auto caps = contactCapabilities(i);
        const bool selected = listTouch.pressed() == i || (modal && storageCursor == i);
        c.fillRoundRect(row.x, row.y, row.width, row.height, 12,
            selected ? PixelRenderer::rgb(27, 52, 53) : PixelRenderer::rgb(17, 28, 37));
        c.drawRoundRect(row.x, row.y, row.width, row.height, 12,
            selected ? PixelRenderer::rgb(115, 226, 183) : PixelRenderer::rgb(48, 66, 74));
        avatar(mon->speciesId, row.x + 40, row.y + row.height / 2);
        const char* status = caps.visiting ? Ui::Storage::VISITING_SHORT
            : !caps.inTeam && caps.locked ? Ui::Storage::INVITE_LOCKED_SHORT : nullptr;
        const int textX = row.x + 80;
        const int right = row.x + row.width - 32;
        const int statusX = status ? right - textWidth(status) : right;
        const int flagSpace = caps.inTeam ? TeamFlag::width(1) + 6 : 0;
        const int width = std::min(textWidth(species->name), std::max(0, right - textX - flagSpace));
        c.setClipRect(textX, std::max(row.y + 14, layout.viewport.y), width,
            std::max(0, std::min(row.y + 30, layout.viewport.y + layout.viewport.height) - std::max(row.y + 14, layout.viewport.y)));
        PixelRenderer::text(textX, row.y + 14, species->name, PixelRenderer::rgb(226, 238, 233));
        c.setClipRect(layout.viewport.x, layout.viewport.y, layout.viewport.width, layout.viewport.height);
        if (caps.inTeam) TeamFlag::draw(c, textX + width + 6, row.y + 15);
        const auto level = static_cast<uint8_t>(Game::Bond::levelFor(mon->bond));
        PixelRenderer::text(textX, row.y + 42, Ui::Storage::BOND_LEVELS[level - 1], bondColor(mon->bond));
        if (status) PixelRenderer::text(statusX, row.y + 42, status, PixelRenderer::rgb(126, 175, 175));
        c.drawLine(row.x + row.width - 22, row.y + 30, row.x + row.width - 16, row.y + 36, PixelRenderer::rgb(126, 145, 145));
        c.drawLine(row.x + row.width - 16, row.y + 36, row.x + row.width - 22, row.y + 42, PixelRenderer::rgb(126, 145, 145));
    }
    c.clearClipRect();
    if (modal && storageCursor < count) {
        PixelRenderer::fillRectAlpha(0, 52, WIDTH, 260, 0, 136);
        if (storageInviteConfirmOpen || storageReleaseConfirmOpen) renderConfirm();
        else renderActions();
    }
    char capacity[12];
    std::snprintf(capacity, sizeof(capacity), Ui::Computer::STORAGE_COUNT_FMT, count);
    RoundPage::nativeHeader(Ui::STORAGE, listTouch.pressed() == TARGET_BACK);
    MenuGrid::label(c, WIDTH / 2, 324, capacity, 120, PixelRenderer::rgb(126, 145, 145));
}

void ContactsUi::renderActions() {
    auto& c = PixelRenderer::canvas();
    const auto panel = actionBounds();
    c.fillRoundRect(panel.x, panel.y, panel.width, panel.height, 6, PixelRenderer::rgb(24, 34, 42));
    c.drawRoundRect(panel.x, panel.y, panel.width, panel.height, 6, PixelRenderer::rgb(67, 97, 101));
    for (uint8_t i = 0; i < actionCount(); ++i) {
        const int y = panel.y + ACTION_PADDING + i * ACTION_ROW_HEIGHT;
        const bool selected = listTouch.pressed() == 128 + i || storageActionCursor == i;
        if (selected) c.fillRect(panel.x + 4, y, panel.width - 8, ACTION_ROW_HEIGHT, PixelRenderer::rgb(42, 61, 68));
        PixelRenderer::text(panel.x + 20, y + 12, actionLabel(i),
            actionAt(i) == Features::Contacts::Action::DELETE ? PixelRenderer::rgb(239, 143, 148)
            : selected ? PixelRenderer::rgb(248, 210, 105) : PixelRenderer::rgb(226, 238, 233));
        if (i + 1 < actionCount()) c.drawFastHLine(panel.x + 12, y + ACTION_ROW_HEIGHT - 1,
            panel.width - 24, PixelRenderer::rgb(48, 62, 68));
    }
}

void ContactsUi::renderConfirm() {
    auto& c = PixelRenderer::canvas();
    const auto panel = confirmBounds();
    const auto* mon = ContactRoster::liveMonsterForContact(state(), storageCursor);
    const auto* species = mon ? findSpecies(mon->speciesId) : nullptr;
    char prompt[80];
    std::snprintf(prompt, sizeof(prompt), storageReleaseConfirmOpen
        ? Ui::Storage::DELETE_CONFIRM_FMT : Ui::Storage::INVITE_CONFIRM_FMT,
        species ? species->name : Ui::Amoled::MONSTER);
    c.fillRoundRect(panel.x, panel.y, panel.width, panel.height, 8, PixelRenderer::rgb(28, 31, 31));
    MenuGrid::label(c, WIDTH / 2, panel.y + 22, prompt, panel.width - 16, PixelRenderer::rgb(226, 238, 233));
    for (bool yes : {true, false}) {
        const auto button = confirmChoice(yes);
        const bool selected = yes == (storageReleaseConfirmOpen ? storageReleaseConfirmYes : storageInviteConfirmYes);
        MenuGrid::card(c, button, listTouch.pressed() == (yes ? 200 : 201), selected);
        MenuGrid::label(c, button.x + button.width / 2, button.y + 14,
            yes ? Ui::Storage::YES : Ui::Storage::NO, button.width - 12,
            yes ? PixelRenderer::rgb(115, 226, 183) : PixelRenderer::rgb(226, 238, 233));
    }
}

bool ContactsUi::onButton(const ButtonEvent& event) {
    cancelTouch();
    if (event.btn == 1 && event.action == BtnAction::PRESSED &&
        !storageActionOpen && !storageInviteConfirmOpen && !storageReleaseConfirmOpen)
        revealCursor = true;
    return handleButton(event);
}

bool ContactsUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = listLayout();
    const bool canScroll = !storageActionOpen && !storageInviteConfirmOpen &&
        !storageReleaseConfirmOpen && layout.viewport.contains(event.x, event.y);
    if (event.type == Platform::IInputDevice::TouchType::DOWN && canScroll) revealCursor = false;
    const auto result = listTouch.update(event, touchTarget(event.x, event.y),
        storageScroll, layout.maxScroll(listItemCount()), canScroll);
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int ContactsUi::touchTarget(int16_t x, int16_t y) {
    if (RoundPage::NATIVE_BACK.contains(x, y)) return TARGET_BACK;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return -1;
    if (storageReleaseConfirmOpen || storageInviteConfirmOpen) {
        for (bool yes : {true, false}) if (confirmChoice(yes).contains(x, y)) return yes ? 200 : 201;
        return -1;
    }
    if (storageActionOpen) {
        const auto panel = actionBounds();
        if (!panel.contains(x, y)) return DISMISS;
        const int localY = y - panel.y - ACTION_PADDING;
        return localY >= 0 && localY < actionCount() * ACTION_ROW_HEIGHT
            ? 128 + localY / ACTION_ROW_HEIGHT : -1;
    }
    return listLayout().itemAt(x, y, storageScroll, listItemCount());
}

bool ContactsUi::onTap(int target) {
    if (target == TARGET_BACK) return onButton({1, BtnAction::LONG_PRESS});
    if (target == DISMISS) {
        if (!storageActionOpen || storageInviteConfirmOpen || storageReleaseConfirmOpen) return false;
        cancelTouch();
        storageActionOpen = false;
        return true;
    }
    if (storageInviteConfirmOpen || storageReleaseConfirmOpen) {
        if (target != 200 && target != 201) return false;
    } else if (storageActionOpen) {
        if (target < 128 || target >= 128 + actionCount()) return false;
    } else if (target < 0 || target >= listItemCount()) return false;
    return handleTap(target);
}

}  // namespace Ui
