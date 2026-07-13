#include "scenes/ExploreScene.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/BattleSystem.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
enum PickupId : uint8_t {
    PICKUP_NONE = 0,
    PICKUP_BALL,
    PICKUP_COIN,
    PICKUP_POTION,
    PICKUP_SUPER_POTION,
    PICKUP_ANTIDOTE,
    PICKUP_RARE_CANDY,
};

struct ExploreWeights {
    uint16_t none;
    uint16_t ball;
    uint16_t coin;
    uint16_t potion;
    uint16_t superPotion;
    uint16_t antidote;
    uint16_t candy;
    uint16_t encounter;
};

struct EncounterEntry {
    uint16_t speciesId;
    uint8_t weight;
    uint8_t minLevel;
    uint8_t maxLevel;
};

constexpr uint16_t eventWeightTotal(const ExploreWeights& weights) {
    return weights.none + weights.ball + weights.coin + weights.potion +
           weights.superPotion + weights.antidote + weights.candy + weights.encounter;
}

template <size_t N>
constexpr uint16_t encounterWeightTotal(const EncounterEntry (&entries)[N], size_t index = 0) {
    return index == N ? 0 : entries[index].weight + encounterWeightTotal(entries, index + 1);
}

static constexpr uint8_t WILD_LEVEL_MIN = 1;
static constexpr uint8_t WILD_LEVEL_MAX = 10;
static constexpr uint8_t WILD_LEVEL_COUNT = WILD_LEVEL_MAX - WILD_LEVEL_MIN + 1;
static constexpr uint8_t WILD_LEVEL_WEIGHTS[WILD_LEVEL_COUNT] = {
    2, 3, 4, 5, 6, 5, 4, 3, 2, 1,
};

static constexpr Game::ItemId CAPTURE_BALLS[] = {
    Game::ItemId::POKE_BALL,
    Game::ItemId::GREAT_BALL,
    Game::ItemId::HEAVY_BALL,
    Game::ItemId::TIMER_BALL,
};
static constexpr uint8_t CAPTURE_BALL_COUNT = sizeof(CAPTURE_BALLS) / sizeof(CAPTURE_BALLS[0]);
static constexpr uint32_t CAPTURE_ANIMATION_MS = 2200;
static constexpr uint32_t EXP_ANIMATION_MS = 900;
static constexpr uint8_t EXPLORE_MAP_TILES_W = 16;
static constexpr uint8_t EXPLORE_MAP_TILES_H = 12;
static constexpr uint16_t EXPLORE_TILE_SIZE = 26;
static constexpr uint16_t EXPLORE_MAP_W = EXPLORE_MAP_TILES_W * EXPLORE_TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_H = EXPLORE_MAP_TILES_H * EXPLORE_TILE_SIZE;
static constexpr uint32_t ROUTE_MOVE_MS = 220;

struct RouteMap {
    const char* name;
    const char* description;
    GameAssets::Kind battleBackground;
    ExploreWeights eventWeights;
    const EncounterEntry* encounters;
    uint8_t encounterCount;
    uint16_t fieldColor;
    uint16_t accentColor;
};

static constexpr EncounterEntry GRASS_PATH_ENCOUNTERS[] = {
    {16, 29, 1, 10},   // Pidgey
    {161, 27, 1, 10},  // Sentret
    {10, 18, 1, 10},   // Caterpie
    {261, 15, 1, 10},  // Poochyena
    {172, 7, 1, 10},   // Pichu
    {133, 3, 4, 8},    // Eevee
    {4, 1, 3, 8},      // Charmander
};

static constexpr EncounterEntry CREEK_SLOPE_ENCOUNTERS[] = {
    {16, 20, 1, 10},   // Pidgey
    {161, 18, 1, 10},  // Sentret
    {261, 16, 1, 10},  // Poochyena
    {278, 13, 1, 10},  // Wingull
    {74, 18, 1, 10},   // Geodude
    {129, 8, 1, 10},   // Magikarp
    {172, 4, 1, 10},   // Pichu
    {133, 2, 4, 8},    // Eevee
    {7, 1, 3, 8},      // Squirtle
};

static constexpr EncounterEntry TALL_GRASS_PARK_ENCOUNTERS[] = {
    {161, 23, 1, 10},  // Sentret
    {16, 20, 1, 10},   // Pidgey
    {10, 20, 1, 10},   // Caterpie
    {172, 15, 1, 10},  // Pichu
    {261, 12, 1, 10},  // Poochyena
    {133, 5, 4, 8},    // Eevee
    {123, 3, 6, 10},   // Scyther
    {278, 1, 1, 10},   // Wingull
    {1, 1, 3, 8},      // Bulbasaur
};

static constexpr EncounterEntry LAKESIDE_CAUSEWAY_ENCOUNTERS[] = {
    {129, 42, 1, 10},  // Magikarp
    {278, 34, 1, 10},  // Wingull
    {16, 8, 1, 10},    // Pidgey
    {161, 5, 1, 10},   // Sentret
    {172, 4, 1, 10},   // Pichu
    {261, 3, 1, 10},   // Poochyena
    {133, 2, 4, 8},    // Eevee
    {147, 2, 5, 10},   // Dratini
};

static constexpr EncounterEntry MIST_FOREST_PATH_ENCOUNTERS[] = {
    {261, 40, 1, 10},  // Poochyena
    {92, 32, 1, 10},   // Gastly
    {161, 10, 1, 10},  // Sentret
    {172, 7, 1, 10},   // Pichu
    {133, 5, 4, 8},    // Eevee
    {123, 4, 6, 10},   // Scyther
    {16, 2, 1, 10},    // Pidgey
};

static constexpr EncounterEntry ANCIENT_WATERFALL_VALLEY_ENCOUNTERS[] = {
    {92, 29, 1, 10},   // Gastly
    {261, 20, 1, 10},  // Poochyena
    {74, 20, 1, 10},   // Geodude
    {172, 8, 1, 10},   // Pichu
    {278, 6, 1, 10},   // Wingull
    {129, 5, 1, 10},   // Magikarp
    {133, 5, 4, 8},    // Eevee
    {123, 4, 6, 10},   // Scyther
    {147, 2, 5, 10},   // Dratini
    {16, 1, 1, 10},    // Pidgey
};

static_assert(encounterWeightTotal(GRASS_PATH_ENCOUNTERS) == 100,
              "grass path encounter weights must sum to 100");
static_assert(encounterWeightTotal(CREEK_SLOPE_ENCOUNTERS) == 100,
              "creek slope encounter weights must sum to 100");
static_assert(encounterWeightTotal(TALL_GRASS_PARK_ENCOUNTERS) == 100,
              "tall grass park encounter weights must sum to 100");
static_assert(encounterWeightTotal(LAKESIDE_CAUSEWAY_ENCOUNTERS) == 100,
              "lakeside causeway encounter weights must sum to 100");
static_assert(encounterWeightTotal(MIST_FOREST_PATH_ENCOUNTERS) == 100,
              "mist forest path encounter weights must sum to 100");
static_assert(encounterWeightTotal(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS) == 100,
              "ancient waterfall valley encounter weights must sum to 100");

#define ENTRY_COUNT(entriesValue) \
    static_cast<uint8_t>(sizeof(entriesValue) / sizeof(entriesValue[0]))

