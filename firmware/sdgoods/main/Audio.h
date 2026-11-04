// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include "core/PcmMixer.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Sdgoods {
class Audio final : public Platform::IAudioDevice {
public:
    explicit Audio(Platform::IMemoryAllocator& allocator) : mixer_(allocator) {}
    bool begin();
    void suspend(bool value);
    void setVolume(uint8_t value) override;
    uint8_t volume() const override { return volume_.load(); }
    bool playPcmU8(const uint8_t* data, size_t count, uint32_t rate) override;
    bool playPcmU8Channel(const uint8_t*, size_t, uint32_t, uint8_t, bool) override;
    void setChannelVolume(uint8_t, uint8_t) override;
    bool playing() const override;
    uint8_t queuedPcm(uint8_t) const override;
    void stop() override;
    void stopChannel(uint8_t) override;
    bool beginMicrophone() override;
    void endMicrophone() override;
    bool recordMicrophone(int16_t*, size_t, uint32_t) override;
    bool microphoneRecording() const override { return recording_; }
    bool microphoneActive() const override { return microphoneOn_; }
private:
    void run();
    void amplifier(bool enabled);
    PcmMixer mixer_;
    SemaphoreHandle_t mutex_ = nullptr, micMutex_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr, rx_ = nullptr;
    std::atomic<uint8_t> volume_{0};
    std::atomic<bool> recording_{false}, microphoneOn_{false};
    bool suspended_ = false, txEnabled_ = false;
    uint32_t microphoneRate_ = 16000;
};
}  // namespace Sdgoods
