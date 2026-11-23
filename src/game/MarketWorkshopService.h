#pragma once
#include "game/GameState.h"
#include "game/WallpaperCatalog.h"

namespace Game::MarketWorkshop {
inline constexpr uint16_t STALL_SPECIES=115;
// Stable furniture ID0. Ownership doubles as the once-per-save gift marker.
inline constexpr uint16_t PHOTO_FRAME=uint16_t(1u<<0);
enum class Result : uint8_t { OK, ALREADY, INSUFFICIENT_COINS, UNKNOWN_WALLPAPER };
inline bool hasFrame(const GameState& state){return (state.room.ownedFurniture&PHOTO_FRAME)!=0;}
inline Result grantFrame(GameState& state){
    if(hasFrame(state))return Result::ALREADY;
    state.room.ownedFurniture|=PHOTO_FRAME;
    return Result::OK;
}

inline Wallpaper equippedWallpaper(const GameState& state) {
    const auto id = static_cast<Wallpaper>(state.wallpaperId);
    return wallpaperEntry(id) ? id : Wallpaper::ORIGINAL;
}

inline Result applyWallpaper(GameState& state, Wallpaper wallpaper) {
    const auto* entry = wallpaperEntry(wallpaper);
    if (!entry) return Result::UNKNOWN_WALLPAPER;
    if (equippedWallpaper(state) == wallpaper) return Result::ALREADY;
    if (state.coins < entry->price) return Result::INSUFFICIENT_COINS;
    state.coins -= entry->price;
    state.wallpaperId = static_cast<uint8_t>(wallpaper);
    return Result::OK;
}

// Before schema 11, the Forest placement bit selected the finish, even after
// single-use purchases stopped setting the old ownership bit.
inline void migrateLegacyWallpaper(GameState& state) {
    constexpr uint16_t forestBit = 1u << 1;
    state.wallpaperId = static_cast<uint8_t>(
        state.room.placedFurniture & forestBit ? Wallpaper::FOREST : Wallpaper::ORIGINAL);
    state.room.ownedFurniture &= static_cast<uint16_t>(~forestBit);
    state.room.placedFurniture &= static_cast<uint16_t>(~forestBit);
}
}