static constexpr RouteMap ROUTE_MAPS[] = {
    {
        Ui::Explore::GRASS_PATH,
        Ui::Explore::AREA_DESCS[0],
        GameAssets::Kind::BATTLE_BG_GRASS,
        {2800, 1800, 1800, 1200, 300, 600, 300, 1200},
        GRASS_PATH_ENCOUNTERS,
        ENTRY_COUNT(GRASS_PATH_ENCOUNTERS),
        0x2227,
        0x5EEE,
    },
    {
        Ui::Explore::CREEK_SLOPE,
        Ui::Explore::AREA_DESCS[1],
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        {2900, 900, 1500, 1500, 600, 700, 300, 1600},
        CREEK_SLOPE_ENCOUNTERS,
        ENTRY_COUNT(CREEK_SLOPE_ENCOUNTERS),
        0x224A,
        0x4D38,
    },
    {
        Ui::Explore::TALL_GRASS_PARK,
        Ui::Explore::AREA_DESCS[2],
        GameAssets::Kind::BATTLE_BG_GRASS,
        {3000, 1300, 1200, 900, 500, 500, 400, 2200},
        TALL_GRASS_PARK_ENCOUNTERS,
        ENTRY_COUNT(TALL_GRASS_PARK_ENCOUNTERS),
        0x2A66,
        0x8EA9,
    },
    {
        Ui::Explore::LAKESIDE_CAUSEWAY,
        Ui::Explore::AREA_DESCS[3],
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        {3000, 700, 1200, 1200, 600, 500, 200, 2600},
        LAKESIDE_CAUSEWAY_ENCOUNTERS,
        ENTRY_COUNT(LAKESIDE_CAUSEWAY_ENCOUNTERS),
        0x1A0B,
        0x4D5B,
    },
    {
        Ui::Explore::MIST_FOREST_PATH,
        Ui::Explore::AREA_DESCS[4],
        GameAssets::Kind::BATTLE_BG_DEEP_FOREST,
        {3100, 500, 700, 700, 700, 700, 200, 3400},
        MIST_FOREST_PATH_ENCOUNTERS,
        ENTRY_COUNT(MIST_FOREST_PATH_ENCOUNTERS),
        0x1945,
        0x644D,
    },
    {
        Ui::Explore::ANCIENT_WATERFALL_VALLEY,
        Ui::Explore::AREA_DESCS[5],
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        {3200, 400, 600, 500, 700, 500, 300, 3800},
        ANCIENT_WATERFALL_VALLEY_ENCOUNTERS,
        ENTRY_COUNT(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS),
        0x1987,
        0x54F7,
    },
};
#undef ENTRY_COUNT

static constexpr uint8_t ROUTE_MAP_COUNT = sizeof(ROUTE_MAPS) / sizeof(ROUTE_MAPS[0]);
static_assert(ROUTE_MAP_COUNT <= 8, "usedMapMask supports at most eight maps");
static_assert(ROUTE_MAP_COUNT == Ui::Explore::AREA_COUNT,
              "explore area strings and route maps must stay aligned");
static_assert(eventWeightTotal(ROUTE_MAPS[0].eventWeights) == 10000,
              "grass path event weights must sum to 10000");
static_assert(eventWeightTotal(ROUTE_MAPS[1].eventWeights) == 10000,
              "creek slope event weights must sum to 10000");
static_assert(eventWeightTotal(ROUTE_MAPS[2].eventWeights) == 10000,
              "tall grass park event weights must sum to 10000");
static_assert(eventWeightTotal(ROUTE_MAPS[3].eventWeights) == 10000,
              "lakeside causeway event weights must sum to 10000");
static_assert(eventWeightTotal(ROUTE_MAPS[4].eventWeights) == 10000,
              "mist forest path event weights must sum to 10000");
static_assert(eventWeightTotal(ROUTE_MAPS[5].eventWeights) == 10000,
              "ancient waterfall valley event weights must sum to 10000");

float routeWorldCoordinate(uint8_t tile) {
    return tile * EXPLORE_TILE_SIZE + EXPLORE_TILE_SIZE * 0.5f;
}

const RouteMap& routeMap(uint8_t index) {
    return ROUTE_MAPS[index < ROUTE_MAP_COUNT ? index : 0];
}

PokemonSprites::WalkDirection inwardDirection(ExploreMapGenerator::Edge edge) {
    switch (edge) {
    case ExploreMapGenerator::Edge::TOP: return PokemonSprites::WalkDirection::DOWN;
    case ExploreMapGenerator::Edge::RIGHT: return PokemonSprites::WalkDirection::LEFT;
    case ExploreMapGenerator::Edge::BOTTOM: return PokemonSprites::WalkDirection::UP;
    case ExploreMapGenerator::Edge::LEFT: return PokemonSprites::WalkDirection::RIGHT;
    }
    return PokemonSprites::WalkDirection::DOWN;
}

void drawGeneratedTileFallback(uint16_t tileId, int x, int y, uint8_t layer,
                               uint16_t fieldColor) {
    auto& canvas = PixelRenderer::canvas();
    if (layer == 0) {
        uint16_t color = fieldColor;
        if (ExploreMapGenerator::isWaterTile(tileId)) color = PixelRenderer::rgb(40, 105, 173);
        else if (ExploreMapGenerator::isRoadTile(tileId)) color = PixelRenderer::rgb(232, 211, 135);
        else if (tileId == 390) color = PixelRenderer::rgb(55, 139, 75);
        else if (ExploreMapGenerator::isForestTile(tileId)) color = PixelRenderer::rgb(29, 91, 51);
        canvas.fillRect(x, y, EXPLORE_TILE_SIZE, EXPLORE_TILE_SIZE, color);
        return;
    }
    if (ExploreMapGenerator::isForestTile(tileId)) {
        canvas.fillRect(x + 3, y + 3, EXPLORE_TILE_SIZE - 6, EXPLORE_TILE_SIZE - 3,
                        PixelRenderer::rgb(24, 73, 42));
    }
}

void drawGeneratedMapViewport(const ExploreMapGenerator::Map& generated,
                              int cameraX, int cameraY, uint16_t fieldColor) {
    int firstX = max(0, cameraX / static_cast<int>(EXPLORE_TILE_SIZE));
    int firstY = max(0, cameraY / static_cast<int>(EXPLORE_TILE_SIZE));
    int lastX = min<int>(ExploreMapGenerator::WIDTH - 1,
                         (cameraX + Hal::DISPLAY_W - 1) / EXPLORE_TILE_SIZE);
    int lastY = min<int>(ExploreMapGenerator::HEIGHT - 1,
                         (cameraY + Hal::DISPLAY_H - 1) / EXPLORE_TILE_SIZE);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                uint16_t tileId = generated.layers[layer][tileY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0) continue;
                int x = tileX * EXPLORE_TILE_SIZE - cameraX;
                int y = tileY * EXPLORE_TILE_SIZE - cameraY;
                if (!GameAssets::drawExploreTile(tileId, x, y)) {
                    drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
}

const char* captureBallName(uint8_t index) {
    return index < CAPTURE_BALL_COUNT ? Ui::Bag::NAMES[index] : Ui::BACK;
}

uint16_t baseStatTotal(const Species& species) {
    return species.stats.hp + species.stats.atk + species.stats.def +
           species.stats.spa + species.stats.spd + species.stats.spe;
}

const EncounterEntry& rollEncounterEntry(const RouteMap& map) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < map.encounterCount; ++i) total += map.encounters[i].weight;

    uint16_t roll = random(0, total);
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        if (roll < map.encounters[i].weight) return map.encounters[i];
        roll -= map.encounters[i].weight;
    }
    return map.encounters[0];
}

uint8_t rollWildLevel(uint8_t minLevel, uint8_t maxLevel) {
    if (minLevel < WILD_LEVEL_MIN) minLevel = WILD_LEVEL_MIN;
    if (maxLevel > WILD_LEVEL_MAX) maxLevel = WILD_LEVEL_MAX;
    if (maxLevel < minLevel) maxLevel = minLevel;

    uint16_t total = 0;
    for (uint8_t level = minLevel; level <= maxLevel; ++level) {
        total += WILD_LEVEL_WEIGHTS[level - WILD_LEVEL_MIN];
    }

    uint16_t r = random(0, total);
    for (uint8_t level = minLevel; level <= maxLevel; ++level) {
        uint8_t weight = WILD_LEVEL_WEIGHTS[level - WILD_LEVEL_MIN];
        if (r < weight) return level;
        r -= weight;
    }
    return minLevel;
}
}

void ExploreScene::onEnter() {
    phase = Phase::SELECT;
    areaCursor = 0;
    resultMessage = nullptr;
    exitAfterFaint = false;
    captureMenuOpen = false;
    captureAnimationActive = false;
    endConfirmOpen = false;
    endConfirmOpenedAt = 0;
    if (GameEngine::ins().exploreTravelPhase() == ExploreTravelPhase::DEPARTING) {
        uint8_t area = GameEngine::ins().pendingExploreArea();
        if (area >= static_cast<uint8_t>(Area::COUNT)) area = 0;
        activeArea = static_cast<Area>(area);
        areaCursor = area;
        resetWalk();
        GameEngine::ins().markExploreActive();
    }
}

void ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    updateRouteMovement(nowMs);
    updateExpAnimation(nowMs);
    serviceBattleLog(nowMs);
    updateCaptureAnimation(nowMs);
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    if ((phase == Phase::LEARN_MOVE || phase == Phase::ENCOUNTER) &&
        event.action == BtnAction::LONG_PRESS) {
        return true;
    }

    if (phase == Phase::WALKING && endConfirmOpen) {
        if (event.action == BtnAction::PRESSED && event.btn == 0) {
            if (endConfirmCursor == 0) {
                if (routeMoving) {
                    routeMoveStarted += Hal::ins().millis() - endConfirmOpenedAt;
                }
                endConfirmOpen = false;
                endConfirmOpenedAt = 0;
            } else {
                requestExploreExit();
            }
            return true;
        }
        if (event.action == BtnAction::PRESSED && event.btn == 1) {
            endConfirmCursor = (endConfirmCursor + 1) % 2;
            return true;
        }
        return event.action == BtnAction::LONG_PRESS;
    }

    if (phase == Phase::WALKING && event.btn == 1 &&
        (event.action == BtnAction::PRESSED || event.action == BtnAction::LONG_PRESS)) {
        endConfirmOpen = true;
        endConfirmCursor = 0;
        endConfirmOpenedAt = Hal::ins().millis();
        return true;
    }

    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        requestExploreExit();
        return true;
    }

    if (event.action != BtnAction::PRESSED) return false;

    if (phase == Phase::SELECT) {
        uint8_t optionCount = static_cast<uint8_t>(Area::COUNT) + 1;
        if (event.btn == 0) {
            if (areaCursor >= static_cast<uint8_t>(Area::COUNT)) {
                GameEngine::ins().requestScene(SceneID::MENU);
            } else {
                activeArea = static_cast<Area>(areaCursor);
                GameEngine::ins().beginExploreDeparture(areaCursor);
            }
            return true;
        }
        if (event.btn == 1) {
            areaCursor = (areaCursor + 1) % optionCount;
            return true;
        }
    }

    if (phase == Phase::WALKING) {
        if (event.btn == 0) {
            walk();
            return true;
        }
    }

    if (phase == Phase::ENCOUNTER) {
        if (captureAnimationActive) return true;
        if (battleLogBusy()) return true;
        if (captureMenuOpen) {
            if (event.btn == 0) {
                if (captureCursor >= CAPTURE_BALL_COUNT) {
                    captureMenuOpen = false;
                } else {
                    tryCapture(CAPTURE_BALLS[captureCursor]);
                }
                return true;
            }
            if (event.btn == 1) {
                captureCursor = (captureCursor + 1) % (CAPTURE_BALL_COUNT + 1);
                return true;
            }
        }
        if (event.btn == 0) {
            if (battleCursor == 0) attackWild();
            else if (battleCursor == 1) openCaptureMenu();
            else fleeEncounter();
            if (exitAfterFaint) {
                requestExploreExit(true);
                return true;
            }
            return true;
        }
        if (event.btn == 1) {
            battleCursor = (battleCursor + 1) % 3;
            return true;
        }
    }

    if (phase == Phase::LEARN_MOVE) {
        if (event.btn == 0) {
            GameEngine::ins().resolvePendingMoveLearn(learnCursor == 0);
            phase = learnReturnPhase;
            return true;
        }
        if (event.btn == 1) {
            learnCursor = (learnCursor + 1) % 2;
            return true;
        }
    }

    if (phase == Phase::RESULT) {
        if (event.btn == 0) {
            const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
            if (routeIndex + 1 >= path.pointCount) resetWalk();
            else resumeWalk();
            return true;
        }
        if (event.btn == 1) {
            requestExploreExit();
            return true;
        }
    }

    return false;
}

void ExploreScene::requestExploreExit(bool fainted) {
    if (phase == Phase::SELECT &&
        GameEngine::ins().exploreTravelPhase() != ExploreTravelPhase::ACTIVE) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return;
    }
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    GameEngine::ins().beginExploreReturn(fainted || mon.fainted || mon.hpCur == 0);
}

void ExploreScene::walk() {
    if (routeMoving) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routeIndex + 1 >= path.pointCount) {
        if (currentMapBlock + 1 < mapBlockCount) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
        } else {
            phase = Phase::RESULT;
            resultMessage = Ui::Explore::EXIT_REACHED;
        }
        return;
    }

    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    ++routeIndex;
    ExploreMapGenerator::Point target = path.points[routeIndex];
    routeTargetX = routeWorldCoordinate(target.x);
    routeTargetY = routeWorldCoordinate(target.y);
    float dx = routeTargetX - routeFromX;
    float dy = routeTargetY - routeFromY;
    if (fabsf(dx) >= fabsf(dy)) {
        routeWalkDirection = static_cast<uint8_t>(
            dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT : PokemonSprites::WalkDirection::LEFT);
    } else {
        routeWalkDirection = static_cast<uint8_t>(
            dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN : PokemonSprites::WalkDirection::UP);
    }
    routeMoveStarted = Hal::ins().millis();
    routeMoving = true;
    ++steps;
    GameEngine::ins().addWalkSteps(1);
}

void ExploreScene::updateRouteMovement(uint32_t nowMs) {
    if (!routeMoving || endConfirmOpen) return;
    uint32_t elapsed = nowMs - routeMoveStarted;
    float progress = min(1.0f, elapsed / static_cast<float>(ROUTE_MOVE_MS));
    float eased = progress * progress * (3.0f - 2.0f * progress);
    routeWorldX = routeFromX + (routeTargetX - routeFromX) * eased;
    routeWorldY = routeFromY + (routeTargetY - routeFromY) * eased;
    if (progress < 1.0f) return;

    routeWorldX = routeTargetX;
    routeWorldY = routeTargetY;
    routeMoving = false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routeIndex + 1 >= path.pointCount) {
        if (currentMapBlock + 1 < mapBlockCount) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
        } else {
            phase = Phase::RESULT;
            resultMessage = Ui::Explore::EXIT_REACHED;
        }
        return;
    }
    if (endConfirmOpen) return;
    if (enterPendingMoveLearn(Phase::WALKING)) return;
    rollSceneEvent(false);
}

