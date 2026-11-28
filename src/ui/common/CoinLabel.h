#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace Ui {
struct CoinLabel {
    static constexpr uint32_t WAN = 10000;
    static constexpr uint32_t TENTH = WAN / 10;
    char text[16]{};

    CoinLabel() = default;
    explicit CoinLabel(uint32_t coins, bool exact = false) {
        const auto value = static_cast<unsigned long>(coins);
        if (exact || coins < WAN) std::snprintf(text, sizeof(text), "%lu", value);
        else if (coins / TENTH % 10)
            std::snprintf(text, sizeof(text), "%lu.%luw", value / WAN, value / TENTH % 10);
        else std::snprintf(text, sizeof(text), "%luw", value / WAN);
    }
    int cells() const { return static_cast<int>(std::strlen(text)); }

    // Consecutive positions are consecutive *visible* balances, including 9999 -> 1w.
    static uint32_t position(uint32_t coins) {
        return coins < WAN ? coins : WAN + (coins - WAN) / TENTH;
    }
    static CoinLabel at(uint32_t position) {
        return CoinLabel(position < WAN ? position : WAN + (position - WAN) * TENTH);
    }
};
} // namespace Ui
