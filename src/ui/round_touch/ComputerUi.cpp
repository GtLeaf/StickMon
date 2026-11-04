#include "ui/compact_touch/ComputerUi.h"

#include "ui/PageProfile.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/RoundPage.h"

#include <cstdio>
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
MenuGrid::Layout pageLayout() {
    return MenuGrid::fit({0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT},
        Ui::Page::WIDTH - 16, 74, 0, 6, 8, 4);
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
}  // namespace

void ComputerUi::reset() {
    computerCursor = 0;
    pageScroll = 0;
    cancelTouch();
}

void ComputerUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const auto layout = pageLayout();
    c.setClipRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT);
    for (int i = 0; i < 2; ++i) {
        const auto rect = layout.cell(i, pageScroll);
        if (rect.y + rect.height <= 0 || rect.y >= Ui::Page::HEIGHT) continue;
        MenuGrid::card(c, rect, gridTouch.pressed() == i, computerCursor == i);
        PixelRenderer::text(rect.x + 16, rect.y + 29, Ui::Computer::ITEMS[i], 0xffff);
        if (i == 1) {
            char value[12];
            std::snprintf(value, sizeof(value), Ui::Computer::STORAGE_COUNT_FMT, host.pageState().storageCount);
            PixelRenderer::text(Ui::Page::WIDTH - 18 - textPixelWidth(value), rect.y + 29, value, 0x77b6);
        } else PixelRenderer::text(Ui::Page::WIDTH - 28, rect.y + 29, ">", 0x77b6);
    }
    c.clearClipRect();
    Ui::SceneSurface::pageHeader(Ui::COMPUTER, gridTouch.pressed() == 230);
}

bool ComputerUi::onButton(const ButtonEvent& event) {
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        host.pageBackTutorial();
        host.pageBack();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        computerCursor = (computerCursor + 1) % 2;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateComputer(computerCursor);
        return true;
    }
    return false;
}

void ComputerUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool ComputerUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = pageLayout();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), pageScroll,
        layout.maxScroll(2), layout.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int ComputerUi::touchTarget(int16_t x, int16_t y) {
    if (RoundPage::NATIVE_BACK.contains(x + 60, y + 60)) return 230;
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    return pageLayout().itemAt(x, y, pageScroll, 2);
}

bool ComputerUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target < 0 || target >= 128) return false;
    computerCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
