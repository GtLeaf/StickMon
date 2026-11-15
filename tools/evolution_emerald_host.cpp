#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TintedSpriteRle.h"
#include "ui/ProgressionScreens.h"
#include "ui/UiTheme.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace AmoledV1;
using Frame=std::vector<uint16_t>;
constexpr int W=368,H=448;
constexpr uint16_t BG=0x1826;

static Frame pixels(Canvas565& c) {
    Frame result;result.reserve(W*H);
    for(int y=0;y<H;++y)for(int x=0;x<W;++x)result.push_back(c.readPixel(x,y));
    return result;
}
static uint16_t blend(uint16_t a,uint16_t b,int t) {
    return uint16_t((((((a>>11)&31)*(255-t)+((b>>11)&31)*t+127)/255)<<11)|
        (((((a>>5)&63)*(255-t)+((b>>5)&63)*t+127)/255)<<5)|
        (((a&31)*(255-t)+(b&31)*t+127)/255));
}
static void capture(Canvas565& c,const char* root,const char* name) {
    if(!root)return;
    FILE* f=std::fopen((std::string(root)+"/"+name+".ppm").c_str(),"wb");assert(f);
    std::fprintf(f,"P6\n368 448\n255\n");
    for(auto p:pixels(c)){const unsigned char rgb[]={uint8_t(((p>>11)&31)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};assert(std::fwrite(rgb,1,3,f)==3);}
    std::fclose(f);
}
static void fixture(Canvas565& c) {
    // Transparent, red, green / blue, opaque black, white.
    const uint16_t raw[]={0x8001,5,0xf800,0x07e0,0x001f,0x0000,0xffff};
    const uint16_t indexed[]={0x8001,5,0x3210,0x0004};
    const uint16_t palette[]={0xf800,0x07e0,0x001f,0,0xffff};
    const uint16_t colors[]={0,0xf800,0x07e0,0x001f,0,0xffff};
    for(bool isIndexed:{false,true})for(bool flip:{false,true})for(float scale:{0.5f,1.0f,1.5f})for(int amount:{0,128,255}) {
        Ui::TintedSpriteRle::Source source{isIndexed?indexed:raw,0,uint32_t(isIndexed?4:7),palette,0,5,3,2,isIndexed};
        c.fillSprite(BG);
        assert(Ui::TintedSpriteRle::draw(c,source,30,100,scale,0xffff,uint8_t(amount),flip));
        const int dw=std::max(1,int(std::lround(3*scale))),dh=std::max(1,int(std::lround(2*scale)));
        for(int y=0;y<dh;++y)for(int x=0;x<dw;++x) {
            int sx=std::min(2,int(x/scale)),sy=std::min(1,int(y/scale));if(flip)sx=2-sx;
            const int i=sy*3+sx;
            assert(c.readPixel(30+x,100+y)==(i==0?BG:blend(colors[i],0xffff,amount)));
        }
        assert(c.readPixel(29,100)==BG&&c.readPixel(30+dw,100)==BG);
    }
}
static void sprites(Canvas565& c,Frame& storage) {
    fixture(c);
    for(int id:{1,25,130,133,134,143}) {
        const auto* f=PokemonSprites::findSpeciesSprite(id,PokemonSprites::SpriteKind::FRONT);assert(f);
        const int w=FlashStorage::readByte(&f->width),h=FlashStorage::readByte(&f->height);
        c.fillSprite(BG);assert(PokemonSprites::drawFrame(f,0,0));const auto original=pixels(c);
        c.fillSprite(0);assert(PokemonSprites::drawFrameSilhouette(f,0,0,0xffff));const auto mask=pixels(c);
        for(float scale:{0.27f,0.5f,1.0f,1.5f})for(bool flip:{false,true})for(int amount:{0,128,255}) {
            c.fillSprite(BG);
            assert(PokemonSprites::drawFrameTintedScaled(f,30,100,scale,0xffff,uint8_t(amount),flip));
            const auto full=pixels(c);
            const int dw=std::max(1,int(std::lround(w*scale))),dh=std::max(1,int(std::lround(h*scale)));
            for(int y=0;y<dh;++y)for(int x=0;x<dw;++x) {
                int sx=std::min(w-1,int(x/scale)),sy=std::min(h-1,int(y/scale));if(flip)sx=w-1-sx;
                const int i=sy*W+sx;
                assert(c.readPixel(30+x,100+y)==(mask[i]?blend(original[i],0xffff,amount):BG));
            }
            c.fillSprite(BG);
            for(int top=0;top<H;top+=13){c.setClipRect(0,top,W,std::min(13,H-top));assert(PokemonSprites::drawFrameTintedScaled(f,30,100,scale,0xffff,uint8_t(amount),flip));assert(c.clipTop()==top);}
            c.clearClipRect();assert(pixels(c)==full);
            c.attach({storage.data(),W,H,true});c.fillSprite(BG);
            assert(PokemonSprites::drawFrameTintedScaled(f,30,100,scale,0xffff,uint8_t(amount),flip));assert(pixels(c)==full);
            c.attach({storage.data(),W,H,false});
        }
        for(float invalid:{0.0f,-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            assert(!PokemonSprites::drawFrameTintedScaled(f,0,0,invalid,0xffff,255));
    }
    std::puts("Tinted sprite: raw/indexed transparency and black, fractional nearest/flips/tint/full/dirty/swap OK");
}
static void checkPage(Canvas565& c,Frame& storage,const ProgressionViewModel& model) {
    c.clearClipRect();c.fillSprite(0xf81f);renderProgressionScreen(c,model);const auto full=pixels(c);
    for(int top=0;top<H;top+=19) {
        const int bottom=std::min(H,top+19);c.fillSprite(0xf81f);renderProgressionScreen(c,model,uint16_t(top),uint16_t(bottom));
        for(int y=0;y<H;++y)for(int x=0;x<W;++x)assert(c.readPixel(x,y)==(y>=top&&y<bottom?full[y*W+x]:0xf81f));
        assert(c.clipLeft()==0&&c.clipTop()==0&&c.clipRight()==W&&c.clipBottom()==H);
    }
    c.attach({storage.data(),W,H,true});c.fillSprite(0xf81f);renderProgressionScreen(c,model);assert(pixels(c)==full);
    c.attach({storage.data(),W,H,false});
    c.fillSprite(0xf81f);renderProgressionScreen(c,model);assert(pixels(c)==full);
}
int main(int argc,char** argv) {
    assert(argc>=2);const char* output=nullptr;bool spriteOnly=false;
    for(int i=2;i<argc;++i)if(std::string(argv[i])=="--sprites-only")spriteOnly=true;else output=argv[i];
    DesktopPlatform desktop(argv[1]);Platform::bind(desktop.serviceBundle());
    assert(desktop.begin()&&Platform::resources().mount()&&GameAssets::begin()&&FontResource::ins().begin());
    Frame storage(W*H);auto& c=PixelRenderer::canvas();c.attach({storage.data(),W,H,false});
    c.setCoordinateScale(1);c.setLayoutScale(1);c.setAssetScale(1);c.setNativeText(true);
    sprites(c,storage);if(spriteOnly)return 0;
    ProgressionViewModel m;m.mode=ProgressionViewModel::Mode::EVOLUTION;m.fromSpeciesId=133;m.toSpeciesId=134;
    using Phase=Game::EvolutionSequence::Phase;
    // Save full production frames for a native-size review GIF.
    if(output)for(int t=0;t<5200;t+=50) {
        m.evolutionElapsedMs=t;m.evolutionReady=t>=4000;
        m.evolutionPhase=t<600?Phase::INTRO:t<2840?Phase::MORPH:t<3180?Phase::FLASH:t<3580?Phase::REVEAL:Phase::COMPLETE;
        renderProgressionScreen(c,m);char name[32];std::snprintf(name,sizeof(name),"frame-%03d",t/50);capture(c,output,name);
    }
    const int times[]={400,900,1200,2130,2920,4200};
    for(int i=0;i<6;++i){m.evolutionElapsedMs=times[i];m.evolutionReady=times[i]>=4000;m.evolutionPhase=times[i]<600?Phase::INTRO:times[i]<2840?Phase::MORPH:times[i]<3180?Phase::FLASH:Phase::COMPLETE;renderProgressionScreen(c,m);char name[24];std::snprintf(name,sizeof(name),"key-%02d",i+1);capture(c,output,name);}
    assert(c.readPixel(0,0)==UiTheme::current().background);
    for(int t:{0,599,600,900,1000,1459,1460,2130,2839,2840,3179,3180,3579,3700,3850,3999,4000}) {
        m.evolutionElapsedMs=t;m.evolutionReady=t>=4000;m.evolutionPhase=t<600?Phase::INTRO:t<2840?Phase::MORPH:t<3180?Phase::FLASH:t<3580?Phase::REVEAL:Phase::COMPLETE;
        m.evolutionCancelHoldProgress=t==900?50:0;checkPage(c,storage,m);
    }
    m.evolutionReady=false;
    for(int t:{0,400,650,850,999,1000}) {m.evolutionElapsedMs=t;m.evolutionPhase=t<650?Phase::CANCEL_MORPH:t<1000?Phase::CANCEL_REVEAL:Phase::CANCELLED;checkPage(c,storage,m);if(t==1000)capture(c,output,"cancelled");}
    assert(c.readPixel(0,0)==UiTheme::current().background);
    m.fromSpeciesId=129;m.toSpeciesId=130;m.evolutionElapsedMs=1600;m.evolutionPhase=Phase::MORPH;checkPage(c,storage,m);
    m.fromSpeciesId=m.toSpeciesId=65535;checkPage(c,storage,m);
    std::puts("Emerald evolution: phase/bounds/cancel/missing/full/dirty/swap and cream result OK");
}
