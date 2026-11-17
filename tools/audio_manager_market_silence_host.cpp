#include "core/AudioManager.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include "core/ImaAdpcm.h"
#include <cassert>
#include <deque>
#include <vector>

namespace {
class RecordingAudio final : public Platform::IAudioDevice {
public:
    using Samples = std::vector<uint8_t>;
    void setVolume(uint8_t volume) override { volume_ = volume; }
    uint8_t volume() const override { return volume_; }
    bool playPcmU8(const uint8_t* data, size_t count, uint32_t rate) override {
        return playPcmU8Channel(data, count, rate, 0, true);
    }
    bool playPcmU8Channel(const uint8_t* data, size_t count, uint32_t rate,
                          uint8_t channel, bool replace) override {
        assert(rate == PcmMixer::OUTPUT_SAMPLE_RATE);
        assert(channel < queues.size());
        if (channel == AudioManager::MUSIC_CHANNEL && rejectNextMusic) {
            rejectNextMusic = false;
            return false;
        }
        if (replace) queues[channel].clear();
        if (volume_ == 0 || queues[channel].size() >= 4) return false;
        Samples block(data, data + count);
        queues[channel].push_back(block);
        if (channel == AudioManager::MUSIC_CHANNEL) submitted.push_back(block);
        return true;
    }
    void setChannelVolume(uint8_t, uint8_t volume) override { assert(volume <= 100); }
    bool playing() const override {
        for (const auto& queue : queues) if (!queue.empty()) return true;
        return false;
    }
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

    void consume() {
        auto& queue = queues[AudioManager::MUSIC_CHANNEL];
        assert(!queue.empty());
        delivered.insert(delivered.end(), queue.front().begin(), queue.front().end());
        queue.pop_front();
    }
    std::array<std::deque<Samples>, CHANNEL_COUNT> queues;
    std::vector<Samples> submitted;
    Samples delivered;
    bool rejectNextMusic = false;
private:
    uint8_t volume_ = 50;
};
}  // namespace


namespace {
class FixedPower final : public Platform::IPowerDevice {
public:
    int batteryLevel() override { return 100; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return random; }
    size_t externalMemorySize() const override { return 0; }
    size_t externalMemoryFree() const override { return 0; }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
    uint32_t random = 0;
};

uint32_t le(const std::vector<uint8_t>& bytes, size_t position, size_t count) {
    uint32_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= uint32_t(bytes.at(position + i)) << (i * 8);
    return value;
}

std::vector<uint8_t> readOriginal(const std::string& root, size_t& blocks) {
    std::ifstream input(root + "/packs/dev/audio/bgm_market.smonaudio", std::ios::binary);
    assert(input.good());
    std::vector<uint8_t> packed((std::istreambuf_iterator<char>(input)), {});
    const uint32_t rate = le(packed, 8, 4), count = le(packed, 12, 4);
    const uint32_t bytes = le(packed, 16, 2), samples = le(packed, 18, 2);
    blocks = le(packed, 20, 4);
    std::vector<uint8_t> native(count);
    for (size_t block = 0, produced = 0; block < blocks; ++block) {
        const size_t wanted = std::min<size_t>(samples, count - produced);
        assert(ImaAdpcm::decodeImaBlock(packed.data() + 36 + block * bytes,
            bytes, native.data() + produced, wanted, wanted) == wanted);
        produced += wanted;
    }
    if (rate == PcmMixer::OUTPUT_SAMPLE_RATE) return native;
    assert(rate * 2 == PcmMixer::OUTPUT_SAMPLE_RATE);
    std::vector<uint8_t> expanded(count * 2);
    for (size_t i = 0; i < native.size(); ++i) {
        expanded[2*i] = static_cast<uint8_t>((unsigned(i ? native[i-1] : native[0]) + native[i]) / 2);
        expanded[2*i+1] = native[i];
    }
    return expanded;
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& base = desktop.serviceBundle();
    RecordingAudio device;
    FixedPower power;
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, device, base.imu, power, base.blobs, base.resources, base.memory, base.peers};
    Platform::bind(services);
    auto& manager = AudioManager::ins();
    const auto reset = [&] {
        manager.stopAll(); manager.setMusicSuspended(false); manager.setPowerSave(false);
        desktop.advanceMs(1000); manager.update(); device.stop();
        device.submitted.clear(); device.delivered.clear(); power.random = 0;
    };
    const auto consume = [&] {
        const size_t count = device.queues[AudioManager::MUSIC_CHANNEL].front().size();
        device.consume();
        desktop.advanceMs(static_cast<uint32_t>(count * 1000 / PcmMixer::OUTPUT_SAMPLE_RATE));
        manager.update();
    };
    size_t blockCount = 0;
    const auto original = readOriginal(argv[1], blockCount);
    const auto checkQuiet = [&] {
        assert(manager.requestedMusic() == MusicTrack::MARKET);
        assert(manager.playingMusic() == MusicTrack::NONE);
        assert(device.queuedPcm(AudioManager::MUSIC_CHANNEL) == 0);
    };
    const auto checkRestart = [&] {
        assert(manager.playingMusic() == MusicTrack::MARKET);
        assert(!device.queues[AudioManager::MUSIC_CHANNEL].empty());
        const auto& first = device.queues[AudioManager::MUSIC_CHANNEL].front();
        assert(std::equal(first.begin(), first.end(), original.begin()));
    };

