#include "ui/compact_touch/MainMenuUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"

namespace Ui {

void MainMenuUi::enter(bool fromHome) {
    resetState(fromHome);
    resetNavigation();
}

void MainMenuUi::resetNavigation() {
    menuGridScroll = MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT)
        .reveal(cursor, 0, ITEM_COUNT);
}

SceneUpdateResult MainMenuUi::update(uint32_t, float) {
    menuGridScroll = std::clamp(menuGridScroll, 0.0f,
        MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT).maxScroll(ITEM_COUNT));
    return SceneUpdateResult::parked();
}

void MainMenuUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    MenuGrid::mainMenu(c, MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT),
        ITEM_COUNT, menuGridScroll, gridTouch.pressed(), cursor);
}

bool MainMenuUi::onButton(const ButtonEvent& event) {
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        advanceCursor();
        menuGridScroll = MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT)
            .reveal(cursor, menuGridScroll, ITEM_COUNT);
        return true;
    }
    return handleButton(event);
}

void MainMenuUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool MainMenuUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto menu = MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT);
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), menuGridScroll,
        menu.maxScroll(ITEM_COUNT), menu.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int MainMenuUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    return MenuGrid::mainLayout(Ui::Page::WIDTH, Ui::Page::HEIGHT)
        .itemAt(x, y, menuGridScroll, ITEM_COUNT);
}

bool MainMenuUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target >= 128) return false;
    cursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
