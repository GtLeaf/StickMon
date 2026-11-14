#pragma once
#include <algorithm>
#include "assets/PokemonSprites.h"
#include "game/ExplorePool.h"

namespace AmoledV1 {
inline constexpr int POSTCARD_SCENE_Y=0, POSTCARD_SCENE_H=380;
inline constexpr int POSTCARD_POOL_TOP=218, POSTCARD_POOL_BOTTOM=372;
inline constexpr int POSTCARD_POOL_WIDTH=344, POSTCARD_POOL_GAP=12;
inline bool postcardPoolAt(int x,int y) {
    return x>=0 && x<368 && y>=POSTCARD_POOL_TOP && y<POSTCARD_POOL_BOTTOM;
}
inline bool postcardSceneGestureAt(int x,int y) {
    return x>=0 && x<368 && y>=76 && y<POSTCARD_POOL_TOP;
}
struct PostcardPoolClock {
    uint32_t dueAt=0;
    bool armed=false;
    void pause(){armed=false;}
    void manual(uint32_t now){dueAt=now+5000;armed=true;}
    bool tick(uint32_t now,bool eligible){
        if(!eligible){pause();return false;}
        if(!armed){dueAt=now+3000;armed=true;return false;}
        if(static_cast<int32_t>(now-dueAt)<0)return false;
        dueAt=now+3000;return true;
    }
};
struct PostcardPoolLayout {
    uint8_t starts[ExplorePool::POOL_CAP]{};
    uint8_t pages=0,page=0,start=0,count=0;
    uint16_t width=0;
};
// Pending frames reserve the widest existing FRONT until loading completes.
inline PostcardPoolLayout postcardPoolLayout(
        const PokemonSprites::SpriteFrame* const* frames,uint8_t count,uint8_t offset) {
    PostcardPoolLayout result;
    count=std::min<uint8_t>(count,ExplorePool::POOL_CAP);
    if(!count)return result;
    auto width=[&](int i){return frames[i]&&frames[i]->width?int(frames[i]->width):168;};
    result.pages=1;int used=0;
    for(int i=0;i<count;++i){
        const int w=width(i);
        if(used && used+POSTCARD_POOL_GAP+w>POSTCARD_POOL_WIDTH){
            result.starts[result.pages++]=uint8_t(i);used=0;
        }
        used+=(used?POSTCARD_POOL_GAP:0)+w;
    }
    if(offset<count)for(int i=1;i<result.pages;++i)
        if(result.starts[i]<=offset)result.page=uint8_t(i);
    result.start=result.starts[result.page];
    const int end=result.page+1<result.pages?result.starts[result.page+1]:count;
    result.count=uint8_t(end-result.start);
    for(int i=result.start;i<end;++i)result.width+=(i>result.start?POSTCARD_POOL_GAP:0)+width(i);
    return result;
}
}
