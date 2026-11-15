#include "ui/ExplorePostcardLayout.h"
#include <cassert>
#include <cstdio>
using namespace AmoledV1;
static void cases(){
    PokemonSprites::SpriteFrame f[6]{};const PokemonSprites::SpriteFrame* frames[6]{};
    for(int i=0;i<6;i++){f[i].height=132;frames[i]=&f[i];}
    auto layout=[&](std::initializer_list<int> widths,uint8_t offset=0){int i=0;for(auto w:widths)f[i++].width=w;return postcardPoolLayout(frames,i,offset);};
    auto p=layout({60,64,58,78,53});assert(p.pages==2&&p.count==3&&p.width==206&&p.starts[1]==3);
    p=layout({168,138,96});assert(p.pages==2&&p.count==1&&p.width==168&&p.starts[1]==1);
    p=layout({168,138,96},2);assert(p.page==1&&p.count==2&&p.width==246);
    p=layout({126,130,112});assert(p.count==2&&p.width==268&&p.starts[1]==2);
    p=layout({130,130});assert(p.pages==1&&p.width==272&&p.count==2);
    p=layout({131,130});assert(p.pages==2&&p.count==1);
    p=postcardPoolLayout(frames,0,0);assert(p.pages==0&&p.count==0);
    frames[0]=nullptr;p=postcardPoolLayout(frames,1,0);assert(p.pages==1&&p.width==168&&p.count==1);
    // Expanded56px toggle stays inside the card; the visual control is48px.
    assert(postcardPetsToggleAt(282,90)&&postcardPetsToggleAt(337,145));
    assert(!postcardPetsToggleAt(281,118)&&!postcardPetsToggleAt(338,118));
    assert(!postcardPetsToggleAt(310,89)&&!postcardPetsToggleAt(310,146));
    assert(postcardPoolAt(36,150)&&postcardPoolAt(331,327));
    assert(!postcardPoolAt(184,149)&&!postcardPoolAt(184,328));
    assert(!postcardPoolAt(368,300)&&!postcardPoolAt(184,388));

}
int main(){cases();std::puts("postcard-layout: native widths, 272px boundary, dynamic pages, loading placeholders and full-width hit regions OK");}
