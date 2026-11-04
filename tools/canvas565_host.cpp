#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cmath>

#include "presentation/Canvas565.h"

namespace {

uint16_t swapped(uint16_t color) {
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

}  // namespace

void checkNativeMaskedBlit() {
    // Compare the optimized path to individual clipped pixels, including odd
    // mask row boundaries, offscreen rectangles and both storage byte orders.
    constexpr int W=37,H=31,TW=26,TH=26;
    std::array<uint16_t,W*H> actual{}, expected{};
    std::array<uint16_t,TW*TH> art{};
    std::array<uint8_t,(TW*TH+7)/8> mask{};
    for(int i=0;i<TW*TH;++i) {
        art[i]=uint16_t(i*1777+13);
        if(i%5 && i%7) mask[i/8]|=1U<<(i%8);
    }
    for(bool swapped:{false,true})for(int x:{-30,-7,0,14,36,50})for(int y:{-28,-3,0,19,30,40})
        for(bool clipped:{false,true}) {
            Canvas565 fast,reference;
            fast.attach({actual.data(),W,H,swapped});reference.attach({expected.data(),W,H,swapped});
            fast.fillSprite(0xa55a);reference.fillSprite(0xa55a);
            if(clipped) {fast.setClipRect(3,5,27,21);reference.setClipRect(3,5,27,21);}
            fast.drawMaskedAssetImage(x,y,TW,TH,art.data(),mask.data());
            for(int row=0;row<TH;++row)for(int col=0;col<TW;++col) {
                const int i=row*TW+col;
                if(mask[i/8]&(1U<<(i%8)))reference.drawPixel(x+col,y+row,art[i]);
            }
            assert(actual==expected);
        }
    Canvas565 unattached;
    unattached.drawMaskedAssetImage(0,0,TW,TH,art.data(),mask.data());
}

void checkFractionalMaskedBlit() {
    constexpr int W = 37, H = 31, TW = 27, TH = 23;
    std::array<uint16_t, W * H> actual{}, expected{};
    std::array<uint16_t, TW * TH> art{};
    std::array<uint8_t, (TW * TH + 7) / 8> mask{};
    for (int i = 0; i < TW * TH; ++i) {
        art[i] = uint16_t(i * 1777 + 13);
        if (i % 5 && i % 7) mask[i / 8] |= 1U << (i % 8);
    }
    for (bool swap : {false, true})
    for (int assetScale : {1, 2})
    for (float scale : {1.2f, 1.5f})
    for (int x : {-55, -7, 0, 13, 36, 72})
    for (int y : {-47, -3, 0, 19, 30, 61}) {
        Canvas565 fast, reference;
        fast.attach({actual.data(), W, H, swap});
        reference.attach({expected.data(), W, H, swap});
        fast.setAssetScale(assetScale);
        fast.fillSprite(0xa55a); reference.fillSprite(0xa55a);
        fast.setClipRect(3, 5, 27, 21);
        reference.setClipRect(3, 5, 27, 21);
        fast.drawMaskedAssetImage(x, y, TW, TH, art.data(), mask.data(), scale);
        const float step = scale * assetScale;
        for (int row = 5; row < 26; ++row) for (int col = 3; col < 30; ++col) {
            const int dx = col - x * assetScale, dy = row - y * assetScale;
            if (dx < 0 || dy < 0 || dx >= int(std::lround(TW * step)) ||
                dy >= int(std::lround(TH * step))) continue;
            const int sx = std::min(TW - 1, int(dx / step));
            const int sy = std::min(TH - 1, int(dy / step));
            const int i = sy * TW + sx;
            if (mask[i / 8] & (1U << (i % 8))) reference.drawPixel(col, row, art[i]);
        }
        assert(actual == expected);
    }
}

void checkDoubleMaskedBlit() {
    constexpr int W = 73, H = 61, TW = 27, TH = 23;
    std::array<uint16_t, W * H> actual{}, expected{};
    std::array<uint16_t, TW * TH> art{};
    std::array<uint8_t, (TW * TH + 7) / 8> mask{};
    for (int i = 0; i < TW * TH; ++i) {
        art[i] = uint16_t(i * 1777 + 13);
        if (i % 5 && i % 7) mask[i / 8] |= 1U << (i % 8);
    }
    for (bool swap : {false, true})
    for (int assetScale : {1, 2})
    for (int x : {-55, -7, 0, 13, 36, 72, 90})
    for (int y : {-47, -3, 0, 19, 60, 80})
    for (int clipWidth : {1, 2, 27, 68, 73})
    for (int clipHeight : {1, 2, 21, 55, 61}) {
        Canvas565 fast, reference;
        fast.attach({actual.data(), W, H, swap});
        reference.attach({expected.data(), W, H, swap});
        fast.setAssetScale(assetScale);
        fast.fillSprite(0xa55a);
        reference.fillSprite(0xa55a);
        fast.setClipRect(3, 5, clipWidth, clipHeight);
        reference.setClipRect(3, 5, clipWidth, clipHeight);
        fast.drawMaskedAssetImage(x, y, TW, TH, art.data(), mask.data(), 2 / assetScale);
        for (int row = 0; row < TH; ++row)
        for (int col = 0; col < TW; ++col) {
            const int i = row * TW + col;
            if (!(mask[i / 8] & (1U << (i % 8)))) continue;
            for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
                reference.drawPixel(x * assetScale + col * 2 + dx,
                    y * assetScale + row * 2 + dy, art[i]);
        }
        assert(actual == expected);
    }
}

void checkNativeImageCopy() {
    constexpr int W = 17, H = 13, IW = 11, IH = 9;
    std::array<uint16_t, W * H> pixels{};
    std::array<uint16_t, IW * IH> source{};
    for (size_t i = 0; i < source.size(); ++i)
        source[i] = static_cast<uint16_t>(0x1200U + i * 37U);

    for (bool swap : {false, true}) {
        Canvas565 canvas;
        canvas.attach({pixels.data(), W, H, swap});
        canvas.fillSprite(0xaaaa);
        canvas.setClipRect(3, 2, 9, 7);
        canvas.pushImageNative(-2, -1, IW, IH, source.data());
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                const bool inside = x >= 3 && x < 9 && y >= 2 && y < 8;
                uint16_t expected = swap ? swapped(0xaaaa) : 0xaaaa;
                if (inside) {
                    const uint16_t value = source[(y + 1) * IW + (x + 2)];
                    expected = value;
                }
                assert(pixels[y * W + x] == expected);
            }
        }
    }
}