    for (uint32_t random : {0U, 120000U}) {
        reset(); power.random = random;
        manager.setMusic(MusicTrack::MARKET);
        while (device.submitted.size() < blockCount) {
            consume(); assert(device.submitted.size() <= blockCount);
        }
        // Decoding the last block must neither discard it nor append a loop.
        assert(manager.playingMusic() == MusicTrack::MARKET);
        const size_t pending = device.queuedPcm(AudioManager::MUSIC_CHANNEL);
        for (int i = 0; i < 10; ++i) manager.update();
        assert(device.queuedPcm(AudioManager::MUSIC_CHANNEL) == pending);
        while (device.queuedPcm(AudioManager::MUSIC_CHANNEL)) consume();
        checkQuiet();
        assert(device.submitted.size() == blockCount);
        assert(device.delivered.size() == original.size());
        const size_t fade = std::min<size_t>(32000, original.size());
        const size_t start = original.size() - fade;
        assert(std::equal(original.begin(), original.begin() + start, device.delivered.begin()));
        for (size_t i = start; i < original.size(); ++i) {
            const double gain = double(original.size() - 1 - i) / double(fade - 1);
            const double expected = 128 + (int(original[i]) - 128) * gain;
            assert(std::abs(int(device.delivered[i]) - expected) < 1.001);
        }
        assert(device.delivered.back() == 128);
        if (argc == 3 && random == 0) {
            std::ofstream output(argv[2], std::ios::binary);
            output.write(reinterpret_cast<const char*>(device.delivered.data()), device.delivered.size());
            assert(output.good());
        }
        assert(manager.playSfx(SfxCue::BUY_ITEM));
        assert(device.queuedPcm(AudioManager::SFX_CHANNEL) > 0);
        checkQuiet();
        manager.setMusic(MusicTrack::HOME);
        assert(manager.playingMusic() == MusicTrack::HOME);
        manager.setMusic(MusicTrack::MARKET); checkQuiet();
        // Repeated page requests, mute, and lock must preserve this same timer.
        for (int i = 0; i < 10; ++i) manager.setMusic(MusicTrack::MARKET);
        manager.setMusicSuspended(true); manager.update();
        manager.setMusicSuspended(false); manager.update();
        device.setVolume(0); manager.update(); device.setVolume(50); manager.update();
        checkQuiet();
        const uint32_t duration = 120000 + random;
        desktop.advanceMs(duration - 1); manager.update(); checkQuiet();
        desktop.advanceMs(1); manager.update(); checkRestart();
    }
    // Interrupting a performance also prevents immediate re-entry/restart.
    for (int interruption = 0; interruption < 4; ++interruption) {
        reset(); manager.setMusic(MusicTrack::MARKET); consume();
        if (interruption == 0) {
            manager.setMusic(MusicTrack::HOME); manager.setMusic(MusicTrack::MARKET);
        } else if (interruption == 1) {
            manager.stopMusic(); manager.setMusic(MusicTrack::MARKET);
        } else if (interruption == 2) {
            manager.setMusicSuspended(true); manager.update();
            desktop.advanceMs(5000); manager.update();
            manager.setMusicSuspended(false); manager.update();
        } else {
            device.setVolume(0); manager.update(); device.setVolume(50); manager.update();
        }
        checkQuiet();
        desktop.advanceMs(120000 - (interruption == 2 ? 5000 : 0) - 1);
        manager.update(); checkQuiet();
        desktop.advanceMs(1); manager.update(); checkRestart();
    }
    // A deadline of zero and a quiet interval crossing wrap must both work.
    for (uint32_t start : {UINT32_MAX - 119999U, UINT32_MAX - 1000U}) {
        reset(); desktop.advanceMs(start - desktop.millis());
        manager.setMusic(MusicTrack::MARKET); manager.stopMusic();
        manager.setMusic(MusicTrack::MARKET); checkQuiet();
        desktop.advanceMs(119999); manager.update(); checkQuiet();
        desktop.advanceMs(1); manager.update(); checkRestart();
    }
    manager.stopAll();
    std::printf("market: %zu samples drained; 2s PCM fade; 120s/240s silence; SFX, re-entry, lock, mute and clock wrap verified\n", original.size());
    return 0;
}
