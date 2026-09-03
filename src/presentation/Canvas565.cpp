#include "presentation/Canvas565.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "platform/api/ProgramMemory.h"

namespace {

void swapInt(int& a, int& b) {
    int value = a;
    a = b;
    b = value;
}

}  // namespace

void Canvas565::attach(const Platform::FrameBuffer565& frameBuffer) {
    pixels_ = frameBuffer.pixels;
    physicalWidth_ = frameBuffer.width;
    physicalHeight_ = frameBuffer.height;
    width_ = physicalWidth_ / coordinateScale_;
    height_ = physicalHeight_ / coordinateScale_;
    byteSwapped_ = frameBuffer.byteSwapped;
    clearClipRect();
}

void Canvas565::setCoordinateScale(uint8_t scale) {
    coordinateScale_ = scale == 0 ? 1 : scale;
    width_ = physicalWidth_ / coordinateScale_;
    height_ = physicalHeight_ / coordinateScale_;
    clearClipRect();
}

uint16_t Canvas565::encodeColor(uint16_t color) const {
    return byteSwapped_
        ? static_cast<uint16_t>((color << 8) | (color >> 8))
        : color;
}

uint16_t Canvas565::decodeColor(uint16_t stored) const {
    return byteSwapped_
        ? static_cast<uint16_t>((stored << 8) | (stored >> 8))
        : stored;
}

bool Canvas565::visible(int x, int y) const {
    return pixels_ && x >= clipLeft_ && x < clipRight_ &&
           y >= clipTop_ && y < clipBottom_;
}

void Canvas565::fillSprite(uint16_t color) {
    if (!pixels_) return;
    const uint16_t stored = encodeColor(color);
    std::fill(pixels_, pixels_ + pixelCount(), stored);
}

void Canvas565::drawPixel(int x, int y, uint16_t color) {
    if (!visible(x, y)) return;
    const int physicalX = x * coordinateScale_;
    const int physicalY = y * coordinateScale_;
    for (int row = 0; row < coordinateScale_; ++row) {
        for (int column = 0; column < coordinateScale_; ++column) {
            drawPhysicalPixel(physicalX + column, physicalY + row, color);
        }
    }
}

void Canvas565::drawPhysicalPixel(int x, int y, uint16_t color) {
    if (!pixels_ || x < clipLeft_ * coordinateScale_ ||
        x >= clipRight_ * coordinateScale_ ||
        y < clipTop_ * coordinateScale_ ||
        y >= clipBottom_ * coordinateScale_ ||
        x < 0 || x >= physicalWidth_ || y < 0 || y >= physicalHeight_) {
        return;
    }
    pixels_[static_cast<uint32_t>(y) * physicalWidth_ + x] = encodeColor(color);
}

uint16_t Canvas565::readPixel(int x, int y) const {
    if (!pixels_ || x < 0 || x >= width_ || y < 0 || y >= height_) {
        return 0;
    }
    return decodeColor(pixels_[static_cast<uint32_t>(y * coordinateScale_) *
                               physicalWidth_ + x * coordinateScale_]);
}

void Canvas565::drawFastHLine(int x, int y, int w, uint16_t color) {
    fillRect(x, y, w, 1, color);
}

void Canvas565::drawFastVLine(int x, int y, int h, uint16_t color) {
    fillRect(x, y, 1, h, color);
}

void Canvas565::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void Canvas565::fillRect(int x, int y, int w, int h, uint16_t color) {
    if (!pixels_ || w <= 0 || h <= 0) return;
    int left = std::max(x, clipLeft_);
    int top = std::max(y, clipTop_);
    int right = std::min(x + w, clipRight_);
    int bottom = std::min(y + h, clipBottom_);
    if (left >= right || top >= bottom) return;
    const uint16_t stored = encodeColor(color);
    const int physicalLeft = left * coordinateScale_;
    const int physicalRight = right * coordinateScale_;
    const int physicalTop = top * coordinateScale_;
    const int physicalBottom = bottom * coordinateScale_;
    for (int py = physicalTop; py < physicalBottom; ++py) {
        uint16_t* row = pixels_ + static_cast<uint32_t>(py) * physicalWidth_;
        std::fill(row + physicalLeft, row + physicalRight, stored);
    }
}

void Canvas565::drawRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    drawFastHLine(x, y, w, color);
    if (h > 1) drawFastHLine(x, y + h - 1, w, color);
    if (h > 2) {
        drawFastVLine(x, y + 1, h - 2, color);
        if (w > 1) drawFastVLine(x + w - 1, y + 1, h - 2, color);
    }
}

void Canvas565::drawCircle(int cx, int cy, int radius, uint16_t color) {
    if (radius < 0) return;
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        drawPixel(cx + x, cy + y, color);
        drawPixel(cx + y, cy + x, color);
        drawPixel(cx - y, cy + x, color);
        drawPixel(cx - x, cy + y, color);
        drawPixel(cx - x, cy - y, color);
        drawPixel(cx - y, cy - x, color);
        drawPixel(cx + y, cy - x, color);
        drawPixel(cx + x, cy - y, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x + 1);
        }
    }
}

