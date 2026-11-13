#pragma once

#include <algorithm>
#include <cmath>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "assets/PokemonSprites.h"
#include "game/MarketSession.h"
#include "game/ExploreTileSemantics.h"
#include "game/ExploreDaylight.h"
#include "presentation/PixelRenderer.h"
#include "presentation/PokemonShadow.h"
#include "ui/SceneSurface.h"

namespace Ui::MarketView {
inline constexpr const char* NAME = Ui::MarketText::NAME;
inline constexpr int SPEECH_ICON_SIZE=21;
struct Viewport {
    int width=240,height=135;
    float scale=1;
    bool round=false;
    bool freeWalk=false;
    int controlScale=1;
    int cameraOffsetY=0;
    int pixel(float world) const {return int(std::lround(world*scale));}
    struct Button { int x,y,width,height; };
    int footer() const { return freeWalk?0:round?80:scale==2?56:24; }
    int mapHeight() const { return height-footer(); }
    Button button(int index) const {
        return round ? Button{(width-216)/2+index*74,mapHeight(),68,36} :
            Button{index*width/3,mapHeight(),(index+1)*width/3-index*width/3,footer()};
    }
    int cameraX(const Market::Session& s) const {
        const int world=pixel(Market::CELL_WIDTH*26);
        return std::clamp(pixel(s.playerPose().x)-width/2,
                          std::min(0,(world-width)/2),std::max(0,world-width));
    }
    int cameraY(const Market::Session& s) const {
        const int world=pixel(Market::CELL_HEIGHT*26);
        if(freeWalk&&mapHeight()>world)return (world-mapHeight())/2+cameraOffsetY;
        return std::clamp(pixel(s.playerPose().y)-mapHeight()/2,
                          std::min(0,(world-mapHeight())/2),std::max(0,world-mapHeight()))+cameraOffsetY;
    }
};

inline Viewport sceneViewport() {
    return {Ui::SceneSurface::width(),Ui::SceneSurface::height(),1,Ui::family==Ui::Family::ROUND_TOUCH};
}

inline bool preload(const Market::Data& d,uint16_t player,uint16_t companion) {
    uint16_t ids[40]{};uint8_t count=0;
    bool overflow=false;
    const auto add=[&](uint16_t id) {
        if(!id)return;
        for(int i=0;i<count;++i)if(ids[i]==id)return;
        if(count<40)ids[count++]=id;else overflow=true;
    };
    add(player);add(companion);
    for(int i=0;i<d.residentCount;++i)add(d.residents[i].species);
    for(int i=0;i<d.familyCount;++i){add(d.families[i].adult);add(d.families[i].juvenile);}
    if(overflow)return false;
    PokemonSprites::setPinnedDynamicSpecies(ids,count);
    return PokemonSprites::preloadDynamicSpecies(ids,count);
}
namespace detail { inline void releaseStaticLayer(); }
inline void release() {
    detail::releaseStaticLayer();
    PokemonSprites::setPinnedDynamicSpecies(nullptr,0);
    PokemonSprites::setDynamicSceneSpecies(nullptr,0);
}

// Temporary on-device market profiling. One log line every three seconds
// while the market is rendering; set STICKMON_MARKET_PERF_LOG=0 to remove.
#ifndef STICKMON_MARKET_PERF_LOG
#define STICKMON_MARKET_PERF_LOG 1
#endif
namespace perf {
struct Frame { uint32_t blit=0,restore=0,water=0,actors=0,tint=0; uint16_t actorCount=0,waterCells=0,rects=0; };
struct Window {
    uint32_t startUs=0,lastRenderUs=0,frames=0,cachedFrames=0,gapCount=0,gapMax=0,over50=0,renderMax=0;
    uint64_t render=0,gaps=0,blit=0,restore=0,water=0,actors=0,tint=0,actorCount=0,waterCells=0,rects=0;
};
struct Events {
    uint32_t buildUs=0,builds=0,tintUs=0,tintBuilds=0;
    const char* fallback="none";
};
inline Window& window() {static Window w;return w;}
inline Events& events() {static Events e;return e;}
inline Frame& current() {static Frame f;return f;}
inline uint32_t micros() {return Platform::clock().micros();}
inline void fallback(const char* reason) {events().fallback=reason;}
inline void finish(uint32_t startedUs,bool cached) {
#if STICKMON_MARKET_PERF_LOG
    const uint32_t nowUs=micros();
    auto& w=window();auto& f=current();
    if(!w.startUs)w.startUs=startedUs;
    if(w.lastRenderUs) {
        const uint32_t gap=startedUs-w.lastRenderUs;
        // Pauses longer than a second are scene changes, not frames.
        if(gap<1000000U){w.gaps+=gap;++w.gapCount;w.gapMax=std::max(w.gapMax,gap);w.over50+=gap>50000U;}
    }
    w.lastRenderUs=startedUs;
    const uint32_t render=nowUs-startedUs;
    ++w.frames;w.render+=render;w.renderMax=std::max(w.renderMax,render);
    if(cached) {
        ++w.cachedFrames;w.blit+=f.blit;w.restore+=f.restore;w.water+=f.water;w.actors+=f.actors;w.tint+=f.tint;
        w.actorCount+=f.actorCount;w.waterCells+=f.waterCells;w.rects+=f.rects;
    }
    f=Frame{};
    const uint32_t elapsed=nowUs-w.startUs;
    if(elapsed<3000000U)return;
    const auto avg=[](uint64_t total,uint32_t count){return unsigned(count?total/count:0);};
    const unsigned fps10=unsigned(uint64_t(w.gapCount)*10000000ULL/std::max<uint64_t>(1,w.gaps));
    auto& e=events();
    Platform::logf("[MarketPerf] path=%s fps=%u.%u gap(avg/max)=%u/%u us over50ms=%u render(avg/max)=%u/%u us "
                   "blit=%u restore=%u water=%u actors=%u tint=%u us n(actors/water/rects)=%u/%u/%u "
                   "build=%u us x%u tintBuild=%u us x%u fallback=%s psramFree=%u KB\n",
                   w.cachedFrames==w.frames?"cache":w.cachedFrames?"mixed":"legacy",fps10/10,fps10%10,
                   avg(w.gaps,w.gapCount),unsigned(w.gapMax),unsigned(w.over50),avg(w.render,w.frames),unsigned(w.renderMax),
                   avg(w.blit,w.cachedFrames),avg(w.restore,w.cachedFrames),avg(w.water,w.cachedFrames),
                   avg(w.actors,w.cachedFrames),avg(w.tint,w.cachedFrames),
                   avg(w.actorCount,w.cachedFrames),avg(w.waterCells,w.cachedFrames),avg(w.rects,w.cachedFrames),
                   unsigned(e.buildUs),unsigned(e.builds),unsigned(e.tintUs),unsigned(e.tintBuilds),e.fallback,
                   unsigned(Platform::memory().externalFree()/1024));
    const uint32_t last=w.lastRenderUs;w=Window{};w.startUs=nowUs;w.lastRenderUs=last;
#else
    (void)startedUs;(void)cached;
#endif
}
} // namespace perf

namespace detail {
// One visible actor, resolved exactly as the original single-pass renderer did.
struct ActorDraw {
    const PokemonSprites::SpriteFrame* sprite=nullptr;
    PokemonShadow::Geometry shadow{};
    float depth=0,spriteScale=1;
    int x=0,y=0,w=0,h=0,bubbleX=0,bubbleY=0;
    bool flip=false,speaking=false;
    int l=0,t=0,r=0,b=0;                 // occluder redraw clip
    int dirtyL=0,dirtyT=0,dirtyR=0,dirtyB=0; // every pixel the actor may write
};

inline int sortedPoses(const Market::Session& session,Market::Pose* poses,uint8_t* order) {
    uint8_t count=0;
    for(int i=0;i<session.count();++i) {
        poses[i]=session.pose(i);
        if(!poses[i].visible)continue;
        int j=count;
        while(j>0&&poses[order[j-1]].y>poses[i].y){order[j]=order[j-1];--j;}
        order[j]=uint8_t(i);++count;
    }
    return count;
}

inline bool prepareActor(const Market::Session& session,const Viewport& v,uint32_t now,int cx,int cy,
                         const Market::Pose& p,ActorDraw& a) {
    PokemonSprites::WalkingAnimation animation{};
    const PokemonSprites::SpriteFrame* sprite=nullptr;bool flip=false;
    if(PokemonSprites::walkingAnimation(p.species,PokemonSprites::WalkDirection(p.direction),animation)) {
        const int index=p.moving&&!session.paused()?(now/150)%animation.frameCount:0;
        sprite=PokemonSprites::findCachedSpeciesSprite(p.species,PokemonSprites::SpriteKind(uint16_t(animation.base)+index));
        flip=animation.flipX;
    }
    if(!sprite)sprite=PokemonSprites::findCachedSpeciesSprite(p.species,PokemonSprites::SpriteKind::ICON_0);
    if(!sprite)return false;
    const int footX=v.pixel(p.x)-cx,footY=v.pixel(p.y)-cy;
    const float spriteScale=(v.freeWalk?1.0f:0.8f)*v.scale;
    const int w=int(std::lround(sprite->width*spriteScale)),h=int(std::lround(sprite->height*spriteScale));
    const int x=footX-w/2,y=footY-int(PokemonSprites::frameGroundOffsetY(sprite)*spriteScale)-h/2-int(std::lround(p.lift*v.scale));
    const int bubbleSize=v.pixel(SPEECH_ICON_SIZE),bubbleX=footX-bubbleSize/2;
    const auto visible=p.speaking?PokemonSprites::frameVisibleBounds(sprite):PokemonSprites::FrameBounds{};
    const int bubbleY=y+int(std::lround(visible.top*spriteScale))-2*v.scale-bubbleSize;
    if(x+w<0||x>=v.width||y+h<0||(p.speaking?bubbleY:y)>=v.mapHeight())return false;
    const auto shadow=PokemonShadow::atGround(sprite,footX,footY,spriteScale,flip);
    a.sprite=sprite;a.flip=flip;a.speaking=p.speaking;a.depth=p.y;a.spriteScale=spriteScale;
    a.x=x;a.y=y;a.w=w;a.h=h;a.bubbleX=bubbleX;a.bubbleY=bubbleY;a.shadow=shadow;
    a.l=std::max(0,std::min(p.speaking?std::min(x,bubbleX):x,shadow.x-shadow.radiusX));
    a.t=std::max(0,std::min(p.speaking?bubbleY:y,shadow.y-shadow.radiusY));
    a.r=std::min(v.width,std::max(p.speaking?std::max(x+w,bubbleX+bubbleSize):x+w,shadow.x+shadow.radiusX+1));
    a.b=std::min(v.mapHeight(),std::max(y+h,shadow.y+shadow.radiusY+1));
    // Conservative write bounds; oversizing is harmless, undersizing is not.
    constexpr int pad=2;
    int dl=std::min(x,shadow.x-shadow.radiusX),dt=std::min(y,shadow.y-shadow.radiusY);
    int dr=std::max(x+w,shadow.x+shadow.radiusX+1),db=std::max(y+h,shadow.y+shadow.radiusY+1);
    if(p.speaking){dl=std::min(dl,bubbleX);dt=std::min(dt,bubbleY);dr=std::max(dr,bubbleX+bubbleSize);db=std::max(db,bubbleY+bubbleSize);}
    a.dirtyL=dl-pad;a.dirtyT=dt-pad;a.dirtyR=dr+pad;a.dirtyB=db+pad;
    return true;
}

inline void drawActor(const Market::Data& d,const Viewport& v,int cx,int cy,int tile,uint8_t frame,const ActorDraw& a) {
    auto& c=PixelRenderer::canvas();
    c.fillEllipse(a.shadow.x,a.shadow.y,a.shadow.radiusX,a.shadow.radiusY,0x4250);
    PokemonSprites::drawFrameScaled(a.sprite,a.x,a.y,a.spriteScale,a.flip,v.freeWalk);
    if(a.speaking)GameAssets::draw(GameAssets::Kind::MARKET_CHAT,a.bubbleX,a.bubbleY,float(SPEECH_ICON_SIZE)/28*v.scale);
    if(a.l<a.r&&a.t<a.b) {
        c.setClipRect(a.l,a.t,a.r-a.l,a.b-a.t);
        for(int z=1;z<3;++z)for(int yy=std::max(0,(a.t+cy)/tile);yy<std::min(Market::CELL_HEIGHT,(a.b+cy+tile-1)/tile);++yy)
            for(int xx=std::max(0,(a.l+cx)/tile);xx<std::min(Market::CELL_WIDTH,(a.r+cx+tile-1)/tile);++xx) {
                const auto id=d.map.layers[z][yy*Market::CELL_WIDTH+xx];
                if(id&&ExploreTileSemantics::objectOccludes(id,uint8_t(z),yy,a.depth,26))
                    GameAssets::drawExploreTileTo(c,id,xx*tile-cx,yy*tile-cy,frame,v.scale);
            }
        c.setClipRect(0,0,v.width,v.mapHeight());
    }
}

// Free-walk ground cache. Every tile layer is composed once per market visit
// at the viewport scale (plus a copy pre-multiplied by the current daylight
// tone). Per frame the view is copied row by row; only animated cells and
// actor rectangles are redrawn, and only those rectangles are tinted.
struct StaticLayer {
    static constexpr int MAX_ROWS=Market::CELL_HEIGHT+16;
    const Market::Data* data=nullptr;
    uint32_t signature=0;
    float scale=0;
    int tile=0,width=0,mapHeight=0,rowBegin=0,rowEnd=0,pixelWidth=0,pixelHeight=0;
    bool swapped=false,ready=false,tintedValid=false;
    uint16_t* plain=nullptr;
    uint16_t* tinted=nullptr;
    ExploreDaylight::Tone tone{};
    uint8_t animated[Market::CELL_WIDTH*MAX_ROWS]{};
};
inline StaticLayer& staticLayer() {static StaticLayer layer;return layer;}
// Host checks switch this off to compare against the original renderer.
inline bool& staticLayerEnabled() {static bool enabled=true;return enabled;}
inline void releaseStaticLayer() {
    auto& s=staticLayer();
    if(s.plain)Platform::memory().release(s.plain);
    if(s.tinted)Platform::memory().release(s.tinted);
    s=StaticLayer{};
}
inline uint16_t cellTile(const Market::Data& d,int x,int y,int z) {
    if(y>=0&&y<Market::CELL_HEIGHT)return d.map.layers[z][y*Market::CELL_WIDTH+x];
    if(z)return 0; // Extended rows repeat ground only.
    const auto id=d.map.layers[0][std::clamp(y,0,Market::CELL_HEIGHT-1)*Market::CELL_WIDTH+x];
    return id?id:385;
}

// Native-size probe: a tile is animated when any of the eight frames differs,
// and the cache is refused when a tile paints outside its own cell.
inline bool classifyTiles(const Market::Data& d,StaticLayer& s) {
    // Half a cell of margin on every side catches any oversized footprint.
    constexpr int N=26,M=N/2,S=N+2*M,MAX_IDS=Market::CELL_WIDTH*StaticLayer::MAX_ROWS*3;
    static uint16_t ids[MAX_IDS];static uint16_t animatedIds[MAX_IDS];
    int count=0,animatedCount=0;
    for(int z=0;z<3;++z)for(int y=s.rowBegin;y<s.rowEnd;++y)for(int x=0;x<Market::CELL_WIDTH;++x)
        if(const auto id=cellTile(d,x,y,z))ids[count++]=id;
    std::sort(ids,ids+count);count=int(std::unique(ids,ids+count)-ids);
    auto* buffer=static_cast<uint16_t*>(Platform::memory().allocate(S*S*2*sizeof(uint16_t),false));
    if(!buffer)buffer=static_cast<uint16_t*>(Platform::memory().allocate(S*S*2*sizeof(uint16_t),true));
    if(!buffer)return false;
    uint16_t* reference=buffer;uint16_t* current=buffer+S*S;
    Canvas565 probe;
    const auto paint=[&](uint16_t* pixels,uint16_t id,uint8_t frame,uint16_t sentinel) {
        probe.attach({pixels,uint16_t(S),uint16_t(S),false});
        std::fill(pixels,pixels+S*S,sentinel);
        GameAssets::drawExploreTileTo(probe,id,M,M,frame,1.0f);
    };
    bool ok=true;
    for(int i=0;i<count&&ok;++i) {
        bool animatedTile=false;
        const auto inside=[&](const uint16_t* pixels,uint16_t sentinel) {
            const auto clear=[&](const uint16_t* begin,int length) {
                return std::all_of(begin,begin+length,[&](uint16_t p){return p==sentinel;});
            };
            if(!clear(pixels,M*S)||!clear(pixels+(M+N)*S,M*S))return false;
            for(int y=M;y<M+N;++y)
                if(!clear(pixels+y*S,M)||!clear(pixels+y*S+M+N,M))return false;
            return true;
        };
        for(const uint16_t sentinel:{uint16_t(0x0000),uint16_t(0xffff)}) {
            paint(reference,ids[i],0,sentinel);
            ok=ok&&inside(reference,sentinel);
            for(uint8_t frame=1;frame<8&&ok;++frame) {
                paint(current,ids[i],frame,sentinel);
                ok=inside(current,sentinel);
                animatedTile|=!std::equal(reference,reference+S*S,current);
            }
        }
        if(animatedTile)animatedIds[animatedCount++]=ids[i];
    }
    Platform::memory().release(buffer);
    if(!ok)return false;
    for(int y=s.rowBegin;y<s.rowEnd;++y)for(int x=0;x<Market::CELL_WIDTH;++x) {
        bool moving=false;
        for(int z=0;z<3&&!moving;++z) {
            const auto id=cellTile(d,x,y,z);
            moving=id&&std::binary_search(animatedIds,animatedIds+animatedCount,id);
        }
        s.animated[(y-s.rowBegin)*Market::CELL_WIDTH+x]=moving;
    }
    return true;
}

inline uint32_t mapSignature(const Market::Data& d) {
    uint32_t hash=2166136261U;
    for(const auto& layer:d.map.layers)for(const auto id:layer)hash=(hash^id)*16777619U;
    return hash;
}

inline bool prepareStaticLayer(const Market::Data& d,const Viewport& v,int tile,bool swapped) {
    auto& s=staticLayer();
    const uint32_t signature=mapSignature(d);
    if(s.data==&d&&s.signature==signature&&s.scale==v.scale&&s.tile==tile&&s.width==v.width&&s.mapHeight==v.mapHeight()&&s.swapped==swapped)
        return s.ready; // A failed build is not retried every frame.
    releaseStaticLayer();
    const uint32_t buildStartedUs=perf::micros();
    s.data=&d;s.signature=signature;s.scale=v.scale;s.tile=tile;s.width=v.width;s.mapHeight=v.mapHeight();s.swapped=swapped;
    // Rows reachable by any camera position, using the free-walk camera rules.
    const int world=v.pixel(Market::CELL_HEIGHT*26),height=v.mapHeight();
    const int yMin=world<height?(world-height)/2:0,yMax=world<height?yMin:world-height;
    s.rowBegin=int(std::floor(float(yMin)/tile));s.rowEnd=int(std::ceil(float(yMax+height)/tile));
    const int rows=s.rowEnd-s.rowBegin;
    s.pixelWidth=Market::CELL_WIDTH*tile;s.pixelHeight=rows*tile;
    if(tile<=0||rows<=0||rows>StaticLayer::MAX_ROWS||s.pixelWidth>65535||s.pixelHeight>65535){perf::fallback("geometry");return false;}
    if(!classifyTiles(d,s)){perf::fallback("tile-footprint-or-probe-alloc");return false;}
    s.plain=static_cast<uint16_t*>(Platform::memory().allocate(size_t(s.pixelWidth)*s.pixelHeight*sizeof(uint16_t),true));
    if(!s.plain){perf::fallback("cache-alloc");return false;}
    Canvas565 layer;layer.attach({s.plain,uint16_t(s.pixelWidth),uint16_t(s.pixelHeight),swapped});
    layer.fillRect(0,0,s.pixelWidth,s.pixelHeight,0x2227);
    auto& screen=PixelRenderer::canvas();
    for(int z=0;z<3;++z)for(int y=s.rowBegin;y<s.rowEnd;++y)for(int x=0;x<Market::CELL_WIDTH;++x) {
        const auto id=cellTile(d,x,y,z);
        if(!id||GameAssets::drawExploreTileTo(layer,id,x*tile,(y-s.rowBegin)*tile,0,v.scale))continue;
        // Off-screen canvases have no packed fallback. If the screen could
        // still draw this id (decode cache exhausted), keep the old renderer.
        screen.setClipRect(0,0,0,0);
        const bool drawable=GameAssets::drawExploreTileTo(screen,id,0,0,0,v.scale);
        screen.clearClipRect();
        if(drawable){Platform::memory().release(s.plain);s.plain=nullptr;perf::fallback("tile-decode");return false;}
    }
    s.ready=true;
    auto& e=perf::events();e.buildUs=perf::micros()-buildStartedUs;++e.builds;e.fallback="none";
    return true;
}

inline bool prepareTinted(StaticLayer& s,ExploreDaylight::Tone tone) {
    if(s.tintedValid&&s.tone==tone)return true;
    const uint32_t startedUs=perf::micros();
    const size_t pixels=size_t(s.pixelWidth)*s.pixelHeight;
    if(!s.tinted)s.tinted=static_cast<uint16_t*>(Platform::memory().allocate(pixels*sizeof(uint16_t),true));
    if(!s.tinted){perf::fallback("tint-alloc");return false;}
    std::copy(s.plain,s.plain+pixels,s.tinted);
    Canvas565 layer;layer.attach({s.tinted,uint16_t(s.pixelWidth),uint16_t(s.pixelHeight),s.swapped});
    layer.multiplyRectRgb(0,0,s.pixelWidth,s.pixelHeight,tone.red,tone.green,tone.blue);
    s.tone=tone;s.tintedValid=true;
    auto& e=perf::events();e.tintUs=perf::micros()-startedUs;++e.tintBuilds;
    return true;
}

struct Rect { int l,t,r,b; };

// Copy the cache window into screen rect [l,r)x[t,b); outside the cached
// world the original background colour is used.
inline void copyFromLayer(const StaticLayer& s,const uint16_t* source,int cx,int cy,const Rect& rect) {
    auto& c=PixelRenderer::canvas();
    uint16_t* screen=c.rawPixels();const int stride=c.physicalWidth();
    const uint16_t background=c.encodeColor(0x2227);
    const int left=-cx,right=s.pixelWidth-cx,originY=s.rowBegin*s.tile-cy;
    const int x0=std::clamp(left,rect.l,rect.r),x1=std::clamp(right,x0,rect.r);
    for(int y=rect.t;y<rect.b;++y) {
        uint16_t* row=screen+y*stride;
        const int sourceY=y-originY;
        if(sourceY<0||sourceY>=s.pixelHeight){std::fill(row+rect.l,row+rect.r,background);continue;}
        std::fill(row+rect.l,row+x0,background);
        if(x1>x0)std::copy(source+size_t(sourceY)*s.pixelWidth+(x0+cx),source+size_t(sourceY)*s.pixelWidth+(x1+cx),row+x0);
        std::fill(row+x1,row+rect.r,background);
    }
}

// Tint the union of rects once per pixel: split into horizontal bands at
// every rect edge, merge the spans active in each band.
inline void tintUnion(const Rect* rects,int count,ExploreDaylight::Tone tone) {
    auto& c=PixelRenderer::canvas();
    static int edges[1024];static Rect spans[512];
    int edgeCount=0;
    for(int i=0;i<count;++i){edges[edgeCount++]=rects[i].t;edges[edgeCount++]=rects[i].b;}
    std::sort(edges,edges+edgeCount);edgeCount=int(std::unique(edges,edges+edgeCount)-edges);
    for(int e=0;e+1<edgeCount;++e) {
        const int top=edges[e],bottom=edges[e+1];int spanCount=0;
        for(int i=0;i<count;++i)if(rects[i].t<=top&&rects[i].b>=bottom)spans[spanCount++]=rects[i];
        std::sort(spans,spans+spanCount,[](const Rect& a,const Rect& b){return a.l<b.l;});
        for(int i=0;i<spanCount;) {
            int l=spans[i].l,r=spans[i].r;
            for(++i;i<spanCount&&spans[i].l<=r;++i)r=std::max(r,spans[i].r);
            c.multiplyRectRgb(l,top,r-l,bottom-top,tone.red,tone.green,tone.blue);
        }
    }
}

inline bool renderStatic(const Market::Session& session,const Viewport& v,uint32_t now,uint32_t gameMinutes,
                         int cx,int cy,int tile,uint8_t frame) {
    auto& c=PixelRenderer::canvas();
    const int mapHeight=v.mapHeight();
    if(!staticLayerEnabled()||!v.freeWalk||c.renderScale()!=1||!c.rawPixels()||
       v.width>c.width()||mapHeight>c.height()){perf::fallback("viewport");return false;}
    const auto& d=session.data();
    if(!prepareStaticLayer(d,v,tile,c.byteSwapped()))return false;
    auto& s=staticLayer();
    // Dialogue can pan beyond the cache. Keep its world pixels and redraw
    // only the missing edge-ground rows; do not rebuild or enlarge the cache.
    constexpr int CAPACITY=512;
    static Rect rects[CAPACITY];
    int count=0;bool overflow=false;
    const auto add=[&](int l,int t,int r,int b) {
        l=std::max(l,0);t=std::max(t,0);r=std::min(r,v.width);b=std::min(b,mapHeight);
        if(l>=r||t>=b)return;
        if(count<CAPACITY)rects[count++]={l,t,r,b};else overflow=true;
    };
    // Uncached bands start as background; edge ground is drawn below and
    // included here so daylight also covers those pixels exactly once.
    const int winL=-cx,winT=s.rowBegin*tile-cy,winR=s.pixelWidth-cx,winB=s.rowEnd*tile-cy;
    add(0,0,v.width,winT);add(0,winB,v.width,mapHeight);
    add(0,winT,winL,winB);add(winR,winT,v.width,winB);
    const int left=std::max(0,cx/tile),right=std::min(Market::CELL_WIDTH,(cx+v.width+tile-1)/tile);
    const int top=int(std::floor(float(cy)/tile));
    const int bottom=int(std::ceil(float(cy+mapHeight)/tile));
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)
        if(y>=s.rowBegin&&y<s.rowEnd&&s.animated[(y-s.rowBegin)*Market::CELL_WIDTH+x])
            add(x*tile-cx,y*tile-cy,x*tile-cx+tile,y*tile-cy+tile);
    Market::Pose poses[Market::CAPACITY]{};uint8_t order[Market::CAPACITY]{};
    ActorDraw actors[Market::CAPACITY];int actorCount=0;
    const int visible=sortedPoses(session,poses,order);
    for(int i=0;i<visible;++i) {
        auto& a=actors[actorCount];
        if(!prepareActor(session,v,now,cx,cy,poses[order[i]],a))continue;
        add(a.dirtyL,a.dirtyT,a.dirtyR,a.dirtyB);++actorCount;
    }
    const auto tone=ExploreDaylight::at(gameMinutes,0);
    const bool tint=tone!=ExploreDaylight::Tone{};
    const bool tinted=tint&&!overflow&&prepareTinted(s,tone);
    auto& f=perf::current();
    uint32_t mark=perf::micros();
    const auto lap=[&]{const uint32_t t=perf::micros(),elapsed=t-mark;mark=t;return elapsed;};
    copyFromLayer(s,tinted?s.tinted:s.plain,cx,cy,{0,0,v.width,mapHeight});
    f.blit=lap();
    if(tinted)for(int i=0;i<count;++i)copyFromLayer(s,s.plain,cx,cy,rects[i]);
    f.restore=lap();
    for(int y=top;y<bottom;++y)for(int x=left;x<right;++x) {
        if(y>=s.rowBegin&&y<s.rowEnd&&!s.animated[(y-s.rowBegin)*Market::CELL_WIDTH+x])continue;
        const int l=std::max(0,x*tile-cx),t=std::max(0,y*tile-cy);
        const int r=std::min(v.width,x*tile-cx+tile),b=std::min(mapHeight,y*tile-cy+tile);
        if(l>=r||t>=b)continue;
        c.setClipRect(l,t,r-l,b-t);
        c.fillRect(l,t,r-l,b-t,0x2227);
        for(int z=0;z<3;++z)if(const auto id=cellTile(d,x,y,z))
            GameAssets::drawExploreTileTo(c,id,x*tile-cx,y*tile-cy,frame,v.scale);
        ++f.waterCells;
    }
    f.water=lap();
    c.setClipRect(0,0,v.width,mapHeight);
    for(int i=0;i<actorCount;++i)drawActor(d,v,cx,cy,tile,frame,actors[i]);
    f.actors=lap();
    c.setClipRect(0,0,v.width,mapHeight);
    if(tinted)tintUnion(rects,count,tone);
    else if(tint)c.multiplyRectRgb(0,0,v.width,mapHeight,tone.red,tone.green,tone.blue);
    f.tint=lap();
    f.actorCount=uint16_t(actorCount);f.rects=uint16_t(count);
    return true;
}
} // namespace detail