void checkRgbMultiply() {
    constexpr int W = 256, H = 256;
    std::array<uint16_t, W * H> actual{};
    const std::array<std::array<uint16_t, 3>, 5> tones{{
        {256, 256, 256}, {0, 0, 0}, {128, 152, 192},
        {400, 257, 127}, {65535, 1, 255}}};
    for (bool swap : {false, true})
    for (int scale : {1, 2, 3})
    for (bool clipped : {false, true})
    for (const auto& tone : tones) {
        Canvas565 canvas;
        canvas.setCoordinateScale(scale);
        canvas.attach({actual.data(), W, H, swap});
        for (int i = 0; i < W * H; ++i)
            actual[i] = swap ? swapped(uint16_t(i)) : uint16_t(i);
        if (clipped) canvas.setClipRect(2, 3, W / scale - 5, H / scale - 7);
        canvas.multiplyRectRgb(-1, -2, W + 3, H + 5, tone[0], tone[1], tone[2]);
        const int left = clipped ? 2 * scale : 0;
        const int top = clipped ? 3 * scale : 0;
        const int right = (W / scale - (clipped ? 3 : 0)) * scale;
        const int bottom = (H / scale - (clipped ? 4 : 0)) * scale;
        for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const uint16_t input = uint16_t(y * W + x);
            uint16_t expected = input;
            if (x >= left && x < right && y >= top && y < bottom) {
                const int r = std::min(31, ((input >> 11) * tone[0] + 128) / 256);
                const int g = std::min(63, (((input >> 5) & 63) * tone[1] + 128) / 256);
                const int b = std::min(31, ((input & 31) * tone[2] + 128) / 256);
                expected = uint16_t((r << 11) | (g << 5) | b);
            }
            assert(actual[y * W + x] == (swap ? swapped(expected) : expected));
        }
    }
}

