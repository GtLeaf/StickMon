#pragma once

#include <cstdint>

namespace Game::BathInteraction {

inline constexpr uint8_t FOAM_CAP = 8;
inline constexpr uint32_t GROWTH_INTERVAL_MS = 600;
inline constexpr uint32_t SOAP_INTERVAL_MS = 180;
inline constexpr uint32_t RUB_INTERVAL_MS = 150;

struct Foam {
    float x = 0, y = 0;
    float restX = 0, restYOffset = 0;
    uint8_t stage = 0, brushProgress = 0;
    uint32_t bornMs = 0;
    bool active = false;
};

// Positions and speeds scale together; the two UIs supply their own geometry.
struct Layout {
    float centerX = 99, centerY = 78;
    float left = 47, right = 151, top = 34, bottom = 113;
    float lowerY = 88, groundY = 120, scale = 1;
};

struct State {
    Foam foam[FOAM_CAP] = {};
    uint8_t spawnCursor = 0, restSlot = 0;
    uint32_t lastGrowthMs = 0;
    bool atmosphereTarget = false;
};

struct RubResult {
    bool touched = false;
    bool merged = false;
};

uint8_t spawn(State& state, const Layout& layout, uint32_t nowMs);
RubResult rub(State& state, const Layout& layout, float x, float y, uint32_t nowMs);
bool merge(State& state, const Layout& layout, Foam& source, uint32_t nowMs);
Foam* partner(State& state, const Layout& layout, const Foam& source);
bool update(State& state, const Layout& layout, float dtSeconds);
float restY(const Layout& layout, uint8_t stage);
uint8_t lowerWeight(const State& state, const Layout& layout);
bool checkAtmosphere(State& state, const Layout& layout);
bool anyFoam(const State& state);
void rinse(State& state);

}  // namespace Game::BathInteraction
