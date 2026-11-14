#include "ui/ExplorePostcardLayout.h"
#include <cassert>
#include <cstdio>
using namespace AmoledV1;
static void cases(){
    PokemonSprites::SpriteFrame f[6]{};const PokemonSprites::SpriteFrame* frames[6]{};
    for(int i=0;i<6;i++){f[i].height=132;frames[i]=&f[i];}
    auto layout=[&](std::initializer_list<int> widths,uint8_t offset=0){int i=0;for(auto w:widths)f[i++].width=w;return postcardPoolLayout(frames,i,offset);};
    auto p=layout({60,64,58,78,53});assert(p.pages==2&&p.count==4&&p.width==296&&p.starts[1]==4);
    p=layout({168,138,96});assert(p.pages==2&&p.count==2&&p.width==318&&p.starts[1]==2);
    p=layout({168,138,96},2);assert(p.page==1&&p.count==1&&p.width==96);
    p=layout({126,130,112});assert(p.count==2&&p.width==268&&p.starts[1]==2);
    p=layout({166,166});assert(p.pages==1&&p.width==344&&p.count==2);
    p=layout({167,166});assert(p.pages==2&&p.count==1);
    p=postcardPoolLayout(frames,0,0);assert(p.pages==0&&p.count==0);
    frames[0]=nullptr;p=postcardPoolLayout(frames,1,0);assert(p.pages==1&&p.width==168&&p.count==1);
    assert(postcardPoolAt(0,218)&&postcardPoolAt(367,371));
    assert(!postcardPoolAt(368,300)&&!postcardPoolAt(184,388));

}
int main(){cases();std::puts("postcard-layout: native widths, 344px boundary, dynamic pages, loading placeholders and full-width hit regions OK");}
