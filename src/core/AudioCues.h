#pragma once

#include <cstdint>

enum class MusicTrack : uint8_t {
    NONE = 0,
    HOME,
    EXPLORE,
    BATTLE,
    BATTLE_SPECIAL,
};

enum class SfxCue : uint8_t {
    UI_CURSOR = 0,
    UI_CONFIRM,
    UI_CANCEL,
    MENU_OPEN,
    MENU_CLOSE,
    DAMAGE_NORMAL,
    DAMAGE_SUPER,
    DAMAGE_WEAK,
    THROW,
    FAINT,
    EXP_GAIN,
    EXP_FULL,
    LEVEL_UP,
    MOVE_LEARNT,
    CONTACT,
};
