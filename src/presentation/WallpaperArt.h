#pragma once

#include <algorithm>
#include "game/WallpaperCatalog.h"
#include "presentation/PixelRenderer.h"

namespace WallpaperArt {
namespace Detail {
struct Swatch {
    const uint8_t* pixels;
    const uint16_t* palette;
};

#ifdef STICKMON_WALLPAPER_ICONS_INCLUDE
#include STICKMON_WALLPAPER_ICONS_INCLUDE
#else
#include "assets/WallpaperIcons.inc"
#endif
static_assert(sizeof(Swatches) / sizeof(Swatches[0]) == Game::MarketWorkshop::WALLPAPER_COUNT);

inline void draw(int x, int y, int size, const uint8_t* pixels, const uint16_t* palette,
                 int sourceSize) {
    auto& canvas = PixelRenderer::canvas();
    for (int py = std::max(y, canvas.clipTop()); py < std::min(y + size, canvas.clipBottom()); ++py) {
        const auto* row = pixels + (py - y) * sourceSize / size * sourceSize;
        for (int px = std::max(x, canvas.clipLeft()); px < std::min(x + size, canvas.clipRight()); ++px) {
            const uint8_t index = row[(px - x) * sourceSize / size];
            if (index) canvas.drawPixel(px, py, palette[index]);
        }
    }
}
}

inline void drawSwatch(int x, int y, int size, Game::MarketWorkshop::Wallpaper wallpaper) {
    const auto index = Game::MarketWorkshop::wallpaperIndex(wallpaper);
    if (index >= Game::MarketWorkshop::WALLPAPER_COUNT) return;
    const auto& swatch = Detail::Swatches[index];
    Detail::draw(x, y, size, swatch.pixels, swatch.palette, 32);
}

inline void drawWorkshopIcon(int x, int y, int size) {
    Detail::draw(x, y, size, Detail::WorkshopPixels, Detail::WorkshopPalette, 48);
}
}
