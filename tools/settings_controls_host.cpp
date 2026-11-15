#include "app/SettingsController.h"
#include "ui/SettingsScreens.h"
#include <cassert>
#include <cstdio>

using namespace AmoledV1;
int main(){
    SettingsController controller;
    Game::PlayerSettings settings{};
    settings.brightness=128;settings.volume=50;
    using Change=SettingsController::Change;
    using Action=SettingsController::Action;
    // Labels above a slider must never clamp its value to the left endpoint.
    assert(controller.beginTouch(80,108,false,settings)==Change::NONE);
    assert(controller.tap(80,108,false,false,settings)==Action::NONE);
    assert(settings.brightness==128 && settings.volume==50);
    // The bottom ink of both 32px labels is still part of the label, not the track.
    for(const auto point : {AmoledUi::Rect{83,126,1,1},AmoledUi::Rect{86,214,1,1}}){
        assert(controller.beginTouch(point.x,point.y,false,settings)==Change::NONE);
        assert(controller.tap(point.x,point.y,false,false,settings)==Action::NONE);
        assert(settings.brightness==128 && settings.volume==50);
    }
    assert(controller.beginTouch(40,148,false,settings)==Change::BRIGHTNESS);
    assert(settings.brightness==32 && controller.sliderDragging());
    assert(controller.dragSlider(328,settings)==Change::BRIGHTNESS);
    assert(settings.brightness==255 && settings.volume==50);
    controller.cancelTouch();
    assert(controller.dragSlider(40,settings)==Change::NONE);
    assert(settings.brightness==255);
    assert(controller.beginTouch(328,236,false,settings)==Change::VOLUME);
    assert(settings.volume==100);
    assert(controller.dragSlider(40,settings)==Change::VOLUME);
    assert(settings.volume==0);
    controller.cancelTouch();
    assert(controller.beginTouch(100,266,false,settings)==Change::NONE);
    assert(controller.tap(100,266,false,false,settings)==Action::NONE);
    settings.speedIndex=0;settings.idleTimeoutIndex=2;
    controller.beginTouch(100,296,false,settings);
    assert(controller.tap(100,350,false,false,settings)==Action::NONE);
    assert(settings.speedIndex==0 && settings.idleTimeoutIndex==2);
    controller.cancelTouch();
    assert(controller.tap(100,296,false,false,settings)==Action::NONE);
    controller.beginTouch(100,296,false,settings);
    assert(controller.tap(100,296,false,false,settings)==Action::SPEED);
    assert(settings.speedIndex==1);
    controller.cancelTouch();
    controller.beginTouch(100,412,true,settings);
    assert(controller.tap(100,412,true,false,settings)==Action::REDRAW);
    assert(controller.resetConfirm());
    controller.beginTouch(80,408,true,settings);
    assert(controller.tap(270,408,true,false,settings)==Action::NONE);
    assert(controller.resetConfirm());
    controller.beginTouch(80,408,true,settings);
    assert(controller.tap(80,408,true,false,settings)==Action::REDRAW);
    assert(!controller.resetConfirm());
#if STICKMON_ENABLE_DEBUG_FEATURES
    controller.beginTouch(270,412,true,settings);
    assert(controller.tap(270,412,true,false,settings)==Action::SYSTEM_UPDATE);
#else
    controller.beginTouch(270,412,true,settings);
    assert(controller.tap(270,412,true,false,settings)==Action::REDRAW);
    assert(controller.resetConfirm());
#endif
    std::puts("settings-controls: label/gap isolation, real tracks, cross-row release, reset and update gate OK");
}
