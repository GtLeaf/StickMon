#pragma once
#include <cstdint>
#include <type_traits>

// Portable authored-map actor simulation. Positions are in map-cell units.
namespace AuthoredNpc {
#ifdef STICKMON_EDITOR_MAP_WIDTH
constexpr int WIDTH=STICKMON_EDITOR_MAP_WIDTH, HEIGHT=STICKMON_EDITOR_MAP_HEIGHT;
#else
constexpr int WIDTH=16, HEIGHT=12;
#endif
constexpr int CELLS=WIDTH*HEIGHT;
using Cell = typename std::conditional<(CELLS <= 256), uint8_t, uint16_t>::type;
static_assert(CELLS <= 32767, "NPC cell indices must fit the path search");
constexpr int CAPACITY = 16, PATROL_CAPACITY = 32, STEP_MS = 100;
enum class Behavior : uint8_t { IDLE, WANDER, PATROL };
struct Definition {
    bool allowed[CELLS]{};
    Cell spawn = 0;
    uint8_t direction = 0;
    Behavior behavior = Behavior::IDLE;
    uint8_t stepTicks = 6;
    uint16_t waitMin = 10, waitMax = 30;
    Cell patrol[PATROL_CAPACITY]{};
    uint8_t patrolCount = 0;
    uint32_t seed = 1;
};
struct Pose { float x, y; uint8_t direction, frame; bool moving; };
struct State {
    Cell from = 0, to = 0;
    uint8_t direction = 0, patrolIndex = 0;
    uint16_t wait = 0;
    uint8_t progress = 0;
    int16_t goal = -1;
    uint32_t random = 1;
    bool moving = false;
    bool active = true, external = false;
};
class Simulation {
    const Definition* definitions_ = nullptr;
    State actors_[CAPACITY]{};
    uint8_t count_ = 0;
    static uint32_t next(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    bool occupied(int cell, int self) const {
        for (int i = 0; i < count_; ++i)
            if (i != self && actors_[i].active && (actors_[i].from == cell || actors_[i].to == cell)) return true;
        return false;
    }
    int route(int self, int goal, bool dynamic) const {
        const auto& d = definitions_[self]; const int start = actors_[self].to;
        int16_t previous[CELLS]; for (auto& p : previous) p = -1;
        Cell queue[CELLS]; int head = 0, tail = 0;
        queue[tail++] = Cell(start); previous[start] = int16_t(start);
        while (head < tail) {
            const int cell = queue[head++];
            if (cell == goal) {
                int result = cell;
                while (previous[result] != start && result != start) result = previous[result];
                return result;
            }
            const int dx[4] = {0,-1,0,1}, dy[4] = {1,0,-1,0};
            for (int j = 0; j < 4; ++j) {
                const int x = cell % WIDTH + dx[j], y = cell / WIDTH + dy[j];
                if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) continue;
                const int n = y * WIDTH + x;
                if (previous[n] >= 0 || !d.allowed[n] || (dynamic && occupied(n,self))) continue;
                previous[n] = int16_t(cell); queue[tail++] = Cell(n);
            }
        }
        return -1;
    }
    uint16_t waitFor(int i) {
        const auto& d = definitions_[i];
        return uint16_t(d.waitMin + next(actors_[i].random) % (d.waitMax - d.waitMin + 1));
    }
public:
    void reset(const Definition* definitions, uint8_t count) {
        definitions_ = definitions; count_ = count <= CAPACITY ? count : CAPACITY;
        for (int i = 0; i < count_; ++i) {
            auto& s = actors_[i]; s = {};
            s.from = s.to = definitions[i].spawn; s.direction = definitions[i].direction;
            s.random = definitions[i].seed ? definitions[i].seed : 1;
            s.goal = -1; s.wait = waitFor(i);
        }
    }
    uint8_t count() const { return count_; }
    const State& state(int i) const { return actors_[i]; }
    bool available(int cell, int self = -1) const { return !occupied(cell,self); }
    void deactivate(int i) { actors_[i].active = false; }
    void activate(int i, Cell cell, uint8_t direction) {
        auto& s = actors_[i]; s = {}; s.from = s.to = cell;
        s.direction = direction; s.active = s.external = true; s.goal = -1;
    }
    // External controllers share the same collision reservations and routing.
    bool walkTo(int i, int goal) {
        auto& s = actors_[i];
        if (!s.active || s.moving || s.to == goal) return false;
        const int target = route(i,goal,true);
        if (target < 0 || target == s.to) return false;
        const int delta = target - s.to;
        s.direction = delta == WIDTH ? 0 : delta == -1 ? 1 : delta == -WIDTH ? 2 : 3;
        s.from = s.to; s.to = Cell(target); s.progress = 0; s.moving = true;
        return true;
    }
    void tick() {
        for (int i = 0; i < count_; ++i) {
            auto& s = actors_[i]; const auto& d = definitions_[i];
            if (s.active && s.moving && ++s.progress >= d.stepTicks) {
                s.from = s.to; s.moving = false; s.progress = 0;
                if (s.to == s.goal) {
                    s.goal = -1; s.wait = waitFor(i);
                    if (d.behavior == Behavior::PATROL && d.patrolCount)
                        s.patrolIndex = (s.patrolIndex + 1) % d.patrolCount;
                }
            }
        }
        for (int i = 0; i < count_; ++i) {
            auto& s = actors_[i]; const auto& d = definitions_[i];
            if (!s.active || s.external || s.moving || d.behavior == Behavior::IDLE) continue;
            if (s.wait) { --s.wait; continue; }
            if (s.goal < 0) {
                if (d.behavior == Behavior::PATROL && d.patrolCount) {
                    s.goal = d.patrol[s.patrolIndex];
                    if (s.goal == s.to) {
                        s.patrolIndex = (s.patrolIndex + 1) % d.patrolCount;
                        s.goal = -1; s.wait = waitFor(i); continue;
                    }
                } else {
                    Cell candidates[CELLS]; int count = 0;
                    // One static flood instead of a path search for every candidate.
                    bool seen[CELLS]{}; Cell queue[CELLS]; int head = 0, tail = 0;
                    queue[tail++] = s.to; seen[s.to] = true;
                    while (head < tail) {
                        const int c = queue[head++];
                        if (c != s.to && !occupied(c,i)) candidates[count++] = Cell(c);
                        const int dx[4] = {0,-1,0,1}, dy[4] = {1,0,-1,0};
                        for (int j=0;j<4;++j) {
                            const int x=c%WIDTH+dx[j], y=c/WIDTH+dy[j], n=y*WIDTH+x;
                            if(x<0||x>=WIDTH||y<0||y>=HEIGHT||seen[n]||!d.allowed[n]) continue;
                            seen[n]=true; queue[tail++]=Cell(n);
                        }
                    }
                    if (count) s.goal = candidates[next(s.random) % count];
                    else { s.wait = waitFor(i); continue; }
                }
            }
            const int target = route(i,s.goal,true);
            if (target < 0 || target == s.to) {
                if (d.behavior == Behavior::WANDER) s.goal = -1;
                s.wait = waitFor(i); continue;
            }
            const int delta = target - s.to;
            s.direction = delta == WIDTH ? 0 : delta == -1 ? 1 : delta == -WIDTH ? 2 : 3;
            s.from = s.to; s.to = Cell(target); s.progress = 0; s.moving = true;
        }
    }
    Pose pose(int i) const {
        const auto& s = actors_[i];
        const float t = s.moving ? float(s.progress) / definitions_[i].stepTicks : 1;
        return {s.from % WIDTH + .5f + (s.to % WIDTH - s.from % WIDTH) * t,
                s.from / WIDTH + .5f + (s.to / WIDTH - s.from / WIDTH) * t,
                s.direction, uint8_t(s.moving ? (s.progress * 4 / definitions_[i].stepTicks) % 4 : 0), s.moving};
    }
};
}  // namespace AuthoredNpc
