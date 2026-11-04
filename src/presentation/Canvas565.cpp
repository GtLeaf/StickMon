#include "presentation/Canvas565.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

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
    width_ = physicalWidth_ / renderScale();
    height_ = physicalHeight_ / renderScale();
    byteSwapped_ = frameBuffer.byteSwapped;
    clearClipRect();
}

void Canvas565::setCoordinateScale(uint8_t scale) {
    coordinateScale_ = scale == 0 ? 1 : scale;
    width_ = physicalWidth_ / renderScale();
    height_ = physicalHeight_ / renderScale();
    clearClipRect();
}

void Canvas565::setLayoutScale(uint8_t scale) {
    layoutScale_ = scale == 0 ? 1 : scale;
    width_ = physicalWidth_ / renderScale();
    height_ = physicalHeight_ / renderScale();
    clearClipRect();
}

void Canvas565::setAssetScale(uint8_t scale) {
    assetScale_ = scale == 0 ? 1 : scale;
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
    const int scale = renderScale();
    const int physicalX = x * scale;
    const int physicalY = y * scale;
    for (int row = 0; row < scale; ++row) {
        for (int column = 0; column < scale; ++column) {
            drawPhysicalPixel(physicalX + column, physicalY + row, color);
        }
    }
}

void Canvas565::drawPhysicalPixel(int x, int y, uint16_t color) {
    const int scale = renderScale();
    if (!pixels_ || x < clipLeft_ * scale ||
        x >= clipRight_ * scale ||
        y < clipTop_ * scale ||
        y >= clipBottom_ * scale ||
        x < 0 || x >= physicalWidth_ || y < 0 || y >= physicalHeight_) {
        return;
    }
    pixels_[static_cast<uint32_t>(y) * physicalWidth_ + x] = encodeColor(color);
}

uint16_t Canvas565::readPixel(int x, int y) const {
    if (!pixels_ || x < 0 || x >= width_ || y < 0 || y >= height_) {
        return 0;
    }
    const int scale = renderScale();
    return decodeColor(pixels_[static_cast<uint32_t>(y * scale) *
                               physicalWidth_ + x * scale]);
}

bool Canvas565::blendRectAlphaNative(int x, int y, int w, int h,
                                    uint16_t color, uint8_t alpha) {
    return blendRoundRectAlphaNative(x, y, w, h, 0, color, alpha);
}

bool Canvas565::blendRoundRectAlphaNative(int x, int y, int w, int h, int radius,
                                         uint16_t color, uint8_t alpha) {
    if (renderScale() != 1) return false;
    if (!pixels_ || w <= 0 || h <= 0 || radius < 0 || alpha == 0) return true;
    radius = std::min(radius, std::min(w, h) / 2);
    if (alpha == 255 && radius == 0) {
        fillRect(x, y, w, h, color);
        return true;
    }

    const int left = std::max(x, clipLeft_);
    const int top = std::max(y, clipTop_);
    const int right = std::min(x + w, clipRight_);
    const int bottom = std::min(y + h, clipBottom_);
    if (left >= right || top >= bottom) return true;

    const uint8_t inverse = static_cast<uint8_t>(255 - alpha);
    const uint8_t sourceR = static_cast<uint8_t>(((color >> 11) & 31) * 255 / 31);
    const uint8_t sourceG = static_cast<uint8_t>(((color >> 5) & 63) * 255 / 63);
    const uint8_t sourceB = static_cast<uint8_t>((color & 31) * 255 / 31);
    uint16_t red[32], green[64], blue[32];
    for (int index = 0; index < 32; ++index) {
        const int channel = index * 255 / 31;
        red[index] = static_cast<uint16_t>(
            ((sourceR * alpha + channel * inverse) / 255) >> 3) << 11;
        blue[index] = static_cast<uint16_t>(
            ((sourceB * alpha + channel * inverse) / 255) >> 3);
    }
    for (int index = 0; index < 64; ++index) {
        const int channel = index * 255 / 63;
        green[index] = static_cast<uint16_t>(
            ((sourceG * alpha + channel * inverse) / 255) >> 2) << 5;
    }
    for (int row = top; row < bottom; ++row) {
        const int cornerY = row < y + radius ? y + radius
                            : row >= y + h - radius ? y + h - radius - 1 : row;
        const int dy = row - cornerY;
        int inset = 0;
        while (inset < radius &&
               (radius - inset) * (radius - inset) + dy * dy > radius * radius) ++inset;
        const int rowLeft = std::max(left, x + inset);
        const int rowRight = std::min(right, x + w - inset);
        if (rowLeft >= rowRight) continue;
        uint16_t* pixel = pixels_ + static_cast<size_t>(row) * physicalWidth_ + rowLeft;
        for (int column = rowLeft; column < rowRight; ++column, ++pixel) {
            const uint16_t previous = decodeColor(*pixel);
            *pixel = encodeColor(static_cast<uint16_t>(
                red[previous >> 11] |
                green[(previous >> 5) & 63] |
                blue[previous & 31]));
        }
    }
    return true;
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
    const int scale = renderScale();
    const int physicalLeft = left * scale;
    const int physicalRight = right * scale;
    const int physicalTop = top * scale;
    const int physicalBottom = bottom * scale;
    for (int py = physicalTop; py < physicalBottom; ++py) {
        uint16_t* row = pixels_ + static_cast<uint32_t>(py) * physicalWidth_;
        std::fill(row + physicalLeft, row + physicalRight, stored);
    }
}

