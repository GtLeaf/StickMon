// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ExploreSelectUi.h"
#include <algorithm>
#include <cstdlib>
#include "ui/round_touch/ExploreSelectionView.h"

namespace Ui::RoundExplore {
PreviewPages previewPages(const ExploreRouteFrame& f){
    PreviewPages pages;if(!f.areaPreviewReady)return pages;
    const int count=std::min<int>(f.areaPreviewCount,ExploreRouteFrame::PREVIEW_CAP);int at=0;
    while(at<count){const int start=at;int width=0;
        do {const int next=PreviewLayout::width(f.areaPreviewFrames[at])+(at>start?PreviewLayout::SPACING:0);if(at>start&&width+next>PET_BOUNDS.width)break;width+=next;++at;}while(at<count);
        pages.starts[pages.count]=uint8_t(start);pages.ends[pages.count++]=uint8_t(at);
    }return pages;
}
bool canPreview(const ExploreRouteFrame& f){return f.areaCursor<std::min<int>(f.visibleAreaCount,ExploreRouteFrame::AREA_CAP)&&(f.areaUnlocked[f.areaCursor]||f.marketPreview);}
void Selection::release(){cancel();previewVisible_=false;previewPage_=0;previewArea_=255;}
void Selection::render(const ExploreRouteFrame& f)const{SelectionView::render(f,pressed,showing(f),previewPage_);}
int Selection::target(const ExploreInputFrame& f,const ExploreUiState& state,int x,int y)const{
    if(f.phase!=ExplorePhase::SELECT||x<0||y<0||(x-180)*(x-180)+(y-180)*(y-180)>=180*180)return -1;
    if(RoundPage::NATIVE_BACK.contains(x,y))return BACK;
    const int count=std::min<int>(f.visibleAreaCount,ExploreRouteFrame::AREA_CAP);if(state.areaCursor>=count)return -1;
    if(TOGGLE_BOUNDS.contains(x,y))return TOGGLE_PREVIEW;
    if(!(previewVisible_&&previewArea_==state.areaCursor)){
        if(PREVIOUS_BOUNDS.contains(x,y)&&state.areaCursor>0)return PREVIOUS;
        if(NEXT_BOUNDS.contains(x,y)&&state.areaCursor+1<count)return NEXT;
    }
    return DEPART_BOUNDS.contains(x,y)?DEPART:-1;
}
bool Selection::tap(ExploreHost& host,ExploreUiState& state,int target){
    const auto input=host.exploreInputFrame();if(input.phase!=ExplorePhase::SELECT)return false;
    if(target==BACK){release();return host.activateExplore(ExploreAction::AREA_BACK);}
    const int count=std::min<int>(input.visibleAreaCount,ExploreRouteFrame::AREA_CAP);if(state.areaCursor>=count)return false;
    const auto frame=host.exploreRouteFrame();
    if(target==TOGGLE_PREVIEW){if(!canPreview(frame))return false;previewVisible_=!showing(frame);previewArea_=state.areaCursor;previewPage_=0;return true;}
    if(target==DEPART){if(!frame.areaUnlocked[state.areaCursor]||(frame.marketPreview&&!frame.marketOpen))return false;release();return host.activateExplore(ExploreAction::DEPART,state.areaCursor);}
    if(showing(frame))return false;
    const int next=int(state.areaCursor)+(target==NEXT?1:target==PREVIOUS?-1:0);
    if(next<0||next>=count||next==state.areaCursor)return false;
    release();state.areaCursor=uint8_t(next);state.areaAnimCursor=next;return host.activateExplore(ExploreAction::AREA_CHANGED,state.areaCursor);
}
bool Selection::page(ExploreHost& host,ExploreUiState& state,int direction){
    const auto f=host.exploreRouteFrame();if(!showing(f))return tap(host,state,direction>0?NEXT:PREVIOUS);
    const auto pages=previewPages(f);if(pages.count<2)return false;
    const int current=std::min<int>(previewPage_,pages.count-1);previewPage_=uint8_t((current+direction+pages.count)%pages.count);return true;
}
bool Selection::touch(ExploreHost& host,ExploreUiState& state,const Platform::IInputDevice::TouchEvent& event){
    using Type=Platform::IInputDevice::TouchType;const auto frame=host.exploreInputFrame();const bool wasPressed=pressed>=0;
    if(event.type==Type::CANCEL||frame.phase!=ExplorePhase::SELECT||event.x<0||event.y<0||(int(event.x)-180)*(int(event.x)-180)+(int(event.y)-180)*(int(event.y)-180)>=180*180){cancel();return wasPressed;}
    if(previewArea_!=state.areaCursor){previewVisible_=false;previewPage_=0;}
    const int hit=target(frame,state,event.x,event.y);
    if(event.type==Type::DOWN){cancel();active=true;startX=event.x;startY=event.y;swipe=CARD_BOUNDS.contains(startX,startY)&&hit<0;pressed=hit;tracker.update(event,hit);return pressed>=0||wasPressed;}
    if(!active)return false;
    const int dx=event.x-startX,dy=event.y-startY;if(pressed>=0&&(dx*dx+dy*dy>100||pressed!=hit))pressed=-1;
    const int clicked=tracker.update(event,hit);if(event.type!=Type::UP)return wasPressed!=(pressed>=0);
    const bool moved=swipe&&std::abs(dx)>=40&&std::abs(dx)>std::abs(dy);cancel();
    if(moved)return page(host,state,dx<0?1:-1)||wasPressed;
    return (clicked>=0&&tap(host,state,clicked))||wasPressed;
}
void Selection::cancel(){tracker.cancel();pressed=-1;active=swipe=false;}
}
