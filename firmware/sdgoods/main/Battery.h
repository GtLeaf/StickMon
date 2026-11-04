// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ChargingTrend.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

namespace Sdgoods {
class Battery {
public:
    Battery() = default;
    ~Battery();
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;
    bool begin();
    void poll(uint32_t now);
    int level(uint32_t now);
    bool chargingEstimate(uint32_t now) { poll(now); return trend_.charging(); }
    void resetChargingEstimate() { trend_.reset(); }
private:
    adc_oneshot_unit_handle_t adc_ = nullptr;
    adc_cali_handle_t calibration_ = nullptr;
    uint32_t sampledAt_ = 0;
    uint32_t levelSampledAt_ = 0;
    int millivolts_ = 0, level_ = -1;
    bool sampled_ = false, levelSampled_ = false;
    ChargingTrend trend_;
};
}  // namespace Sdgoods
