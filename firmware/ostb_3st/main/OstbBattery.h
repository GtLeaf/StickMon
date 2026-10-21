#pragma once

namespace Ostb {

// Raw 12-bit ADC2/CH6 curve recovered from this device's factory 2.2.2 image.
// This is a voltage-based estimate, not a coulomb-counting fuel gauge.
inline int batteryPercentFromRaw(int raw) {
    if (raw <= 0 || raw >= 4095) return -1;
    constexpr int samples[] = {1890, 1950, 2010, 2070, 2130, 2190,
                              2250, 2310, 2370, 2430, 2460, 2489};
    constexpr int levels[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100};
    if (raw <= samples[0]) return 0;
    for (int i = 1; i < 12; ++i) {
        if (raw < samples[i])
            return levels[i - 1] + (raw - samples[i - 1]) *
                (levels[i] - levels[i - 1]) / (samples[i] - samples[i - 1]);
    }
    return 100;
}

}  // namespace Ostb