void checkRoundAlpha() {
    constexpr int W=47,H=39;
    std::array<uint16_t,W*H> actual{}, expected{};
    for (bool swap : {false,true})
    for (int x : {-15,3,31})
    for (int y : {-12,4,28})
    for (int width : {1,14,29})
    for (int height : {1,12,27})
    for (int radius : {0,1,7,30})
    for (uint8_t alpha : {0,1,128,255}) {
        Canvas565 fast,reference;
        fast.attach({actual.data(),W,H,swap});reference.attach({expected.data(),W,H,swap});
        for (int i=0;i<W*H;++i) actual[i]=expected[i]=uint16_t(i*977);
        fast.setClipRect(5,7,33,25);reference.setClipRect(5,7,33,25);
        constexpr uint16_t color=0x6957;
        assert(fast.blendRoundRectAlphaNative(x,y,width,height,radius,color,alpha));
        const int r=std::min(radius,std::min(width,height)/2);
        for (int py=y;py<y+height;++py) for (int px=x;px<x+width;++px) {
            const int cx=px<x+r?x+r:px>=x+width-r?x+width-r-1:px;
            const int cy=py<y+r?y+r:py>=y+height-r?y+height-r-1:py;
            if ((px-cx)*(px-cx)+(py-cy)*(py-cy)>r*r || !alpha) continue;
            const uint16_t bg=reference.readPixel(px,py);
            const auto mix=[&](int a,int b,int maximum,int shift) {
                return (((a*255/maximum)*alpha+(b*255/maximum)*(255-alpha))/255)>>shift;
            };
            const uint16_t result=uint16_t((mix(color>>11,bg>>11,31,3)<<11)|
                (mix((color>>5)&63,(bg>>5)&63,63,2)<<5)|mix(color&31,bg&31,31,3));
            reference.drawPixel(px,py,result);
        }
        assert(actual==expected);
    }
    Canvas565 scaled;scaled.setCoordinateScale(2);scaled.attach({actual.data(),W,H,false});
    expected=actual;
    assert(!scaled.blendRoundRectAlphaNative(0,0,15,15,7,0xFFFF,128));
    assert(actual==expected);
}

int main() {
    checkRoundAlpha();
    checkRgbMultiply();
    checkNativeMaskedBlit();
    checkDoubleMaskedBlit();
    checkFractionalMaskedBlit();
    checkNativeImageCopy();
    std::array<uint16_t, 8 * 6> pixels{};
    Platform::FrameBuffer565 frame;
    frame.pixels = pixels.data();
    frame.width = 8;
    frame.height = 6;
    frame.byteSwapped = true;

    Canvas565 canvas;
    canvas.attach(frame);
    assert(canvas.attached());
    assert(canvas.width() == 8);
    assert(canvas.height() == 6);

    canvas.fillSprite(0xF81F);
    for (uint16_t pixel : pixels) assert(pixel == swapped(0xF81F));
    assert(canvas.readPixel(7, 5) == 0xF81F);

    canvas.setClipRect(2, 1, 3, 3);
    canvas.fillRect(0, 0, 8, 6, 0x07E0);
    assert(canvas.readPixel(1, 1) == 0xF81F);
    assert(canvas.readPixel(2, 1) == 0x07E0);
    assert(canvas.readPixel(4, 3) == 0x07E0);
    assert(canvas.readPixel(5, 3) == 0xF81F);

    canvas.clearClipRect();
    canvas.drawLine(0, 0, 7, 5, 0x001F);
    assert(canvas.readPixel(0, 0) == 0x001F);
    assert(canvas.readPixel(7, 5) == 0x001F);

    canvas.fillTriangle(0, 5, 3, 1, 6, 5, 0xFFFF);
    assert(canvas.readPixel(3, 3) == 0xFFFF);

    const uint16_t image[] = {
        0x0001, 0x0002,
        0x0003, 0x0004,
    };
    canvas.pushImage(6, 0, 2, 2, image);
    assert(canvas.readPixel(6, 0) == 0x0001);
    assert(canvas.readPixel(7, 1) == 0x0004);

    canvas.drawPixel(-1, -1, 0);
    canvas.drawPixel(8, 6, 0);
    assert(canvas.readPixel(-1, 0) == 0);
    assert(canvas.readPixel(8, 0) == 0);

    std::array<uint16_t, 8 * 6> physicalPixels{};
    frame.pixels = physicalPixels.data();
    frame.width = 8;
    frame.height = 6;
    Canvas565 scaledCanvas;
    scaledCanvas.attach(frame);
    scaledCanvas.setCoordinateScale(2);
    assert(scaledCanvas.width() == 4);
    assert(scaledCanvas.height() == 3);
    assert(scaledCanvas.physicalWidth() == 8);
    assert(scaledCanvas.physicalHeight() == 6);
    scaledCanvas.drawPixel(1, 1, 0x07E0);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 2 && y < 4;
            assert(physicalPixels[static_cast<size_t>(y) * 8 + x] ==
                   (inside ? swapped(0x07E0) : 0));
        }
    }
    assert(scaledCanvas.readPixel(1, 1) == 0x07E0);

    std::array<uint16_t, 8 * 6> assetPixels{};
    frame.pixels = assetPixels.data();
    frame.width = 8;
    frame.height = 6;
    Canvas565 assetCanvas;
    assetCanvas.attach(frame);
    assetCanvas.setAssetScale(2);
    assetCanvas.drawAssetPixel(1, 1, 0xF800);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 2 && y < 4;
            assert(assetPixels[static_cast<size_t>(y) * 8 + x] ==
                   (inside ? swapped(0xF800) : 0));
        }
    }
    return 0;
}
