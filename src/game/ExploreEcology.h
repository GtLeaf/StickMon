#pragma once

#include <cstdint>

#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"

namespace ExploreEcology {

inline constexpr uint8_t CAPACITY = 3;
inline constexpr uint8_t GROUP_CHANCE_PERCENT = 50;
inline constexpr uint16_t GROUP_PAIRS[][2] = {{286, 285}, {183, 298}};
inline uint16_t groupPartner(uint16_t species) {
    for (const auto& pair : GROUP_PAIRS) {
        if (species == pair[0]) return pair[1];
        if (species == pair[1]) return pair[0];
    }
    return 0;
}

enum class Surface : uint8_t { NONE, GRASS, ROCK, ICE, WATER };
enum class Temperament : uint8_t { SHY, CURIOUS, RELAXED, AERIAL };
enum class Locomotion : uint8_t { GROUND, FLAP, FLOAT };
enum class Cover : uint8_t { NONE, GRASS, SOLID };
enum class Activity : uint8_t {
    REST, WALK, WATCH, DOZE, WAKE, APPROACH, OBSERVE, RETURN,
    FLEE, HIDE, PEEK, TAKEOFF, FLY, LAND, GATHER, JUMP, SPLASH, RUSTLE,
    BURROW, EMERGE, CRUISE, ROAM, DEPART, AWAY, ARRIVE
};
inline constexpr uint8_t ACTIVITY_COUNT = uint8_t(Activity::ARRIVE) + 1;

Temperament temperamentForSpecies(uint16_t speciesId);
Locomotion locomotionForSpecies(uint16_t speciesId);
bool wideFlightSpecies(uint16_t speciesId);
bool isEpisode(Activity activity);

struct Pose {
    uint16_t speciesId = 0;
    float x = 0, y = 0;
    uint8_t direction = 0;  // Down, left, up, right.
    uint8_t frame = 0;
    bool walking = false;
    bool hidden = false;
    Surface surface = Surface::GRASS;
    Activity activity = Activity::REST;
    int8_t lift = 0;  // Body height in world pixels; the ground anchor stays put.
    int8_t sway = 0;
    Cover cover = Cover::NONE;
    bool airborne = false;
    bool absent = false;
};

inline bool samePose(const Pose& a, const Pose& b) {
    return a.speciesId == b.speciesId && a.x == b.x && a.y == b.y &&
        a.direction == b.direction && a.frame == b.frame &&
        a.walking == b.walking && a.hidden == b.hidden && a.surface == b.surface &&
        a.activity == b.activity && a.lift == b.lift && a.sway == b.sway &&
        a.cover == b.cover && a.airborne == b.airborne && a.absent == b.absent;
}

struct View {
    float x, y, width, height;
    bool contains(float px, float py, float margin = 30) const {
        return px >= x - margin && px < x + width + margin &&
            py >= y - margin && py < y + height + margin;
    }
};

class Simulation {
public:
    void reset(const ExploreMapGenerator::Map& map, uint8_t pathIndex,
               const ExplorePool::Pool& pool, uint8_t hiddenPoolMask,
               uint32_t nowMs);
    void update(uint32_t nowMs, bool active, float playerX, float playerY,
                View view);
    void revealSpecies(uint16_t speciesId);
    uint8_t count() const { return count_; }
    bool hasGroup() const { return group_.active; }
    const Pose& pose(uint8_t index) const { return actors_[index].pose; }
    bool safeCell(int x, int y) const;
    Surface surfaceAt(int x, int y) const;
    bool canOccupy(uint16_t speciesId, int x, int y) const;
    bool canStand(uint16_t speciesId, float x, float y) const;
    Cover coverAt(float x, float y) const;
    Surface flightSurfaceAt(int x, int y) const;
    bool canFlyAt(float x, float y) const;

private:
    // A group owns slots 0/1 (adult/young), sharing a validated ground trail.
    struct Group {
        enum class Phase : uint8_t { PATROL, GATHER, REUNION, WATCH };
        bool active = false;
        uint8_t route[10]{}, routeCount = 0;
        uint8_t links[ExploreMapGenerator::CELL_COUNT]{};
        uint8_t recent[8] = {255, 255, 255, 255, 255, 255, 255, 255};
        uint8_t recentIndex = 0;
        Phase phase = Phase::PATROL;
        float leader = 26, follower = 0, stop = 0;
        uint32_t random = 1, pendingMs = 0, clockMs = 0, cycleMs = 0;
        uint32_t cycleDurationMs = 20000, waitMs = 600, glanceMs = 0;
        uint32_t nextGlanceMs = 6000, attentionCooldownMs = 0;
    } group_;
    bool spawnGroup(const ExploreMapGenerator::Path& path,
                    const ExplorePool::Pool& pool, uint8_t hiddenPoolMask,
                    uint32_t& random);
    bool planGroupRoute(const ExploreMapGenerator::Path* path = nullptr);
    void groupPoint(float travel, float& x, float& y) const;
    void advanceGroup(uint32_t dt, float playerX, float playerY, bool visible);
    struct Actor {
        Pose pose;
        float homeX = 0, homeY = 0;
        float fromX = 0, fromY = 0, toX = 0, toY = 0;
        uint32_t random = 1, pendingMs = 0;
        uint32_t elapsedMs = 0, durationMs = 0, attentionCooldownMs = 0;
        uint32_t episodeCooldownMs = 0;
        uint32_t wingMs = 0;
        float episodeX = 0, episodeY = 0;
        struct Flight {
            uint8_t route[ExploreMapGenerator::CELL_COUNT]{};
            uint8_t routeCount = 0, routeIndex = 0, region = 0;
            uint8_t recent[4] = {255, 255, 255, 255}, recentIndex = 0;
            uint8_t portal = 0, edge = 0;
            bool edgeLeg = false;
            float heading = 0;
            uint32_t departMs = 0;
        } flight;
    };
    bool occupied(float x, float y, uint8_t except) const;
    bool safeLeg(uint16_t species, float x, float y, float tx, float ty) const;
    bool wander(uint8_t index);
    bool startStep(uint8_t index, uint8_t direction, float length,
                   Activity activity, uint32_t durationMs);
    bool chooseStep(uint8_t index, float playerX, float playerY, bool retreat,
                    Activity activity, uint32_t durationMs);
    void beginPeek(uint8_t index);
    void cruise(uint8_t index);
    void buildFlightHabitat(const ExploreMapGenerator::Map& map);
    void beginWideFlight(uint8_t index);
    bool planFlight(uint8_t index, bool depart);
    bool flightPoint(const Actor& actor, float x, float y) const;
    void advanceFlight(uint8_t index, uint32_t dt);
    bool claimEpisode(uint8_t index);
    void start(Actor& actor, Activity activity, uint32_t durationMs);
    void rest(uint8_t index, bool endEpisode);
    void returnToOrigin(uint8_t index);
    void advance(uint8_t index, uint32_t dt, float playerX, float playerY, bool visible);
    Actor actors_[CAPACITY]{};
    Surface surfaces_[ExploreMapGenerator::CELL_COUNT]{};
    Cover cover_[ExploreMapGenerator::CELL_COUNT]{};
    bool solidFootprint_[ExploreMapGenerator::CELL_COUNT]{};
    Surface flightSurfaces_[ExploreMapGenerator::CELL_COUNT]{};
    uint8_t flightRegions_[ExploreMapGenerator::CELL_COUNT]{};
    uint8_t count_ = 0;
    uint8_t episodeOwner_ = CAPACITY;
    uint32_t lastUpdateMs_ = 0;
};

}  // namespace ExploreEcology
