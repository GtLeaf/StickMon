// SPDX-License-Identifier: Apache-2.0
#include "Audio.h"

#include <algorithm>
#include "AudioPolicy.h"
#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/task.h"

namespace Sdgoods {
void Audio::amplifier(bool enabled) {
    gpio_set_level(BOARD_AUDIO_PA_EN_GPIO, enabled ? BOARD_AUDIO_PA_EN_ACTIVE_LEVEL
                                                : 1 - BOARD_AUDIO_PA_EN_ACTIVE_LEVEL);
}

bool Audio::begin() {
    gpio_config_t pa{};
    pa.pin_bit_mask = 1ULL << BOARD_AUDIO_PA_EN_GPIO;
    pa.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&pa) != ESP_OK) return false;
    amplifier(false);
    gpio_hold_dis(BOARD_AUDIO_PA_EN_GPIO);
    mutex_ = xSemaphoreCreateMutex();
    micMutex_ = xSemaphoreCreateMutex();
    if (!mutex_ || !micMutex_) return false;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    channel.dma_desc_num = 6;
    channel.dma_frame_num = 240;
    if (i2s_new_channel(&channel, &tx_, nullptr) != ESP_OK) return false;
    i2s_std_config_t config{};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(PcmMixer::OUTPUT_SAMPLE_RATE);
    config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = BOARD_SPK_I2S_GPIO_MCLK;
    config.gpio_cfg.bclk = BOARD_SPK_I2S_GPIO_BCLK;
    config.gpio_cfg.ws = BOARD_SPK_I2S_GPIO_WS;
    config.gpio_cfg.dout = BOARD_SPK_I2S_GPIO_DOUT;
    config.gpio_cfg.din = GPIO_NUM_NC;
    if (i2s_channel_init_std_mode(tx_, &config) == ESP_OK &&
        xTaskCreatePinnedToCore([](void* context) { static_cast<Audio*>(context)->run(); },
            "sg_audio", 4096, this, 2, nullptr, 1) == pdPASS) {
        Platform::logLine("[SDGOODS] I2S speaker ready; initial volume=0 PA=OFF; microphone on demand");
        return true;
    }
    i2s_del_channel(tx_); tx_ = nullptr;
    return false;
}

void Audio::run() {
    int16_t mono[PcmMixer::MIX_BLOCK_SAMPLES]{};
    int16_t stereo[PcmMixer::MIX_BLOCK_SAMPLES * 2]{};
    for (;;) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const bool active = !suspended_ && mixer_.anyActive();
        const bool audible = AudioPolicy::audible(volume_, active, suspended_);
        if (!audible) amplifier(false);
        if (active) {
            mixer_.mix(mono, PcmMixer::MIX_BLOCK_SAMPLES);
            AudioPolicy::stereo(mono, stereo, PcmMixer::MIX_BLOCK_SAMPLES, volume_);
        }
        // Gate output while muted; still retire queued PCM at its normal rate.
        if (audible && !txEnabled_) {
            txEnabled_ = i2s_channel_enable(tx_) == ESP_OK;
        }
        bool ok = true;
        if (audible && txEnabled_) {
            size_t written = 0;
            ok = i2s_channel_write(tx_, stereo, sizeof(stereo), &written, 100) == ESP_OK &&
                 written == sizeof(stereo);
            amplifier(ok);
        } else if (txEnabled_) {
            i2s_channel_disable(tx_);
            txEnabled_ = false;
        }
        if (!ok) {
            amplifier(false);
            mixer_.stopAll();
            Platform::logLine("[SDGOODS] audio write failed; output muted");
        }
        xSemaphoreGive(mutex_);
        if (!audible || !txEnabled_ || !ok)
            vTaskDelay(pdMS_TO_TICKS(active ? 16 : 10));
    }
}

