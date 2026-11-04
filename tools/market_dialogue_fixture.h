#pragma once

#include "game/MarketSession.h"
#include "ui/common/MarketView.h"
#include <cassert>
#include <vector>

inline void marketCheckShiftedCache(const Market::Session& s,Ui::MarketView::Viewport v,uint32_t minutes,int cameraOffset=120) {
    v.cameraOffsetY=cameraOffset;
    auto& canvas=PixelRenderer::canvas();
    Ui::MarketView::render(s,v,s.timeMs(),minutes);
    std::vector<uint16_t> pixels;
    for(int y=0;y<v.height;++y)for(int x=0;x<v.width;++x)pixels.push_back(canvas.readPixel(x,y));
    Ui::MarketView::detail::staticLayerEnabled()=false;
    Ui::MarketView::render(s,v,s.timeMs(),minutes);
    for(int y=0;y<v.height;++y)for(int x=0;x<v.width;++x)assert(pixels[y*v.width+x]==canvas.readPixel(x,y));
    Ui::MarketView::detail::staticLayerEnabled()=true;
}

// Relocate only the player's party onto distinct authored ground near a real
// NPC. Keep the real NPC identity, group, route and current edge intact.
inline bool marketStandNear(Market::Session& s,int target) {
    const auto saved=s;const int player=s.data().residentCount,partner=player+1;
    const int tx=s.actor(target).to%Market::WIDTH,ty=s.actor(target).to/Market::WIDTH;
    const auto open=[&](int x,int y) {
        if(x<0||x>=Market::WIDTH||y<0||y>=Market::HEIGHT)return false;
        const auto n=Market::node(x,y);if(!s.data().pedestrian.has(n))return false;
        for(int i=0;i<s.count();++i)if(i!=player&&i!=partner&&s.actor(i).stage!=4&&
            (s.actor(i).from==n||s.actor(i).to==n))return false;
        return true;
    };
    const auto place=[&](int i,int x,int y) {
        auto& a=const_cast<Market::Actor&>(s.actor(i));a.from=a.to=Market::node(x,y);
        a.stage=2;a.goal=Market::NONE;a.motionMs=0;a.progress=0;
    };
    for(int dy:{2,1,0,-1,-2})for(int dx:{0,1,-1,2,-2}) {
        if(dx*dx+dy*dy>6||!open(tx+dx,ty+dy))continue;
        place(player,tx+dx,ty+dy);
        if(s.nearbyNpc(target,true)!=target)continue;
        for(int px:{-1,1})if(open(tx+dx+px,ty+dy)) {
            place(partner,tx+dx+px,ty+dy);return true;
        }
    }
    s=saved;return false;
}
inline int marketPairNear(Market::Session& s) {
    const int first=s.data().residentCount+2;
    for(int g=0;g<s.data().maxGroups;++g)if(s.visitPhase(g)==Market::VisitPhase::VISITING) {
        const int i=first+2*g;
        if(s.actor(i).species&&s.actor(i+1).species&&s.actor(i).stage==2&&s.actor(i+1).stage==2&&marketStandNear(s,i))return i;
    }
    return -1;
}
