#pragma once

#include <cstdint>

#include "AmoledGeometry.h"

namespace AmoledV1::UiMetrics {

inline constexpr int HOME_HEADER_HEIGHT = 48;
inline constexpr int HOME_ROOM_HEIGHT = 296;
inline constexpr int HOME_STATUS_TOP = HOME_HEADER_HEIGHT + HOME_ROOM_HEIGHT;
inline constexpr int HEADER_HEIGHT = 56;
inline constexpr int PAGE_HEADER_HEIGHT = 76;
inline constexpr int PAGE_HEADER_BACK_HIT_WIDTH = 80;
inline constexpr int CONTENT_TOP = PAGE_HEADER_HEIGHT;
inline constexpr uint16_t PAGE_BACKGROUND = 0x0000;
inline constexpr int DETAIL_BUTTON_Y = 354;
inline constexpr int DETAIL_BUTTON_HEIGHT = 72;
inline constexpr AmoledUi::Rect DETAIL_ACTION_RECT{
    36, DETAIL_BUTTON_Y, 140, DETAIL_BUTTON_HEIGHT};
inline constexpr AmoledUi::Rect DETAIL_BACK_RECT{
    192, DETAIL_BUTTON_Y, 140, DETAIL_BUTTON_HEIGHT};

}  // namespace AmoledV1::UiMetrics
