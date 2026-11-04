// Fixed-camera-path rendering probe; desktop timings are not device FPS.
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "game/ExploreMapGenerator.h"
#include "ui/ExploreScreens.h"
#include "ui/RenderCaches.h"
#include "ui/models/ScreenModels.h"

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    assert(Platform::resources().mount());
    std::vector<uint16_t> pixels(368 * 448);
    auto& canvas = PixelRenderer::canvas();
    canvas.attach({pixels.data(), 368, 448, true});
    canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1);
    canvas.setAssetScale(1);
    for (uint8_t area = 0; area < 6; ++area) {
        ExploreMapGenerator::Map map{};
        assert(ExploreMapGenerator::generate(12152 + area,
            ExploreMapGenerator::Edge::LEFT, area, map));
        AmoledV1::ExploreRouteViewModel model{};
        model.map = &map;
        model.area = area;
        model.worldX = 180;
        model.worldY = 160;
        model.walking = true;
        AmoledV1::PixelCache565 cache;
        std::vector<double> samples;
        uint64_t hash = 1469598103934665603ULL;
        // Warm all map animation phases before measuring steady scrolling.
        for (int frame = -32; frame < 240; ++frame) {
            const int phase = frame + 32;
            model.cameraX = (phase * 3) % 200;
            model.cameraY = (phase * 2) % 120;
            model.mapFrame = (phase / 6) % 8;
            model.petFrame = phase / 4;
            const auto begin = std::chrono::steady_clock::now();
            AmoledV1::renderExploreRouteScreen(canvas, model, cache, 0, 344);
            const auto end = std::chrono::steady_clock::now();
            if (frame < 0) continue;
            samples.push_back(std::chrono::duration<double, std::micro>(end - begin).count());
            for (const auto pixel : pixels) { hash ^= pixel; hash *= 1099511628211ULL; }
        }
        std::sort(samples.begin(), samples.end());
        std::printf("SCROLL area=%u median_us=%.2f p95_us=%.2f hash=%016llx\n",
            area, samples[samples.size()/2], samples[samples.size()*95/100],
            static_cast<unsigned long long>(hash));
    }
}
