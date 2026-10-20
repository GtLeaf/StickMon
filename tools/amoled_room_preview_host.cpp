#include "AmoledApp.h"
#include "core/RoomResource.h"
#include "core/RoomRenderer.h"
#include "core/UiStrings.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>

class TrackingMemory final : public Platform::IMemoryAllocator {
public:
    explicit TrackingMemory(Platform::IMemoryAllocator& base) : base_(base) {}
    void* allocate(size_t bytes, bool external) override {
        ++allocations;
        if (failNext) { failNext = false; return nullptr; }
        void* p = base_.allocate(bytes, external);
        if (p) { sizes[p] = bytes; live += bytes; peak = std::max(peak, live); }
        return p;
    }
    void release(void* p) override {
        if (p) { const auto it = sizes.find(p); assert(it != sizes.end()); live -= it->second; sizes.erase(it); }
        base_.release(p);
    }
    size_t externalFree() const override { return 8 * 1024 * 1024 - live; }
    bool failNext = false;
    size_t live = 0, peak = 0, allocations = 0;
private:
    Platform::IMemoryAllocator& base_;
    std::unordered_map<void*, size_t> sizes;
};
class TrackingResources final : public Platform::IResourceStore {
public:
    explicit TrackingResources(Platform::IResourceStore& base) : base_(base) {}
    bool mount() override { return base_.mount(); }
    size_t totalBytes() const override { return base_.totalBytes(); }
    size_t usedBytes() const override { return base_.usedBytes(); }
    Platform::ResourceFile open(const char* path) override { ++opens; return base_.open(path); }
    size_t opens = 0;
private:
    Platform::IResourceStore& base_;
};