bool ExploreScene::rollSceneEvent(bool forceEvent) {
    uint32_t r = random(0, 10000);
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    const ExploreWeights& weights = map.eventWeights;
    uint32_t cursor = weights.none;
    if (!forceEvent && r < cursor) return false;

    uint8_t pickup = PICKUP_NONE;
    if (forceEvent && r < cursor) r = cursor;
    cursor += weights.ball;
    if (r < cursor) pickup = PICKUP_BALL;
    else {
        cursor += weights.coin;
        if (r < cursor) pickup = PICKUP_COIN;
        else {
            cursor += weights.potion;
            if (r < cursor) pickup = PICKUP_POTION;
            else {
                cursor += weights.superPotion;
                if (r < cursor) pickup = PICKUP_SUPER_POTION;
                else {
                    cursor += weights.antidote;
                    if (r < cursor) pickup = PICKUP_ANTIDOTE;
                    else {
                        cursor += weights.candy;
                        if (r < cursor && GameEngine::ins().gameState().stepsToday >= 5000) pickup = PICKUP_RARE_CANDY;
                        else {
                            cursor += weights.encounter;
                            if (r < cursor) {
                                rollEncounter();
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (pickup == PICKUP_NONE) return false;
    resolvePickup(pickup);
    return true;
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    switch (pickupId) {
    case PICKUP_BALL:
        GameEngine::ins().addBalls(1);
        break;
    case PICKUP_COIN: {
        uint8_t level = GameEngine::ins().activeMonster().level;
        uint32_t upper = 10 + min<uint8_t>(40, level);
        uint32_t coins = random(10, upper + 1);
        GameEngine::ins().addCoins(coins);
        return;
    }
    case PICKUP_POTION:
        GameEngine::ins().addPotion(1);
        break;
    case PICKUP_SUPER_POTION:
        GameEngine::ins().addSuperPotion(1);
        break;
    case PICKUP_ANTIDOTE:
        GameEngine::ins().addAntidote(1);
        break;
    case PICKUP_RARE_CANDY:
        GameEngine::ins().addCandy(1);
        break;
    default:
        return;
    }
}

void ExploreScene::rollEncounter() {
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    const EncounterEntry& encounter = rollEncounterEntry(map);
    wild = findSpecies(encounter.speciesId);
    if (!wild) wild = &speciesTable()[0];
    uint8_t wildLevel = rollWildLevel(encounter.minLevel, encounter.maxLevel);
    wildRuntime = GameEngine::ins().createMonster(wild->id, wildLevel);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    battleCursor = 0;
    battleTurns = 0;
    captureMenuOpen = false;
    captureAnimationActive = false;
    fleeAttempts = 0;
    phase = Phase::ENCOUNTER;
    clearBattleLogs();
    enqueueBattleLog(Ui::Explore::WILD_APPEARED);
}

void ExploreScene::clearBattleLogs() {
    battleLogHead = 0;
    battleLogCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleResultPending = false;
    expAnimationPending = false;
    expAnimationActive = false;
    expAnimationFrom = 0;
    expAnimationTo = 0;
    expAnimationStarted = 0;
    for (uint8_t i = 0; i < BATTLE_LOG_QUEUE_CAP; ++i) {
        battleLogCues[i] = BattleLogCue::NONE;
    }
    for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
        battleLogVisible[i][0] = '\0';
    }
}

void ExploreScene::enqueueBattleLog(const char* text, BattleLogCue cue) {
    if (!text || !text[0]) return;
    if (battleLogCount >= BATTLE_LOG_QUEUE_CAP) {
        if (battleLogCues[battleLogHead] == BattleLogCue::EXP_GAIN) {
            expAnimationPending = false;
        }
        battleLogCues[battleLogHead] = BattleLogCue::NONE;
        battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
        battleLogCount--;
    }
    uint8_t tail = (battleLogHead + battleLogCount) % BATTLE_LOG_QUEUE_CAP;
    strncpy(battleLogQueue[tail], text, BATTLE_LOG_LEN - 1);
    battleLogQueue[tail][BATTLE_LOG_LEN - 1] = '\0';
    battleLogCues[tail] = cue;
    battleLogCount++;
    serviceBattleLog(Hal::ins().millis());
}

void ExploreScene::serviceBattleLog(uint32_t nowMs) {
    if (battleLogActive && (int32_t)(nowMs - battleLogUntil) < 0) return;
    if (battleLogCount == 0) {
        battleLogActive = false;
        battleLogVisibleCount = 0;
        for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            battleLogVisible[i][0] = '\0';
        }
        if (battleResultPending) {
            battleResultPending = false;
            if (!enterPendingMoveLearn(Phase::RESULT)) phase = Phase::RESULT;
        }
        return;
    }

    if (battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        battleLogVisibleCount++;
    } else {
        strncpy(battleLogVisible[0], battleLogVisible[1], BATTLE_LOG_LEN - 1);
        battleLogVisible[0][BATTLE_LOG_LEN - 1] = '\0';
    }
    uint8_t line = battleLogVisibleCount - 1;
    uint8_t queueIndex = battleLogHead;
    strncpy(battleLogVisible[line], battleLogQueue[queueIndex], BATTLE_LOG_LEN - 1);
    battleLogVisible[line][BATTLE_LOG_LEN - 1] = '\0';
    BattleLogCue cue = battleLogCues[queueIndex];
    battleLogCues[queueIndex] = BattleLogCue::NONE;
    battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
    battleLogCount--;
    if (cue == BattleLogCue::EXP_GAIN) startExpAnimation(nowMs);
    battleLogActive = true;
    battleLogUntil = nowMs + 1000;
}

bool ExploreScene::battleLogBusy() const {
    return captureAnimationActive || battleLogActive || battleLogCount > 0 ||
           battleResultPending || expAnimationPending || expAnimationActive;
}

void ExploreScene::prepareExpAnimation(uint32_t fromExp, uint32_t toExp) {
    expAnimationFrom = fromExp;
    expAnimationTo = toExp;
    expAnimationStarted = 0;
    expAnimationActive = false;
    expAnimationPending = toExp > fromExp;
}

void ExploreScene::startExpAnimation(uint32_t nowMs) {
    if (!expAnimationPending) return;
    expAnimationPending = false;
    expAnimationActive = true;
    expAnimationStarted = nowMs;
}

void ExploreScene::updateExpAnimation(uint32_t nowMs) {
    if (!expAnimationActive || nowMs < expAnimationStarted) return;
    if (nowMs - expAnimationStarted >= EXP_ANIMATION_MS) {
        expAnimationActive = false;
    }
}

uint32_t ExploreScene::battleExpForRender(uint32_t nowMs) const {
    if (expAnimationPending) return expAnimationFrom;
    if (!expAnimationActive || nowMs <= expAnimationStarted) {
        return expAnimationActive ? expAnimationFrom : GameEngine::ins().activeMonster().exp;
    }
    uint32_t elapsed = min<uint32_t>(EXP_ANIMATION_MS, nowMs - expAnimationStarted);
    uint64_t distance = static_cast<uint64_t>(expAnimationTo - expAnimationFrom) * elapsed;
    return expAnimationFrom + static_cast<uint32_t>(distance / EXP_ANIMATION_MS);
}

bool ExploreScene::enterPendingMoveLearn(Phase returnPhase) {
    if (!GameEngine::ins().hasPendingMoveLearn()) return false;
    learnCursor = 0;
    learnReturnPhase = returnPhase;
    phase = Phase::LEARN_MOVE;
    return true;
}

void ExploreScene::attackWild() {
    if (!wild || wildHp == 0) return;
    auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }
    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    if (battleTurns < 255) battleTurns++;
    wildRuntime.hpCur = wildHp;
    uint8_t specialSlot = BattleSystem::rollSpecialMoveSlot(activeMon);
    auto result = BattleSystem::calcBasicDamage(activeMon, activeSpecies, wildRuntime, *wild, specialSlot);
    wildHp = result.damage >= wildHp ? 0 : wildHp - result.damage;
    wildRuntime.hpCur = wildHp;

    if (result.statusBlocked) {
        enqueueBattleLog(Ui::Explore::CANNOT_MOVE);
    } else if (result.effectiveness == 0) {
        const MoveInfo* move = findMove(result.special
            ? specialMoveIdForMonster(activeMon, result.specialSlot)
            : moveIdForMonster(activeSpecies, activeMon, false));
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
    } else {
        const MoveInfo* move = findMove(result.special
            ? specialMoveIdForMonster(activeMon, result.specialSlot)
            : moveIdForMonster(activeSpecies, activeMon, false));
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        if (result.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
        if (result.effectiveness > 100) enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
        else if (result.effectiveness < 100) enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::DAMAGE_FMT, result.damage);
        enqueueBattleLog(logBuf);
        if (result.damage > 0 && activeMon.proficiency < 100) {
            activeMon.proficiency++;
            GameEngine::ins().markDirty(false);
        }
    }

    if (wildHp == 0) {
        uint16_t expGain = max<uint16_t>(1, wildRuntime.level * 3);
        uint32_t expBefore = activeMon.exp;
        GameEngine::ins().addExperience(expGain);
        prepareExpAnimation(expBefore, GameEngine::ins().activeMonster().exp);
        GameEngine::ins().grantEffortFrom(*wild);
        GameEngine::ins().addCoins(10);
        enqueueBattleLog(Ui::Explore::BATTLE_WIN);
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::EXP_GAIN_FMT, expGain);
        enqueueBattleLog(logBuf, BattleLogCue::EXP_GAIN);
        uint8_t levelUp = 0;
        if (GameEngine::ins().consumePendingLevelUp(levelUp)) {
            snprintf(logBuf, sizeof(logBuf), Ui::Common::LEVEL_UP_FMT, levelUp);
            enqueueBattleLog(logBuf);
        }
        snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::BATTLE_WIN_FMT, expGain);
        resultMessage = resultBuf;
        battleResultPending = true;
        lastCaptureSuccess = false;
        return;
    }

    wildCounterattack();
}

void ExploreScene::wildCounterattack() {
    if (!wild || wildHp == 0) return;
    auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }

    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    uint8_t specialSlot = BattleSystem::rollSpecialMoveSlot(wildRuntime);
    auto result = BattleSystem::calcBasicDamage(wildRuntime, *wild, activeMon, activeSpecies, specialSlot);
    activeMon.hpCur = result.damage >= activeMon.hpCur ? 0 : activeMon.hpCur - result.damage;
    GameEngine::ins().markDirty(false);

    if (activeMon.hpCur == 0) {
        finishPlayerFaint();
        return;
    }

    if (result.statusBlocked) {
        enqueueBattleLog(Ui::Explore::WILD_CANNOT_MOVE);
    } else if (result.effectiveness == 0) {
        const MoveInfo* move = findMove(result.special
            ? specialMoveIdForMonster(wildRuntime, result.specialSlot)
            : moveIdForMonster(*wild, wildRuntime, false));
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
    } else {
        const MoveInfo* move = findMove(result.special
            ? specialMoveIdForMonster(wildRuntime, result.specialSlot)
            : moveIdForMonster(*wild, wildRuntime, false));
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
        enqueueBattleLog(logBuf);
        if (result.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
        if (result.effectiveness > 100) enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
        else if (result.effectiveness < 100) enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_DAMAGE_FMT, result.damage);
        enqueueBattleLog(logBuf);
    }
}

void ExploreScene::finishPlayerFaint() {
    uint32_t loss = GameEngine::ins().applyActiveFaintPenalty();
    snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, (unsigned long)loss);
    resultMessage = resultBuf;
    lastCaptureSuccess = false;
    wild = nullptr;
    wildHp = wildHpMax = 0;
    exitAfterFaint = true;
}

