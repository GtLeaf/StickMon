#include "AmoledApp.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/ExploreScreens.h"

namespace AmoledV1 {
void AmoledApp::openPhotoCamera(uint32_t nowMs){
    const auto& route=exploreSession_.state();
    if(sceneFlow.current()!=AppSceneFlow::Scene::EXPLORE_MENU||!exploreSessionActive||route.exploreRouteComplete||route.exploreRouteExitConfirm||route.exploreRouteExiting||route.exploreRouteIceSliding||exploreTransition_.fade().active())return;
    sceneFlow.closeExploreMenu();
    cancelTouch();pauseExploreRoute(nowMs);photoClockMs_=nowMs;
    photoFlow_.openCamera(selectedExploreArea,gameState.gameMinutesTotal/1440+1);
    requestFullRender();
}
void AmoledApp::renderPhotoWorld(Canvas565& canvas,uint16_t top,uint16_t bottom)const{
    auto model=exploreRouteModel();model.photoMode=true;model.toast=nullptr;model.complete=false;model.exitConfirm=false;
    model.walking=false;model.companionWalking=false;
    model.renderColumnBegin=dirtyColumnBegin;model.renderColumnEnd=dirtyColumnEnd;
    renderExploreRouteScreen(canvas,model,renderCaches_.exploreWorld,top,bottom);
    explorePresenter_.recordDrawn(model);
}
void AmoledApp::handlePhotoTouch(const TouchEvent& event){
    if(displayLocked){cancelTouch();return;}
    using Type=Platform::IInputDevice::TouchType;
    const auto type=event.type==TouchEventType::DOWN?Type::DOWN:event.type==TouchEventType::MOVE?Type::MOVE:Type::UP;
    pointerDown=type!=Type::UP;
    const int oldOffset=photoFlow_.offset();
    const auto request=photoFlow_.touch({type,event.x,event.y,event.timestampMs});
    if(request==PhotoSession::Request::CAPTURE){
        auto& canvas=PixelRenderer::canvas();
        // Draw a complete clean world before sampling, never read old HUD pixels.
        const auto l=dirtyColumnBegin,r=dirtyColumnEnd;dirtyColumnBegin=0;dirtyColumnEnd=368;
        canvas.clearClipRect();renderPhotoWorld(canvas,0,448);dirtyColumnBegin=l;dirtyColumnEnd=r;
        photoFlow_.capture(canvas,gameState.gameMinutesTotal/1440+1);
    }else if(request==PhotoSession::Request::CLOSE_CAMERA){
        const bool saved=photoFlow_.saved();photoFlow_.close();
        const auto now=Platform::clock().millis();lastInteractionMs=now;resumeExploreRoute(now);
        if(saved)setToast(Ui::Photo::SAVED,now);
    }else if(request==PhotoSession::Request::CLOSE_ALBUM)photoFlow_.close();
    if(type!=Type::MOVE)requestFullRender();
    else if(photoFlow_.offset()!=oldOffset)requestRenderRows(80,448);
}
}