void Audio::setVolume(uint8_t value) {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    volume_ = std::min<uint8_t>(value, 100);
    if (!volume_) amplifier(false);
    if (mutex_) xSemaphoreGive(mutex_);
    Platform::logf("[SDGOODS] volume=%u; mute PA=%s\n", unsigned(volume()), volume() ? "eligible" : "OFF");
}
void Audio::suspend(bool value) {
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    suspended_ = value;
    if (value) {
        amplifier(false); mixer_.stopAll();
        if (txEnabled_) { i2s_channel_disable(tx_); txEnabled_ = false; }
    }
    xSemaphoreGive(mutex_);
}
bool Audio::playPcmU8(const uint8_t* data, size_t count, uint32_t rate) {
    return playPcmU8Channel(data, count, rate, 0, false);
}
bool Audio::playPcmU8Channel(const uint8_t* data, size_t count, uint32_t rate, uint8_t ch, bool replace) {
    if (!tx_ || !mutex_ || ch >= CHANNEL_COUNT) return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool ok = !suspended_ && mixer_.start(static_cast<PcmMixer::Channel>(ch),
        {data, count, rate, 100, false}, replace ? PcmMixer::StartMode::REPLACE : PcmMixer::StartMode::APPEND);
    xSemaphoreGive(mutex_);
    return ok;
}
void Audio::setChannelVolume(uint8_t ch, uint8_t value) {
    if (!mutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.setVolume(static_cast<PcmMixer::Channel>(ch), std::min<uint8_t>(value, 100));
    xSemaphoreGive(mutex_);
}
bool Audio::playing() const {
    if (!mutex_) return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const bool result = mixer_.anyActive();
    xSemaphoreGive(mutex_); return result;
}
uint8_t Audio::queuedPcm(uint8_t ch) const {
    if (!mutex_ || ch >= CHANNEL_COUNT) return 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const auto result = mixer_.queuedBlocks(static_cast<PcmMixer::Channel>(ch));
    xSemaphoreGive(mutex_); return result;
}
void Audio::stop() {
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.stopAll(); amplifier(false);
    xSemaphoreGive(mutex_);
}
void Audio::stopChannel(uint8_t ch) {
    if (!mutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.stop(static_cast<PcmMixer::Channel>(ch));
    if (!mixer_.anyActive()) amplifier(false);
    xSemaphoreGive(mutex_);
}

bool Audio::beginMicrophone() {
    if (!micMutex_) return false;
    xSemaphoreTake(micMutex_, portMAX_DELAY);
    bool ok = true;
    if (!rx_) {
        i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
        ok = i2s_new_channel(&channel, nullptr, &rx_) == ESP_OK;
        if (ok) {
            i2s_std_config_t config{};
            config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(microphoneRate_);
            config.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
            config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
            config.gpio_cfg.mclk = GPIO_NUM_NC;
            config.gpio_cfg.bclk = BOARD_MIC_I2S_GPIO_BCLK;
            config.gpio_cfg.ws = BOARD_MIC_I2S_GPIO_WS;
            config.gpio_cfg.dout = GPIO_NUM_NC;
            config.gpio_cfg.din = BOARD_MIC_I2S_GPIO_DIN;
            ok = i2s_channel_init_std_mode(rx_, &config) == ESP_OK;
            if (!ok) { i2s_del_channel(rx_); rx_ = nullptr; }
        }
    }
    if (ok && !microphoneOn_) {
        ok = i2s_channel_enable(rx_) == ESP_OK;
        microphoneOn_ = ok;
    }
    xSemaphoreGive(micMutex_);
    return ok;
}
void Audio::endMicrophone() {
    if (!micMutex_) return;
    xSemaphoreTake(micMutex_, portMAX_DELAY);
    if (microphoneOn_) i2s_channel_disable(rx_);
    if (rx_) { i2s_del_channel(rx_); rx_ = nullptr; }
    microphoneOn_ = false;
    xSemaphoreGive(micMutex_);
}
bool Audio::recordMicrophone(int16_t* data, size_t count, uint32_t rate) {
    if (!data || !count || rate < 8000 || rate > 48000 || !beginMicrophone()) return false;
    xSemaphoreTake(micMutex_, portMAX_DELAY);
    bool ok = microphoneOn_;
    if (ok && rate != microphoneRate_) {
        i2s_channel_disable(rx_);
        i2s_std_clk_config_t clock = I2S_STD_CLK_DEFAULT_CONFIG(rate);
        ok = i2s_channel_reconfig_std_clock(rx_, &clock) == ESP_OK;
        if (ok) microphoneRate_ = rate;
        microphoneOn_ = i2s_channel_enable(rx_) == ESP_OK;
        ok = ok && microphoneOn_;
    }
    recording_ = ok;
    int32_t raw[256];
    size_t offset = 0;
    while (ok && offset < count) {
        const size_t n = std::min<size_t>(256, count - offset);
        size_t bytes = 0;
        ok = i2s_channel_read(rx_, raw, n * sizeof(int32_t), &bytes, 500) == ESP_OK && bytes != 0;
        for (size_t i = 0; ok && i < bytes / sizeof(int32_t); ++i)
            data[offset++] = AudioPolicy::microphoneSample(raw[i]);
    }
    recording_ = false;
    xSemaphoreGive(micMutex_);
    return ok && offset == count;
}
}  // namespace Sdgoods
