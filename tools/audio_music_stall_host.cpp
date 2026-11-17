#include "AudioUnderrunHold.h"
#include "core/AudioManager.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>

namespace {
// Plays queued music in real time, so a late refill shows up as underrun.
class TimedAudio final : public Platform::IAudioDevice {
public:
    void setVolume(uint8_t volume) override { volume_ = volume; }
    uint8_t volume() const override { return volume_; }
    bool playPcmU8(const uint8_t* data, size_t count, uint32_t rate) override {
        return playPcmU8Channel(data, count, rate, 0, true);
    }
    bool playPcmU8Channel(const uint8_t*, size_t count, uint32_t rate,
                          uint8_t channel, bool replace) override {
        assert(rate == PcmMixer::OUTPUT_SAMPLE_RATE && channel < queues.size());
        if (replace) queues[channel].clear();
        if (volume_ == 0 || queues[channel].size() >= 4) return false;
        queues[channel].push_back(count);
        return true;
    }
    void setChannelVolume(uint8_t, uint8_t) override {}
    bool playing() const override { return !queues[AudioManager::MUSIC_CHANNEL].empty(); }
    uint8_t queuedPcm(uint8_t channel) const override {
        return static_cast<uint8_t>(queues.at(channel).size());
    }
    void stop() override { for (auto& queue : queues) queue.clear(); }
    void stopChannel(uint8_t channel) override { queues.at(channel).clear(); }
    bool beginMicrophone() override { return false; }
    void endMicrophone() override {}
    bool recordMicrophone(int16_t*, size_t, uint32_t) override { return false; }
    bool microphoneRecording() const override { return false; }
    bool microphoneActive() const override { return false; }

    void play(uint32_t ms) {
        size_t samples = size_t(ms) * PcmMixer::OUTPUT_SAMPLE_RATE / 1000;
        auto& queue = queues[AudioManager::MUSIC_CHANNEL];
        while (samples) {
            if (queue.empty()) { underrunSamples += samples; return; }
            const size_t used = std::min(samples, queue.front());
            queue.front() -= used; samples -= used;
            if (!queue.front()) queue.pop_front();
        }
    }
    std::array<std::deque<size_t>, CHANNEL_COUNT> queues;
    size_t underrunSamples = 0;
private:
    uint8_t volume_ = 50;
};
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 4);
    const uint32_t stallMs = uint32_t(std::atoi(argv[2]));
    const bool expectUnderrun = std::atoi(argv[3]) != 0;

    // A drained mixer holds the output for 19 silent blocks, then releases it;
    // an intentional stop releases at once.
    Stickmon::AudioUnderrunHold hold;
    assert(!hold.holdNext());
    hold.played();
    for (int i = 0; i < Stickmon::AudioUnderrunHold::HOLD_BLOCKS; ++i) assert(hold.holdNext());
    assert(!hold.holdNext());
    hold.played(); assert(hold.holdNext()); hold.cancel(); assert(!hold.holdNext());

    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& base = desktop.serviceBundle();
    TimedAudio device;
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, device, base.imu, base.power, base.blobs, base.resources,
        base.memory, base.peers};
    Platform::bind(services);
    auto& manager = AudioManager::ins();
    manager.stopAll();
    manager.setMusic(MusicTrack::MARKET);
    for (int i = 0; i < 8; ++i) manager.update();
    assert(device.playing());

    // 40 ms stall-panel frames with a long frame every second. Stay inside
    // the first play-through: the track ends in a deliberate silence.
    for (int frame = 0; frame < 600; ++frame) {
        device.play(frame % 25 == 24 ? stallMs : 40);
        manager.update();
    }
    std::printf("stall=%lums target=%d refill=%d underrun=%zu samples\n",
                static_cast<unsigned long>(stallMs), STICKMON_MUSIC_QUEUE_TARGET_BLOCKS,
                STICKMON_MUSIC_REFILL_BLOCKS, device.underrunSamples);
    assert((device.underrunSamples > 0) == expectUnderrun);
    return 0;
}
