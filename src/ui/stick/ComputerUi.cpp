#include "ui/stick/ComputerUi.h"
#include <cstdio>
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
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
}

void ComputerUi::reset() { cancelTouch(); computerCursor = 0; }
SceneUpdateResult ComputerUi::update(uint32_t, float) { return SceneUpdateResult::parked(); }
void ComputerUi::cancelTouch() { Scene::cancelTouch(); }
bool ComputerUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }

bool ComputerUi::onButton(const ButtonEvent& event) {
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        host.pageBack();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        computerCursor = (computerCursor + 1) % COMPUTER_ITEM_COUNT;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateComputer(computerCursor);
        return true;
    }
    return false;
}

int ComputerUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 18 || y >= TouchUi::CONTENT_H) return -1;
    const int index = (y - 18) / 30;
    return index < COMPUTER_ITEM_COUNT ? index : -1;
}

bool ComputerUi::onTap(int target) {
    if (target < 0 || target >= COMPUTER_ITEM_COUNT) return false;
    computerCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

void ComputerUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int ROW_Y = 18;
    static constexpr int ROW_H = 30;
    static constexpr int SEP_Y_OFFSET = 23;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    for (uint8_t i = 0; i < COMPUTER_ITEM_COUNT; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == computerCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y - 1, 4, 18, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, Ui::Computer::ITEMS[i], color, 1);
        if (i == 1) {
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), Ui::Computer::STORAGE_COUNT_FMT,
                     host.pageState().storageCount);
            PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(countBuf) - 12, y, countBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < COMPUTER_ITEM_COUNT) {
            c.drawFastHLine(ROW_X, y + SEP_Y_OFFSET, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }
}

}  // namespace Ui
