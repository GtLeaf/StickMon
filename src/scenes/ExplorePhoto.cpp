#include "scenes/ExploreScene.h"
#if STICKMON_UI_ROUND_TOUCH
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "ui/round_touch/PhotoUi.h"
#include <cstdio>

bool ExploreScene::openPhoto(){
    if(phase!=Phase::WALKING||market_||exploreSubViewOpen||exploreMenuOpen||iceSliding||pendingFrostFall||GameEngine::ins().sceneFadeActive())return false;
    cancelTouch();photoClock_=photoPauseAt_=Hal::ins().millis();photoWasPaused_=routePausedForTouch;
    if(!photoWasPaused_)routePausedAt=photoPauseAt_;
    routePausedForTouch=true;
    photo_.openCamera(generatedMap.areaIndex,GameEngine::ins().gameMinutesTotal()/1440+1);
    return true;
}
void ExploreScene::closePhoto(bool saved){
    if(!photo_.active())return;
    const auto now=Hal::ins().millis();photo_.close();
    if(!photoWasPaused_)routeMoveStarted+=now-photoPauseAt_;
    routePausedForTouch=photoWasPaused_;
    ui.updateRouteVisual(now,!routePausedForTouch);
    if(saved){std::snprintf(departureToast,sizeof(departureToast),"%s",Ui::Photo::SAVED);departureToastUntil=now+1400;}
}
bool ExploreScene::photoTouch(const Platform::IInputDevice::TouchEvent& e){
    using R=Ui::PhotoFlow::Request;using T=Platform::IInputDevice::TouchType;
    if(Platform::display().locked()){photo_.cancelTouch();return false;}
    const auto request=photo_.touch(e);
    if(request==R::CAPTURE){
        if(ui.capturePhoto(photo_,GameEngine::ins().gameMinutesTotal()/1440+1))ui.updateRouteVisual(Hal::ins().millis(),false);
    }else if(request==R::CLOSE_CAMERA)closePhoto(photo_.saved());
    return e.type!=T::MOVE;
}
SceneUpdateResult ExploreScene::updatePhoto(uint32_t now){
    if(Platform::display().locked()){photo_.cancelTouch();photoClock_=now;return SceneUpdateResult::parked();}
    if(!photo_.viewfinder())return SceneUpdateResult::parked();
    const auto elapsed=now-photoClock_;
    if(elapsed<66)return SceneUpdateResult::pollAfter(66-elapsed);
    photoClock_=now;photo_.update(elapsed);ui.updateRouteVisual(now,true);
    return SceneUpdateResult::animate(66);
}
#endif
