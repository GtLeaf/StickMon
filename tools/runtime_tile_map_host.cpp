#include "game/ExploreMapGenerator.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc != 4 && argc != 7) {
        std::fprintf(stderr, "usage: runtime_tile_map_host SEED ENTRY_EDGE AREA_INDEX [LEVEL COUNT LADDER]\n");
        return 2;
    }
    uint32_t seed = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));
    unsigned long edgeValue = std::strtoul(argv[2], nullptr, 0);
    unsigned long areaValue = std::strtoul(argv[3], nullptr, 0);
    if (edgeValue > 3 || areaValue > 255) return 2;
    ExploreMapGenerator::FrostContext frost{};
    if (argc == 7) {
        frost.level = static_cast<uint8_t>(std::strtoul(argv[4], nullptr, 0));
        frost.levelCount = static_cast<uint8_t>(std::strtoul(argv[5], nullptr, 0));
        frost.enteredByLadder = std::strtoul(argv[6], nullptr, 0) != 0;
    }

    ExploreMapGenerator::Map map;
    if (!ExploreMapGenerator::generate(
            seed, static_cast<ExploreMapGenerator::Edge>(edgeValue),
            static_cast<uint8_t>(areaValue), map, frost)) {
        return 1;
    }
    std::printf("%08x\n", ExploreMapGenerator::fingerprint(map));
    return 0;
}
