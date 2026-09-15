#pragma once

#include <cstddef>
#include <cstdint>

#include "platform/api/PlatformServices.h"

class PcmMixer {
public:
    enum class Channel : uint8_t {
        MUSIC = 0,
        SFX,
        CRY,
        COUNT,
    };

    enum class StartMode : uint8_t {
        APPEND = 0,
        REPLACE,
    };

    struct Voice {
        const uint8_t* samples = nullptr;
        size_t sampleCount = 0;
        uint32_t sampleRate = 0;
        uint8_t volume = 100;
        bool loop = false;

        bool valid() const {
            return samples && sampleCount > 0 && sampleRate > 0 &&
                   volume <= 100;
        }
    };

    static constexpr uint32_t OUTPUT_SAMPLE_RATE = 16000;
    static constexpr size_t MIX_BLOCK_SAMPLES = 256;
    static constexpr uint8_t CHANNEL_COUNT =
        static_cast<uint8_t>(Channel::COUNT);

    explicit PcmMixer(Platform::IMemoryAllocator& allocator);
    ~PcmMixer();

    PcmMixer(const PcmMixer&) = delete;
    PcmMixer& operator=(const PcmMixer&) = delete;

    // start() copies the source before returning. Callers may release
    // temporary buffers after the call completes.
    bool start(Channel channel, const Voice& voice, StartMode mode);
    void stop(Channel channel);
    void stopAll();
    void setVolume(Channel channel, uint8_t percent);
    bool active(Channel channel) const;
    bool anyActive() const;
    size_t mix(int16_t* output, size_t sampleCount);

    // The platform uses this to retire submitted-block counters without
    // exposing the mixer's internal queue to the UI task.
    uint8_t takeFinishedBlocks(Channel channel);
    uint8_t queuedBlocks(Channel channel) const;

    static constexpr uint8_t channelIndex(Channel channel) {
        return static_cast<uint8_t>(channel);
    }

private:
    static constexpr uint8_t BLOCK_QUEUE_CAPACITY = 4;

    struct Block {
        uint8_t* samples = nullptr;
        size_t sampleCount = 0;
        size_t position = 0;
        uint8_t volume = 100;
        bool loop = false;
    };

    struct ChannelState {
        Block blocks[BLOCK_QUEUE_CAPACITY] = {};
        uint8_t head = 0;
        uint8_t count = 0;
        uint8_t volume = 100;
        uint8_t finishedBlocks = 0;
    };

    static bool validChannel(Channel channel);
    static int32_t clampSample(int32_t sample);
    void releaseFront(ChannelState& state);
    void clearChannel(ChannelState& state);

    Platform::IMemoryAllocator& allocator_;
    ChannelState channels_[CHANNEL_COUNT] = {};
};
