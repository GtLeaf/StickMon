#include "presentation/TransformedSpriteRle.h"
#include "presentation/PixelRenderer.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::vector<uint16_t> encode(const std::vector<int>& pixels, bool indexed) {
    std::vector<uint16_t> out;
    for (size_t p=0;p<pixels.size();) {
        const bool skip=pixels[p]<0;
        size_t q=p+1;
        while(q<pixels.size() && (pixels[q]<0)==skip && q-p<32767) ++q;
        out.push_back(uint16_t(q-p) | (skip?0x8000:0));
        if(!skip) {
            if(indexed) for(size_t i=p;i<q;i+=4) {
                uint16_t word=0;
                for(size_t j=0;j<4 && i+j<q;++j) word|=uint16_t(pixels[i+j])<<(4*j);
                out.push_back(word);
            } else for(size_t i=p;i<q;++i) out.push_back(uint16_t(pixels[i]));
        }
        p=q;
    }
    return out;
}
}
int main() {
    constexpr int width=13,height=11,iw=7,ih=5,ox=3,oy=2,W=96,H=88;
    const uint16_t palette[]={0,0xffff,0x1234,0xf800,0x07e0,0x001f};
    unsigned cases=0;
    for(bool indexed:{false,true}) for(bool poseFlip:{false,true}) {
        std::vector<int> image(iw*ih,-1),full(width*height,-1);
        for(int r=0;r<ih;++r) for(int c=0;c<iw;++c) {
            if((r*3+c)%5==0) continue;
            const int index=(r*iw+c)%6;
            image[r*iw+c]=indexed?index:palette[index];
            full[(oy+r)*width+ox+(poseFlip?iw-1-c:c)]=image[r*iw+c];
        }
        auto cropped=encode(image,indexed), legacy=encode(full,indexed);
        const Ui::TintedSpriteRle::Source source{cropped.data(),0,uint32_t(cropped.size()),palette,0,6,iw,ih,indexed};
        const Ui::TintedSpriteRle::Source original{legacy.data(),0,uint32_t(legacy.size()),palette,0,6,width,height,indexed};
        const Ui::TransformedSpriteRle::Geometry geometry{width,height,ox,oy,poseFlip};
        for(bool swapped:{false,true}) for(int coordinate:{1,2}) for(int layout:{1,2}) for(int asset:{1,2})
        for(bool flip:{false,true}) for(bool nearest:{false,true}) for(float scale:{0.07f,0.4f,0.66f,1.f,1.2f,1.5f,1.66f,2.f,2.3f})
        for(int mode=0;mode<3;++mode) for(bool clipped:{false,true}) for(int x:{-4,3}) {
            std::vector<uint16_t> pixels(W*H);
            PixelRenderer::bind({pixels.data(),W,H,swapped});
            auto& canvas=PixelRenderer::canvas();
            canvas.setCoordinateScale(coordinate);canvas.setLayoutScale(layout);canvas.setAssetScale(asset);
            canvas.clearClipRect(); if(clipped) canvas.setClipRect(2,3,27,25);
            canvas.fillSprite(0x2345);
            const int y=-1;
            if(mode==2) Ui::TintedSpriteRle::draw(canvas,original,x,y,scale,0xf81f,137,flip);
            else if(mode==1) {
                if(indexed) PixelRenderer::drawIndexed4RleSolid(x,y,width,height,legacy.data(),0,legacy.size(),0x5678,flip);
                else PixelRenderer::drawRgb565RleSolid(x,y,width,height,legacy.data(),0,legacy.size(),0x5678,flip);
            } else if(indexed) PixelRenderer::drawIndexed4RleScaled(x,y,width,height,legacy.data(),0,legacy.size(),palette,0,6,scale,flip,255,nearest);
            else PixelRenderer::drawRgb565RleScaled(x,y,width,height,legacy.data(),0,legacy.size(),scale,flip,nearest);
            const auto expected=pixels;
            canvas.fillSprite(0x2345);
            const auto kind=mode==2?Ui::TransformedSpriteRle::Mode::Tinted:mode==1?Ui::TransformedSpriteRle::Mode::Solid:Ui::TransformedSpriteRle::Mode::Normal;
            if(!Ui::TransformedSpriteRle::draw(canvas,source,geometry,x,y,mode==1?1.f:scale,flip,nearest,kind,mode==1?0x5678:0xf81f,137) || pixels!=expected) {
                std::fprintf(stderr,"mismatch indexed=%d poseFlip=%d swapped=%d coord=%d layout=%d asset=%d flip=%d nearest=%d scale=%g mode=%d clip=%d x=%d\n",indexed,poseFlip,swapped,coordinate,layout,asset,flip,nearest,scale,mode,clipped,x);return 1;
            }
            ++cases;
        }
    }
    std::printf("pose rendering: %u raster parity cases passed\n",cases);
}
