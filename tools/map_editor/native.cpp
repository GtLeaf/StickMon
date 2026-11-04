#include "wire.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include <iostream>
#include <iomanip>
#include <string>

int main(int argc, char** argv) {
    using namespace ExploreMapGenerator;
    unsigned n;
    if (argc > 1 && std::string(argv[1]) == "catalog") {
        while (std::cin >> n) {
            std::cout << n << ' ' << isRoadTile(n) << ' ' << isWaterTile(n)
                      << ' ' << isForestTile(n) << ' ' << ExploreTileSemantics::bridgeDeck(n);
            for (int z = 0; z < 3; ++z) {
                std::cout << ' ' << ExploreTileSemantics::objectDepthOffsetRows(n, z)
                          << ' ' << ExploreTileSemantics::upwardLadder(n, z)
                          << ' ' << ExploreTileSemantics::groundBehindDecoration(n, z);
            }
            std::cout << '\n';
        }
        return 0;
    }
    Map m{};
    if (!readEditorMap(m)) return 2;
    std::cout << std::hex << std::setw(8) << std::setfill('0') << fingerprint(m) << '\n';
    std::cout << std::dec;
    for (unsigned i = 0; i < m.pathCount; ++i) {
        for (unsigned j = 0; j < m.paths[i].pointCount; ++j) {
            auto p = ExploreRouteGeometry::pathPoint(m.paths[i], j);
            std::cout << p.x << ' ' << p.y << '\n';
        }
    }
}
