#include "game/ExploreGrass.h"

#include <algorithm>
#include <cmath>
#include "game/ExploreRouteGeometry.h"

namespace ExploreGrass {
namespace {
constexpr int TILE = ExploreRouteGeometry::TILE_SIZE;
constexpr int WIDTH = ExploreMapGenerator::WIDTH;
}
void Simulation::reset(uint32_t nowMs) {
    for (auto& cell : cells_) cell = {};
    std::fill_n(valid_, ACTORS, false);
    lastMs_ = nowMs; area_ = 255;
}
Kind Simulation::kindAt(const ExploreMapGenerator::Map& map, uint16_t cell) {
    if (cell >= ExploreMapGenerator::CELL_COUNT) return Kind::NONE;
    if (map.layers[0][cell] == 399 || map.layers[1][cell] == 391) return Kind::HIGH;
    return map.layers[0][cell] == 390 ? Kind::SHORT : Kind::NONE;
}
Frame Simulation::frame(uint16_t cell) const {
    if (cell >= ExploreMapGenerator::CELL_COUNT || cells_[cell].age >= 720) return {};
    const auto& c = cells_[cell];
    return {c.kind, uint8_t(1 + c.age / 90), c.contactX};
}
int Simulation::bend(Frame f) {
    constexpr int offsets[] = {0, 1, 2, 3, 2, -1, -2, 1, 0};
    return offsets[std::min<int>(f.phase, 8)];
}
void Simulation::touch(const ExploreMapGenerator::Map& map, float x, float y) {
    const int cx = int(std::floor(x / TILE)), cy = int(std::floor(y / TILE));
    if (cx < 0 || cx >= WIDTH || cy < 0 || cy >= ExploreMapGenerator::HEIGHT) return;
    int index = cy * WIDTH + cx;
    const Kind kind = kindAt(map, index);
    if (kind == Kind::NONE) return;
    // The tall top and its base share one impulse and one animation clock.
    if (map.layers[1][index] == 391 && cy + 1 < ExploreMapGenerator::HEIGHT &&
        map.layers[0][index + WIDTH] == 399) index += WIDTH;
    auto& cell = cells_[index];
    if (cell.age < 270) return;
    cell.kind = kind; cell.age = 0;
    cell.contactX = uint8_t(std::clamp(int(x) - cx * TILE, 3, TILE - 4));
}
void Simulation::update(const ExploreMapGenerator::Map& map, uint32_t nowMs, bool active,
                         const Contact* contacts, uint8_t count) {
    if (area_ != map.areaIndex || seed_ != map.seed) {
        reset(nowMs); area_ = map.areaIndex; seed_ = map.seed;
    }
    const uint32_t dt = std::min<uint32_t>(nowMs - lastMs_, 250);
    lastMs_ = nowMs;
    if (!active) return;
    for (auto& cell : cells_) cell.age = std::min<uint32_t>(720, cell.age + dt);
    for (uint8_t i = 0; i < ACTORS; ++i) {
        if (i >= count || !contacts[i].grounded) { valid_[i] = false; continue; }
        const Contact contact = contacts[i];
        if (valid_[i]) {
            const float dx = contact.x - previous_[i].x, dy = contact.y - previous_[i].y;
            const float length = std::hypot(dx, dy);
            if (length < 6) continue;
            if (length <= TILE * 2) {
                // Sample a step so a low-frequency update cannot skip a grass cell.
                const int samples = std::max(1, int(std::ceil(length / 3)));
                for (int s = 1; s <= samples; ++s)
                    touch(map, previous_[i].x + dx * s / samples, previous_[i].y + dy * s / samples);
            }
        }
        previous_[i] = contact; valid_[i] = true;
    }
}
}  // namespace ExploreGrass
