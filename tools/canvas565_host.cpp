#include <array>
#include <cassert>
#include <cstdint>

#include "presentation/Canvas565.h"

namespace {

uint16_t swapped(uint16_t color) {
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

}  // namespace

int main() {
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
    return 0;
}
