#pragma once

#include <cstddef>
#include <cstdint>

namespace Game::MarketWorkshop {
// Existing saves refer to these IDs. The table order controls browsing only.
enum class Wallpaper : uint8_t { ORIGINAL = 0, FOREST = 1 };

struct WallpaperEntry {
    Wallpaper id;
    const char* name;
    uint16_t price;
    const char* asset;
};

inline constexpr WallpaperEntry WALLPAPERS[] = {
#ifdef STICKMON_WALLPAPER_CATALOG_INCLUDE
#include STICKMON_WALLPAPER_CATALOG_INCLUDE
#else
#include "assets/WallpaperCatalog.inc"
#endif
};
inline constexpr size_t WALLPAPER_COUNT = sizeof(WALLPAPERS) / sizeof(WALLPAPERS[0]);

inline constexpr size_t wallpaperIndex(Wallpaper id) {
    for (size_t i = 0; i < WALLPAPER_COUNT; ++i) {
        if (WALLPAPERS[i].id == id) return i;
    }
    return WALLPAPER_COUNT;
}

inline constexpr const WallpaperEntry* wallpaperEntry(Wallpaper id) {
    const auto index = wallpaperIndex(id);
    return index < WALLPAPER_COUNT ? &WALLPAPERS[index] : nullptr;
}
}
