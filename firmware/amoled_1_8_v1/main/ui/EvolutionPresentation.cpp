#include "ui/EvolutionPresentation.h"
#include "ui/EvolutionBackgroundData.h"
#include "ui/UiTheme.h"
#include "assets/PokemonSprites.h"
#include "core/UiStrings.h"
#include "platform/api/FlashStorage.h"
#include "presentation/TextBlock.h"
#include "presentation/TintedSpriteRle.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AmoledV1 {
namespace {
using Phase = Game::EvolutionSequence::Phase;
constexpr uint16_t BASE = UiTheme::rgb565(9, 30, 43);
constexpr float PI = 3.14159265f;
float smooth(float a, float b, float x) {
    const float t = std::clamp((x-a)/(b-a), 0.0f, 1.0f);
    return t*t*(3-2*t);
}
uint16_t mix(uint16_t a, uint16_t b, float t) {
    return Ui::TintedSpriteRle::blend(a, b, uint8_t(std::lround(std::clamp(t, 0.0f, 1.0f)*255)));
}
// Symmetric palette pairs need only 136 colors. 77 distinct vertical envelope
// strengths take 21 KiB, independent of framebuffer, sprite size and dirty bands.
struct BackgroundCache {
    static constexpr std::size_t STRENGTH_COUNT=sizeof(EvolutionBackgroundData::strengths);
    uint16_t colors[STRENGTH_COUNT][136]{};
    uint8_t pair[256]{};
    int16_t sampleX[368]{};
    bool initialized = false;
    int paletteRow = -1, warm = -1;
    uint16_t cream = 0;
    void prepare(int nextRow, uint8_t nextWarm, uint16_t nextCream) {
        if (!initialized) {
            for (int a=0, index=0; a<16; ++a) for (int b=a; b<16; ++b, ++index)
                pair[a*16+b] = pair[b*16+a] = uint8_t(index);
            for (int x=0; x<368; ++x) sampleX[x] = int16_t(int((x-184)/1.55f)+120);
            initialized = true;
        }
        if (paletteRow == nextRow && warm == nextWarm && cream == nextCream) return;
        paletteRow = nextRow; warm = nextWarm; cream = nextCream;
        for (int a=0, index=0; a<16; ++a) for (int b=a; b<16; ++b, ++index) {
            const uint16_t waves = mix(EvolutionBackgroundData::palettes[nextRow*16+a],
                                      EvolutionBackgroundData::palettes[nextRow*16+b], .5f);
            for (std::size_t s=0; s<STRENGTH_COUNT; ++s) {
                const uint16_t cool = Ui::TintedSpriteRle::blend(BASE, waves, EvolutionBackgroundData::strengths[s]);
                colors[s][index] = Ui::TintedSpriteRle::blend(cool, nextCream, nextWarm);
            }
        }
    }
};
BackgroundCache backgroundCache;
uint8_t tilePixel(const uint16_t* map, int x, int y) {
    x &= 255; y &= 255;
    const uint16_t tile = map[(y/8)*32+x/8];
    int tx=x&7, ty=y&7;
    if (tile & 1024) tx=7-tx;
    if (tile & 2048) ty=7-ty;
    const int id=tile&1023;
    const int pixel=((id/16)*8+ty)*128+(id%16)*8+tx;
    return (EvolutionBackgroundData::tiles[pixel/2] >> ((pixel&1)*4)) & 15;
}
void backdrop(Canvas565& canvas, uint32_t ms, float warm) {
    const uint16_t cream=UiTheme::current().background;
    canvas.fillRect(0,0,368,448,mix(BASE,cream,warm));
    if (warm >= 1) return;
    const int scale=canvas.renderScale();
    const int top=std::max(65,canvas.clipTop());
    const int bottom=std::min(292,canvas.clipBottom());
    const int left=std::max(0,canvas.clipLeft());
    const int right=std::min(368,canvas.clipRight());
    if (top>=bottom || left>=right) return;
    const int row=13+(ms>600 ? ((ms-600)/70)%24 : 0);
    backgroundCache.prepare(row,uint8_t(std::lround(warm*255)),cream);
    const float theta=ms*.0017f;
    const int ax=int(4*std::cos(theta)), ay=int(4*std::sin(theta));
    for (int y=top;y<bottom;++y) {
        const int sy=int((y-184)/1.55f)+64;
        const auto* colors=backgroundCache.colors[EvolutionBackgroundData::rowStrength[y]];
        for (int x=left;x<right;++x) {
            const int sx=backgroundCache.sampleX[x];
            const int a=tilePixel(EvolutionBackgroundData::inner,sx+ax+8,sy+ay+16);
            const int b=tilePixel(EvolutionBackgroundData::outer,sx-ax+8,sy-ay+16);
            const auto color=colors[backgroundCache.pair[a*16+b]];
            if (scale==1) canvas.rawPixels()[y*canvas.physicalWidth()+x]=canvas.encodeColor(color);
            else canvas.drawPixel(x,y,color);
        }
    }
}
const PokemonSprites::SpriteFrame* frame(uint16_t species) {
    const auto* cached=PokemonSprites::findCachedSpeciesSprite(species,PokemonSprites::SpriteKind::FRONT);
    return cached ? cached : PokemonSprites::findSpeciesSprite(species,PokemonSprites::SpriteKind::FRONT);
}
struct BoundsCache {
    uint16_t species=0;
    uint32_t reload=~0U;
    PokemonSprites::FrameBounds bounds{};
    PokemonSprites::FrameBounds get(const PokemonSprites::SpriteFrame* f) {
        if (!f) return {};
        const auto id=FlashStorage::readWord(&f->speciesId);
        const auto generation=PokemonSprites::cacheStats().reloadCount;
        if (species!=id || reload!=generation) {
            species=id; reload=generation;
            bounds=PokemonSprites::frameVisibleBounds(f);
        }
        return bounds;
    }
};
void actor(const PokemonSprites::SpriteFrame* f, PokemonSprites::FrameBounds bounds,
           float scale, float whitening) {
    if (!f || !bounds.width || !bounds.height) return;
    const int visibleWidth=std::max(1,int(std::lround(bounds.width*scale)));
    const int visibleHeight=std::max(1,int(std::lround(bounds.height*scale)));
    const int x=184-visibleWidth/2-int(std::lround(bounds.left*scale));
    const int y=184-visibleHeight/2-int(std::lround(bounds.top*scale));
    PokemonSprites::drawFrameTintedScaled(f,x,y,scale,0xffff,
        uint8_t(std::lround(std::clamp(whitening,0.0f,1.0f)*255)));
}
void sparkles(Canvas565& canvas, uint32_t ms, float strength, float progress, bool outward) {
    if (strength<=0) return;
    const float radius=outward ? 34+132*progress : 136-100*progress;
    for (int i=0;i<10;++i) {
        const float angle=i*.6283185f+ms*.002f;
        const int x=184+int(radius*std::cos(angle)), y=184+int(radius*.42f*std::sin(angle));
        const uint16_t color=mix(BASE,0xffff,strength*(i%3==0?.6f:1));
        canvas.fillRect(x-2,y,5,1,color); canvas.fillRect(x,y-2,1,5,color);
    }
}
void centered(Canvas565& canvas, Ui::TextBlock& block, const char* value, int y,
              uint16_t color, BitmapText::Font font=BitmapText::Font::PX32) {
    Ui::TextBlock::Options options;
    options.width=320; options.maxHeight=64; options.lineGap=8;
    options.font=font; options.align=Ui::TextBlock::Align::CENTER;
    block.setOptions(options); block.setText(value); block.draw(canvas,24,y,color);
}
} // namespace