inline void render(const Market::Session& session,const Viewport& v,uint32_t now,uint32_t gameMinutes,uint32_t animationOffsetMs=0) {
    const uint32_t perfStartedUs=perf::micros();
    now=session.timeMs()+animationOffsetMs;
    auto& c=PixelRenderer::canvas();
    const auto assetScale=c.assetScale();c.setAssetScale(1);
    const auto& map=session.data().map;
    const int cx=v.cameraX(session),cy=v.cameraY(session),tile=v.pixel(26);
    const uint8_t frame=uint8_t(now/140%8);
    if(detail::renderStatic(session,v,now,gameMinutes,cx,cy,tile,frame)) {
        c.clearClipRect();c.setAssetScale(assetScale);perf::finish(perfStartedUs,true);return;
    }
    c.setClipRect(0,0,v.width,v.mapHeight());c.fillRect(0,0,v.width,v.mapHeight(),0x2227);
    const int left=std::max(0,cx/tile),right=std::min(Market::CELL_WIDTH,(cx+v.width+tile-1)/tile);
    const int top=v.freeWalk?int(std::floor(float(cy)/tile)):std::max(0,cy/tile);
    const int bottom=v.freeWalk?int(std::ceil(float(cy+v.mapHeight())/tile)):std::min(Market::CELL_HEIGHT,(cy+v.mapHeight()+tile-1)/tile);
    const auto target=session.selectedTarget();
    for(int z=0;z<3;++z) {
        for(int y=top;y<bottom;++y)for(int x=left;x<right;++x) {
            if(y<0||y>=Market::CELL_HEIGHT) {
                // Extend ground only: stalls, signs and fences are never tiled.
                if(z==0&&v.freeWalk) {
                    const auto id=map.layers[0][std::clamp(y,0,Market::CELL_HEIGHT-1)*Market::CELL_WIDTH+x];
                    GameAssets::drawExploreTileTo(c,id?id:385,x*tile-cx,y*tile-cy,frame,v.scale);
                }
                continue;
            }
            const auto id=map.layers[z][y*Market::CELL_WIDTH+x];
            if(id)GameAssets::drawExploreTileTo(c,id,x*tile-cx,y*tile-cy,frame,v.scale);
        }
        if(z==0&&!v.freeWalk)c.drawCircle(v.pixel(target%Market::WIDTH*13)-cx,v.pixel(target/Market::WIDTH*13)-cy,v.pixel(4),0xffe0);
    }
    Market::Pose poses[Market::CAPACITY]{};uint8_t order[Market::CAPACITY]{};
    const int count=detail::sortedPoses(session,poses,order);
    for(int i=0;i<count;++i) {
        detail::ActorDraw a;
        if(detail::prepareActor(session,v,now,cx,cy,poses[order[i]],a))
            detail::drawActor(session.data(),v,cx,cy,tile,frame,a);
    }
    const auto tone=ExploreDaylight::at(gameMinutes,0);
    c.multiplyRectRgb(0,0,v.width,v.mapHeight(),tone.red,tone.green,tone.blue);
    c.clearClipRect();
    if(v.freeWalk){c.setAssetScale(assetScale);perf::finish(perfStartedUs,false);return;}
    c.fillRect(0,v.mapHeight(),v.width,v.footer(),0x08a3);
    const char* labels[]={Ui::BACK,session.paused()?Ui::MarketText::CONTINUE:Ui::MarketText::PAUSE,Ui::MarketText::WALK};
    for(int i=0;i<3;++i) {
        const auto button=v.button(i);
        if(v.round)c.fillRoundRect(button.x,button.y,button.width,button.height,8,0x2187);
        const int textWidth=32*v.scale;
        PixelRenderer::text(button.x+(button.width-textWidth)/2,button.y+(v.round?10:4*v.scale),labels[i],0xffff);
    }
    c.setAssetScale(assetScale);
    perf::finish(perfStartedUs,false);
}

inline int targetAt(const Market::Session& s,const Viewport& v,int x,int y) {
    if(x<0||x>=v.width||y<0||y>=v.height)return -1;
    if(v.round) {
        const int dx=2*x-v.width+1,dy=2*y-v.height+1;
        if(dx*dx+dy*dy>=v.width*v.width)return -1;
    }
    if(y>=v.mapHeight()) {
        for(int i=0;i<3;++i) {
            const auto b=v.button(i);
            if(x>=b.x&&x<b.x+b.width&&y>=b.y&&y<b.y+b.height)return Market::NODES+i;
        }
        return -1;
    }
    const auto n=s.nearest(float(x+v.cameraX(s))/v.scale,float(y+v.cameraY(s))/v.scale);
    return n==Market::NONE?-1:int(n);
}
inline void activate(Market::Session& s,int target) {
    if(target==Market::NODES)s.leave();
    else if(target==Market::NODES+1)s.pause(!s.paused());
    else if(target==Market::NODES+2)s.walkSelected();
    else if(target>=0&&target<Market::NODES)s.walkTo(uint16_t(target));
}
} // namespace Ui::MarketView
