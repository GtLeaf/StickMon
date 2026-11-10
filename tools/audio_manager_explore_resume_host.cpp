#include "core/AudioManager.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <array>
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
    void setChannelVolume(uint8_t, uint8_t) override {}
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
        queue.pop_front();
    }
    std::array<std::deque<Samples>, CHANNEL_COUNT> queues;
    std::vector<Samples> submitted;
    bool rejectNextMusic = false;
private:
    uint8_t volume_ = 50;
};
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& base = desktop.serviceBundle();
    RecordingAudio device;
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, device, base.imu, base.power, base.blobs, base.resources,
        base.memory, base.peers};
    Platform::bind(services);
    auto& manager = AudioManager::ins();
    manager.stopAll();

    // Record uninterrupted decoded output across several loops. Resumed PCM
    // must match this reference, including short tails and unaligned loops.
    manager.setMusic(MusicTrack::EXPLORE);
    for (size_t i = 0; i < 1500; ++i) {
        device.consume();
        manager.update();
    }
    const auto reference = device.submitted;
    manager.stopAll();
    assert(reference.size() > 1500);

    for (MusicTrack battle : {MusicTrack::BATTLE, MusicTrack::BATTLE_SPECIAL}) {
        for (size_t consumed : {size_t(9), size_t(147), size_t(511), size_t(1300)}) {
            for (uint8_t pending = 0; pending <= STICKMON_MUSIC_QUEUE_TARGET_BLOCKS; ++pending) {
                device.submitted.clear();
                manager.setMusic(MusicTrack::EXPLORE);
                for (size_t i = 0; i < consumed; ++i) {
                    device.consume();
                    manager.update();
                }
                while (device.queuedPcm(AudioManager::MUSIC_CHANNEL) < pending) manager.update();
                while (device.queuedPcm(AudioManager::MUSIC_CHANNEL) > pending) device.consume();
                const size_t resumeIndex = device.submitted.size() - pending;
                manager.setMusic(battle);
                assert(manager.playingMusic() == battle);
                for (size_t i = 0; i < 7; ++i) {
                    device.consume();
                    manager.update();
                }
                // Lock/unlock, mute/unmute and battle context changes must
                // not overwrite the exploration bookmark with battle PCM.
                manager.setMusicSuspended(true);
                manager.setMusic(battle);
                manager.update();
                manager.setMusicSuspended(false);
                manager.update();
                device.setVolume(0);
                manager.update();
                device.setVolume(50);
                manager.update();
                manager.setMusic(MusicTrack::BATTLE_SPECIAL);
                device.submitted.clear();
                manager.setMusic(MusicTrack::EXPLORE);
                assert(manager.playingMusic() == MusicTrack::EXPLORE);
                for (size_t i = 0; i < 8; ++i) {
                    device.consume();
                    manager.update();
                }
                for (size_t i = 0; i < device.submitted.size(); ++i) {
                    assert(device.submitted[i] == reference.at(resumeIndex + i));
                }
                // Another immediate encounter resumes the same audible block.
                const auto next = device.queues[AudioManager::MUSIC_CHANNEL].front();
                manager.setMusic(MusicTrack::BATTLE);
                device.submitted.clear();
                manager.setMusic(MusicTrack::EXPLORE);
                assert(device.submitted.front() == next);
                manager.stopAll();
            }
        }
    }

    // Implicit release while exploring must keep a bookmark for the encounter.
    for (bool suspend : {false, true}) {
        manager.setMusic(MusicTrack::EXPLORE);
        for (size_t i = 0; i < 19; ++i) { device.consume(); manager.update(); }
        const auto next = device.queues[AudioManager::MUSIC_CHANNEL].front();
        if (suspend) manager.setMusicSuspended(true);
        else { device.setVolume(0); manager.update(); }
        manager.setMusic(MusicTrack::BATTLE);
        manager.setMusic(MusicTrack::EXPLORE);
        manager.update();
        assert(manager.playingMusic() == MusicTrack::NONE);
        device.submitted.clear();
        if (suspend) manager.setMusicSuspended(false);
        else device.setVolume(50);
        manager.update();
        assert(device.submitted.front() == next);
        manager.stopAll();
    }

    // Leaving exploration or explicitly stopping discards the old bookmark.
    for (int exit = 0; exit < 5; ++exit) {
        manager.setMusic(MusicTrack::EXPLORE);
        for (size_t i = 0; i < 9; ++i) { device.consume(); manager.update(); }
        manager.setMusic(MusicTrack::BATTLE);
        switch (exit) {
        case 0: manager.setMusic(MusicTrack::HOME); break;
        case 1: manager.setMusic(MusicTrack::MARKET); break;
        case 2: manager.setMusic(MusicTrack::NONE); break;
        case 3: manager.stopMusic(); break;
        case 4: manager.stopAll(); break;
        }
        manager.setMusic(MusicTrack::BATTLE);
        device.submitted.clear();
        manager.setMusic(MusicTrack::EXPLORE);
        assert(device.submitted[0] == reference[0]);
        assert(device.submitted[1] == reference[1]);
        manager.stopAll();
    }

    // A failed initial submission must not consume the saved route position.
    manager.setMusic(MusicTrack::EXPLORE);
    for (size_t i = 0; i < 17; ++i) { device.consume(); manager.update(); }
    const auto retryBlock = device.queues[AudioManager::MUSIC_CHANNEL].front();
    manager.setMusic(MusicTrack::BATTLE);
    device.rejectNextMusic = true;
    manager.setMusic(MusicTrack::EXPLORE);
    assert(manager.playingMusic() == MusicTrack::NONE);
    device.submitted.clear();
    manager.update();
    assert(device.submitted.front() == retryBlock);
    const size_t submitted = device.submitted.size();
    manager.setMusic(MusicTrack::EXPLORE);
    assert(device.submitted.size() == submitted);
    manager.stopAll();

    // A route requested during the power-save silence resumes after waking.
    manager.setMusic(MusicTrack::EXPLORE);
    for (size_t i = 0; i < 17; ++i) { device.consume(); manager.update(); }
    const auto wakeBlock = device.queues[AudioManager::MUSIC_CHANNEL].front();
    manager.setMusic(MusicTrack::BATTLE);
    manager.setPowerSave(true);
    desktop.advanceMs(1001);
    manager.update();
    manager.setMusic(MusicTrack::EXPLORE);
    assert(manager.playingMusic() == MusicTrack::NONE);
    manager.setPowerSave(false);
    desktop.advanceMs(1001);
    device.submitted.clear();
    manager.update();
    assert(device.submitted.front() == wakeBlock);
    manager.stopAll();
    Platform::bind(base);
    return 0;
}