void drawEvolutionPresentation(Canvas565& canvas, const ProgressionViewModel& model) {
    const uint32_t ms=model.evolutionElapsedMs;
    const bool cancelled=model.evolutionPhase==Phase::CANCELLED;
    const bool cancelling=model.evolutionPhase==Phase::CANCEL_MORPH || model.evolutionPhase==Phase::CANCEL_REVEAL;
    const bool done=model.evolutionReady || cancelled;
    const float warm=done?1:cancelling?smooth(650,1000,float(ms)):smooth(3700,4000,float(ms));
    backdrop(canvas,ms,warm);
    const auto* source=frame(model.fromSpeciesId);
    const auto* target=frame(model.toSpeciesId);
    static BoundsCache sourceBounds,targetBounds;
    const auto a=sourceBounds.get(source), b=targetBounds.get(target);
    const float baseScale=std::min(1.5f,std::min(168.0f/std::max(1,int(std::max(a.height,b.height))),
                                              244.0f/std::max(1,int(std::max(a.width,b.width)))));
    if (cancelled || cancelling) {
        actor(source,a,baseScale,cancelled?0:1-smooth(0,650,float(ms)));
        if (cancelling) sparkles(canvas,ms,1-smooth(0,650,float(ms)),smooth(0,650,float(ms)),false);
    } else if (model.evolutionReady) {
        actor(target,b,baseScale,0);
    } else if (ms<600) {
        actor(source,a,baseScale,0);
    } else if (ms<1000) {
        actor(source,a,baseScale,(ms-600)/400.0f);
        sparkles(canvas,ms,.9f,(ms-600)/400.0f,false);
    } else if (ms<2840) {
        constexpr int durations[]={460,360,300,240,200,160,120};
        int elapsed=int(ms)-1000,cycle=0;
        while (cycle<6 && elapsed>=durations[cycle]) elapsed-=durations[cycle++];
        const float q=(1-std::cos(std::clamp(elapsed/float(durations[cycle]),0.0f,1.0f)*PI))*.5f;
        const bool reversed=cycle%2;
        actor(reversed?target:source,reversed?b:a,baseScale*(1-.82f*q),1);
        actor(reversed?source:target,reversed?a:b,baseScale*(.18f+.82f*q),1);
    } else if (ms<3180) {
        actor(target,b,baseScale,1);
        const float t=(ms-2840)/340.0f;
        sparkles(canvas,ms,1-t,t,true);
    } else {
        actor(target,b,baseScale,1-smooth(3180,3580,float(ms)));
    }
    const auto& p=UiTheme::current();
    const Species* species=findSpecies(cancelled || !model.evolutionReady ? model.fromSpeciesId : model.toSpeciesId);
    // Do not interpolate light text through the lightening background: that
    // would make both colors meet and disappear halfway through the fade.
    const bool dark=warm>=.5f;
    const uint16_t light=UiTheme::rgb565(230,244,250);
    static Ui::TextBlock title,name,instruction,button;
    centered(canvas,title,cancelled?"进化已取消":model.evolutionReady?"进化完成":"进化中...",32,dark?p.title:light);
    centered(canvas,name,species && species->name?species->name:Ui::Status::MOVE_UNKNOWN,296,dark?p.text:light);
    if (done) {
        canvas.fillRoundRect(24,364,320,64,22,p.accent);
        centered(canvas,button,Ui::Common::CONTINUE,380,p.accentText);
    } else {
        centered(canvas,instruction,cancelling?Ui::Amoled::CANCELLING_EVOLUTION:Ui::Amoled::HOLD_TO_CANCEL_EVOLUTION,
                 394,dark?p.text:(warm>0?light:UiTheme::rgb565(155,187,203)),BitmapText::Font::PX16);
        if (!cancelling && model.evolutionCancelHoldProgress) {
            canvas.fillRoundRect(124,376,120,4,2,UiTheme::rgb565(39,58,62));
            canvas.fillRoundRect(124,376,120*std::min(100,int(model.evolutionCancelHoldProgress))/100,4,2,light);
        }
    }
}
} // namespace AmoledV1