void ExploreScene::openCaptureMenu() {
    captureCursor = 0;
    while (captureCursor < CAPTURE_BALL_COUNT &&
           GameEngine::ins().itemCount(CAPTURE_BALLS[captureCursor]) == 0) {
        captureCursor++;
    }
    if (captureCursor >= CAPTURE_BALL_COUNT) {
        enqueueBattleLog(Ui::Explore::NO_BALLS);
        return;
    }
    captureMenuOpen = true;
}

void ExploreScene::tryCapture(Game::ItemId ball) {
    if (!wild) return;
    if (!GameEngine::ins().removeItem(ball)) {
        captureMenuOpen = false;
        enqueueBattleLog(Ui::Explore::NO_BALLS);
        return;
    }

    uint16_t chance = 35;
    if (wild->evolveTo == 0) chance = 25;
    if (wild->stats.hp < 45) chance += 20;
    uint8_t hpMissing = wildHpMax > 0 ? (uint8_t)((wildHpMax - wildHp) * 50 / wildHpMax) : 0;
    chance += hpMissing;

    uint16_t multiplier = 100;
    if (ball == Game::ItemId::GREAT_BALL) {
        multiplier = 150;
    } else if (ball == Game::ItemId::HEAVY_BALL) {
        uint16_t total = baseStatTotal(*wild);
        multiplier = total >= 450 ? 180 : (total >= 350 ? 140 : 100);
    } else if (ball == Game::ItemId::TIMER_BALL) {
        multiplier = min<uint16_t>(200, 100 + static_cast<uint16_t>(battleTurns) * 15);
    }
    chance = min<uint16_t>(95, chance * multiplier / 100);

    captureBall = ball;
    captureOutcome = random(0, 100) < chance;
    captureAnimationStarted = Hal::ins().millis();
    captureAnimationActive = true;
    captureMenuOpen = false;
    lastCaptureSuccess = false;
}

void ExploreScene::updateCaptureAnimation(uint32_t nowMs) {
    if (!captureAnimationActive) return;
    if (nowMs - captureAnimationStarted < CAPTURE_ANIMATION_MS) return;
    finishCaptureAnimation();
}

void ExploreScene::finishCaptureAnimation() {
    captureAnimationActive = false;
    if (captureOutcome) {
        wildRuntime.hpCur = wildHp;
        lastCaptureSuccess = GameEngine::ins().recordCapture(
            wildRuntime, mapBlocks[currentMapBlock]);
        resultMessage = lastCaptureSuccess && wild ? wild->name : Ui::Shop::BAG_FULL;
        phase = Phase::RESULT;
        return;
    }

    lastCaptureSuccess = false;
    enqueueBattleLog(Ui::Explore::BROKE_FREE);
    wildCounterattack();
}

void ExploreScene::fleeEncounter() {
    if (!wild) return;
    const auto& activeMon = GameEngine::ins().activeMonster();
    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    uint16_t activeSpeed = statFor(activeSpecies, activeMon, 5);
    uint16_t wildSpeed = statFor(*wild, wildRuntime, 5);
    bool escaped = wildSpeed == 0 || activeSpeed >= wildSpeed;
    if (!escaped) {
        fleeAttempts++;
        uint32_t odds = ((uint32_t)activeSpeed * 128) / wildSpeed + 30UL * fleeAttempts;
        escaped = odds >= 256 || random(0, 256) < odds;
    }

    if (!escaped) {
        enqueueBattleLog(Ui::Explore::FLEE_FAILED);
        wildCounterattack();
        return;
    }

    GameEngine::ins().addCoins(2);
    lastCaptureSuccess = false;
    phase = Phase::RESULT;
    resultMessage = Ui::Explore::RELEASED;
}

void ExploreScene::resetWalk() {
    phase = Phase::WALKING;
    generateMapBlocks();
    currentMapBlock = 0;
    steps = 0;
    resetRouteSegment();
    resumeWalk();
}

void ExploreScene::resumeWalk() {
    phase = Phase::WALKING;
    wild = nullptr;
    wildHp = wildHpMax = 0;
    battleResultPending = false;
    captureMenuOpen = false;
    captureAnimationActive = false;
    resultMessage = nullptr;
    endConfirmOpen = false;
    endConfirmOpenedAt = 0;
}

void ExploreScene::resetRouteSegment() {
    uint8_t mapIndex = mapBlocks[currentMapBlock];
    uint32_t mapSeed = ExploreMapGenerator::deriveSeed(
        expeditionSeed, currentMapBlock, mapIndex);
    if (!ExploreMapGenerator::generate(mapSeed, pendingEntryEdge, generatedMap)) {
        ExploreMapGenerator::generate(mapSeed ^ 0x6D2B79F5U, pendingEntryEdge, generatedMap);
    }
    Serial.printf("[ExploreMap] block=%u area=%u seed=%08lx fingerprint=%08lx entry=%u coast=%u forest=%u\n",
                  currentMapBlock,
                  mapIndex,
                  static_cast<unsigned long>(generatedMap.seed),
                  static_cast<unsigned long>(ExploreMapGenerator::fingerprint(generatedMap)),
                  static_cast<unsigned>(generatedMap.entry.edge),
                  generatedMap.hasCoast ? 1 : 0,
                  generatedMap.hasForest ? 1 : 0);
    prepareMapRoutes();
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    routeIndex = 0;
    routeMoving = false;
    routeWalkDirection = static_cast<uint8_t>(inwardDirection(generatedMap.entry.edge));
    mapTargetSteps = max<uint16_t>(1, path.pointCount - 1);
    ExploreMapGenerator::Point start = path.points[0];
    routeWorldX = routeWorldCoordinate(start.x);
    routeWorldY = routeWorldCoordinate(start.y);
    routeFromX = routeTargetX = routeWorldX;
    routeFromY = routeTargetY = routeWorldY;
}

