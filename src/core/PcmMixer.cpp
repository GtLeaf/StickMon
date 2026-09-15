#include "core/PcmMixer.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr int32_t PCM_MIN = -32768;
constexpr int32_t PCM_MAX = 32767;
}

PcmMixer::PcmMixer(Platform::IMemoryAllocator& allocator)
    : allocator_(allocator) {}

PcmMixer::~PcmMixer() {
    stopAll();
}

bool PcmMixer::validChannel(Channel channel) {
    return channelIndex(channel) < CHANNEL_COUNT;
}

int32_t PcmMixer::clampSample(int32_t sample) {
    return std::max(PCM_MIN, std::min(PCM_MAX, sample));
}

void PcmMixer::releaseFront(ChannelState& state) {
    if (state.count == 0) return;
    Block& block = state.blocks[state.head];
    if (block.samples) allocator_.release(block.samples);
    block = Block{};
    state.head = static_cast<uint8_t>(
        (state.head + 1U) % BLOCK_QUEUE_CAPACITY);
    --state.count;
    if (state.finishedBlocks < 255) ++state.finishedBlocks;
}

void PcmMixer::clearChannel(ChannelState& state) {
    while (state.count > 0) releaseFront(state);
    state.head = 0;
}

bool PcmMixer::start(Channel channel, const Voice& voice, StartMode mode) {
    if (!validChannel(channel) || !voice.valid() ||
        voice.sampleRate != OUTPUT_SAMPLE_RATE) {
        return false;
    }
    ChannelState& state = channels_[channelIndex(channel)];
    if (mode == StartMode::REPLACE) clearChannel(state);
    if (state.count >= BLOCK_QUEUE_CAPACITY) return false;

    uint8_t* samples = static_cast<uint8_t*>(
        allocator_.allocate(voice.sampleCount, true));
    if (!samples) return false;
    std::memcpy(samples, voice.samples, voice.sampleCount);

    uint8_t slot = static_cast<uint8_t>(
        (state.head + state.count) % BLOCK_QUEUE_CAPACITY);
    state.blocks[slot] = Block{
        samples, voice.sampleCount, 0, voice.volume, voice.loop};
    ++state.count;
    return true;
}

void PcmMixer::stop(Channel channel) {
    if (!validChannel(channel)) return;
    clearChannel(channels_[channelIndex(channel)]);
}

void PcmMixer::stopAll() {
    for (ChannelState& state : channels_) clearChannel(state);
}

void PcmMixer::setVolume(Channel channel, uint8_t percent) {
    if (!validChannel(channel)) return;
    channels_[channelIndex(channel)].volume = std::min<uint8_t>(percent, 100);
}

bool PcmMixer::active(Channel channel) const {
    return validChannel(channel) &&
           channels_[channelIndex(channel)].count > 0;
}

bool PcmMixer::anyActive() const {
    for (const ChannelState& state : channels_) {
        if (state.count > 0) return true;
    }
    return false;
}

size_t PcmMixer::mix(int16_t* output, size_t sampleCount) {
    if (!output || sampleCount == 0) return 0;
    for (size_t sample = 0; sample < sampleCount; ++sample) {
        int32_t mixed = 0;
        for (ChannelState& state : channels_) {
            while (state.count > 0) {
                Block& block = state.blocks[state.head];
                if (block.position < block.sampleCount) break;
                if (block.loop) {
                    block.position = 0;
                    break;
                }
                releaseFront(state);
            }
            if (state.count == 0) continue;

            Block& block = state.blocks[state.head];
            int32_t pcm =
                (static_cast<int32_t>(block.samples[block.position]) - 128) << 8;
            uint32_t gain = static_cast<uint32_t>(block.volume) * state.volume;
            mixed += static_cast<int32_t>(
                (static_cast<int64_t>(pcm) * gain) / 10000);
            ++block.position;
            if (block.position >= block.sampleCount && !block.loop) {
                releaseFront(state);
            }
        }
        output[sample] = static_cast<int16_t>(clampSample(mixed));
    }
    return sampleCount;
}

uint8_t PcmMixer::takeFinishedBlocks(Channel channel) {
    if (!validChannel(channel)) return 0;
    ChannelState& state = channels_[channelIndex(channel)];
    uint8_t finished = state.finishedBlocks;
    state.finishedBlocks = 0;
    return finished;
}

uint8_t PcmMixer::queuedBlocks(Channel channel) const {
    return validChannel(channel) ? channels_[channelIndex(channel)].count : 0;
}
