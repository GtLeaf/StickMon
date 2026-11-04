#include "ui/compact_touch/ShowerUi.h"

#include "ui/PageProfile.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "ui/common/ShowerUiContent.h"

namespace Ui {
namespace {
constexpr int TOOL_TARGET = 100;
constexpr int EXIT_YES = 110, EXIT_NO = 111;
constexpr int SOAP_TARGET = 120, SOAP_CANCEL = 130;

MenuGrid::Rect toolButton(const ShowerLayout& layout, int index) {
    return {layout.width - 66, 26 + index * 48, 48, 44};
}
constexpr MenuGrid::Rect SOAP_POPUP{16, 40, Page::WIDTH - 32, 160};
constexpr MenuGrid::Rect SOAP_CLOSE{Page::WIDTH - 56, 44, 32, 32};
constexpr int SOAP_STEP = (SOAP_POPUP.width - 12) / 3;
MenuGrid::Rect soapChoice(int index) { return {24 + index * SOAP_STEP, 92, SOAP_STEP - 4, 96}; }
constexpr MenuGrid::Rect YES_CHOICE{64, 77, 52, 28};
constexpr MenuGrid::Rect NO_CHOICE{135, 77, 52, 28};

bool toolsAvailable(ShowerMode mode) {
    return mode == ShowerMode::MENU || mode == ShowerMode::SOAP_SELECT ||
           mode == ShowerMode::SOAPING || mode == ShowerMode::BRUSHING ||
           mode == ShowerMode::RINSING;
}

void drawTools(const ShowerFrame& frame, const ShowerLayout& layout) {
    auto& c = PixelRenderer::canvas();
    for (int i = 0; i < SHOWER_MENU_COUNT; ++i) {
        const auto rect = toolButton(layout, i);
        const bool selected =
            (frame.mode == ShowerMode::MENU && frame.visuals->menuCursor == i) ||
            ((frame.mode == ShowerMode::SOAP_SELECT || frame.mode == ShowerMode::SOAPING) && i == 0) ||
            (frame.mode == ShowerMode::BRUSHING && i == 1) ||
            (frame.mode == ShowerMode::RINSING && i == 2);
        MenuGrid::card(c, rect, false, selected);
        const int x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
        if (i == 0)
            GameAssets::drawCentered(GameAssets::Kind::SHOWER_MENU_SOAP, x, y, 0.72f);
        else if (i == 1)
            GameAssets::drawCentered(GameAssets::Kind::SHOWER_MENU_BRUSH, x, y, 0.58f);
        else if (i == 2)
            GameAssets::drawCentered(GameAssets::Kind::SHOWER_MENU_SPRINKLER, x, y, 0.78f);
        else {
            c.drawLine(x - 7, y - 7, x + 7, y + 7, 0xffff);
            c.drawLine(x + 7, y - 7, x - 7, y + 7, 0xffff);
        }
    }
}

void drawSoapChoice(const ShowerFrame& frame, const ShowerLayout&) {
    auto& c = PixelRenderer::canvas();
    PixelRenderer::fillRectAlpha(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0, 150);
    c.fillRoundRect(SOAP_POPUP.x, SOAP_POPUP.y, SOAP_POPUP.width, SOAP_POPUP.height,
        10, PixelRenderer::rgb(17, 27, 34));
    c.drawRoundRect(SOAP_POPUP.x, SOAP_POPUP.y, SOAP_POPUP.width, SOAP_POPUP.height,
        10, PixelRenderer::rgb(82, 117, 117));
    PixelRenderer::text(116 + (Page::WIDTH - 296) / 2, 56, Ui::Amoled::CHOOSE_SOAP, 0xffff);
    c.drawLine(SOAP_CLOSE.x + 11, 55, SOAP_CLOSE.x + 21, 65, 0xffff);
    c.drawLine(SOAP_CLOSE.x + 11, 65, SOAP_CLOSE.x + 21, 55, 0xffff);
    for (int i = 0; i < Game::SOAP_VARIANT_COUNT; ++i) {
        const auto rect = soapChoice(i);
        const auto stock = frame.soapStocks[i];
        const bool selected = stock && frame.visuals->soapCursor == i;
        MenuGrid::card(c, rect, false, selected);
        const auto kind = static_cast<GameAssets::Kind>(
            static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) + i);
        GameAssets::drawCenteredAlpha(kind, rect.x + rect.width / 2, rect.y + 27,
            1.0f, stock ? 255 : 80);
        const int nameWidth = int(std::strlen(Ui::Shower::SOAP_NAMES[i])) / 3 * 16;
        if (Page::WIDTH < 296 && nameWidth > rect.width) {
            // Keep the complete name on two lines in the narrow picker.
            char first[7]{};
            std::memcpy(first, Ui::Shower::SOAP_NAMES[i], 6);
            MenuGrid::label(c, rect.x + rect.width / 2, rect.y + 48, first, rect.width, stock ? 0xffff : 0x7bef);
            MenuGrid::label(c, rect.x + rect.width / 2, rect.y + 64, Ui::Shower::SOAP_NAMES[i] + 6,
                rect.width, stock ? 0xffff : 0x7bef);
        } else PixelRenderer::text(rect.x + (rect.width - nameWidth) / 2, rect.y + 54,
            Ui::Shower::SOAP_NAMES[i], stock ? 0xffff : 0x7bef);
        char count[12];
        std::snprintf(count, sizeof(count), "x%u", unsigned(stock));
        PixelRenderer::text(rect.x + (rect.width - int(std::strlen(count)) * 8) / 2,
            rect.y + (Page::WIDTH < 296 ? 80 : 76), count, stock ? 0xffe0 : 0x7bef);
    }
}
}  // namespace