void ExploreScene::generateMapBlocks() {
    mapBlockCount = random(2, MAP_BLOCK_CAP + 1);
    for (uint8_t i = 0; i < MAP_BLOCK_CAP; ++i) mapBlocks[i] = 0xFF;
    uint8_t firstMap = static_cast<uint8_t>(activeArea);
    if (firstMap >= ROUTE_MAP_COUNT) firstMap = 0;
    mapBlocks[0] = firstMap;
    expeditionSeed = static_cast<uint32_t>(random(1, 0x7FFFFFFF));
    pendingEntryEdge = static_cast<ExploreMapGenerator::Edge>((expeditionSeed >> 8) & 0x03);
    usedMapMask = static_cast<uint8_t>(1U << firstMap);
    currentRoutePath = 0;
    activeExitMask = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
}

void ExploreScene::prepareMapRoutes() {
    uint8_t pathCount = min<uint8_t>(MAP_EXIT_CAP, generatedMap.pathCount);
    activeExitMask = 0;
    currentRoutePath = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    if (pathCount == 0) return;

    uint8_t pathIds[MAP_EXIT_CAP] = {0, 1};
    for (uint8_t i = pathCount; i > 1; --i) {
        uint8_t swapIndex = random(0, i);
        uint8_t value = pathIds[i - 1];
        pathIds[i - 1] = pathIds[swapIndex];
        pathIds[swapIndex] = value;
    }

    if (currentMapBlock + 1 >= mapBlockCount) {
        for (uint8_t i = 0; i < pathCount; ++i) {
            activeExitMask = static_cast<uint8_t>(activeExitMask | (1U << pathIds[i]));
        }
        currentRoutePath = pathIds[random(0, pathCount)];
        return;
    }

    uint8_t available[ROUTE_MAP_COUNT];
    uint8_t availableCount = 0;
    for (uint8_t i = 0; i < ROUTE_MAP_COUNT; ++i) {
        if ((usedMapMask & (1U << i)) == 0) available[availableCount++] = i;
    }
    for (uint8_t i = availableCount; i > 1; --i) {
        uint8_t swapIndex = random(0, i);
        uint8_t value = available[i - 1];
        available[i - 1] = available[swapIndex];
        available[swapIndex] = value;
    }

    uint8_t linkCount = min<uint8_t>(pathCount, availableCount);
    for (uint8_t i = 0; i < linkCount; ++i) {
        uint8_t pathId = pathIds[i];
        activeExitMask = static_cast<uint8_t>(activeExitMask | (1U << pathId));
        exitNextMaps[pathId] = available[i];
    }
    if (linkCount > 0) currentRoutePath = pathIds[random(0, linkCount)];
}

void ExploreScene::advanceMapBlock(uint8_t nextMap) {
    if (currentMapBlock + 1 >= mapBlockCount) return;
    if (nextMap >= ROUTE_MAP_COUNT || (usedMapMask & (1U << nextMap)) != 0) {
        phase = Phase::RESULT;
        resultMessage = Ui::Explore::EXIT_REACHED;
        return;
    }
    pendingEntryEdge = ExploreMapGenerator::opposite(
        generatedMap.paths[currentRoutePath].exit.edge);
    mapBlocks[currentMapBlock + 1] = nextMap;
    usedMapMask = static_cast<uint8_t>(usedMapMask | (1U << nextMap));
    ++currentMapBlock;
    resetRouteSegment();
    phase = Phase::WALKING;
    resultMessage = nullptr;
}

void ExploreScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(11, 22, 27));

    switch (phase) {
    case Phase::SELECT: renderAreaMenu(); break;
    case Phase::WALKING: renderWalking(); break;
    case Phase::ENCOUNTER: renderEncounter(); break;
    case Phase::LEARN_MOVE: renderLearnMove(); break;
    case Phase::RESULT: renderResult(); break;
    }
}

void ExploreScene::renderAreaMenu() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);
    static constexpr uint8_t VISIBLE_ROWS = 5;
    static constexpr int ROW_STEP = 25;
    static constexpr int START_Y = 4;
    static constexpr int SEPARATOR_Y_OFFSET = 21;
    uint8_t count = static_cast<uint8_t>(Area::COUNT) + 1;
    uint8_t first = areaCursor >= VISIBLE_ROWS ? areaCursor - VISIBLE_ROWS + 1 : 0;
    if (first + VISIBLE_ROWS > count) first = count - VISIBLE_ROWS;

    for (uint8_t slot = 0; slot < VISIBLE_ROWS; ++slot) {
        uint8_t i = first + slot;
        int y = START_Y + slot * ROW_STEP;
        bool active = i == areaCursor;
        const char* name = i < ROUTE_MAP_COUNT ? routeMap(i).name : Ui::BACK;
        const char* description = i < ROUTE_MAP_COUNT
            ? routeMap(i).description
            : Ui::Explore::AREA_DESCS[Ui::Explore::AREA_COUNT];
        uint16_t color = active ? 0xFFE0 : 0xFFFF;
        if (active) c.fillRect(4, y, 4, 16, 0xFFE0);
        PixelRenderer::text(14, y, name, color, 1);
        PixelRenderer::text(112, y, description,
                            active ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF, 1);
        if (slot + 1 < VISIBLE_ROWS) {
            c.fillRect(4, y + SEPARATOR_Y_OFFSET, Hal::DISPLAY_W - 8, 1, 0x7BEF);
        }
    }
}

