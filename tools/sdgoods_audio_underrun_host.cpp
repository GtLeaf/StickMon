// SPDX-License-Identifier: Apache-2.0
// Host model of SDGOODS music streaming: the real AudioManager and PcmMixer
// feed a simulated I2S DMA ring while the UI loop calls update() every 66 ms
// and periodically stalls. Prints the longest stall that plays without a gap.
// Build with -DSIM_DMA_DESC=<n> and -DSTICKMON_MUSIC_QUEUE_TARGET_BLOCKS=<n>.
#include "core/AudioManager.h"
#include "core/PcmMixer.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <algorithm>
#include <cassert>
#include <cstdio>

#ifndef SIM_DMA_DESC
#define SIM_DMA_DESC 6
#endif

namespace {
constexpr uint32_t DMA_SAMPLES = SIM_DMA_DESC * 240;
constexpr uint32_t SAMPLES_PER_MS = PcmMixer::OUTPUT_SAMPLE_RATE / 1000;

class SimAudio final : public Platform::IAudioDevice {
public:
    explicit SimAudio(Platform::IMemoryAllocator& memory) : mixer_(memory) {}
    void setVolume(uint8_t v) override { volume_ = v; }
    uint8_t volume() const override { return volume_; }
    bool playPcmU8(const uint8_t* d, size_t n, uint32_t r) override { return playPcmU8Channel(d, n, r, 0, false); }
    bool playPcmU8Channel(const uint8_t* d, size_t n, uint32_t r, uint8_t ch, bool replace) override {
        return mixer_.start(static_cast<PcmMixer::Channel>(ch), {d, n, r, 100, false},
            replace ? PcmMixer::StartMode::REPLACE : PcmMixer::StartMode::APPEND);
    }
    void setChannelVolume(uint8_t ch, uint8_t v) override { mixer_.setVolume(static_cast<PcmMixer::Channel>(ch), v); }
    bool playing() const override { return mixer_.anyActive(); }
    uint8_t queuedPcm(uint8_t ch) const override { return mixer_.queuedBlocks(static_cast<PcmMixer::Channel>(ch)); }
    void stop() override { mixer_.stopAll(); }
    void stopChannel(uint8_t ch) override { mixer_.stop(static_cast<PcmMixer::Channel>(ch)); }
    bool beginMicrophone() override { return false; }
    void endMicrophone() override {}
    bool recordMicrophone(int16_t*, size_t, uint32_t) override { return false; }
    bool microphoneRecording() const override { return false; }
    bool microphoneActive() const override { return false; }

    // One millisecond of playback. The audio task keeps the DMA ring topped up
    // with whole mixer blocks; the speaker drains it at 16 samples per ms.
    void tick() {
        while (dma_ + PcmMixer::MIX_BLOCK_SAMPLES <= DMA_SAMPLES && mixer_.anyActive()) {
            int16_t block[PcmMixer::MIX_BLOCK_SAMPLES];
            mixer_.mix(block, PcmMixer::MIX_BLOCK_SAMPLES);
            dma_ += PcmMixer::MIX_BLOCK_SAMPLES;
        }
        if (dma_ >= SAMPLES_PER_MS) dma_ -= SAMPLES_PER_MS;
        else { dma_ = 0; ++gapMs_; }
    }
    uint32_t gapMs_ = 0;
private:
    PcmMixer mixer_;
    uint8_t volume_ = 50;
    uint32_t dma_ = 0;
};

uint32_t gapFor(DesktopPlatform& desktop, Platform::Services& base, uint32_t stallMs) {
    SimAudio audio(base.memory);
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, audio, base.imu, base.power, base.blobs, base.resources,
        base.memory, base.peers};
    Platform::bind(services);
    AudioManager& manager = AudioManager::ins();
    manager.stopAll();
    manager.setMusic(MusicTrack::EXPLORE);
    assert(manager.playingMusic() == MusicTrack::EXPLORE);
    // Warm up so the queue reaches its steady state before measuring.
    uint32_t nextUpdate = 0;
    for (uint32_t ms = 0; ms < 2000; ++ms) {
        if (ms >= nextUpdate) { manager.update(); nextUpdate = ms + 66; }
        audio.tick(); desktop.advanceMs(1);
    }
    audio.gapMs_ = 0;
    // 30 s: a stall of stallMs every 2 s, at varying phase to the 66 ms tick.
    nextUpdate = 0;
    for (uint32_t ms = 0; ms < 30000; ++ms) {
        if (ms >= nextUpdate) {
            manager.update();
            const bool stall = (ms / 2000) != ((ms + 66) / 2000);
            nextUpdate = ms + 66 + (stall ? stallMs + (ms / 2000) * 7 % 61 : 0);
        }
        audio.tick(); desktop.advanceMs(1);
    }
    manager.stopAll();
    Platform::bind(base);
    return audio.gapMs_;
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    Platform::Services& base = desktop.serviceBundle();
    assert(gapFor(desktop, base, 0) == 0);
    uint32_t best = 0;
    for (uint32_t stall = 0; stall <= 800; stall += 10) {
        if (gapFor(desktop, base, stall) != 0) break;
        best = stall;
    }
    std::printf("dma_desc=%u queue_target=%u max_gapless_stall_ms=%u\n",
        unsigned(SIM_DMA_DESC), unsigned(STICKMON_MUSIC_QUEUE_TARGET_BLOCKS), unsigned(best));
    return 0;
}
