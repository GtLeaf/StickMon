#pragma once
#include "AmoledGeometry.h"
#include "core/BuildConfig.h"

namespace AmoledV1 {
inline constexpr int SETTINGS_BRIGHTNESS_ITEM=0;
inline constexpr int SETTINGS_VOLUME_ITEM=1;
inline constexpr int SETTINGS_GAME_SPEED_ITEM=2;
inline constexpr int SETTINGS_POWER_SAVE_ITEM=3;
inline constexpr int SETTINGS_RESET_GAME_ITEM=4;
inline constexpr int SETTINGS_SYSTEM_UPDATE_ITEM=5;
inline constexpr int SETTINGS_SLIDER_LEFT=40;
inline constexpr int SETTINGS_SLIDER_RIGHT=328;

namespace SettingsLayout {
inline constexpr AmoledUi::Rect SLIDER_CARD{20,84,328,176};
inline constexpr AmoledUi::Rect OPTIONS_CARD{20,272,328,104};
inline constexpr int RADIUS=20;
constexpr bool hasSystemUpdate(bool supported){
    return STICKMON_ENABLE_DEBUG_FEATURES && supported;
}
constexpr int sliderCenter(int item){return 148+item*88;}
constexpr AmoledUi::Rect itemRect(int item,bool systemUpdate){
    switch(item){
    case 0:return {20,84,328,88};
    case 1:return {20,172,328,88};
    case 2:return {20,272,328,52};
    case 3:return {20,324,328,52};
    case 4:return {20,388,hasSystemUpdate(systemUpdate)?158:328,44};
    case 5:return hasSystemUpdate(systemUpdate)?AmoledUi::Rect{190,388,158,44}:AmoledUi::Rect{0,0,0,0};
    default:return {0,0,0,0};
    }
}
constexpr AmoledUi::Rect hitRect(int item,bool systemUpdate){
    // Labels occupy the full 32px above this band; keep their bottom rows inert.
    if(item==0 || item==1)return {28,sliderCenter(item)-20,312,44};
    return itemRect(item,systemUpdate);
}
constexpr AmoledUi::Rect resetChoiceRect(int choice){
    return {choice==0?20:190,384,158,48};
}
} // namespace SettingsLayout
} // namespace AmoledV1
