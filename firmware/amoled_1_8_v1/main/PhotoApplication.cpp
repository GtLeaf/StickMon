#include "AmoledApp.h"
#include <cstdlib>
#include "core/UiStrings.h"
#include "core/RoomResource.h"
#include "game/HomeFurnitureService.h"
#include "HomeScreen.h"
#include "presentation/PixelRenderer.h"
#include "ui/ExploreScreens.h"

namespace AmoledV1 {
#if STICKMON_ENABLE_DEBUG_FEATURES
void AmoledApp::debugReportPhotoState() const {
    const auto count = photoFlow_.count();
    const auto* pixels = photoFlow_.image();
    uint32_t hash = 2166136261u;
    if (pixels) {
        for (size_t i = 0; i < Photos::PIXELS; ++i)
            hash = (hash ^ pixels[i]) * 16777619u;
    }
    Platform::logf("[PhotoDiag] scene=%u page=%u count=%u newest=%llu image=%08lx error=%s\n",
                   unsigned(sceneFlow.current()), unsigned(photoFlow_.page()), count,
                   count ? static_cast<unsigned long long>(photoFlow_.entry(0).id) : 0ull,
                   static_cast<unsigned long>(pixels ? hash : 0ul),
                   photoFlow_.error() ? photoFlow_.error() : "none");
}
#endif
void AmoledApp::openPhotoCamera(uint32_t nowMs){
    if (market_ && sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        if (!marketMenuAvailable()) return;
        cancelTouch();
        sceneFlow.enter(AppSceneFlow::Scene::MARKET);
        photoClockMs_ = nowMs;
        photoFlow_.openCamera(Photos::MARKET_AREA, gameState.gameMinutesTotal / 1440 + 1);
        requestFullRender();
        return;
    }
    const auto& route=exploreSession_.state();
    if(sceneFlow.current()!=AppSceneFlow::Scene::EXPLORE_MENU||!exploreSessionActive||route.exploreRouteComplete||route.exploreRouteExitConfirm||route.exploreRouteExiting||route.exploreRouteIceSliding||exploreTransition_.fade().active())return;
    sceneFlow.closeExploreMenu();
    cancelTouch();pauseExploreRoute(nowMs);photoClockMs_=nowMs;
    photoFlow_.openCamera(selectedExploreArea,gameState.gameMinutesTotal/1440+1);
    requestFullRender();
}
void AmoledApp::renderPhotoWorld(Canvas565& canvas,uint16_t top,uint16_t bottom)const{
    if (market_ && sceneFlow.current() == AppSceneFlow::Scene::MARKET) {
        Ui::MarketView::render(*market_, marketViewport(), market_->timeMs(), gameState.gameMinutesTotal);
        return;
    }
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
    const auto oldCount = photoFlow_.count();
    const auto request=photoFlow_.touch({type,event.x,event.y,event.timestampMs});
    if (photoFlow_.count() < oldCount) wallPhotoLoaded_ = false;
    if(request==PhotoSession::Request::CAPTURE){
        auto& canvas=PixelRenderer::canvas();
        // Draw a complete clean world before sampling, never read old HUD pixels.
        const auto l=dirtyColumnBegin,r=dirtyColumnEnd;dirtyColumnBegin=0;dirtyColumnEnd=368;
        canvas.clearClipRect();renderPhotoWorld(canvas,0,448);dirtyColumnBegin=l;dirtyColumnEnd=r;
        photoFlow_.capture(canvas,gameState.gameMinutesTotal/1440+1);
    }else if(request==PhotoSession::Request::CLOSE_CAMERA){
        const bool saved=photoFlow_.saved();photoFlow_.close();
        const auto now=Platform::clock().millis();lastInteractionMs=now;
        photoClosedSinceUpdate_ = true;
        if (!market_) resumeExploreRoute(now);
        if(saved)setToast(Ui::Photo::SAVED,now);
    }else if(request==PhotoSession::Request::CLOSE_ALBUM)photoFlow_.close();
    if(type!=Type::MOVE)requestFullRender();
    else if(photoFlow_.offset()!=oldOffset)requestRenderRows(80,448);
}

void AmoledApp::requestFramePhotoRows() {
    constexpr int top = Ui::HomePhotoFrame::PHOTO_TOP + Ui::HomePhotoFrame::RECT_PANEL_Y;
    requestRenderRows(top, top + Ui::HomePhotoFrame::PHOTO_HEIGHT);
}

bool AmoledApp::handleWallFrameTouch(const TouchEvent& event) {
    using Type = TouchTap::Type;
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    const auto type = event.type == TouchEventType::DOWN ? Type::DOWN
                    : event.type == TouchEventType::MOVE ? Type::MOVE : Type::UP;
    const TouchTap::Event touch{type, event.x, event.y, event.timestampMs};
    if (homePhotoFrame_.active()) {
        if (displayLocked) cancelTouch();
        else if (homePhotoFrame_.touch(touch)) {
            if (homePhotoFrame_.photosOnly()) requestFramePhotoRows();
            else requestFullRender();
        }
        return true;
    }

    const auto frame = homeWallFrameBounds(std::lround(homePresenter_.cameraX()),
                                           std::lround(homePresenter_.cameraY()));
    // Follow the rendered room camera and clip the hit box to the visible wall.
    constexpr int hitSize = 56;
    const int left = frame.x + (frame.width - hitSize) / 2;
    const int top = frame.y + (frame.height - hitSize) / 2;
    const bool hit = !displayLocked && gameState.oobeDone && !visitRecallConfirm &&
        visitAwayFlow_.state().phase == VisitDeparture::NONE &&
        !exploreTransition_.state().pending &&
        exploreTransition_.state().phase == ExpeditionDeparturePhase::NONE &&
        RoomResource::ins().available() && Game::HomeFurniture::placed(gameState, 0) &&
        frame.x < AmoledUi::WIDTH && frame.x + frame.width > 0 &&
        frame.y < HOME_STATUS_TOP && frame.y + frame.height > HOME_ROOM_TOP &&
        event.x >= 0 && event.x < AmoledUi::WIDTH &&
        event.y >= HOME_ROOM_TOP && event.y < HOME_STATUS_TOP &&
        event.x >= left && event.x < left + hitSize &&
        event.y >= top && event.y < top + hitSize
#if STICKMON_ENABLE_DEBUG_FEATURES
        && !debugContactPending && !debugContactEventPending
#endif
        ;
    if (type == Type::DOWN) {
        wallFramePressed_ = hit;
        if (hit) {
            pointerDown = dragging = false;
            touchStartX = touchLastX = event.x;
            touchStartY = touchLastY = event.y;
            homePanCandidate_ = homePanAllowed(event.y);
        }
    }
    if (!wallFramePressed_) return false;
    // A sideways drag that starts on the frame looks around the room
    // instead; hand the gesture to the shared home pan path.
    if (type == Type::MOVE && homePanCandidate_ &&
        std::abs(event.x - touchStartX) > 10 &&
        std::abs(event.x - touchStartX) > std::abs(event.y - touchStartY)) {
        wallFrameTap_.cancel();
        wallFramePressed_ = false;
        pointerDown = true;
        dragging = false;
        return false;
    }
    const bool activate = wallFrameTap_.update(touch, hit ? 0 : -1) == 0;
    if (type == Type::UP) wallFramePressed_ = false;
    if (activate) {
        if (!wallPhotoLoaded_) {
            homePhotoFrame_.enter(true);
            wallPhotoLoaded_ = true;
        }
        if (!homePhotoFrame_.open()) setToast(Ui::Photo::NO_MEMORY, event.timestampMs);
        requestFullRender();
    }
    return true;
}
}