namespace AmoledV1 {
struct AmoledRoomPreviewTestAccess {
    static void run(DesktopPlatform& desktop, Canvas565& canvas, const char* output, TrackingMemory& memory, TrackingResources& resources) {
        AmoledApp app;
        app.begin(desktop.millis());
        auto& room = RoomResource::ins();
        assert(room.begin());
        assert(room.layered() && room.furniture().count() == 6);
        const std::string originalBed = room.furniture().selected("bed");
        const std::string originalRug = room.furniture().selected("rug");
        auto exportFrame = [&](const std::string& name, bool night) {
            auto* old = canvas.rawPixels();
            std::vector<uint16_t> full(room.artWidth() * room.artHeight());
            canvas.attach({full.data(), room.artWidth(), room.artHeight(), false});
            assert(RoomRenderer::drawViewport(0, 0, room.artWidth(), room.artHeight(), 0, 0, night, room.artScale()));
            const std::string path = std::string(output) + "/" + name + (night ? "-night.rgb565" : "-day.rgb565");
            FILE* file = std::fopen(path.c_str(), "wb"); assert(file);
            assert(std::fwrite(full.data(), 2, full.size(), file) == full.size()); std::fclose(file);
            canvas.attach({old, 368, 448, false});
            return full;
        };
        const auto original = exportFrame("original", false);
        exportFrame("original", true);
        const int foodX = room.foodX(), bedX = room.bedX(), doorX = room.doorwayInsideX();
        auto snapshot = [&]() {
            std::vector<uint8_t> result;
            for (const char* key : {"state", "state_a", "state_b", "hatch"}) {
                std::vector<uint8_t> blob(desktop.blobSize("stickmon", key));
                if (!blob.empty()) assert(desktop.readBlob("stickmon", key, blob.data(), blob.size()));
                result.insert(result.end(), blob.begin(), blob.end());
            }
            return result;
        };
        const auto raw = snapshot();
        app.sceneFlow.enter(AppSceneFlow::Scene::DEBUG);
        app.debugCategory = DebugViewModel::Category::ENV;
        app.handleDebugTap(100, 216, desktop.millis());
        assert(app.debugCategory == DebugViewModel::Category::ROOM);
        assert(app.debugBedStyle == 0 && app.debugRugStyle == 0);
        auto choose = [&](bool bed, uint8_t style) {
            app.handleDebugTap(100, bed ? 88 : 152, desktop.millis());
            assert(app.debugPopup == (bed ? DebugViewModel::Popup::BED_STYLE : DebugViewModel::Popup::RUG_STYLE));
            app.handleDebugTap(120, 152 + (style - 1) * 44, desktop.millis());
            assert(app.debugPopup == DebugViewModel::Popup::NONE);
            assert((bed ? app.debugBedStyle : app.debugRugStyle) == style);
            assert(room.foodX() == foodX && room.bedX() == bedX && room.doorwayInsideX() == doorX);
        };
        choose(true, 1);
        assert(app.debugRugStyle == 0);
        assert(originalRug == room.furniture().selected("rug"));
        const auto revision = room.revision();
        const auto compressed = room.baseCompressedData();
        const std::string beforeFailure = room.furniture().selected("bed");
        const auto memoryBeforeFailure = memory.live;
        memory.failNext = true;
        assert(!room.setFurniture("bed", "bed_cloud"));
        assert(!room.selectRoom("missing-preview"));
        assert(!room.selectRoom("bad_layout"));
        assert(!room.selectRoom("truncated_layout"));
        assert(!room.setFurniture("bed", "missing"));
        assert(!room.setFurniture("bed", "bad_crc"));
        assert(!room.setFurniture("bed", "oversize"));
        assert(!room.setFurniture("unknown_slot", "bed_cloud"));
        assert(!room.setFurniture("bed", "../bed_cloud"));
        assert(!room.selectRoom("../standard"));
        assert(room.revision() == revision && room.baseCompressedData() == compressed);
        assert(beforeFailure == room.furniture().selected("bed"));
        assert(memory.live == memoryBeforeFailure);
        app.handleDebugTap(100, 152, desktop.millis());
        app.handleDebugTap(180, 320, desktop.millis());
        assert(app.debugPopup == DebugViewModel::Popup::NONE);
        assert(app.debugRugStyle == 0 && room.revision() == revision);

        std::vector<std::vector<uint16_t>> frames;
        for (int repeat = 0; repeat < 3; ++repeat) {
            for (uint8_t bed = 1; bed < Ui::Debug::FURNITURE_STYLE_COUNT; ++bed) {
                choose(true, bed);
                for (uint8_t rug = 1; rug < Ui::Debug::FURNITURE_STYLE_COUNT; ++rug) {
                    choose(false, rug);
                    assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, false, 2));
                    std::vector<uint16_t> day(canvas.rawPixels(), canvas.rawPixels() + 368 * 448);
                    assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, true, 2));
                    std::vector<uint16_t> night(canvas.rawPixels(), canvas.rawPixels() + 368 * 448);
                    assert(day != night);
                    if (repeat == 0) {
                        frames.push_back(day);
                        const std::string name = "b" + std::to_string(bed) + "_r" + std::to_string(rug);
                        exportFrame(name, false); exportFrame(name, true);
                    }
                    else assert(day == frames[(bed - 1) * (Ui::Debug::FURNITURE_STYLE_COUNT - 1) + rug - 1]);
                }
            }
        }
        for (size_t i = 0; i < frames.size(); ++i)
            for (size_t j = i + 1; j < frames.size(); ++j) assert(frames[i] != frames[j]);
        app.handleDebugTap(100, 216, desktop.millis());
        assert(app.debugCategory == DebugViewModel::Category::ENV);
        assert(snapshot() == raw);
        assert(room.selectRoom("alternate"));
        assert(room.layered() && room.height() == 185 && room.foodX() == foodX + 10 && room.furniture().count() == 6);
        assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, true, 2));
        exportFrame("alternate", true);
        assert(room.selectRoom("legacy"));
        assert(!room.layered() && room.furniture().count() == 0);
        assert(!room.setFurniture("bed", "bed_cloud"));
        assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, false, 2));
        assert(room.selectRoom("standard"));
        assert(originalBed == room.furniture().selected("bed"));
        assert(originalRug == room.furniture().selected("rug"));
        assert(original == exportFrame("restored", false));
        const auto logSize = desktop.logs().size();
        const auto allocations = memory.allocations, opens = resources.opens, live = memory.live;
        for (int i = 0; i < 100; ++i)
            assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, false, 2));
        assert(desktop.logs().size() == logSize);
        assert(memory.allocations == allocations && resources.opens == opens);
        for (int i = 0; i < 60; ++i) {
            assert(room.setFurniture("bed", i % 2 ? "bed_cloud" : "bed_forest_leaf"));
            assert(room.setFurniture("rug", i % 2 ? "rug_starry_night" : "rug_cloud"));
            assert(RoomRenderer::drawViewport(0, 56, 368, 288, 0, 0, i % 2, 2));
        }
        assert(room.selectRoom("standard"));
        assert(memory.live == live);
        std::printf("layered room memory: retained=%zu peak=%zu; 100 cached frames performed no IO or allocations\n", memory.live, memory.peak);
        std::puts("room preview: independent selection, cancel, nested back, failed load, cache revisions, day/night and save isolation passed");
    }
};
}
class PreviewPower final : public Platform::IPowerDevice {
public:
    int batteryLevel() override { return 100; }
    Platform::WakeReason wakeReason() const override { return Platform::WakeReason::NORMAL; }
    uint32_t hardwareRandom() override { return 12345; }
    size_t externalMemorySize() const override { return 8 * 1024 * 1024; }
    size_t externalMemoryFree() const override { return externalMemorySize(); }
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}
};
int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    auto& base = desktop.serviceBundle();
    PreviewPower power;
    TrackingMemory memory(base.memory);
    TrackingResources resources(base.resources);
    Platform::Services services{base.lifecycle, base.clock, base.logger, base.input,
        base.display, base.audio, base.imu, power, base.blobs, resources, memory, base.peers};
    Platform::bind(services);
    desktop.begin();
    std::vector<uint16_t> pixels(368 * 448);
    Canvas565& canvas = PixelRenderer::canvas();
    canvas.attach({pixels.data(), 368, 448, false});
    canvas.setNativeText(true);
    AmoledV1::AmoledRoomPreviewTestAccess::run(desktop, canvas, argv[2], memory, resources);
}
