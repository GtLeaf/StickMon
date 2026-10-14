#include "game/BathInteraction.h"

#include <algorithm>
#include <cmath>

#include "game/GameRandom.h"

namespace Game::BathInteraction {

float restY(const Layout& layout, uint8_t stage) {
    static constexpr uint8_t HEIGHTS[] = {8, 16, 24, 32};
    return layout.groundY - HEIGHTS[std::min<uint8_t>(stage, 3)] * layout.scale * 0.5f;
}

uint8_t spawn(State& state, const Layout& layout, uint32_t nowMs) {
    static constexpr int8_t SPOTS[][2] = {
        {-20, -26}, {18, -20}, {-10, -6}, {24, 2},
        {-24, 8}, {8, 16}, {-14, 26}, {20, 28},
    };
    const uint8_t count = 1 + static_cast<uint8_t>(GameRandom::random(2));
    uint8_t spawned = 0;
    for (; spawned < count; ++spawned) {
        Foam* target = nullptr;
        for (Foam& foam : state.foam) {
            if (!foam.active) { target = &foam; break; }
        }
        if (!target) break;
        const auto& spot = SPOTS[state.spawnCursor];
        target->x = std::clamp(layout.centerX +
            (spot[0] + GameRandom::random(-3, 4)) * layout.scale, layout.left, layout.right);
        target->y = std::clamp(layout.centerY +
            (spot[1] + GameRandom::random(-3, 4)) * layout.scale, layout.top, layout.bottom);
        target->restX = target->x;
        target->restYOffset = 0;
        target->stage = GameRandom::random(3) == 0 ? 1 : 0;
        target->brushProgress = 0;
        target->bornMs = nowMs;
        target->active = true;
        state.spawnCursor = (state.spawnCursor + 1) % FOAM_CAP;
    }
    return spawned;
}

Foam* partner(State& state, const Layout& layout, const Foam& source) {
    Foam* nearest = nullptr;
    float best = INFINITY;
    const bool upper = source.y < layout.lowerY;
    for (Foam& candidate : state.foam) {
        if (!candidate.active || &candidate == &source) continue;
        const bool compatible = source.stage <= 1
            ? candidate.stage <= 1 && (candidate.y < layout.lowerY) == upper
            : candidate.stage == source.stage &&
              candidate.y + 0.5f * layout.scale >= restY(layout, candidate.stage) + candidate.restYOffset;
        if (!compatible) continue;
        const float dx = source.x - candidate.x, dy = source.y - candidate.y;
        const float distance = dx * dx + dy * dy;
        if (distance < best) { nearest = &candidate; best = distance; }
    }
    return nearest;
}

bool merge(State& state, const Layout& layout, Foam& source, uint32_t nowMs) {
    const uint8_t required = source.stage <= 1 ? 2 : 4;
    if (!source.active || source.stage >= 3 || source.brushProgress < required ||
        nowMs - state.lastGrowthMs < GROWTH_INTERVAL_MS) return false;
    if (source.stage >= 2 && source.y + 0.5f * layout.scale < restY(layout, source.stage) + source.restYOffset) return false;
    Foam* other = partner(state, layout, source);
    if (!other) return false;
    source.x = (source.x + other->x) * 0.5f;
    source.y = (source.y + other->y) * 0.5f;
    source.stage = source.stage <= 1 ? 2 : 3;
    source.brushProgress = 0;
    source.bornMs = nowMs;
    state.restSlot = (state.restSlot + 1) % 5;
    source.restX = std::clamp(layout.centerX +
        (static_cast<int>(state.restSlot) - 2) * 14 * layout.scale, layout.left, layout.right);
    source.restYOffset = -static_cast<float>((state.restSlot * 4 + source.stage * 3) % 11) * layout.scale;
    if (source.stage >= 3) source.y = restY(layout, source.stage) + source.restYOffset;
    other->active = false;
    other->brushProgress = 0;
    state.lastGrowthMs = nowMs;
    return true;
}

RubResult rub(State& state, const Layout& layout, float x, float y, uint32_t nowMs) {
    Foam* nearest = nullptr;
    float best = 24 * 24 * layout.scale * layout.scale;
    for (Foam& foam : state.foam) {
        if (!foam.active) continue;
        const float dx = x - foam.x, dy = y - foam.y;
        const float distance = dx * dx + dy * dy;
        if (distance < best) { nearest = &foam; best = distance; }
    }
    if (!nearest) return {};
    if (nearest->brushProgress < 255) ++nearest->brushProgress;
    return {true, merge(state, layout, *nearest, nowMs)};
}

bool update(State& state, const Layout& layout, float dtSeconds) {
    const float dt = std::clamp(dtSeconds, 0.0f, 0.05f);
    bool changed = false;
    for (Foam& foam : state.foam) {
        if (!foam.active || foam.stage < 2) continue;
        const float target = restY(layout, foam.stage) + foam.restYOffset;
        const float oldX = foam.x, oldY = foam.y;
        if (foam.stage == 2 && foam.y < target) {
            foam.y = std::min(target, foam.y + 24 * layout.scale * dt);
            foam.x += std::clamp(foam.restX - foam.x, -18 * layout.scale * dt, 18 * layout.scale * dt);
        } else if (foam.stage == 3) {
            foam.y = std::min(target, foam.y + 14 * layout.scale * dt);
            foam.x += std::clamp(foam.restX - foam.x, -12 * layout.scale * dt, 12 * layout.scale * dt);
        }
        changed = changed || oldX != foam.x || oldY != foam.y;
    }
    return changed;
}

uint8_t lowerWeight(const State& state, const Layout& layout) {
    uint8_t total = 0;
    for (const Foam& foam : state.foam) {
        if (foam.active && foam.y >= layout.lowerY)
            total += foam.stage >= 3 ? 4 : foam.stage == 2 ? 2 : 1;
    }
    return total;
}

bool checkAtmosphere(State& state, const Layout& layout) {
    if (state.atmosphereTarget || lowerWeight(state, layout) <= 6) return false;
    bool large = false;
    for (const Foam& foam : state.foam)
        large = large || (foam.active && foam.stage >= 2 && foam.y >= layout.lowerY);
    if (!large) return false;
    for (Foam& foam : state.foam) foam = {};
    state.atmosphereTarget = true;
    return true;
}

bool anyFoam(const State& state) {
    for (const Foam& foam : state.foam) if (foam.active) return true;
    return false;
}

void rinse(State& state) {
    for (Foam& foam : state.foam) {
        if (!foam.active) continue;
        if (foam.stage > 0) --foam.stage;
        else foam.active = false;
    }
    if (!anyFoam(state)) state.atmosphereTarget = false;
}

}  // namespace Game::BathInteraction
