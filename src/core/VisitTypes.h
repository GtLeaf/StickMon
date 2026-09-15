#pragma once

#include <cstdint>

enum class VisitHostResult : uint8_t {
    ACCEPTED = 0,
    STORAGE_FULL = 1,
    NO_MONSTER = 2,
    TEAM_NOT_SOLO = 3,
};
