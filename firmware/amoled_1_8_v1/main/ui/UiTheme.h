#pragma once
#include <cstdint>

namespace AmoledV1::UiTheme {

constexpr uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
    return uint16_t(((r&0xf8U)<<8)|((g&0xfcU)<<3)|(b>>3));
}

struct Palette {
    uint16_t background;
    uint16_t surface;
    uint16_t control;
    uint16_t text;
    uint16_t secondaryText;
    uint16_t title;
    uint16_t accent;
    uint16_t accentText;
    uint16_t muted;
    uint16_t track;
    uint16_t border;
    uint16_t pressed;
    uint16_t danger;
    uint16_t dangerSurface;
    uint16_t disabledSurface;
    uint16_t disabledText;
    uint16_t sceneText;
    uint16_t sceneStatus;
    uint16_t sceneScrim;
    uint16_t sceneControl;
    uint16_t sceneShadow;
};

inline constexpr Palette CREAM_APRICOT{
    rgb565(247,236,217), // background
    rgb565(255,246,231), // surface
    rgb565(234,215,186), // control
    rgb565(89,67,46),    // text
    rgb565(128,105,80),  // secondaryText
    rgb565(128,83,57),   // title
    rgb565(168,89,56),   // accent
    rgb565(255,247,235), // accentText
    rgb565(182,160,128), // muted
    rgb565(229,211,184), // track
    rgb565(213,186,147), // border
    rgb565(244,230,207), // pressed
    rgb565(167,70,56),   // danger
    rgb565(241,218,203), // dangerSurface
    rgb565(227,212,191), // disabledSurface
    rgb565(154,134,110), // disabledText
    rgb565(255,244,224), // sceneText
    rgb565(255,241,217), // sceneStatus
    rgb565(34,21,10),    // sceneScrim
    rgb565(48,32,22),    // sceneControl
    rgb565(49,36,23),    // sceneShadow
};

// UI-thread configuration only. Callers request a full redraw after replacement.
// Copying the palette lets callers safely pass a temporary or local value.
inline Palette activePalette=CREAM_APRICOT;
inline const Palette& current(){return activePalette;}
inline void setPalette(const Palette& palette){activePalette=palette;}
inline void resetPalette(){setPalette(CREAM_APRICOT);}

} // namespace AmoledV1::UiTheme
