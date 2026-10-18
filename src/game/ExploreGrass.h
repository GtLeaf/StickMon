#pragma once

#include <cstdint>
#include "game/ExploreMapGenerator.h"

namespace ExploreGrass {
inline constexpr uint8_t ACTORS = 5;
enum class Kind : uint8_t { NONE, SHORT, HIGH };
struct Contact { float x = 0, y = 0; bool grounded = false; };
struct Frame {
    Kind kind = Kind::NONE;
    uint8_t phase = 0, contactX = 13;
};
inline bool operator==(Frame a, Frame b) {
    return a.kind == b.kind && a.phase == b.phase && a.contactX == b.contactX;
}
inline bool operator!=(Frame a, Frame b) { return !(a == b); }

class Simulation {
public:
    void reset(uint32_t nowMs);
    void update(const ExploreMapGenerator::Map& map, uint32_t nowMs, bool active,
                const Contact* contacts, uint8_t count);
    Frame frame(uint16_t cell) const;
    static Kind kindAt(const ExploreMapGenerator::Map& map, uint16_t cell);
    static int bend(Frame frame);
private:
    struct Cell { uint16_t age = 720; uint8_t contactX = 13; Kind kind = Kind::NONE; };
    Cell cells_[ExploreMapGenerator::CELL_COUNT]{};
    Contact previous_[ACTORS]{};
    bool valid_[ACTORS]{};
    uint32_t lastMs_ = 0, seed_ = 0;
    uint8_t area_ = 255;
    void touch(const ExploreMapGenerator::Map& map, float x, float y);
};
}  // namespace ExploreGrass