void ExploreScene::renderWalking() {
    auto& c = PixelRenderer::canvas();
    int cameraX = constrain(static_cast<int>(roundf(routeWorldX)) - Hal::DISPLAY_W / 2,
                            0, static_cast<int>(EXPLORE_MAP_W) - Hal::DISPLAY_W);
    int cameraY = constrain(static_cast<int>(roundf(routeWorldY)) - Hal::DISPLAY_H / 2,
                            0, static_cast<int>(EXPLORE_MAP_H) - Hal::DISPLAY_H);
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    drawGeneratedMapViewport(generatedMap, cameraX, cameraY, map.fieldColor);

    drawRouteEndpoints(cameraX, cameraY);

    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    const PokemonSprites::SpriteFrame* marker = nullptr;
    bool markerFlipX = false;
    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            activeSpecies.id,
            static_cast<PokemonSprites::WalkDirection>(routeWalkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (routeMoving) {
            uint32_t animationNow = endConfirmOpen ? endConfirmOpenedAt : Hal::ins().millis();
            uint32_t elapsed = min<uint32_t>(animationNow - routeMoveStarted, ROUTE_MOVE_MS - 1);
            uint8_t cycleFrames = animation.frameCount + 1;
            uint8_t cycleIndex = min<uint8_t>(
                cycleFrames - 1,
                static_cast<uint8_t>(elapsed * cycleFrames / ROUTE_MOVE_MS));
            frameIndex = cycleIndex < animation.frameCount ? cycleIndex : 0;
        }
        marker = PokemonSprites::findSpeciesSprite(
            activeSpecies.id,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        markerFlipX = animation.flipX;
    }
    if (!marker) {
        marker = PokemonSprites::findSpeciesSprite(activeSpecies.id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (marker) {
        float scale = 0.8f;
        int markerW = static_cast<int>(marker->width * scale);
        int markerH = static_cast<int>(marker->height * scale);
        int markerX = static_cast<int>(roundf(routeWorldX)) - cameraX - markerW / 2;
        int markerY = static_cast<int>(roundf(routeWorldY)) - cameraY - markerH / 2;
        c.fillEllipse(markerX + markerW / 2, markerY + markerH - 10,
                      10, 3,
                      PixelRenderer::rgb(68, 87, 74));
        PokemonSprites::drawFrameScaled(marker, markerX, markerY, scale, markerFlipX);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), Ui::Explore::STEP_FMT, routeIndex, mapTargetSteps);
    const uint16_t panel = PixelRenderer::rgb(20, 32, 31);
    c.fillRect(0, 0, Hal::DISPLAY_W, 18, panel);
    PixelRenderer::text(7, 4, map.name, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(164, 4, buf, PixelRenderer::rgb(220, 231, 214), 1);
    PixelRenderer::bar(80, 5, 76, 8, (routeIndex * 100) / mapTargetSteps,
                       map.accentColor, PixelRenderer::rgb(51, 61, 52));
    if (endConfirmOpen) renderEndConfirm();
}

void ExploreScene::drawRouteEndpoints(int cameraX, int cameraY) {
    auto& c = PixelRenderer::canvas();
    auto drawEndpoint = [&](const ExploreMapGenerator::Endpoint& endpoint, bool exitPoint) {
        int x = static_cast<int>(routeWorldCoordinate(endpoint.point.x)) - cameraX;
        int y = static_cast<int>(routeWorldCoordinate(endpoint.point.y)) - cameraY;
        int outwardX = 0;
        int outwardY = 0;
        switch (endpoint.edge) {
        case ExploreMapGenerator::Edge::TOP: outwardY = -1; break;
        case ExploreMapGenerator::Edge::RIGHT: outwardX = 1; break;
        case ExploreMapGenerator::Edge::BOTTOM: outwardY = 1; break;
        case ExploreMapGenerator::Edge::LEFT: outwardX = -1; break;
        }
        int markerX = x - outwardX * 18;
        int markerY = y - outwardY * 18;
        if (markerX < -20 || markerX >= Hal::DISPLAY_W + 20 ||
            markerY < 19 || markerY >= Hal::DISPLAY_H - 12) {
            return;
        }

        uint16_t color = exitPoint ? PixelRenderer::rgb(255, 190, 72)
                                   : PixelRenderer::rgb(92, 222, 112);
        int directionX = exitPoint ? outwardX : -outwardX;
        int directionY = exitPoint ? outwardY : -outwardY;
        if (directionX > 0) {
            c.fillTriangle(markerX + 8, markerY, markerX - 5, markerY - 6,
                           markerX - 5, markerY + 6, color);
        } else if (directionX < 0) {
            c.fillTriangle(markerX - 8, markerY, markerX + 5, markerY - 6,
                           markerX + 5, markerY + 6, color);
        } else if (directionY > 0) {
            c.fillTriangle(markerX, markerY + 8, markerX - 6, markerY - 5,
                           markerX + 6, markerY - 5, color);
        } else {
            c.fillTriangle(markerX, markerY - 8, markerX - 6, markerY + 5,
                           markerX + 6, markerY + 5, color);
        }
        PixelRenderer::text(constrain(markerX - 16, 2, Hal::DISPLAY_W - 36),
                            constrain(markerY + 9, 20, Hal::DISPLAY_H - 12),
                            exitPoint ? Ui::Explore::MAP_EXIT : Ui::Explore::MAP_ENTRY,
                            0xFFFF, 1);
    };

    drawEndpoint(generatedMap.entry, false);
    for (uint8_t i = 0; i < generatedMap.pathCount && i < MAP_EXIT_CAP; ++i) {
        if ((activeExitMask & (1U << i)) != 0) drawEndpoint(generatedMap.paths[i].exit, true);
    }
}

void ExploreScene::renderEndConfirm() {
    auto& c = PixelRenderer::canvas();
    const uint16_t background = PixelRenderer::rgb(31, 38, 44);
    const uint16_t border = PixelRenderer::rgb(226, 229, 218);
    const uint16_t active = PixelRenderer::rgb(255, 216, 72);
    const uint16_t inactive = PixelRenderer::rgb(164, 174, 178);
    c.fillRect(30, 34, 180, 68, background);
    c.drawRect(30, 34, 180, 68, border);
    PixelRenderer::text(62, 46, Ui::Explore::END_CONFIRM_TITLE, border, 1);
    if (endConfirmCursor == 0) c.fillRect(55, 76, 4, 14, active);
    else c.fillRect(137, 76, 4, 14, active);
    PixelRenderer::text(64, 76, Ui::Explore::KEEP_EXPLORING,
                        endConfirmCursor == 0 ? active : inactive, 1);
    PixelRenderer::text(146, 76, Ui::Explore::END_EXPLORING,
                        endConfirmCursor == 1 ? active : inactive, 1);
}

void ExploreScene::renderEncounter() {
    auto& c = PixelRenderer::canvas();
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    if (!GameAssets::drawBattleBackground(map.battleBackground)) {
        c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(197, 220, 192));
        c.fillRect(0, 78, Hal::DISPLAY_W, 24, PixelRenderer::rgb(228, 225, 190));
        c.fillEllipse(174, 77, 34, 8, PixelRenderer::rgb(155, 181, 142));
        c.fillEllipse(58, 101, 38, 9, PixelRenderer::rgb(171, 159, 128));
    }

    uint32_t captureElapsed = captureAnimationActive ? Hal::ins().millis() - captureAnimationStarted : 0;
    bool hideWild = captureAnimationActive && captureElapsed >= 560 &&
                    (captureOutcome || captureElapsed < 1780);
    if (wild && !hideWild) drawMonsterBlock(*wild, 174, 45);
    drawMonsterBlock(GameEngine::ins().activeSpecies(), 58, 70, true);
    if (captureAnimationActive) renderCaptureAnimation();
    renderBattleHud();
    renderCommandBox();
}

void ExploreScene::renderCaptureAnimation() {
    if (!captureAnimationActive) return;
    uint32_t elapsed = Hal::ins().millis() - captureAnimationStarted;
    int ballX = 174;
    int ballY = 69;
    GameAssets::Kind kind = GameAssets::ballFrameKind(captureBall, 0);

    if (elapsed < 520) {
        float progress = elapsed / 520.0f;
        ballX = static_cast<int>(58 + (174 - 58) * progress);
        ballY = static_cast<int>(75 + (42 - 75) * progress - 34.0f * 4.0f * progress * (1.0f - progress));
        kind = GameAssets::ballFrameKind(captureBall, (elapsed / 55) % 8);
    } else if (elapsed < 760) {
        ballY = 45;
        kind = GameAssets::ballOpenKind(captureBall);
        GameAssets::drawCentered(GameAssets::Kind::BALL_BURST_STAR, 174, 44, 1.0f);
    } else if (elapsed < 1780) {
        uint32_t shakeTime = elapsed - 760;
        int shake = static_cast<int>(sinf(shakeTime * 0.035f) * 5.0f);
        ballX += shake;
        kind = GameAssets::ballFrameKind(captureBall, (shakeTime / 90) % 8);
    } else if (!captureOutcome) {
        kind = GameAssets::ballOpenKind(captureBall);
        GameAssets::drawCentered(GameAssets::Kind::BALL_BURST_STAR, ballX, ballY - 12, 1.0f);
    } else {
        kind = GameAssets::ballFrameKind(captureBall, 0);
        float pulse = 1.0f + 0.125f * (1.0f + sinf((elapsed - 1780) * 0.035f));
        GameAssets::drawCentered(GameAssets::Kind::BALL_BURST_STAR, ballX - 18, ballY - 18, pulse);
        GameAssets::drawCentered(GameAssets::Kind::BALL_BURST_STAR, ballX + 18, ballY - 12, pulse);
    }

    if (!GameAssets::drawCentered(kind, ballX, ballY)) {
        auto& c = PixelRenderer::canvas();
        c.fillCircle(ballX, ballY, 8, PixelRenderer::rgb(239, 85, 85));
        c.drawFastHLine(ballX - 8, ballY, 16, 0xFFFF);
    }
}

void ExploreScene::renderLearnMove() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(18, 24, 32));
    c.fillRect(24, 26, 192, 86, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(24, 26, 192, 86, PixelRenderer::rgb(241, 242, 232));

    const MoveInfo* move = findMove(GameEngine::ins().pendingMoveLearnId());
    PixelRenderer::text(46, 38, Ui::Explore::LEARN_TITLE, PixelRenderer::rgb(255, 216, 72), 1);

    char line[64];
    snprintf(line, sizeof(line), Ui::Explore::LEARN_MOVE_FMT,
             move ? move->name : Ui::Status::MOVE_UNKNOWN);
    PixelRenderer::text(36, 58, line, PixelRenderer::rgb(241, 242, 232), 1);

    const auto& state = GameEngine::ins().gameState();
    uint8_t slot = GameEngine::ins().pendingMoveLearnSlot();
    const MoveInfo* oldMove = nullptr;
    bool fillsSecondSpecialSlot = false;
    if (slot < state.teamCount && slot < Game::TEAM_CAP) {
        const Game::MonsterRuntime& mon = state.team[slot];
        if (mon.move2Id != 0 && mon.move3Id == 0) {
            fillsSecondSpecialSlot = true;
        } else if (mon.move2Id != 0 && mon.move3Id != 0) {
            oldMove = findMove(mon.move3Id);
        }
    }
    if (oldMove) {
        snprintf(line, sizeof(line), Ui::Explore::LEARN_REPLACE_FMT, oldMove->name);
        PixelRenderer::text(48, 76, line, PixelRenderer::rgb(135, 214, 238), 1);
    } else {
        PixelRenderer::text(64, 76,
                            fillsSecondSpecialSlot ? Ui::Explore::LEARN_EMPTY_SLOT_2 : Ui::Explore::LEARN_EMPTY_SLOT,
                            PixelRenderer::rgb(135, 214, 238), 1);
    }

    uint16_t yesColor = learnCursor == 0 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = learnCursor == 1 ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(72, 96, Ui::Bag::YES, yesColor, 1);
    PixelRenderer::text(142, 96, Ui::Bag::NO, noColor, 1);
}

void ExploreScene::renderResult() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(42, 42, 156, 58, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(42, 42, 156, 58, PixelRenderer::rgb(95, 110, 126));
    PixelRenderer::text(58, 58, lastCaptureSuccess ? Ui::Explore::CAPTURE_SUCCESS : Ui::Explore::RESULT_END,
                        lastCaptureSuccess ? PixelRenderer::rgb(92, 222, 112) : PixelRenderer::rgb(241, 242, 232), 1);
    if (resultMessage && resultMessage[0]) {
        PixelRenderer::text(58, 78, resultMessage, PixelRenderer::rgb(255, 216, 72), 1);
    } else if (wild) {
        PixelRenderer::text(58, 78, wild->name, PixelRenderer::rgb(255, 216, 72), 1);
    }
}

void ExploreScene::drawWildBlock(int x, int y) {
    if (!wild) return;
    drawMonsterBlock(*wild, x, y);
}

void ExploreScene::drawMonsterBlock(const Species& species, int x, int y, bool back) {
    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 32, 25, 7, PixelRenderer::rgb(23, 27, 34));
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, back ? PokemonSprites::SpriteKind::BACK : PokemonSprites::SpriteKind::FRONT);
    if (frame) {
        uint8_t w = pgm_read_byte(&frame->width);
        uint8_t h = pgm_read_byte(&frame->height);
        PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2);
        return;
    }
    c.fillRect(x - 18, y - 22, 36, 40, species.colorA);
    c.fillRect(x - 11, y - 13, 22, 23, species.colorB);
    c.fillCircle(x - 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
    c.fillCircle(x + 6, y - 5, 2, PixelRenderer::rgb(24, 30, 38));
}

