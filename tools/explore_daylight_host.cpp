#include "game/ExploreDaylight.h"
#include "presentation/Canvas565.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

using ExploreDaylight::Tone;
uint16_t expected(uint16_t pixel, Tone tone) {
    const int r = std::min(31, (((pixel >> 11) & 31) * tone.red + 128) / 256);
    const int g = std::min(63, (((pixel >> 5) & 63) * tone.green + 128) / 256);
    const int b = std::min(31, ((pixel & 31) * tone.blue + 128) / 256);
    return uint16_t((r << 11) | (g << 5) | b);
}
int main() {
    const Tone day{}, night = ExploreDaylight::at(0, 0);
    assert(ExploreDaylight::at(360, 0) == day);
    assert(ExploreDaylight::at(1079, 0) == day);
    assert(ExploreDaylight::at(1080, 0) == day);
    assert(ExploreDaylight::at(1200, 0) == night);
    assert(ExploreDaylight::at(239, 0) == night);
    assert(ExploreDaylight::at(240, 0) == night);
    assert(ExploreDaylight::at(1439, 0) == night);
    assert(ExploreDaylight::at(1140, 0).red > ExploreDaylight::at(1140, 0).green);
    assert(night.blue > night.green && night.green > night.red);
    for (uint8_t area = 0; area < 6; ++area) {
        int neutral = 0, dark = 0;
        for (uint32_t minute = 0; minute < 1440; ++minute) {
            const auto tone = ExploreDaylight::at(minute, area);
            const auto next = ExploreDaylight::at(minute + 1, area);
            assert(tone == ExploreDaylight::at(minute + 1440 * 23, area));
            assert(std::abs(int(tone.red) - next.red) <= 3);
            assert(std::abs(int(tone.green) - next.green) <= 3);
            assert(std::abs(int(tone.blue) - next.blue) <= 3);
            if (tone == day) ++neutral;
            if (tone == night) ++dark;
            if (area == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA) assert(tone == day);
        }
        if (area != ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA) assert(neutral > dark && dark >= 480);
    }
    // Exhaustively validate RGB565 values in both native byte orders.
    for (bool swapped : {false, true}) {
        Canvas565 canvas;
        std::vector<uint16_t> pixels(65536);
        canvas.attach({pixels.data(), 256, 256, swapped});
        for (Tone tone : {day, night, ExploreDaylight::at(1140, 0), ExploreDaylight::at(300, 0)}) {
            for (unsigned i = 0; i < pixels.size(); ++i) pixels[i] = canvas.encodeColor(i);
            canvas.multiplyRectRgb(0, 0, 256, 256, tone.red, tone.green, tone.blue);
            for (unsigned i = 0; i < pixels.size(); ++i)
                assert(canvas.decodeColor(pixels[i]) == expected(i, tone));
        }
        // Cropped/negative rectangles, native clips and legacy scaling.
        for (int scale : {1, 2, 4}) {
            canvas.setCoordinateScale(scale);
            for (unsigned i = 0; i < pixels.size(); ++i) pixels[i] = canvas.encodeColor(i);
            canvas.setClipRect(3, 4, 7, 9);
            canvas.multiplyRectRgb(-10, -10, 18, 20, night.red, night.green, night.blue);
            for (int y = 0; y < 256; ++y) {
                for (int x = 0; x < 256; ++x) {
                    const uint16_t original = y * 256 + x;
                    const bool inside = x >= 3 * scale && x < 8 * scale && y >= 4 * scale && y < 10 * scale;
                    assert(canvas.decodeColor(pixels[original]) == (inside ? expected(original, night) : original));
                }
            }
            const auto before = pixels;
            canvas.multiplyRectRgb(8, 8, -1, -1, night.red, night.green, night.blue);
            assert(before == pixels);
            canvas.clearClipRect();
        }
    }
    std::puts("daylight: long-day boundaries, continuous wrap, cave exclusion, all RGB565 colors, byte order, clipping and raster scales verified");
}
