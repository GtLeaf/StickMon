// SPDX-License-Identifier: Apache-2.0
#include "Audio.h"

#include <algorithm>
#include <iterator>
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
    // 8 x 240 frames = 120 ms of DMA cushion at 16 kHz.
    channel.dma_desc_num = 8;
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

void Audio::publishQueued() {
    for (uint8_t ch = 0; ch < CHANNEL_COUNT; ++ch)
        queued_[ch] = mixer_.queuedBlocks(static_cast<PcmMixer::Channel>(ch));
}

void Audio::run() {
    int16_t mono[PcmMixer::MIX_BLOCK_SAMPLES]{};
    int16_t stereo[PcmMixer::MIX_BLOCK_SAMPLES * 2]{};
    AudioPolicy::OutputGate gate;
    for (;;) {
        // Hold the mixer lock only while mixing; the I2S write below blocks
        // for about one DMA block and must not stall UI-side audio calls.
        xSemaphoreTake(mutex_, portMAX_DELAY);
        const bool active = !suspended_ && mixer_.anyActive();
        if (active) mixer_.mix(mono, PcmMixer::MIX_BLOCK_SAMPLES);
        publishQueued();
        xSemaphoreGive(mutex_);

        const uint8_t volume = volume_.load();
        const bool output = gate.next(volume > 0 && !suspended_, active);
        if (!output) {
            amplifier(false);
            if (txEnabled_) { i2s_channel_disable(tx_); txEnabled_ = false; }
            // Muted playback still retires queued PCM at its normal rate.
            vTaskDelay(pdMS_TO_TICKS(active ? 16 : 10));
            continue;
        }
        if (active) AudioPolicy::stereo(mono, stereo, PcmMixer::MIX_BLOCK_SAMPLES, volume);
        else std::fill(std::begin(stereo), std::end(stereo), int16_t(0));
        if (!txEnabled_) txEnabled_ = i2s_channel_enable(tx_) == ESP_OK;
        size_t written = 0;
        const bool ok = txEnabled_ &&
            i2s_channel_write(tx_, stereo, sizeof(stereo), &written, 100) == ESP_OK &&
            written == sizeof(stereo);
        // Re-check after the blocking write: mute/suspend may have happened.
        amplifier(ok && volume_.load() > 0 && !suspended_);
        if (!ok) {
            amplifier(false);
            xSemaphoreTake(mutex_, portMAX_DELAY);
            mixer_.stopAll();
            publishQueued();
            xSemaphoreGive(mutex_);
            Platform::errorLine("[SDGOODS] audio write failed; output muted");
            vTaskDelay(pdMS_TO_TICKS(16));
        }
    }
}

void Audio::setVolume(uint8_t value) {
    volume_ = std::min<uint8_t>(value, 100);
    if (!volume_) amplifier(false);
    Platform::logf("[SDGOODS] volume=%u; mute PA=%s\n", unsigned(volume()), volume() ? "eligible" : "OFF");
}
void Audio::suspend(bool value) {
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    suspended_ = value;
    if (value) {
        // The audio task owns I2S and closes it on its next block (<=16 ms).
        amplifier(false); mixer_.stopAll(); publishQueued();
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
    publishQueued();
    xSemaphoreGive(mutex_);
    return ok;
}
void Audio::setChannelVolume(uint8_t ch, uint8_t value) {
    if (!mutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.setVolume(static_cast<PcmMixer::Channel>(ch), std::min<uint8_t>(value, 100));
    xSemaphoreGive(mutex_);
}
// Lock-free: counts are republished under mutex_ after every mixer change.
bool Audio::playing() const {
    for (const auto& queued : queued_) if (queued.load()) return true;
    return false;
}
uint8_t Audio::queuedPcm(uint8_t ch) const {
    return ch < CHANNEL_COUNT ? queued_[ch].load() : 0;
}
void Audio::stop() {
    if (!mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.stopAll(); publishQueued(); amplifier(false);
    xSemaphoreGive(mutex_);
}
void Audio::stopChannel(uint8_t ch) {
    if (!mutex_ || ch >= CHANNEL_COUNT) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    mixer_.stop(static_cast<PcmMixer::Channel>(ch));
    publishQueued();
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
