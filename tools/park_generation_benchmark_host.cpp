#include "game/ExploreMapGenerator.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace ExploreMapGenerator;

int main(int argc, char** argv) {
    const unsigned count = argc > 1 ? std::strtoul(argv[1], nullptr, 0) : 1024;
    // Warm code and data before timing; no resource decoding or rendering here.
    Map map{};
    for (unsigned i = 0; i < 8; ++i)
        if (!generate(i, Edge(i % 4), TALL_GRASS_PARK_AREA, map)) return 1;
    for (unsigned i = 0; i < count; ++i) {
        const uint32_t seed = i < 16 ? i : deriveSeed(i * 0x9e3779b9U, i % 256, TALL_GRASS_PARK_AREA);
        for (unsigned edge = 0; edge < 4; ++edge) {
            const auto start = std::chrono::steady_clock::now();
            const bool ok = generate(seed, Edge(edge), TALL_GRASS_PARK_AREA, map);
            const auto stop = std::chrono::steady_clock::now();
            const double us = std::chrono::duration<double, std::micro>(stop - start).count();
            if (!ok) return 1;
            std::printf("%u %u %.3f %08x ", seed, edge, us, fingerprint(map));
            // Exact geometry and tile comparison, in addition to the fingerprint.
            for (const auto& layer : map.layers)
                for (uint16_t tile : layer) std::printf("%04x", tile);
            std::printf("%02x%02x%02x%02x%02x%02x", unsigned(map.entry.edge),
                        map.entry.point.x, map.entry.point.y, map.junction.x, map.junction.y, map.pathCount);
            for (unsigned p = 0; p < map.pathCount; ++p) {
                const auto& path = map.paths[p];
                std::printf("%02x%02x%02x%02x", unsigned(path.exit.edge), path.exit.point.x,
                            path.exit.point.y, path.pointCount);
                for (unsigned j = 0; j < path.pointCount; ++j)
                    std::printf("%02x%02x", path.points[j].x, path.points[j].y);
            }
            std::puts("");
        }
    }
}