void Canvas565::fillCircle(int cx, int cy, int radius, uint16_t color) {
    if (radius < 0) return;
    for (int y = -radius; y <= radius; ++y) {
        int span = static_cast<int>(
            std::sqrt(static_cast<float>(radius * radius - y * y)));
        drawFastHLine(cx - span, cy + y, span * 2 + 1, color);
    }
}

void Canvas565::fillEllipse(int cx, int cy, int rx, int ry,
                            uint16_t color) {
    if (rx < 0 || ry < 0) return;
    if (ry == 0) {
        drawFastHLine(cx - rx, cy, rx * 2 + 1, color);
        return;
    }
    for (int y = -ry; y <= ry; ++y) {
        float normalized = static_cast<float>(y) / ry;
        int span = static_cast<int>(
            rx * std::sqrt(std::max(0.0f, 1.0f - normalized * normalized)));
        drawFastHLine(cx - span, cy + y, span * 2 + 1, color);
    }
}

void Canvas565::fillTriangle(int x0, int y0, int x1, int y1,
                             int x2, int y2, uint16_t color) {
    if (y0 > y1) {
        swapInt(y0, y1);
        swapInt(x0, x1);
    }
    if (y1 > y2) {
        swapInt(y1, y2);
        swapInt(x1, x2);
    }
    if (y0 > y1) {
        swapInt(y0, y1);
        swapInt(x0, x1);
    }
    if (y0 == y2) {
        int left = std::min(x0, std::min(x1, x2));
        int right = std::max(x0, std::max(x1, x2));
        drawFastHLine(left, y0, right - left + 1, color);
        return;
    }

    auto edgeX = [](int xa, int ya, int xb, int yb, int y) {
        if (yb == ya) return xa;
        return xa + static_cast<int>(
            static_cast<int64_t>(xb - xa) * (y - ya) / (yb - ya));
    };
    for (int y = y0; y <= y2; ++y) {
        int xa = edgeX(x0, y0, x2, y2, y);
        int xb = y < y1
            ? edgeX(x0, y0, x1, y1, y)
            : edgeX(x1, y1, x2, y2, y);
        if (xa > xb) swapInt(xa, xb);
        drawFastHLine(xa, y, xb - xa + 1, color);
    }
}

void Canvas565::fillRoundRect(int x, int y, int w, int h,
                              int radius, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    if (radius == 0) {
        fillRect(x, y, w, h, color);
        return;
    }
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius || row >= h - radius) {
            int distance = row < radius
                ? radius - row
                : row - (h - radius - 1);
            int span = static_cast<int>(std::sqrt(
                static_cast<float>(radius * radius -
                                   distance * distance)));
            inset = radius - span;
        }
        drawFastHLine(x + inset, y + row, w - inset * 2, color);
    }
}

void Canvas565::drawRoundRect(int x, int y, int w, int h,
                              int radius, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    if (radius == 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        if (row < radius || row >= h - radius) {
            int distance = row < radius
                ? radius - row
                : row - (h - radius - 1);
            int span = static_cast<int>(std::sqrt(
                static_cast<float>(radius * radius -
                                   distance * distance)));
            inset = radius - span;
        }
        int lineWidth = w - inset * 2;
        if (row == 0 || row == h - 1) {
            drawFastHLine(x + inset, y + row, lineWidth, color);
        } else if (lineWidth > 0) {
            drawPixel(x + inset, y + row, color);
            if (lineWidth > 1) {
                drawPixel(x + w - inset - 1, y + row, color);
            }
        }
    }
}

void Canvas565::pushImage(int x, int y, int w, int h,
                          const uint16_t* source) {
    if (!source || w <= 0 || h <= 0) return;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            drawPixel(x + px, y + py,
                      source[static_cast<uint32_t>(py) * w + px]);
        }
    }
}

void Canvas565::drawRgb565Rle(int x, int y, int w, int h,
                              const uint16_t* data, uint32_t offset,
                              uint32_t length, bool flipX) {
    if (!pixels_ || !data || w <= 0 || h <= 0) return;

    const uint32_t total = static_cast<uint32_t>(w * h);
    uint32_t index = 0;
    uint32_t pixel = 0;
    while (index < length && pixel < total) {
        uint16_t token = Platform::readProgramWord(&data[offset + index++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;
        if (token & 0x8000) {
            pixel = std::min<uint32_t>(total, pixel + run);
            continue;
        }
        for (uint16_t runIndex = 0;
             runIndex < run && index < length && pixel < total;
             ++runIndex, ++pixel) {
            uint16_t color =
                Platform::readProgramWord(&data[offset + index++]);
            int column = static_cast<int>(pixel % w);
            int row = static_cast<int>(pixel / w);
            if (flipX) column = w - 1 - column;
            drawPixel(x + column, y + row, color);
        }
    }
}

void Canvas565::setClipRect(int x, int y, int w, int h) {
    clipLeft_ = std::max(0, x);
    clipTop_ = std::max(0, y);
    clipRight_ = std::min(width_, x + std::max(0, w));
    clipBottom_ = std::min(height_, y + std::max(0, h));
}

void Canvas565::clearClipRect() {
    clipLeft_ = 0;
    clipTop_ = 0;
    clipRight_ = width_;
    clipBottom_ = height_;
}