void Canvas565::drawAssetPixel(int x, int y, uint16_t color) {
    const int scale = assetScale_;
    if (scale <= 1) {
        drawPixel(x, y, color);
        return;
    }
    fillRect(x * scale, y * scale, scale, scale, color);
}

void Canvas565::fillAssetRect(int x, int y, int w, int h, uint16_t color) {
    const int scale = assetScale_;
    if (scale <= 1) {
        fillRect(x, y, w, h, color);
        return;
    }
    fillRect(x * scale, y * scale, w * scale, h * scale, color);
}

void Canvas565::multiplyRectRgb(int x, int y, int w, int h,
                                 uint16_t red, uint16_t green, uint16_t blue) {
    if (!pixels_ || w <= 0 || h <= 0 || (red == 256 && green == 256 && blue == 256)) return;
    const int scale = renderScale();
    const int left = std::max(x, clipLeft_) * scale;
    const int right = std::min(x + w, clipRight_) * scale;
    const int top = std::max(y, clipTop_) * scale;
    const int bottom = std::min(y + h, clipBottom_) * scale;
    if (left >= right || top >= bottom) return;
    uint16_t r[32], g[64], b[32];
    for (int i = 0; i < 64; ++i) {
        g[i] = encodeColor(uint16_t(std::min(63, (i * green + 128) / 256) << 5));
        if (i >= 32) continue;
        r[i] = encodeColor(uint16_t(std::min(31, (i * red + 128) / 256) << 11));
        b[i] = encodeColor(uint16_t(std::min(31, (i * blue + 128) / 256)));
    }
    // Decode channel indices directly from storage. Keep byte-order branching
    // and function calls outside the full-screen PSRAM pixel loop.
    if (byteSwapped_) {
        for (int row = top; row < bottom; ++row) {
            uint16_t* pixel = pixels_ + row * physicalWidth_ + left;
            for (int column = left; column < right; ++column, ++pixel) {
                const uint16_t stored = *pixel;
                *pixel = r[(stored >> 3) & 31] |
                         g[((stored & 7) << 3) | (stored >> 13)] |
                         b[(stored >> 8) & 31];
            }
        }
    } else {
        for (int row = top; row < bottom; ++row) {
            uint16_t* pixel = pixels_ + row * physicalWidth_ + left;
            for (int column = left; column < right; ++column, ++pixel) {
                const uint16_t color = *pixel;
                *pixel = r[color >> 11] | g[(color >> 5) & 63] | b[color & 31];
            }
        }
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
    if (renderScale() == 1 && assetScale_ == 1 && !byteSwapped_ &&
        x >= clipLeft_ && y >= clipTop_ &&
        x + w <= clipRight_ && y + h <= clipBottom_ &&
        x >= 0 && y >= 0 && x + w <= physicalWidth_ &&
        y + h <= physicalHeight_) {
        for (int row = 0; row < h; ++row) {
            std::memcpy(pixels_ + static_cast<size_t>(y + row) * physicalWidth_ + x,
                        source + static_cast<size_t>(row) * w,
                        static_cast<size_t>(w) * sizeof(uint16_t));
        }
        return;
    }
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            drawAssetPixel(x + px, y + py,
                           source[static_cast<uint32_t>(py) * w + px]);
        }
    }
}

