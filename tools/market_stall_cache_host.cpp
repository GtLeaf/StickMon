#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "market_dialogue_fixture.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

class CacheFailureMemory final : public Platform::IMemoryAllocator {
public:
    CacheFailureMemory(Platform::IMemoryAllocator& backing, size_t bytes, unsigned failAt)
        : backing_(backing), bytes_(bytes), failAt_(failAt) {}
    void* allocate(size_t bytes, bool external) override {
        if (bytes == bytes_ && ++seen_ == failAt_) { failed = true; return nullptr; }
        return backing_.allocate(bytes, external);
    }
    void release(void* pointer) override { backing_.release(pointer); }
    size_t externalFree() const override { return backing_.externalFree(); }
    bool failed = false;
private:
    Platform::IMemoryAllocator& backing_;
    size_t bytes_;
    unsigned failAt_, seen_ = 0;
};

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app, bool requireCache) {
        app.gameState = {}; app.gameState.oobeDone = true; app.gameState.teamCount = 2;
        app.gameState.team[0] = Game::MonsterFactory::create(25, 18);
        app.gameState.team[1] = Game::MonsterFactory::create(133, 18);
        app.syncHomeActors(0); assert(app.startMarket(0));
        auto& session = *app.market_;
        Market::Data data = session.data();
        assert(session.reset(data, 1, 25, 133, 0)); session.enableManual();
        auto& c = PixelRenderer::canvas();
        std::array<uint16_t, Market::CELL_WIDTH * 2> saved{};
        for (int x = 0; x < Market::CELL_WIDTH; ++x) {
            saved[x] = data.map.layers[0][x];
            saved[x + Market::CELL_WIDTH] = data.map.layers[0][(Market::CELL_HEIGHT - 1) * Market::CELL_WIDTH + x];
        }
        // Find a real animated, cell-sized tile for a synthetic edge row.
        uint16_t animated = 0;
        std::array<uint16_t, 26 * 26> reference{}, pixels{};
        Canvas565 probe;
        for (const auto& layer : data.map.layers) for (const auto id : layer) {
            if (!id || animated) continue;
            probe.attach({reference.data(), 26, 26, false}); probe.fillRect(0, 0, 26, 26, 0x2227);
            GameAssets::drawExploreTileTo(probe, id, 0, 0, 0, 1);
            for (uint8_t frame = 1; frame < 8; ++frame) {
                probe.attach({pixels.data(), 26, 26, false}); probe.fillRect(0, 0, 26, 26, 0x2227);
                GameAssets::drawExploreTileTo(probe, id, 0, 0, frame, 1);
                if (reference != pixels) { animated = id; break; }
            }
        }
        assert(animated);
        unsigned comparisons = 0;
        for (int edgeMode : {0, 1, 2}) {
            for (int x = 0; x < Market::CELL_WIDTH; ++x) {
                data.map.layers[0][x] = edgeMode == 0 ? saved[x] : edgeMode == 1 ? 0 : animated;
                data.map.layers[0][(Market::CELL_HEIGHT - 1) * Market::CELL_WIDTH + x] =
                    edgeMode == 0 ? saved[x + Market::CELL_WIDTH] : edgeMode == 1 ? 0 : animated;
            }
            for (const auto dimensions : {std::array<int, 3>{368, 448, 15}, {360, 360, 10}, {240, 135, 10}}) {
                for (bool swapped : {false, true}) {
                    const int width = dimensions[0], height = dimensions[1];
                    std::vector<uint16_t> screen(width * height), expected(width * height);
                    c.attach({screen.data(), uint16_t(width), uint16_t(height), swapped});
                    Ui::MarketView::Viewport v{width, height, dimensions[2] / 10.0f, width == 360, true};
                    Ui::MarketView::detail::releaseStaticLayer();
                    Ui::MarketView::detail::staticLayerEnabled() = true;
                    Ui::MarketView::render(session, v, 0, 0);
                    const auto* plain = Ui::MarketView::detail::staticLayer().plain;
                    const auto* tinted = Ui::MarketView::detail::staticLayer().tinted;
                    const auto builds = Ui::MarketView::perf::events().builds;
                    assert(plain && tinted);
                    const int baseCamera = v.cameraY(session);
                    const int upperEdge = -baseCamera - 1;
                    const int lowerEdge = v.pixel(Market::CELL_HEIGHT * 26) - height - baseCamera + 1;
                    for (int shift : {-240, -120, -1, 0, 1, 94, 120, 240, upperEdge, lowerEdge}) {
                        v.cameraOffsetY = shift;
                        for (unsigned minutes : {0U, 720U}) for (unsigned frame = 0; frame < 8; ++frame) {
                            Ui::MarketView::detail::staticLayerEnabled() = true;
                            const auto before = Ui::MarketView::perf::window().cachedFrames;
                            Ui::MarketView::render(session, v, 0, minutes, frame * 140);
                            if (requireCache && Ui::MarketView::perf::window().cachedFrames != before + 1) {
                                std::fprintf(stderr, "cache fallback: edge=%d %dx%d swapped=%d shift=%d reason=%s\n",
                                    edgeMode, width, height, swapped, shift, Ui::MarketView::perf::events().fallback);
                                assert(false);
                            }
                            expected = screen;
                            Ui::MarketView::detail::staticLayerEnabled() = false;
                            Ui::MarketView::render(session, v, 0, minutes, frame * 140);
                            if (screen != expected) {
                                size_t mismatch = 0; while (screen[mismatch] == expected[mismatch]) ++mismatch;
                                std::fprintf(stderr, "pixel mismatch: edge=%d %dx%d swapped=%d shift=%d minute=%u frame=%u x=%zu y=%zu\n",
                                    edgeMode, width, height, swapped, shift, minutes, frame, mismatch % width, mismatch / width);
                                assert(false);
                            }
                            const auto& cache = Ui::MarketView::detail::staticLayer();
                            assert(cache.plain == plain && cache.tinted == tinted && Ui::MarketView::perf::events().builds == builds);
                            ++comparisons;
                        }
                    }
                }
            }
        }
        for (int x = 0; x < Market::CELL_WIDTH; ++x) {
            data.map.layers[0][x] = saved[x];
            data.map.layers[0][(Market::CELL_HEIGHT - 1) * Market::CELL_WIDTH + x] = saved[x + Market::CELL_WIDTH];
        }
        Ui::MarketView::detail::staticLayerEnabled() = true;
        Ui::MarketView::detail::releaseStaticLayer();
        // Deny the plain cache and then the tinted cache separately. Both
        // must retain the direct renderer's pixels, including uncached rows.
        std::vector<uint16_t> screen(368 * 448), expected(368 * 448);
        c.attach({screen.data(), 368, 448, false});
        Ui::MarketView::Viewport v{368, 448, 1.5f, false, true}; v.cameraOffsetY = 240;
        Ui::MarketView::detail::staticLayerEnabled() = false;
        Ui::MarketView::render(session, v, 0, 0, 420); expected = screen;
        Ui::MarketView::detail::staticLayerEnabled() = true;
        auto& base = Platform::services();
        for (unsigned failAt : {1U, 2U}) {
            const size_t bytes = size_t(Market::CELL_WIDTH * 39) * Market::CELL_HEIGHT * 39 * sizeof(uint16_t);
            CacheFailureMemory memory(base.memory, bytes, failAt);
            Platform::Services fault{base.lifecycle, base.clock, base.logger, base.input, base.display,
                base.audio, base.imu, base.power, base.blobs, base.resources, memory, base.peers};
            Platform::bind(fault);
            Ui::MarketView::render(session, v, 0, 0, 420);
            assert(memory.failed && screen == expected);
            Ui::MarketView::detail::releaseStaticLayer();
            Platform::bind(base);
        }
        app.closeMarket(0);
        std::printf("market-stall-cache: %u pixel comparisons, authored/empty/animated edges, stable allocations and allocation failures OK\n", comparisons);
    }
};
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    DesktopPlatform desktop(argv[1]); Platform::bind(desktop.serviceBundle()); assert(desktop.begin());
    assert(Platform::resources().mount()); assert(GameAssets::begin()); assert(FontResource::ins().begin());
    auto& c = PixelRenderer::canvas();
    c.setCoordinateScale(1); c.setLayoutScale(1); c.setAssetScale(1); c.setNativeText(true);
    AmoledV1::AmoledApp app; app.begin(0);
    AmoledV1::AmoledAppTestAccess::run(app, argc == 2 || std::strcmp(argv[2], "--allow-fallback") != 0);
}
