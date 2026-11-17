#include "ui/compact_touch/RoomUi.h"

#include "ui/PageProfile.h"
#include "ui/SceneSurface.h"
#include "ui/round_touch/RoundPage.h"

#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "core/AudioManager.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
MenuGrid::Layout roomLayout() {
    return MenuGrid::fit(MenuGrid::Rect{0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT}, Ui::Page::WIDTH - 16, 72, 0, 6, 8, 4);
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

void RoomUi::reset() {
    homeLibrary.close();
    roomCursor = 0;
    roomScroll = 0;
    cancelTouch();
}

SceneUpdateResult RoomUi::update(uint32_t,float dt) {
    if(!homeLibrary.active())return SceneUpdateResult::parked();
    const bool changed=homeLibrary.update(uint32_t(std::max(0.0f,dt)*1000),host.pageState());
    if(homeLibrary.moving())return SceneUpdateResult::animate(66);
    return changed?SceneUpdateResult::frame():SceneUpdateResult::parked();
}

void RoomUi::render() {
    if(homeLibrary.active()){homeLibrary.render(host.pageState());return;}
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const auto layout = roomLayout();
    c.setClipRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT);
    for (int i = 0; i < 3; ++i) {
        const auto rect = layout.cell(i, roomScroll);
        if (rect.y + rect.height <= 0 || rect.y >= Ui::Page::HEIGHT) continue;
        MenuGrid::card(c, rect, gridTouch.pressed() == i, roomCursor == i);
        if(i==2)FurnitureArt::drawFrame(c,rect.x+38,rect.y+36,40);
        else GameAssets::drawCentered(i==0?GameAssets::Kind::ITEM_NORMAL_FOOD:GameAssets::Kind::SHOWER_MENU_SPRINKLER,
            rect.x+38,rect.y+36,i==0?1.5f:1.2f);
        PixelRenderer::text(rect.x+80,rect.y+28,i==2?HomeLibraryText::TITLE:Ui::Room::ITEMS[i],0xffff);
        if (i == 0) {
            char value[12];
            std::snprintf(value, sizeof(value), "x%u", unsigned(Features::Room::totalFoodStock(host.pageState())));
            PixelRenderer::text(Ui::Page::WIDTH - 18 - textPixelWidth(value), rect.y + 13, value, 0xffe0);
        }
    }
    c.clearClipRect();
    Ui::SceneSurface::pageHeader(Ui::ROOM, gridTouch.pressed() == 230);
}

bool RoomUi::onButton(const ButtonEvent& event) {
    if(homeLibrary.active()){
        if(event.btn==0&&event.action==BtnAction::LONG_PRESS){host.pageHome();return true;}
        if(event.btn==1&&event.action==BtnAction::LONG_PRESS){handleHomeLibraryRequest(homeLibrary.back(host.pageState()));return true;}
        return true;
    }
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
        roomCursor = (roomCursor + 1) % 3;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        if(roomCursor==2){homeLibrary.open();gridTouch.cancel();Ui::SceneSurface::mainMenu(false,true);}
        else activateRoom(roomCursor);
        return true;
    }
    return false;
}

void RoomUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
    homeLibrary.cancelTouch();
}

void RoomUi::handleHomeLibraryRequest(HomeLibraryUi::Request request){
    if(request.kind!=HomeLibraryUi::Request::NONE)homeLibrary.committed(host.changeHomeFurniture(request.id,request.kind==HomeLibraryUi::Request::PLACE));
    SfxCue cue;if(homeLibrary.takeSfx(cue))AudioManager::ins().playSfx(cue);
    if(!homeLibrary.active())Ui::SceneSurface::mainMenu(false,false);
}

bool RoomUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    if(homeLibrary.active()){
        handleHomeLibraryRequest(homeLibrary.touch(event,host.pageState()));
        return true;
    }
    const auto layout = roomLayout();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), roomScroll,
        layout.maxScroll(3), layout.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int RoomUi::touchTarget(int16_t x, int16_t y) {
    if (RoundPage::NATIVE_BACK.contains(x + 60, y + 60)) return 230;
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    return roomLayout().itemAt(x, y, roomScroll, 3);
}

bool RoomUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target < 0 || target >= 128) return false;
    roomCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
