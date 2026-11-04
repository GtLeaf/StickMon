#include "ui/common/RoomUiBase.h"

#include "core/UiStrings.h"

namespace Ui {

void RoomUiBase::activateRoom(uint8_t index) {
    using Features::Room::Action;
    switch (Features::Room::roomAction(index)) {
    case Action::FOOD:
        host.openRoomFood();
        break;
    case Action::SHOWER:
        if (Features::Room::canShower(host.pageState())) host.openRoomShower();
        else host.pageNotice(Ui::Menu::HATCH_FIRST, 1100);
        break;
    case Action::BACK:
        host.pageBack();
        break;
    case Action::NONE:
        break;
    }
}

void FoodUiBase::activateFood(uint8_t index) {
    if (Features::Room::foodBack(index)) {
        host.pageBack();
        return;
    }
    host.pageNotice(host.selectPageFood(index) ? Ui::Room::FOOD_SELECTED : Ui::Room::FOOD_NO_STOCK, 900);
}

}  // namespace Ui
