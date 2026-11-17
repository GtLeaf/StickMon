// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "core/TouchTap.h"
#include "ui/common/ExploreUiModel.h"

namespace Ui::RoundExplore {
constexpr int BACK=230,PREVIOUS=231,NEXT=232,DEPART=233,TOGGLE_PREVIEW=234;
constexpr MenuGrid::Rect CARD_BOUNDS{64,92,232,176};
constexpr MenuGrid::Rect PREVIOUS_BOUNDS{10,158,44,44},NEXT_BOUNDS{306,158,44,44};
constexpr MenuGrid::Rect DEPART_BOUNDS{108,304,144,40},TOGGLE_BOUNDS{244,96,44,44};
constexpr MenuGrid::Rect PET_BOUNDS{74,132,212,132};
// Kept for the shared scene's preload interfaces; automatic strip animation is
// disabled for the card selector. Native1x frames are grouped by actual width.
struct PreviewLayout {
    static constexpr int SPACING=8,STATIC_WIDTH=PET_BOUNDS.width;
    static int width(const PokemonSprites::SpriteFrame* f){return f?int(f->width):80;}
    static int stripWidth(const PokemonSprites::SpriteFrame* const* frames,int count){int n=0;for(int i=0;i<count;++i)n+=width(frames[i])+(i?SPACING:0);return n;}
    static bool scrolls(const PokemonSprites::SpriteFrame* const* frames,int count){return count>1&&stripWidth(frames,count)>STATIC_WIDTH;}
};
struct PreviewPages {
    uint8_t starts[ExploreRouteFrame::PREVIEW_CAP]{},ends[ExploreRouteFrame::PREVIEW_CAP]{};
    uint8_t count=0;
};
PreviewPages previewPages(const ExploreRouteFrame& frame);
bool canPreview(const ExploreRouteFrame& frame);
class Selection {
public:
    Selection()=default;
    Selection(const Selection&)=delete;
    Selection& operator=(const Selection&)=delete;
    void release();
    void render(const ExploreRouteFrame& frame)const;
    int target(const ExploreInputFrame&,const ExploreUiState&,int x,int y)const;
    bool tap(ExploreHost&,ExploreUiState&,int target);
    bool touch(ExploreHost&,ExploreUiState&,const Platform::IInputDevice::TouchEvent&);
    void cancel();
    bool previewVisible()const{return previewVisible_;}
    uint8_t previewPage()const{return previewPage_;}
private:
    TouchTap tracker;
    int pressed=-1;
    int16_t startX=0,startY=0;
    bool active=false,swipe=false,previewVisible_=false;
    uint8_t previewPage_=0,previewArea_=255;
    bool showing(const ExploreRouteFrame& f)const{return previewVisible_&&previewArea_==f.areaCursor&&canPreview(f);}
    bool page(ExploreHost&,ExploreUiState&,int direction);
};
}
