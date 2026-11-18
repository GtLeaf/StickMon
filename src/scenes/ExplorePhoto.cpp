#include "scenes/ExploreScene.h"
#if STICKMON_UI_ROUND_TOUCH
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "ui/round_touch/PhotoUi.h"
#include <cstdio>

bool ExploreScene::openPhoto(){
    if(phase!=Phase::WALKING||exploreSubViewOpen||GameEngine::ins().sceneFadeActive())return false;
    if(market_){
        if(!marketMenuOpen_||!marketMenuAvailable())return false;
        marketMenuOpen_=false;
        cancelTouch();
        photoClock_=Hal::ins().millis();
    }else{
        if(!exploreMenuOpen||iceSliding||pendingFrostFall)return false;
        closeExploreMenu();
        cancelTouch();photoClock_=photoPauseAt_=Hal::ins().millis();photoWasPaused_=routePausedForTouch;
        if(!photoWasPaused_)routePausedAt=photoPauseAt_;
        routePausedForTouch=true;
    }
    departureToastUntil=0;
    photo_.openCamera(market_?Photos::MARKET_AREA:generatedMap.areaIndex,GameEngine::ins().gameMinutesTotal()/1440+1);
    return true;
}
void ExploreScene::closePhoto(bool saved){
    if(!photo_.active())return;
    const auto now=Hal::ins().millis();photo_.close();
    if(market_){
        marketUi_.update(*market_,now,false);
    }else{
        if(!photoWasPaused_)routeMoveStarted+=now-photoPauseAt_;
        routePausedForTouch=photoWasPaused_;
        ui.updateRouteVisual(now,!routePausedForTouch);
    }
    if(saved){std::snprintf(departureToast,sizeof(departureToast),"%s",Ui::Photo::SAVED);departureToastUntil=now+1400;}
}
bool ExploreScene::photoTouch(const Platform::IInputDevice::TouchEvent& e){
    using R=Ui::PhotoFlow::Request;using T=Platform::IInputDevice::TouchType;
    if(Platform::display().locked()){photo_.cancelTouch();return false;}
    const auto request=photo_.touch(e);
    if(request==R::CAPTURE){
        const auto day=GameEngine::ins().gameMinutesTotal()/1440+1;
        if(market_){
            auto& canvas=PixelRenderer::canvas();
            const int left=canvas.clipLeft(),top=canvas.clipTop(),right=canvas.clipRight(),bottom=canvas.clipBottom();
            renderMarketPhotoWorld();
            photo_.capture(canvas,day);
            canvas.setClipRect(left,top,right-left,bottom-top);
        }else if(ui.capturePhoto(photo_,day))ui.updateRouteVisual(Hal::ins().millis(),false);
    }else if(request==R::CLOSE_CAMERA)closePhoto(photo_.saved());
    return e.type!=T::MOVE;
}
SceneUpdateResult ExploreScene::updatePhoto(uint32_t now){
    if(Platform::display().locked()){photo_.cancelTouch();photoClock_=now;return SceneUpdateResult::parked();}
    if(!photo_.viewfinder())return SceneUpdateResult::parked();
    const auto elapsed=now-photoClock_;
    if(elapsed<66)return SceneUpdateResult::pollAfter(66-elapsed);
    photoClock_=now;photo_.update(elapsed);
    if(market_)marketUi_.update(*market_,now);
    else ui.updateRouteVisual(now,true);
    return SceneUpdateResult::animate(66);
}
void ExploreScene::renderMarketPhotoWorld(){
    Ui::MarketView::render(*market_,Ui::MarketWalkUi::viewport(),market_->timeMs(),
                           GameEngine::ins().gameState().gameMinutesTotal,
                           marketUi_.dialogue().environmentMs());
}
#endif
