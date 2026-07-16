#pragma once

#include <cstdint>

long random(long maximum);
long random(long minimum, long maximum);

template <typename T>
constexpr T min(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
constexpr T max(T a, T b) {
    return a > b ? a : b;
}