void ExploreScene::renderBattleHud() {
    auto& c = PixelRenderer::canvas();
    char buf[24];

    if (wild) {
        c.fillRect(6, 8, 88, 36, PixelRenderer::rgb(248, 248, 232));
        c.drawRect(6, 8, 88, 36, PixelRenderer::rgb(74, 91, 75));
        PixelRenderer::text(10, 10, wild->name, PixelRenderer::rgb(25, 31, 40), 1);
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, wildRuntime.level);
        PixelRenderer::text(62, 10, buf, PixelRenderer::rgb(25, 31, 40), 1);
        PixelRenderer::bar(14, 31, 70, 7, wildHpMax ? (wildHp * 100 / wildHpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    }

    const auto& active = GameEngine::ins().activeMonster();
    const Species& species = GameEngine::ins().activeSpecies();
    uint32_t shownExp = battleExpForRender(Hal::ins().millis());
    uint8_t shownLevel = levelForExp(species.growthRate, shownExp);
    uint32_t levelFloor = minimumExpForLevel(species.growthRate, shownLevel);
    uint32_t levelCeiling = shownLevel >= Game::LEVEL_MAX
        ? levelFloor
        : minimumExpForLevel(species.growthRate, shownLevel + 1);
    uint8_t expPercent = shownLevel >= Game::LEVEL_MAX
        ? 100
        : static_cast<uint8_t>(min<uint32_t>(
            100,
            (shownExp - levelFloor) * 100UL / max<uint32_t>(1, levelCeiling - levelFloor)));

    c.fillRect(124, 53, 112, 47, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(124, 53, 112, 47, PixelRenderer::rgb(74, 91, 75));
    PixelRenderer::text(130, 55, species.name, PixelRenderer::rgb(25, 31, 40), 1);
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, shownLevel);
    PixelRenderer::text(198, 55, buf, PixelRenderer::rgb(25, 31, 40), 1);
    PixelRenderer::bar(132, 73, 96, 7, active.hpMax ? (active.hpCur * 100 / active.hpMax) : 0,
                       PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
    snprintf(buf, sizeof(buf), Ui::Explore::HP_VALUE_FMT, active.hpCur, active.hpMax);
    int hpTextX = max<int>(132, 228 - static_cast<int>(strlen(buf)) * 8);
    PixelRenderer::text(hpTextX, 79, buf, PixelRenderer::rgb(25, 31, 40), 1);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(PixelRenderer::rgb(46, 102, 145));
    c.setTextDatum(top_left);
    c.drawString(Ui::Explore::EXP_LABEL, 132, 91);
    c.fillRect(152, 92, 76, 6, PixelRenderer::rgb(49, 68, 78));
    c.fillRect(153, 93, 74, 4, PixelRenderer::rgb(205, 214, 205));
    int expFill = 74 * expPercent / 100;
    if (expFill > 0) {
        c.fillRect(153, 93, expFill, 4, PixelRenderer::rgb(64, 166, 230));
    }
}

void ExploreScene::renderCommandBox() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 101, Hal::DISPLAY_W, 34, PixelRenderer::rgb(248, 248, 232));
    c.drawRect(0, 101, Hal::DISPLAY_W, 34, PixelRenderer::rgb(74, 91, 75));

    serviceBattleLog(Hal::ins().millis());
    if (phase != Phase::ENCOUNTER) return;
    if (captureAnimationActive) {
        PixelRenderer::text(82, 110, Ui::Explore::CAPTURING,
                            PixelRenderer::rgb(25, 31, 40), 1);
        return;
    }
    if (captureMenuOpen) {
        if (captureCursor >= CAPTURE_BALL_COUNT) {
            PixelRenderer::text(100, 104, Ui::BACK, PixelRenderer::rgb(25, 31, 40), 1);
        } else {
            char line[32];
            snprintf(line, sizeof(line), Ui::Explore::BALL_SELECT_FMT,
                     captureBallName(captureCursor),
                     GameEngine::ins().itemCount(CAPTURE_BALLS[captureCursor]));
            PixelRenderer::text(56, 104, line, PixelRenderer::rgb(25, 31, 40), 1);
        }
        PixelRenderer::text(60, 119, Ui::Explore::BALL_SELECT_HINT,
                            PixelRenderer::rgb(74, 91, 75), 1);
        return;
    }
    if (battleLogActive || battleLogVisibleCount > 0) {
        uint8_t start = battleLogVisibleCount > BATTLE_LOG_VISIBLE_CAP
            ? battleLogVisibleCount - BATTLE_LOG_VISIBLE_CAP
            : 0;
        for (uint8_t i = start; i < battleLogVisibleCount; ++i) {
            int y = 104 + (i - start) * 15;
            PixelRenderer::text(12, y, battleLogVisible[i], PixelRenderer::rgb(25, 31, 40), 1);
        }
        return;
    }

    static constexpr int xs[] = {30, 104, 178};
    static constexpr int ys[] = {110, 110, 110};
    static constexpr const char* items[] = {
        Ui::Explore::CMD_ATTACK,
        Ui::Explore::CMD_BAG,
        Ui::Explore::CMD_FLEE,
    };
    for (uint8_t i = 0; i < 3; ++i) {
        if (battleCursor == i) PixelRenderer::text(xs[i] - 14, ys[i], ">", PixelRenderer::rgb(25, 31, 40), 1);
        PixelRenderer::text(xs[i], ys[i], items[i], PixelRenderer::rgb(25, 31, 40), 1);
    }
}
