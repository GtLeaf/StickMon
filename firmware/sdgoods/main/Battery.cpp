// SPDX-License-Identifier: Apache-2.0
#include "Battery.h"
#include "BatteryPolicy.h"
#include "esp_adc/adc_cali_scheme.h"
#include "core/BuildConfig.h"
#include "platform/api/PlatformServices.h"

namespace Sdgoods {
Battery::~Battery() {
    if (calibration_) adc_cali_delete_scheme_curve_fitting(calibration_);
    if (adc_) adc_oneshot_del_unit(adc_);
}
bool Battery::begin() {
    if (adc_ && calibration_) return true;
    if (!adc_) {
        adc_oneshot_unit_init_cfg_t unit{};
        unit.unit_id = ADC_UNIT_1;
        if (adc_oneshot_new_unit(&unit, &adc_) != ESP_OK) return false;
        adc_oneshot_chan_cfg_t channel{};
        channel.atten = ADC_ATTEN_DB_12;
        channel.bitwidth = ADC_BITWIDTH_12;
        if (adc_oneshot_config_channel(adc_, ADC_CHANNEL_7, &channel) != ESP_OK) {
            adc_oneshot_del_unit(adc_); adc_ = nullptr; return false;
        }
    }
    adc_cali_curve_fitting_config_t config{};
    config.unit_id = ADC_UNIT_1;
    config.chan = ADC_CHANNEL_7;
    config.atten = ADC_ATTEN_DB_12;
    config.bitwidth = ADC_BITWIDTH_12;
    if (adc_cali_create_scheme_curve_fitting(&config, &calibration_) != ESP_OK) {
        calibration_ = nullptr;
        Platform::errorLine("[SDGOODS] battery calibration unavailable; level unknown");
        return false;
    }
    sampled_ = levelSampled_ = false;
    trend_.reset();
    return true;
}
int Battery::level(uint32_t now) {
    poll(now);
    return level_;
}
void Battery::poll(uint32_t now) {
    if (sampled_ && now - sampledAt_ < ChargingTrend::SAMPLE_MS) return;
    sampledAt_ = now; sampled_ = true;
    int readings[8]{};
    unsigned count = 0;
    for (unsigned i = 0; adc_ && calibration_ && i < 8; ++i) {
        int raw = 0, mv = 0;
        if (adc_oneshot_read(adc_, ADC_CHANNEL_7, &raw) != ESP_OK || raw <= 0 || raw >= 4095 ||
            adc_cali_raw_to_voltage(calibration_, raw, &mv) != ESP_OK || mv < 834 || mv > 1500) continue;
        readings[count++] = mv * 3;
    }
    const int previous = level_;
    const bool wasCharging = trend_.charging();
    if (count < 6) {
        millivolts_ = 0; level_ = -1;
        levelSampled_ = false;
        trend_.reset();
        if (previous >= 0) Platform::errorLine("[SDGOODS] battery read failed; level unknown");
        if (wasCharging) Platform::errorLine("[SDGOODS] charging_estimate=0; ADC unavailable");
        return;
    }
    // Trim one high/low reading, then smooth without retaining stale failures.
    // Eight readings do not need the STL introsort's larger-array fast path.
    // A bounded insertion sort also avoids GCC's -Os array-bounds false alarm.
    for (unsigned i = 1; i < count; ++i) {
        const int value = readings[i];
        unsigned j = i;
        while (j > 0 && readings[j - 1] > value) {
            readings[j] = readings[j - 1];
            --j;
        }
        readings[j] = value;
    }
    int total = 0;
    for (unsigned i = 1; i + 1 < count; ++i) total += readings[i];
    const int mv = (total + (count - 2) / 2) / (count - 2);
    trend_.sample(now, mv);
    if (!levelSampled_ || now - levelSampledAt_ >= 10000U) {
        levelSampled_ = true;
        levelSampledAt_ = now;
        millivolts_ = millivolts_ ? (millivolts_ * 4 + mv) / 5 : mv;
        level_ = batteryPercent(millivolts_);
        if (previous != level_) Platform::logf("[SDGOODS] battery=%dmV estimate=%d%%\n", millivolts_, level_);
#if STICKMON_ENABLE_TRACE_LOGS
        Platform::logf("[ChargeEstimate] sample_mv=%d charging=%u source=voltage_trend\n",
            mv, unsigned(trend_.charging()));
#endif
    }
    if (wasCharging != trend_.charging())
        Platform::logf("[SDGOODS] charging_estimate=%u source=voltage_trend mv=%d\n",
            unsigned(trend_.charging()), mv);
}
}  // namespace Sdgoods
