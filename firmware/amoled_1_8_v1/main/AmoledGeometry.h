#pragma once

#include <cstdint>

// AMOLED pages use the panel's native coordinate space. Gameplay and packed
// pixel assets still originate from the 184x224 profile, so their conversion
// is explicit at the page/resource boundary instead of being hidden in the
// framebuffer rasterizer.
namespace AmoledUi {

inline constexpr int WIDTH = 368;
inline constexpr int HEIGHT = 448;
inline constexpr int LEGACY_WIDTH = 184;
inline constexpr int LEGACY_HEIGHT = 224;
inline constexpr int RESOURCE_SCALE = 2;

constexpr int nativeCoordinate(int value) {
    return value * RESOURCE_SCALE;
}

constexpr int nativeExtent(int value) {
    return value * RESOURCE_SCALE;
}

constexpr int nativeRow(int value) {
    return value * RESOURCE_SCALE;
}

}  // namespace AmoledUi
