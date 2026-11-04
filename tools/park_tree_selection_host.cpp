// Exercise the optimized scorer against a deliberately exhaustive reference.
#include "game/ExploreMapGenerator.cpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using namespace ExploreMapGenerator;

bool referenceTree(Rng& rng, const CellMask& road, const CellMask& trees,
                   uint8_t width, bool boundary, uint8_t target,
                   Point& selected, bool adjacent) {
    CellMask forbidden = boundary || adjacent ? trees : expandedMask(trees, 1);
    forbidden.unite(road);
    int scores[CELL_COUNT];
    for (auto& score : scores) score = -32768;
    int best = -32768;
    for (uint8_t y = 0; y < HEIGHT - 2; ++y) {
        for (uint8_t x = 0; x <= WIDTH - width; ++x) {
            if ((boundary && y != 0) || forbidden.intersectsRect(x, y, x + width - 1, y + 2)) continue;
            CellMask after = trees;
            int distance = 6;
            for (uint8_t dy = 0; dy < 3; ++dy) {
                for (uint8_t dx = 0; dx < width; ++dx) {
                    after.add(x + dx, y + dy);
                    for (uint8_t ty = 0; ty < HEIGHT; ++ty)
                        for (uint8_t tx = 0; tx < WIDTH; ++tx)
                            if (trees.contains(tx, ty))
                                distance = minimum(distance, absolute(x + dx - tx) + absolute(y + dy - ty));
                }
            }
            const int capacity = parkGrassAvailable(road, after).count();
            if (capacity < target + 8) continue;
            const int margin = minimum(minimum(x, WIDTH - x - width), minimum(y, HEIGHT - y - 3));
            const int score = capacity * 8 + distance * 2 + (boundary ? 0 : minimum(margin, 2) * 3);
            scores[y * WIDTH + x] = score;
            if (score > best) best = score;
        }
    }
    if (best == -32768) return false;
    unsigned count = 0;
    for (int score : scores) count += score >= best - 8;
    unsigned choice = rng.bounded(count);
    for (unsigned cell = 0; cell < CELL_COUNT; ++cell) {
        if (scores[cell] >= best - 8 && choice-- == 0) {
            selected = {uint8_t(cell % WIDTH), uint8_t(cell / WIDTH)};
            return true;
        }
    }
    return false;
}

int main() {
    unsigned checked = 0;
    for (unsigned sample = 0; sample < 260; ++sample) {
        CellMask road, trees;
        Rng mask(sample + 17);
        for (unsigned cell = 0; cell < CELL_COUNT; ++cell) {
            const auto roll = mask.bounded(100);
            // Empty, full, boundary-only, then sparse/dense mixed masks.
            if (sample == 1 || (sample == 2 && cell / WIDTH == HEIGHT - 1) ||
                (sample >= 4 && roll < sample % 17)) trees.add(cell % WIDTH, cell / WIDTH);
            if (sample == 3 || (sample >= 4 && roll >= 80)) road.add(cell % WIDTH, cell / WIDTH);
        }
        uint8_t distances[CELL_COUNT];
        parkTreeDistances(trees, distances);
        for (unsigned cell = 0; cell < CELL_COUNT; ++cell) {
            int expected = 6;
            for (unsigned other = 0; other < CELL_COUNT; ++other)
                if (trees.contains(other % WIDTH, other / WIDTH))
                    expected = minimum(expected, absolute(int(cell % WIDTH) - int(other % WIDTH)) +
                                                 absolute(int(cell / WIDTH) - int(other / WIDTH)));
            assert(distances[cell] == expected);
        }
        for (uint8_t width : {2, 3, 4})
            for (bool boundary : {false, true})
                for (bool adjacent : {false, true})
                    for (uint8_t target : {40, 45, 50}) {
                        Rng original(sample * 101 + target), optimized(sample * 101 + target);
                        Point a{255,255}, b{255,255};
                        const bool first = referenceTree(original, road, trees, width, boundary, target, a, adjacent);
                        const bool second = chooseParkTree(optimized, road, trees, width, boundary, target, b, adjacent);
                        assert(first == second && samePoint(a, b));
                        assert(original.next() == optimized.next());
                        ++checked;
                    }
    }
    std::printf("park-tree=ok cases=%u distances=%u\n", checked, 260U * CELL_COUNT);
}
