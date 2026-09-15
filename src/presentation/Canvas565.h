#pragma once

#include <cstdint>
#include "platform/api/PlatformServices.h"

class Canvas565 {
public:
    void attach(const Platform::FrameBuffer565& frameBuffer);
    bool attached() const { return pixels_ != nullptr; }

    // coordinateScale is the raster scale used by legacy targets.
    void setCoordinateScale(uint8_t scale);
    uint8_t coordinateScale() const { return coordinateScale_; }
    // Keep page layout scaling independent from the raster scale. AMOLED uses
    // a direct 1x raster with the existing 2x page profile during migration.
    void setLayoutScale(uint8_t scale);
    uint8_t layoutScale() const { return layoutScale_; }
    // Packed gameplay art is authored in the legacy 184x224 pixel grid while
    // page UI is authored directly in the AMOLED framebuffer grid.
    void setAssetScale(uint8_t scale);
    uint8_t assetScale() const { return assetScale_; }
    void setNativeText(bool enabled) { nativeText_ = enabled; }
    bool nativeText() const { return nativeText_; }
    uint8_t renderScale() const {
        return static_cast<uint8_t>(coordinateScale_ * layoutScale_);
    }
    int width() const { return width_; }
    int height() const { return height_; }
    int physicalWidth() const { return physicalWidth_; }
    int physicalHeight() const { return physicalHeight_; }
    bool byteSwapped() const { return byteSwapped_; }
    uint32_t pixelCount() const {
        return static_cast<uint32_t>(physicalWidth_) * physicalHeight_;
    }

    uint16_t* rawPixels() { return pixels_; }
    const uint16_t* rawPixels() const { return pixels_; }
    uint16_t encodeColor(uint16_t color) const;
    uint16_t decodeColor(uint16_t stored) const;

    void fillSprite(uint16_t color);
    void drawPixel(int x, int y, uint16_t color);
    void drawPhysicalPixel(int x, int y, uint16_t color);
    uint16_t readPixel(int x, int y) const;
    void drawFastHLine(int x, int y, int w, uint16_t color);
    void drawFastVLine(int x, int y, int h, uint16_t color);
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void drawCircle(int cx, int cy, int radius, uint16_t color);
    void fillCircle(int cx, int cy, int radius, uint16_t color);
    void fillEllipse(int cx, int cy, int rx, int ry, uint16_t color);
    void fillTriangle(int x0, int y0, int x1, int y1,
                      int x2, int y2, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h,
                       int radius, uint16_t color);
    void drawRoundRect(int x, int y, int w, int h,
                       int radius, uint16_t color);
    void pushImage(int x, int y, int w, int h, const uint16_t* source);
    void drawMaskedAssetImage(int x, int y, int w, int h,
                              const uint16_t* pixels,
                              const uint8_t* opaqueMask,
                              uint8_t pixelScale = 1);
    void drawAssetPixel(int x, int y, uint16_t color);
    void fillAssetRect(int x, int y, int w, int h, uint16_t color);
    void drawRgb565Rle(int x, int y, int w, int h,
                       const uint16_t* data, uint32_t offset,
                       uint32_t length, bool flipX = false);

    void setClipRect(int x, int y, int w, int h);
    void clearClipRect();

private:
    bool visible(int x, int y) const;

    uint16_t* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int physicalWidth_ = 0;
    int physicalHeight_ = 0;
    uint8_t coordinateScale_ = 1;
    uint8_t layoutScale_ = 1;
    uint8_t assetScale_ = 1;
    bool nativeText_ = false;
    bool byteSwapped_ = false;
    int clipLeft_ = 0;
    int clipTop_ = 0;
    int clipRight_ = 0;
    int clipBottom_ = 0;
};