ShowerLayout ShowerUi::layout() const {
    return ShowerLayout{Ui::Page::WIDTH, Ui::Page::HEIGHT};
}

UiMotion::StepResult ShowerUi::updateMenu() {
    state.menuAnimCursor = state.menuCursor;
    return {false, false};
}

int ShowerUi::touchTarget(int16_t x, int16_t y) {
    const auto frame = host.showerFrame();
    const auto mode = frame.mode;
    if (mode == ShowerMode::EXIT_CONFIRM) {
        if (YES_CHOICE.contains(x, y)) return EXIT_YES;
        if (NO_CHOICE.contains(x, y)) return EXIT_NO;
        return -1;
    }
    if (!toolsAvailable(mode)) return -1;
    if (mode == ShowerMode::SOAP_SELECT) {
        if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
        if (SOAP_CLOSE.contains(x, y) || !SOAP_POPUP.contains(x, y)) return SOAP_CANCEL;
        for (int i = 0; i < Game::SOAP_VARIANT_COUNT; ++i)
            if (soapChoice(i).contains(x, y)) return frame.soapStocks[i] ? SOAP_TARGET + i : -1;
        return -1;
    }
    const auto geometry = layout();
    for (int i = 0; i < SHOWER_MENU_COUNT; ++i)
        if (toolButton(geometry, i).contains(x, y)) return TOOL_TARGET + i;
    return -1;
}

bool ShowerUi::onTap(int target) {
    cancelTouch();
    const auto mode = host.showerFrame().mode;
    if (mode == ShowerMode::EXIT_CONFIRM) {
        if (target == EXIT_YES || target == EXIT_NO)
            return host.activateShower(ShowerAction::EXIT_CHOICE, target == EXIT_YES);
        return false;
    }
    if (!toolsAvailable(mode)) return false;
    if (mode == ShowerMode::SOAP_SELECT) {
        if (target == SOAP_CANCEL) return host.activateShower(ShowerAction::MENU);
        const int index = target - SOAP_TARGET;
        if (index >= 0 && index < Game::SOAP_VARIANT_COUNT && host.showerFrame().soapStocks[index]) {
            state.soapCursor = static_cast<uint8_t>(index);
            return host.activateShower(ShowerAction::BEGIN_SOAP);
        }
        return false;
    }
    if (target < TOOL_TARGET || target >= TOOL_TARGET + SHOWER_MENU_COUNT) return false;
    const int index = target - TOOL_TARGET;
    state.menuCursor = static_cast<uint8_t>(index);
    if ((index == 0 && (mode == ShowerMode::SOAP_SELECT || mode == ShowerMode::SOAPING)) ||
        (index == 1 && mode == ShowerMode::BRUSHING) ||
        (index == 2 && mode == ShowerMode::RINSING)) return true;
    if (index == 3) return host.activateShower(ShowerAction::REQUEST_EXIT);
    clearWater();
    switch (index) {
    case 0: return host.activateShower(ShowerAction::CHOOSE_SOAP);
    case 1: return host.activateShower(ShowerAction::BEGIN_BRUSH);
    default: return host.activateShower(ShowerAction::BEGIN_RINSE);
    }
}

void ShowerUi::cancelTouch() {
    tap.cancel();
    dragging = false;
    dragTravel = 0;
}

void ShowerUi::placeTool(int x, int y) {
    const auto geometry = layout();
    float& toolX = dragMode == ShowerMode::SOAPING ? state.soapX : state.brushX;
    float& toolY = dragMode == ShowerMode::SOAPING ? state.soapY : state.brushY;
    const float nextX = std::clamp(float(x), float(geometry.toolLeftX), float(geometry.toolRightX));
    const float nextY = std::clamp(float(y), float(geometry.toolTopY), float(geometry.toolBottomY));
    dragTravel += std::hypot(nextX - toolX, nextY - toolY);
    toolX = nextX;
    toolY = nextY;
}

bool ShowerUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const auto mode = host.showerFrame().mode;
    const auto geometry = layout();
    const MenuGrid::Rect workArea{16, 32, geometry.width - 98, geometry.height - 48};
    if (event.type == Type::CANCEL) {
        const bool changed = dragging;
        cancelTouch();
        return changed;
    }
    if (event.type == Type::DOWN) {
        cancelTouch();
        if ((mode == ShowerMode::SOAPING || mode == ShowerMode::BRUSHING) &&
            workArea.contains(event.x, event.y)) {
            dragging = true;
            dragMode = mode;
            placeTool(event.x, event.y);
            dragTravel = 0;  // Picking up the tool alone does not scrub.
            return true;
        }
    } else if (dragging) {
        if (event.type == Type::UP || mode != dragMode || !workArea.contains(event.x, event.y)) {
            cancelTouch();
            return true;
        }
        if (event.type == Type::MOVE) {
            placeTool(event.x, event.y);
            return true;
        }
    }
    const int target = tap.update(event, touchTarget(event.x, event.y));
    return target >= 0 && onTap(target);
}

ShowerMotion ShowerUi::readMotion(uint32_t, float) {
    if (!dragging) return {};
    if (host.showerFrame().mode != dragMode) {
        cancelTouch();
        return {};
    }
    const bool moved = dragTravel >= 3.0f;
    if (moved) dragTravel = 0;
    return {0, 0, moved ? 1.0f : 0.0f};
}

void ShowerUi::render() {
    ShowerFrame frame = host.showerFrame();
    frame.visuals = &state;
    ShowerUiContent::draw(frame, layout(), drawTools, drawSoapChoice);
}

}  // namespace Ui
