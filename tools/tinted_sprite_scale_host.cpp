#include "presentation/TintedSpriteRle.h"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    constexpr int W=368,H=448;
    const uint16_t data[]={1,0xf800};
    const Ui::TintedSpriteRle::Source source{data,0,2,nullptr,0,0,1,1,false};
    std::vector<uint16_t> storage(W*H);
    Canvas565 c;
    for (bool swapped:{false,true}) for (int coordinate:{1,2}) for (int layout:{1,2}) for (int asset:{1,2}) {
        c.attach({storage.data(),W,H,swapped});
        c.setCoordinateScale(coordinate);c.setLayoutScale(layout);c.setAssetScale(asset);
        const int x=(c.width()-4)/asset,y=(c.height()-4)/asset;
        for (bool clipped:{false,true}) {
            c.clearClipRect();
            if(clipped)c.setClipRect(x*asset,y*asset,1,1);
            c.fillSprite(0x1826);c.fillAssetRect(x,y,1,1,0xf800);
            const auto expected=storage;
            c.fillSprite(0x1826);
            assert(Ui::TintedSpriteRle::draw(c,source,x,y,1,0xffff,0));
            assert(storage==expected);
        }
    }
    std::puts("Tinted sprite: logical clips with coordinate/layout/asset scale and byte order OK");
}