void Canvas565::pushImageNative(int x, int y, int w, int h,
                                const uint16_t* source) {
    if (!pixels_ || !source || w <= 0 || h <= 0 || renderScale() != 1 ||
        assetScale_ != 1) return;
    const int left = std::max(x, clipLeft_);
    const int top = std::max(y, clipTop_);
    const int right = std::min(x + w, clipRight_);
    const int bottom = std::min(y + h, clipBottom_);
    if (left >= right || top >= bottom) return;
    for (int row = top; row < bottom; ++row) {
        const int sourceX = left - x;
        const uint16_t* src = source + static_cast<size_t>(row - y) * w + sourceX;
        uint16_t* dst = pixels_ + static_cast<size_t>(row) * physicalWidth_ + left;
        const size_t count = static_cast<size_t>(right - left);
        std::memcpy(dst, src, count * sizeof(uint16_t));
    }
}

void Canvas565::drawMaskedAssetImage(int x, int y, int w, int h,
                                     const uint16_t* pixels,
                                     const uint8_t* opaqueMask,
                                     float pixelScale) {
    if (!pixels_ || !pixels || !opaqueMask || w <= 0 || h <= 0) return;
    if (pixelScale > 0.0f && pixelScale != std::floor(pixelScale)) {
        const int renderScale = this->renderScale();
        const float step = pixelScale * assetScale_ * renderScale;
        const int originX = x * assetScale_ * renderScale;
        const int originY = y * assetScale_ * renderScale;
        const int left = std::max(originX, clipLeft_ * renderScale);
        const int right = std::min(originX + int(std::lround(w * step)), clipRight_ * renderScale);
        const int top = std::max(originY, clipTop_ * renderScale);
        const int bottom = std::min(originY + int(std::lround(h * step)), clipBottom_ * renderScale);
        if (left >= right || top >= bottom) return;
        // Destination sampling keeps transparent edges exact and preserves
        // the same source pixel when clipping a fractional-size tile.
        for (int row = top; row < bottom; ++row) {
            const int sourceRow = std::min(h - 1, int((row - originY) / step));
            uint16_t* dest = pixels_ + row * physicalWidth_ + left;
            for (int col = left; col < right; ++col, ++dest) {
                const int sourceCol = std::min(w - 1, int((col - originX) / step));
                const int source = sourceRow * w + sourceCol;
                if (opaqueMask[source >> 3] & (1U << (source & 7)))
                    *dest = encodeColor(pixels[source]);
            }
        }
        return;
    }
    const int assetScale = assetScale_ * std::max<int>(1, pixelScale);
    const int renderScale = this->renderScale();
    const int logicalLeft = x * assetScale_;
    const int logicalTop = y * assetScale_;
    const int physicalScale = assetScale * renderScale;
    const int physicalWidth = physicalWidth_;
    const int physicalHeight = physicalHeight_;

    // Native pixel art needs one clipped row walk, not a scaled rectangle
    // calculation and fill call for every opaque source pixel.
    if (assetScale == 1 && renderScale == 1) {
        const int left = std::max(logicalLeft, clipLeft_);
        const int right = std::min(logicalLeft + w, clipRight_);
        const int top = std::max(logicalTop, clipTop_);
        const int bottom = std::min(logicalTop + h, clipBottom_);
        if (left >= right || top >= bottom) return;
        for (int row = top; row < bottom; ++row) {
            uint32_t source = static_cast<uint32_t>(row - logicalTop) * w + left - logicalLeft;
            uint16_t* dest = pixels_ + row * physicalWidth_ + left;
            if (byteSwapped_) {
                for (int col = left; col < right; ++col, ++source, ++dest)
                    if (opaqueMask[source >> 3] & (1U << (source & 7))) {
                        const uint16_t color = pixels[source];
                        *dest = static_cast<uint16_t>((color << 8) | (color >> 8));
                    }
            } else {
                for (int col = left; col < right; ++col, ++source, ++dest)
                    if (opaqueMask[source >> 3] & (1U << (source & 7)))
                        *dest = pixels[source];
            }
        }
        return;
    }

    // Clip once, then expand each visible source pixel to two adjacent rows.
    // The two clipped edge columns are handled separately from the hot loop.
    if (assetScale == 2 && renderScale == 1) {
        const int left = std::max(logicalLeft, clipLeft_);
        const int right = std::min(logicalLeft + w * 2, clipRight_);
        const int top = std::max(logicalTop, clipTop_);
        const int bottom = std::min(logicalTop + h * 2, clipBottom_);
        if (left >= right || top >= bottom) return;
        const int firstRow = (top - logicalTop) / 2;
        const int lastRow = (bottom - logicalTop + 1) / 2;
        const int firstCol = (left - logicalLeft) / 2;
        for (int sourceRow = firstRow; sourceRow < lastRow; ++sourceRow) {
            const int y0 = std::max(top, logicalTop + sourceRow * 2);
            const int y1 = std::min(bottom - 1, logicalTop + sourceRow * 2 + 1);
            uint16_t* row0 = pixels_ + y0 * physicalWidth_ + left;
            uint16_t* row1 = pixels_ + y1 * physicalWidth_ + left;
            uint32_t source = static_cast<uint32_t>(sourceRow) * w + firstCol;
            int remaining = right - left;
            if ((left - logicalLeft) & 1) {
                if (opaqueMask[source >> 3] & (1U << (source & 7)))
                    *row0 = *row1 = encodeColor(pixels[source]);
                ++row0; ++row1; ++source; --remaining;
            }
            for (; remaining >= 2; remaining -= 2, ++source, row0 += 2, row1 += 2) {
                if (!(opaqueMask[source >> 3] & (1U << (source & 7)))) continue;
                const uint16_t color = encodeColor(pixels[source]);
                row0[0] = row0[1] = row1[0] = row1[1] = color;
            }
            if (remaining && (opaqueMask[source >> 3] & (1U << (source & 7))))
                *row0 = *row1 = encodeColor(pixels[source]);
        }
        return;
    }

    for (int sourceRow = 0; sourceRow < h; ++sourceRow) {
        const int logicalY = logicalTop + sourceRow * assetScale;
        if (logicalY >= clipBottom_ ||
            logicalY + assetScale <= clipTop_) {
            continue;
        }
        for (int sourceCol = 0; sourceCol < w; ++sourceCol) {
            const uint32_t pixelIndex =
                static_cast<uint32_t>(sourceRow) * w + sourceCol;
            if ((opaqueMask[pixelIndex >> 3] &
                 (1U << (pixelIndex & 7))) == 0) {
                continue;
            }
            const int logicalX = logicalLeft + sourceCol * assetScale;
            if (logicalX >= clipRight_ ||
                logicalX + assetScale <= clipLeft_) {
                continue;
            }
            const int physicalLeft = logicalX * renderScale;
            const int physicalTop = logicalY * renderScale;
            const int physicalRight = std::min(
                physicalWidth, physicalLeft + physicalScale);
            const int physicalBottom = std::min(
                physicalHeight, physicalTop + physicalScale);
            const int clippedLeft = std::max(
                physicalLeft, clipLeft_ * renderScale);
            const int clippedTop = std::max(
                physicalTop, clipTop_ * renderScale);
            const int clippedRight = std::min(
                physicalRight, clipRight_ * renderScale);
            const int clippedBottom = std::min(
                physicalBottom, clipBottom_ * renderScale);
            if (clippedLeft >= clippedRight || clippedTop >= clippedBottom) {
                continue;
            }
            const uint16_t stored = encodeColor(pixels[pixelIndex]);
            for (int physicalY = clippedTop; physicalY < clippedBottom;
                 ++physicalY) {
                std::fill(
                    pixels_ + static_cast<uint32_t>(physicalY) * physicalWidth +
                        clippedLeft,
                    pixels_ + static_cast<uint32_t>(physicalY) * physicalWidth +
                        clippedRight,
                    stored);
            }
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
            drawAssetPixel(x + column, y + row, color);
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
