#include <cassert>
#include <cstdlib>
#include <cstdint>

#include "core/PcmMixer.h"

namespace {
class HostAllocator final : public Platform::IMemoryAllocator {
public:
    void* allocate(size_t bytes, bool) override {
        void* memory = std::malloc(bytes);
        if (memory) ++activeAllocations;
        return memory;
    }

    void release(void* memory) override {
        if (!memory) return;
        assert(activeAllocations > 0);
        --activeAllocations;
        std::free(memory);
    }

    size_t externalFree() const override { return 0; }

    size_t activeAllocations = 0;
};
}  // namespace

int main() {
    static_assert(PcmMixer::CHANNEL_COUNT == 3, "expected three audio channels");
    static_assert(PcmMixer::OUTPUT_SAMPLE_RATE == 16000,
                  "AMOLED mixer output must stay at 16 kHz");
    static_assert(PcmMixer::MIX_BLOCK_SAMPLES == 256,
                  "mixer block must match the codec write block");

    HostAllocator allocator;
    PcmMixer mixer(allocator);
    assert(!PcmMixer::Voice{}.valid());

    const uint8_t music[] = {160, 96};
    const uint8_t sfx[] = {144, 112};
    assert(mixer.start(PcmMixer::Channel::MUSIC,
                       {music, 2, 16000, 100, false},
                       PcmMixer::StartMode::APPEND));
    assert(mixer.start(PcmMixer::Channel::SFX,
                       {sfx, 2, 16000, 100, false},
                       PcmMixer::StartMode::APPEND));
    int16_t output[2] = {};
    assert(mixer.mix(output, 2) == 2);
    assert(output[0] == 12288);
    assert(output[1] == -12288);
    assert(mixer.takeFinishedBlocks(PcmMixer::Channel::MUSIC) == 1);
    assert(mixer.takeFinishedBlocks(PcmMixer::Channel::SFX) == 1);
    assert(allocator.activeAllocations == 0);

    const uint8_t loud[] = {255};
    assert(mixer.start(PcmMixer::Channel::MUSIC,
                       {loud, 1, 16000, 100, false},
                       PcmMixer::StartMode::APPEND));
    assert(mixer.start(PcmMixer::Channel::CRY,
                       {loud, 1, 16000, 100, false},
                       PcmMixer::StartMode::APPEND));
    assert(mixer.mix(output, 1) == 1);
    assert(output[0] == 32767);
    assert(mixer.takeFinishedBlocks(PcmMixer::Channel::MUSIC) == 1);
    assert(mixer.takeFinishedBlocks(PcmMixer::Channel::CRY) == 1);

    const uint8_t oldMusic[] = {140, 140};
    const uint8_t newMusic[] = {130};
    assert(mixer.start(PcmMixer::Channel::MUSIC,
                       {oldMusic, 2, 16000, 100, false},
                       PcmMixer::StartMode::APPEND));
    assert(mixer.start(PcmMixer::Channel::MUSIC,
                       {newMusic, 1, 16000, 100, false},
                       PcmMixer::StartMode::REPLACE));
    assert(mixer.queuedBlocks(PcmMixer::Channel::MUSIC) == 1);
    assert(mixer.mix(output, 1) == 1);
    assert(output[0] == 512);
    assert(mixer.takeFinishedBlocks(PcmMixer::Channel::MUSIC) == 2);
    assert(allocator.activeAllocations == 0);

    assert(!mixer.start(PcmMixer::Channel::SFX,
                        {sfx, 2, 8000, 100, false},
                        PcmMixer::StartMode::APPEND));
    return 0;
}
