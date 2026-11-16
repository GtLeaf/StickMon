#include "assets/MenuAssets.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
class CountingFile final : public Platform::IResourceFile {
public:
    CountingFile(Platform::ResourceFile file, size_t& calls) : file_(std::move(file)), calls_(calls) {}
    bool valid() const override { ++calls_; return bool(file_); }
    size_t size() const override { ++calls_; return file_.size(); }
    size_t position() const override { ++calls_; return file_.position(); }
    size_t read(void* out, size_t n) override { ++calls_; return file_.read(static_cast<uint8_t*>(out), n); }
    bool seek(size_t pos) override { ++calls_; return file_.seek(pos); }
    void close() override { ++calls_; file_.close(); }
private:
    Platform::ResourceFile file_;
    size_t& calls_;
};
class CountingStore final : public Platform::IResourceStore {
public:
    explicit CountingStore(Platform::IResourceStore& store) : store_(store) {}
    bool mount() override { ++calls; return store_.mount(); }
    size_t totalBytes() const override { ++calls; return store_.totalBytes(); }
    size_t usedBytes() const override { ++calls; return store_.usedBytes(); }
    Platform::ResourceFile open(const char* path) override {
        ++calls;
        auto file = store_.open(path);
        return file ? Platform::ResourceFile(new CountingFile(std::move(file), calls)) : Platform::ResourceFile{};
    }
    mutable size_t calls = 0;
private:
    Platform::IResourceStore& store_;
};
class CountingMemory final : public Platform::IMemoryAllocator {
public:
    CountingMemory(Platform::IMemoryAllocator& memory, bool fail) : memory_(memory), fail_(fail) {}
    void* allocate(size_t bytes, bool external) override {
        assert(external);
        ++allocations;
        if (fail_) return nullptr;
        void* pointer = memory_.allocate(bytes, external);
        if (pointer) ++live;
        return pointer;
    }
    void release(void* pointer) override { if (pointer) --live; memory_.release(pointer); }
    size_t externalFree() const override { return memory_.externalFree(); }
    size_t allocations = 0;
    size_t live = 0;
private:
    Platform::IMemoryAllocator& memory_;
    bool fail_;
};
uint16_t word(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
std::vector<int> pixels(const uint16_t* words, const MenuAssets::RleFrame& frame) {
    std::vector<int> output;
    size_t pos = frame.offset;
    const size_t end = pos + frame.length;
    while (pos < end) {
        uint16_t control = words[pos++];
        size_t run = control & 0x7fff;
        assert(run && output.size() + run <= 1600);
        if (control & 0x8000) output.insert(output.end(), run, -1);
        else { assert(pos + run <= end); while (run--) output.push_back(words[pos++]); }
    }
    assert(output.size() == 1600);
    return output;
}
}
int main(int argc, char** argv) {
    assert(argc == 5);
    DesktopPlatform platform(argv[1]);
    auto& original = platform.serviceBundle();
    CountingStore store(original.resources);
    CountingMemory memory(original.memory, std::string(argv[4]) == "1");
    Platform::Services services{original.lifecycle, original.clock, original.logger,
        original.input, original.display, original.audio, original.imu, original.power,
        original.blobs, store, memory, original.peers};
    Platform::bind(services);
    assert(platform.begin());
    const bool wanted = std::string(argv[3]) == "1";
#if STICKMON_EXTERNAL_MENU_ICONS
    assert(!MenuAssets::mainIconFrames() && !MenuAssets::mainIconRle());
#endif
    assert(store.calls == 0 && memory.allocations == 0);
    assert(MenuAssets::begin() == wanted);
    const auto* frames = MenuAssets::mainIconFrames();
    const auto* rle = MenuAssets::mainIconRle();
    if (!wanted) {
        assert(!frames && !rle && memory.live == 0);
        const size_t calls = store.calls;
        for (int i = 0; i < 100; ++i) assert(!MenuAssets::mainIconFrames() && !MenuAssets::mainIconRle());
        assert(store.calls == calls);
        return 0;
    }
    assert(frames && rle);
#if STICKMON_EXTERNAL_MENU_ICONS
    assert(memory.allocations == 1 && memory.live == 1);
#else
    assert(store.calls == 0 && memory.allocations == 0);
#endif
    std::ifstream input(argv[2], std::ios::binary);
    std::vector<uint8_t> expected{std::istreambuf_iterator<char>(input), {}};
    assert(expected.size() > 36);
    std::vector<uint16_t> expectedWords;
    for (size_t i = 36; i < expected.size(); i += 2) expectedWords.push_back(word(expected.data()+i));
    size_t calls = store.calls;
    for (int loop = 0; loop < 100; ++loop) {
        assert(MenuAssets::begin());
        assert(MenuAssets::mainIconFrames() == frames && MenuAssets::mainIconRle() == rle);
        for (int i = 0; i < 9; ++i) {
            MenuAssets::RleFrame expectedFrame{word(expected.data()+i*4), word(expected.data()+i*4+2)};
            assert(frames[i].offset == expectedFrame.offset && frames[i].length == expectedFrame.length);
            assert(std::memcmp(rle+frames[i].offset, expectedWords.data()+expectedFrame.offset, frames[i].length*2) == 0);
            assert(pixels(rle, frames[i]) == pixels(expectedWords.data(), expectedFrame));
        }
    }
    assert(store.calls == calls);
}
