#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

// Set to 0 to restore the complete pre-trial selection layout and geometry.
#ifndef STICKMON_EXPLORE_POSTCARD
#define STICKMON_EXPLORE_POSTCARD 1
#endif
inline constexpr bool EXPLORE_POSTCARD = STICKMON_EXPLORE_POSTCARD != 0;

class PixelCache565;

inline constexpr int EXPLORE_SELECTOR_TOP_HEIGHT = EXPLORE_POSTCARD ? 218 : 104;
inline constexpr int EXPLORE_SELECTOR_CENTER_X = 184;
inline constexpr int EXPLORE_SELECTOR_AREA_SPACING = 144;
// Keep the centered action group close to the physical bottom of the AMOLED
// display while leaving a 12-pixel native bottom margin.
inline constexpr int EXPLORE_SELECTOR_BUTTON_TOP = 384;
inline constexpr int EXPLORE_PREVIEW_TOP = EXPLORE_POSTCARD ? 218 : 168;
inline constexpr int EXPLORE_PREVIEW_BOTTOM = EXPLORE_POSTCARD ? 372 : 328;

AmoledUi::Rect exploreRouteActorBounds(const ExploreRouteViewModel& model);
AmoledUi::Rect exploreRouteWildlifeBounds(const ExploreRouteViewModel& model,
                                         uint8_t index);

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         PixelCache565& backgroundCache,
                         uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount);
bool exploreStartAt(int x, int y);
bool exploreSelectionBackAt(int x, int y);

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& mapCache,
                              uint16_t rowBegin = 0,
                              uint16_t rowEnd = AmoledUi::HEIGHT);
bool exploreRouteBackAt(int x, int y);
bool exploreRouteBagAt(int x, int y);
bool exploreRouteMenuAt(int x, int y);
int exploreRouteExitChoiceAt(int x, int y);
bool exploreRouteMapAt(int x, int y);

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin = 0,
                             uint16_t rowEnd = AmoledUi::HEIGHT);
bool exploreRouteMenuBackAt(int x, int y);
int exploreRouteMenuItemAt(int x, int y);

}  // namespace AmoledV1
