#pragma once
#include "game/ExploreMapGenerator.h"
#include <iostream>
inline bool readEditorMap(ExploreMapGenerator::Map& m) {
    using namespace ExploreMapGenerator;
    unsigned n;
    auto byte = [&](auto& v) { std::cin >> n; v = static_cast<decltype(v + 0)>(n); };
    auto point = [&](Point& p) { byte(p.x); byte(p.y); };
    auto endpoint = [&](Endpoint& e) { point(e.point); std::cin >> n; e.edge = static_cast<Edge>(n); };
    std::cin >> m.seed; byte(m.areaIndex); endpoint(m.entry); point(m.junction);
    byte(m.valleyJourney); byte(m.valleyNode); byte(m.hasCoast); byte(m.hasForest);
    byte(m.hasCreek); byte(m.hasCliff); byte(m.hasWaterfall); byte(m.pathCount);
    if (m.pathCount > MAX_PATH_COUNT) return false;
    for (unsigned i = 0; i < m.pathCount; ++i) {
        auto& p = m.paths[i]; byte(p.pointCount);
        if (p.pointCount > MAX_PATH_POINTS) return false;
        endpoint(p.exit); byte(p.fallsToNextLevel); byte(p.cellCentered);
        byte(p.nextValleyNode); byte(p.nextValleyEntrance); byte(p.terminal); byte(p.interiorExit);
        for (unsigned j = 0; j < p.pointCount; ++j) point(p.points[j]);
    }
    for (auto& layer : m.layers) for (auto& tile : layer) byte(tile);
    return bool(std::cin);
}
