#include "scenes/ExploreScene.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "assets/GameAssets.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/AudioManager.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/ProgressionUi.h"
#include "core/UiStrings.h"
#include "game/BattleSystem.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreEncounters.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreItemProgression.h"
#include "game/ExplorePool.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreRunRules.h"
#include "game/FriendshipPity.h"
#include "game/FriendshipService.h"
#include "game/FriendshipSystem.h"
#include "game/GameRandom.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TutorialOverlay.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"

namespace {
enum PickupId : uint8_t {
    PICKUP_NONE = 0,
    PICKUP_COIN,
    PICKUP_POTION,
    PICKUP_SUPER_POTION,
    PICKUP_ANTIDOTE,
    PICKUP_RARE_CANDY,
    PICKUP_MAX_POTION,
    PICKUP_FULL_RESTORE,
    PICKUP_FULL_HEAL,
    PICKUP_REVIVE,
    PICKUP_MAX_REPEL,
    PICKUP_HONEY,
    PICKUP_NUGGET,
    PICKUP_BIG_PEARL,
    PICKUP_STAR_PIECE,
    PICKUP_HEART_SCALE,
};

// 分地图捡拾池：每张 RouteMap 挂一张 {pickupId, weight} 小表，
// 金币数量由 RouteMap 的 minCoin/maxCoin 区间决定（高级地图钱更多）。
struct PickupEntry {
    uint8_t pickupId;
    uint16_t weight;
};

using EncounterEntry = ExploreEncounters::Entry;

// C++11 constexpr 限制：用递归代替循环。
constexpr uint16_t pickupWeightTotal(const PickupEntry* table, uint8_t count) {
    return count == 0
        ? 0
        : table[0].weight + pickupWeightTotal(table + 1, count - 1);
}

constexpr bool pickupAvailable(uint8_t pickupId, uint16_t stepsToday) {
    return pickupId != PICKUP_RARE_CANDY || stepsToday >= 5000;
}

static constexpr uint8_t WILD_LEVEL_MIN = 1;
static constexpr uint8_t WILD_LEVEL_MAX = Game::LEVEL_MAX;
static constexpr uint8_t WILD_LEVEL_VARIANCE = 2;
static constexpr uint16_t DEPTH_MIDDLE_START_PERMILLE = 333;
static constexpr uint16_t DEPTH_DEEP_START_PERMILLE = 667;
static constexpr uint8_t ENCOUNTER_COOLDOWN_STEP_COUNT = 5;
static constexpr uint8_t MAX_ENCOUNTERS_PER_MAP = 2;
static constexpr uint16_t ICE_SLIDE_STEP_MS = 180;
static constexpr uint16_t MAP_PICKUP_CHANCE = 6500;
static constexpr uint32_t MAP_GENERATION_SAFE_SEED = 1;
static constexpr uint32_t MAP_GENERATION_RETRY_SALTS[] = {
    0,
    0x6D2B79F5U,
    0x9E3779B9U,
    0x85EBCA6BU,
};
static constexpr uint32_t AREA_PREVIEW_CYCLE_MS = 2800;
static constexpr uint32_t AREA_PREVIEW_MOVE_MS = 500;
static constexpr uint32_t AREA_PREVIEW_HOLD_MS =
    AREA_PREVIEW_CYCLE_MS - AREA_PREVIEW_MOVE_MS;
static constexpr float AREA_CURSOR_LERP = 0.5f;
static constexpr uint32_t PREVIEW_LOAD_AFTER_CURSOR_MS = 80;
static constexpr uint32_t PREVIEW_BACKGROUND_LOAD_INTERVAL_MS = 80;

float routeAirOffsetY(uint16_t speciesId, uint32_t nowMs, float scale) {
    const PokemonMotion::AirProfile air =
        PokemonMotion::airProfileForSpecies(speciesId);
    if (air.height <= 0.0f) return 0.0f;
    float phase = static_cast<float>((nowMs + speciesId * 97U) % 1600U) *
                  0.00392699f;
    return (air.height + sinf(phase) * air.bobAmplitude) * scale;
}

constexpr uint8_t cooldownAfterCompletedStep(uint8_t cooldown) {
    return cooldown > 0 ? cooldown - 1 : 0;
}

constexpr uint8_t cooldownAfterCompletedSteps(uint8_t cooldown, uint8_t steps) {
    return steps == 0
        ? cooldown
        : cooldownAfterCompletedSteps(cooldownAfterCompletedStep(cooldown), steps - 1);
}

constexpr bool encounterGateOpen(uint8_t cooldown, uint8_t encounterCount) {
    return cooldown == 0 && encounterCount < MAX_ENCOUNTERS_PER_MAP;
}

constexpr bool encounterGateAllows(uint8_t cooldown, uint8_t encounterCount,
                                   bool bypassGate) {
    return bypassGate || encounterGateOpen(cooldown, encounterCount);
}

constexpr bool repelAllowsEncounter(bool guaranteed,
                                    bool repelActiveThisStep) {
    return guaranteed || !repelActiveThisStep;
}

constexpr bool canScheduleGuaranteedEncounter(uint8_t pathPointCount, uint8_t cooldown) {
    return pathPointCount >= 3 && cooldown + 1 <= pathPointCount - 2;
}

constexpr uint8_t guaranteedEncounterIndex(uint8_t pathPointCount, uint8_t cooldown) {
    return pathPointCount * 2 / 3 < cooldown + 1
        ? cooldown + 1
        : (pathPointCount * 2 / 3 > pathPointCount - 2
            ? pathPointCount - 2
            : pathPointCount * 2 / 3);
}

static constexpr uint32_t EXP_ANIMATION_MS = 900;
static constexpr uint32_t BATTLE_HIT_DELAY_MS = 260;
static constexpr uint32_t BATTLE_HIT_SHAKE_MS = 280;
static constexpr uint32_t BATTLE_HP_DRAIN_MS = 420;
static constexpr uint32_t BATTLE_ACTION_MS =
    BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS + BATTLE_HP_DRAIN_MS;
static constexpr uint32_t BATTLE_SWITCH_PHASE_MS = 360;
static constexpr uint32_t FOOD_THROW_DURATION_MS = 700;
static constexpr int FOOD_THROW_START_X = 78;
static constexpr int FOOD_THROW_START_Y = 83;
static constexpr int FOOD_THROW_END_X = 166;
static constexpr int FOOD_THROW_END_Y = 48;
static constexpr int FOOD_THROW_ARC_HEIGHT = 34;
static constexpr int BATTLE_SWITCH_TRAVEL_X = 120;
static constexpr uint8_t EXPLORE_MAP_TILES_W = 16;
static constexpr uint8_t EXPLORE_MAP_TILES_H = 12;
static constexpr uint16_t EXPLORE_TILE_SIZE =
    ExploreRouteGeometry::TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_W = EXPLORE_MAP_TILES_W * EXPLORE_TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_H = EXPLORE_MAP_TILES_H * EXPLORE_TILE_SIZE;
static constexpr float ROUTE_FOLLOWER_START_OFFSET = EXPLORE_TILE_SIZE;
static constexpr uint16_t ROUTE_FOLLOWER_DELAY_MS = 120;
static constexpr uint8_t ROUTE_FOLLOWER_GAP_STEPS = 2;
static constexpr int ROUTE_PICKUP_VISUAL_OFFSET_Y = 6;
static constexpr float ROUTE_BOSS_PATROL_DISTANCE = 5.0f;
static constexpr uint32_t ROUTE_BOSS_PATROL_CYCLE_MS = 6000;
static constexpr uint32_t ROUTE_BOSS_PATROL_OUT_START_MS = 1600;
static constexpr uint32_t ROUTE_BOSS_PATROL_OUT_END_MS = 2300;
static constexpr uint32_t ROUTE_BOSS_PATROL_RETURN_START_MS = 3800;
static constexpr uint32_t ROUTE_BOSS_PATROL_RETURN_END_MS = 4500;
static constexpr float ROUTE_EXIT_MARGIN = EXPLORE_TILE_SIZE * 2.0f;
static constexpr uint8_t EXPLORE_HUD_ALPHA = 150;
static constexpr int BATTLE_ASCII_ADVANCE = 8;
static constexpr uint8_t BATTLE_FOOTER_ALPHA = 153;

constexpr uint16_t routeBossPatrolPhasePermille(uint32_t phase) {
    return phase < ROUTE_BOSS_PATROL_OUT_START_MS
        ? 0
        : (phase < ROUTE_BOSS_PATROL_OUT_END_MS
            ? static_cast<uint16_t>(
                (phase - ROUTE_BOSS_PATROL_OUT_START_MS) * 1000U /
                (ROUTE_BOSS_PATROL_OUT_END_MS - ROUTE_BOSS_PATROL_OUT_START_MS))
            : (phase < ROUTE_BOSS_PATROL_RETURN_START_MS
                ? 1000
                : (phase < ROUTE_BOSS_PATROL_RETURN_END_MS
                    ? static_cast<uint16_t>(
                        1000U -
                        (phase - ROUTE_BOSS_PATROL_RETURN_START_MS) * 1000U /
                        (ROUTE_BOSS_PATROL_RETURN_END_MS -
                         ROUTE_BOSS_PATROL_RETURN_START_MS))
                    : 0)));
}

constexpr uint16_t routeBossPatrolPermille(uint32_t nowMs) {
    return routeBossPatrolPhasePermille(nowMs % ROUTE_BOSS_PATROL_CYCLE_MS);
}

constexpr bool routeBossPatrolFacesOutward(uint32_t phase) {
    return phase >= ROUTE_BOSS_PATROL_OUT_START_MS &&
           phase < ROUTE_BOSS_PATROL_RETURN_START_MS;
}

static_assert(routeBossPatrolPermille(0) == 0 &&
                  routeBossPatrolPermille(ROUTE_BOSS_PATROL_OUT_END_MS) == 1000 &&
                  routeBossPatrolPermille(ROUTE_BOSS_PATROL_RETURN_END_MS) == 0,
              "route boss patrol must leave and return to its anchor");
static_assert(!routeBossPatrolFacesOutward(0) &&
                  routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_OUT_START_MS) &&
                  routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_OUT_END_MS) &&
                  !routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_RETURN_START_MS),
              "route boss must hold its last movement direction at each endpoint");

struct BattleLayout {
    static constexpr int PLAYER_NAME_Y = 66;

    static constexpr int WILD_X = 178;
    static constexpr int WILD_GROUND_Y = PLAYER_NAME_Y;
    static constexpr int WILD_MAX_W = 116;
    static constexpr int WILD_MAX_H = WILD_GROUND_Y;

    static constexpr int PLAYER_X = 52;
    static constexpr int PLAYER_GROUND_Y = 107;
    static constexpr int PLAYER_MAX_W = 105;
    static constexpr int PLAYER_MAX_H = 65;

    static constexpr int FOOTER_Y = 99;
    static constexpr int FOOTER_H = Hal::DISPLAY_H - FOOTER_Y;

    static constexpr int EXP_LABEL_X = 118;
    static constexpr int EXP_LABEL_VISUAL_H = 9;
    static constexpr int EXP_BAR_X = 149;
    static constexpr int EXP_BAR_Y = 85;
    static constexpr int EXP_BAR_W = 81;
    static constexpr int EXP_BAR_H = 6;
    static constexpr int EXP_LABEL_Y =
        EXP_BAR_Y + EXP_BAR_H / 2 - EXP_LABEL_VISUAL_H / 2;
};

static_assert(BattleLayout::EXP_LABEL_Y +
                  BattleLayout::EXP_LABEL_VISUAL_H / 2 ==
                  BattleLayout::EXP_BAR_Y + BattleLayout::EXP_BAR_H / 2,
              "battle EXP label and bar must share the same vertical center");

static_assert(BattleLayout::WILD_GROUND_Y - BattleLayout::WILD_MAX_H == 0,
              "wild sprite bounds must reach the top edge of the display");
static_assert(BattleLayout::PLAYER_X - BattleLayout::PLAYER_MAX_W / 2 == 0,
              "player sprite bounds must reach the left edge of the display");
static_assert(BattleLayout::PLAYER_GROUND_Y - BattleLayout::PLAYER_MAX_H == 42,
              "player sprite bounds must use the lowered top edge");

void drawBattleText(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::textOutlined(
        x, y, value, color, PixelRenderer::rgb(255, 255, 255), 1);
}

void drawBattleDarkText(int x, int y, const char* value) {
    drawBattleText(x, y, value, PixelRenderer::rgb(25, 31, 40));
}

void drawBattleFooterText(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::text(x, y, value, color, 1);
}

void drawBattleFooterDarkText(int x, int y, const char* value) {
    drawBattleFooterText(x, y, value, PixelRenderer::rgb(25, 31, 40));
}

void drawBattleAsciiRightAligned(int rightX, int y, const char* value) {
    drawBattleDarkText(rightX - static_cast<int>(strlen(value)) * BATTLE_ASCII_ADVANCE,
                       y, value);
}

void drawBattleCompactExpLabel(int x, int y, uint16_t color) {
    // Native 8x8 Unscii E/X/P glyphs keep the compact label pixel-aligned.
    static constexpr uint32_t FOREGROUND[8] = {
        0x000000U, 0x3EC37EU, 0x666606U, 0x663C06U,
        0x3E183EU, 0x063C06U, 0x066606U, 0x06C37EU,
    };
    static constexpr uint32_t OUTLINE[10] = {
        0x00000000U, 0x007D86FCU, 0x00824902U, 0x010332F2U,
        0x01028472U, 0x00824882U, 0x00728472U, 0x001332F2U,
        0x00124902U, 0x000D86FCU,
    };
    auto& canvas = PixelRenderer::canvas();
    uint16_t outline = PixelRenderer::rgb(255, 255, 255);
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 26; ++col) {
            if (OUTLINE[row] & (1UL << col)) {
                canvas.drawPixel(x + col - 1, y + row - 1, outline);
            }
        }
    }
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 24; ++col) {
            if (FOREGROUND[row] & (1UL << col)) {
                canvas.drawPixel(x + col, y + row, color);
            }
        }
    }
}

void drawBattleConditionEffects(int centerX, int groundY,
                                Game::MajorStatus majorStatus,
                                const BattleSystem::BattleActorState& battleState,
                                uint32_t nowMs) {
    auto& c = PixelRenderer::canvas();
    const uint8_t pulse = static_cast<uint8_t>((nowMs / 180U) % 4U);
    const uint16_t outline = PixelRenderer::rgb(48, 45, 55);

    switch (majorStatus) {
    case Game::MajorStatus::POISON:
    case Game::MajorStatus::TOXIC: {
        const uint16_t color = majorStatus == Game::MajorStatus::TOXIC
            ? PixelRenderer::rgb(151, 68, 190)
            : PixelRenderer::rgb(185, 91, 204);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 16 + i * 15;
            int y = groundY - 8 - ((pulse + i) % 4) * 4;
            int radius = 2 + ((pulse + i) & 1);
            c.fillCircle(x, y, radius + 1, outline);
            c.fillCircle(x, y, radius, color);
        }
        break;
    }
    case Game::MajorStatus::PARALYSIS: {
        const uint16_t color = PixelRenderer::rgb(255, 215, 55);
        const int jitter = (pulse & 1) ? 2 : 0;
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * (23 + jitter);
            int y = groundY - 35 + (side > 0 ? 5 : 0);
            c.drawLine(x, y, x - side * 5, y + 6, outline);
            c.drawLine(x - side * 5, y + 6, x + side * 1, y + 6, outline);
            c.drawLine(x + side * 1, y + 6, x - side * 4, y + 13, outline);
            c.drawLine(x + side, y, x - side * 4, y + 6, color);
            c.drawLine(x - side * 4, y + 6, x + side * 2, y + 6, color);
            c.drawLine(x + side * 2, y + 6, x - side * 3, y + 13, color);
        }
        break;
    }
    case Game::MajorStatus::SLEEP: {
        const uint16_t color = PixelRenderer::rgb(116, 169, 232);
        int rise = static_cast<int>((nowMs / 240U) % 3U);
        PixelRenderer::textOutlined(centerX + 18, groundY - 45 - rise * 2,
                                    "Z", color, outline, 1);
        PixelRenderer::textOutlined(centerX + 28, groundY - 55 - rise * 2,
                                    "Z", color, outline, 1);
        break;
    }
    case Game::MajorStatus::BURN: {
        const int sway = (pulse & 1) ? 2 : -1;
        const uint16_t red = PixelRenderer::rgb(226, 76, 42);
        const uint16_t yellow = PixelRenderer::rgb(255, 191, 46);
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * 19;
            int y = groundY - 10;
            c.fillTriangle(x - 5, y + 7, x + 5, y + 7,
                           x + sway * side, y - 7, outline);
            c.fillTriangle(x - 4, y + 6, x + 4, y + 6,
                           x + sway * side, y - 6, red);
            c.fillTriangle(x - 2, y + 5, x + 2, y + 5,
                           x - sway * side, y, yellow);
        }
        break;
    }
    case Game::MajorStatus::FREEZE: {
        const uint16_t color = PixelRenderer::rgb(116, 219, 239);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 20 + i * 20;
            int y = groundY - 13 - ((pulse + i) & 1) * 5;
            c.drawLine(x - 4, y, x + 4, y, outline);
            c.drawLine(x, y - 4, x, y + 4, outline);
            c.drawLine(x - 3, y - 3, x + 3, y + 3, outline);
            c.drawLine(x - 3, y + 3, x + 3, y - 3, outline);
            c.drawLine(x - 3, y, x + 3, y, color);
            c.drawLine(x, y - 3, x, y + 3, color);
        }
        break;
    }
    default:
        break;
    }

    if (battleState.confusionTurns > 0) {
        const uint16_t colors[] = {
            PixelRenderer::rgb(255, 215, 55),
            PixelRenderer::rgb(239, 119, 116),
            PixelRenderer::rgb(111, 211, 221),
        };
        float angle = nowMs * 0.006f;
        for (uint8_t i = 0; i < 3; ++i) {
            float pointAngle = angle + i * 2.0943951f;
            int x = centerX + static_cast<int>(roundf(cosf(pointAngle) * 17.0f));
            int y = groundY - 50 + static_cast<int>(roundf(sinf(pointAngle) * 4.0f));
            c.fillCircle(x, y, 3, outline);
            c.fillCircle(x, y, 2, colors[i]);
        }
    }

    if (battleState.bindTurns > 0) {
        const uint16_t color = PixelRenderer::rgb(205, 154, 86);
        int y = groundY - 22 + ((pulse & 1) ? 1 : -1);
        c.drawFastHLine(centerX - 24, y, 48, outline);
        c.drawFastHLine(centerX - 24, y + 5, 48, outline);
        c.drawFastHLine(centerX - 23, y + 1, 46, color);
        c.drawFastHLine(centerX - 23, y + 4, 46, color);
        c.drawLine(centerX - 24, y, centerX - 20, y + 5, color);
        c.drawLine(centerX + 23, y, centerX + 19, y + 5, color);
    }

    if (battleState.yawnTurns > 0 && majorStatus != Game::MajorStatus::SLEEP) {
        const uint16_t color = PixelRenderer::rgb(181, 202, 225);
        int drift = static_cast<int>((nowMs / 260U) % 3U);
        for (uint8_t i = 0; i < 3; ++i) {
            c.fillCircle(centerX + 15 + i * 6,
                         groundY - 45 - i * 2 - drift,
                         2, outline);
            c.fillCircle(centerX + 15 + i * 6,
                         groundY - 45 - i * 2 - drift,
                         1, color);
        }
    }
}

struct RouteMap {
    const char* name;
    const char* description;
    GameAssets::Kind battleBackground;
    uint8_t averageLevel;
    uint8_t depthSpread;
    uint8_t minMapCount;
    uint8_t maxMapCount;
    uint16_t encounterChance;
    const PickupEntry* pickupTable;
    uint8_t pickupEntryCount;
    uint8_t minCoin;
    uint8_t maxCoin;
    const EncounterEntry* encounters;
    uint8_t encounterCount;
    uint16_t fieldColor;
    uint16_t accentColor;
};

static constexpr auto& GRASS_PATH_ENCOUNTERS = ExploreEncounters::GRASS_PATH;
static constexpr auto& CREEK_SLOPE_ENCOUNTERS = ExploreEncounters::CREEK_SLOPE;
static constexpr auto& TALL_GRASS_PARK_ENCOUNTERS =
    ExploreEncounters::TALL_GRASS_PARK;
static constexpr auto& FROST_CRYSTAL_CAVE_ENCOUNTERS =
    ExploreEncounters::FROST_CRYSTAL_CAVE;
static constexpr auto& MIST_FOREST_PATH_ENCOUNTERS =
    ExploreEncounters::MIST_FOREST_PATH;
static constexpr auto& ANCIENT_WATERFALL_VALLEY_ENCOUNTERS =
    ExploreEncounters::ANCIENT_WATERFALL_VALLEY;

#define ENTRY_COUNT(entriesValue) \
    static_cast<uint8_t>(sizeof(entriesValue) / sizeof(entriesValue[0]))

// 分地图捡拾池（权重；糖果另有 stepsToday>=5000 门槛，金币按区间滚点）。
static constexpr PickupEntry GRASS_PATH_PICKUPS[] = {
    {PICKUP_COIN, 40}, {PICKUP_POTION, 25}, {PICKUP_ANTIDOTE, 10},
    {PICKUP_HONEY, 10}, {PICKUP_NUGGET, 3}, {PICKUP_RARE_CANDY, 2},
};
static constexpr PickupEntry CREEK_SLOPE_PICKUPS[] = {
    {PICKUP_COIN, 40}, {PICKUP_POTION, 20}, {PICKUP_ANTIDOTE, 10},
    {PICKUP_MAX_REPEL, 8}, {PICKUP_HONEY, 8}, {PICKUP_NUGGET, 5},
    {PICKUP_RARE_CANDY, 2},
};
static constexpr PickupEntry TALL_GRASS_PARK_PICKUPS[] = {
    {PICKUP_COIN, 38}, {PICKUP_SUPER_POTION, 20}, {PICKUP_ANTIDOTE, 8},
    {PICKUP_REVIVE, 5}, {PICKUP_MAX_REPEL, 8}, {PICKUP_NUGGET, 6},
    {PICKUP_BIG_PEARL, 3}, {PICKUP_RARE_CANDY, 2},
};
static constexpr PickupEntry FROST_CRYSTAL_CAVE_PICKUPS[] = {
    {PICKUP_COIN, 36}, {PICKUP_SUPER_POTION, 18}, {PICKUP_FULL_HEAL, 8},
    {PICKUP_REVIVE, 6}, {PICKUP_MAX_REPEL, 6}, {PICKUP_NUGGET, 4},
    {PICKUP_BIG_PEARL, 6}, {PICKUP_HEART_SCALE, 2}, {PICKUP_RARE_CANDY, 2},
};
static constexpr PickupEntry MIST_FOREST_PATH_PICKUPS[] = {
    {PICKUP_COIN, 34}, {PICKUP_MAX_POTION, 15}, {PICKUP_FULL_HEAL, 8},
    {PICKUP_REVIVE, 7}, {PICKUP_BIG_PEARL, 7}, {PICKUP_STAR_PIECE, 4},
    {PICKUP_HEART_SCALE, 2}, {PICKUP_RARE_CANDY, 2},
};
static constexpr PickupEntry ANCIENT_WATERFALL_VALLEY_PICKUPS[] = {
    {PICKUP_COIN, 32}, {PICKUP_MAX_POTION, 15}, {PICKUP_FULL_RESTORE, 6},
    {PICKUP_REVIVE, 8}, {PICKUP_BIG_PEARL, 5}, {PICKUP_STAR_PIECE, 7},
    {PICKUP_HEART_SCALE, 2}, {PICKUP_RARE_CANDY, 2},
};

static constexpr RouteMap ROUTE_MAPS[] = {
    {
        Ui::Explore::GRASS_PATH,
        Ui::Explore::AREA_DESCS[0],
        ExploreAreaCatalog::battleBackground(0),
        ExploreAreaCatalog::recommendedLevel(0),
        2,
        3,
        4,
        500,
        GRASS_PATH_PICKUPS, ENTRY_COUNT(GRASS_PATH_PICKUPS), 10, 30,
        GRASS_PATH_ENCOUNTERS,
        ENTRY_COUNT(GRASS_PATH_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(0),
        0x5EEE,
    },
    {
        Ui::Explore::CREEK_SLOPE,
        Ui::Explore::AREA_DESCS[1],
        ExploreAreaCatalog::battleBackground(1),
        ExploreAreaCatalog::recommendedLevel(1),
        3,
        4,
        5,
        600,
        CREEK_SLOPE_PICKUPS, ENTRY_COUNT(CREEK_SLOPE_PICKUPS), 15, 40,
        CREEK_SLOPE_ENCOUNTERS,
        ENTRY_COUNT(CREEK_SLOPE_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(1),
        0x4D38,
    },
    {
        Ui::Explore::TALL_GRASS_PARK,
        Ui::Explore::AREA_DESCS[2],
        ExploreAreaCatalog::battleBackground(2),
        ExploreAreaCatalog::recommendedLevel(2),
        3,
        4,
        6,
        700,
        TALL_GRASS_PARK_PICKUPS, ENTRY_COUNT(TALL_GRASS_PARK_PICKUPS), 20, 60,
        TALL_GRASS_PARK_ENCOUNTERS,
        ENTRY_COUNT(TALL_GRASS_PARK_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(2),
        0x8EA9,
    },
    {
        Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::AREA_DESCS[3],
        ExploreAreaCatalog::battleBackground(3),
        ExploreAreaCatalog::recommendedLevel(3),
        4,
        5,
        7,
        900,
        FROST_CRYSTAL_CAVE_PICKUPS, ENTRY_COUNT(FROST_CRYSTAL_CAVE_PICKUPS), 30, 80,
        FROST_CRYSTAL_CAVE_ENCOUNTERS,
        ENTRY_COUNT(FROST_CRYSTAL_CAVE_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(3),
        0x5D7F,
    },
    {
        Ui::Explore::MIST_FOREST_PATH,
        Ui::Explore::AREA_DESCS[4],
        ExploreAreaCatalog::battleBackground(4),
        ExploreAreaCatalog::recommendedLevel(4),
        4,
        6,
        8,
        1100,
        MIST_FOREST_PATH_PICKUPS, ENTRY_COUNT(MIST_FOREST_PATH_PICKUPS), 40, 110,
        MIST_FOREST_PATH_ENCOUNTERS,
        ENTRY_COUNT(MIST_FOREST_PATH_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(4),
        0x644D,
    },
    {
        Ui::Explore::ANCIENT_WATERFALL_VALLEY,
        Ui::Explore::AREA_DESCS[5],
        ExploreAreaCatalog::battleBackground(5),
        ExploreAreaCatalog::recommendedLevel(5),
        5,
        7,
        9,
        1300,
        ANCIENT_WATERFALL_VALLEY_PICKUPS,
        ENTRY_COUNT(ANCIENT_WATERFALL_VALLEY_PICKUPS), 50, 150,
        ANCIENT_WATERFALL_VALLEY_ENCOUNTERS,
        ENTRY_COUNT(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(5),
        0x54F7,
    },
};
#undef ENTRY_COUNT

static constexpr uint8_t ROUTE_MAP_COUNT = sizeof(ROUTE_MAPS) / sizeof(ROUTE_MAPS[0]);
static_assert(ROUTE_MAP_COUNT == Game::EXPLORE_AREA_COUNT,
              "route tables and persistent pool counters must stay aligned");

// 从区域遭遇表提取活跃池源视图。
uint8_t buildPoolSource(const RouteMap& map, ExplorePool::SourceEntry* out,
                        uint8_t cap) {
    uint8_t count = MathUtil::min<uint8_t>(cap, map.encounterCount);
    for (uint8_t i = 0; i < count; ++i) {
        out[i] = ExplorePool::SourceEntry{
            map.encounters[i].speciesId,
            map.encounters[i].weight,
            map.encounters[i].rarity,
        };
    }
    return count;
}

const EncounterEntry* findEncounterEntry(const RouteMap& map, uint16_t speciesId) {
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        if (map.encounters[i].speciesId == speciesId) return &map.encounters[i];
    }
    return nullptr;
}

int uiTextWidth(const char* value) {
    if (!value) return 0;
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

void drawAreaPreviewFrame(const PokemonSprites::SpriteFrame* frame,
                          int centerX, int centerY, bool silhouette) {
    if (!frame) return;

    int x = centerX - static_cast<int>(frame->width) / 2;
    int y = centerY - static_cast<int>(frame->height) / 2;
    if (silhouette) {
        PokemonSprites::drawFrameSilhouette(frame, x, y, 0x0000);
    } else {
        PokemonSprites::drawFrame(frame, x, y);
    }
}

// Rare members stay hidden until the player has actually met them.
void drawPreviewMember(const ExplorePool::Pool& pool, uint8_t index,
                       const PokemonSprites::SpriteFrame* frame,
                       int centerX, int centerY) {
    bool rare = index < pool.count &&
                ExplorePool::isRare(pool.entries[index].rarity);
    bool hidden = rare &&
        !GameEngine::ins().hasEncounteredSpecies(pool.entries[index].speciesId);
    drawAreaPreviewFrame(frame, centerX, centerY, hidden);
}

constexpr uint8_t mapCountForRoll(const RouteMap& map, uint8_t roll) {
    return map.maxMapCount <= map.minMapCount
        ? map.minMapCount
        : (map.maxMapCount == map.minMapCount + 1
            ? (roll < 50 ? map.minMapCount : map.maxMapCount)
            : (roll < 25
                ? map.minMapCount
                : (roll < 75 ? map.minMapCount + 1 : map.maxMapCount)));
}

template <size_t N>
constexpr bool routeLevelsStrictlyIncrease(const RouteMap (&maps)[N], size_t index = 1) {
    return index >= N ||
           (maps[index - 1].averageLevel < maps[index].averageLevel &&
            routeLevelsStrictlyIncrease(maps, index + 1));
}

template <size_t N>
constexpr bool routeTuningValid(const RouteMap (&maps)[N], size_t index = 0) {
    return index >= N ||
           (maps[index].minMapCount >= 3 &&
            maps[index].maxMapCount >= maps[index].minMapCount &&
            maps[index].maxMapCount <= 9 &&
            maps[index].maxMapCount - maps[index].minMapCount <= 2 &&
            maps[index].encounterChance <= 10000 &&
            maps[index].pickupEntryCount > 0 &&
            pickupWeightTotal(maps[index].pickupTable, maps[index].pickupEntryCount) > 0 &&
            maps[index].maxCoin >= maps[index].minCoin &&
            routeTuningValid(maps, index + 1));
}

constexpr int8_t depthLevelOffset(uint16_t progressPermille, uint8_t spread) {
    return progressPermille >= DEPTH_DEEP_START_PERMILLE
        ? static_cast<int8_t>(spread)
        : (progressPermille >= DEPTH_MIDDLE_START_PERMILLE
            ? 0
            : -static_cast<int8_t>(spread));
}

static_assert(ROUTE_MAP_COUNT == Ui::Explore::AREA_COUNT,
              "explore area strings and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == Game::EXPLORE_AREA_COUNT,
              "save reroll counters and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == ExploreBoss::AREA_COUNT,
              "explore boss configs and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == ExploreSpecial::AREA_COUNT,
              "special encounter areas and route maps must stay aligned");
static_assert(routeLevelsStrictlyIncrease(ROUTE_MAPS),
              "explore area average levels must follow menu order");
static_assert(routeTuningValid(ROUTE_MAPS),
              "explore encounter chances and pickup weights must be valid");
static_assert(mapCountForRoll(ROUTE_MAPS[0], 0) == 3 &&
                  mapCountForRoll(ROUTE_MAPS[0], 49) == 3 &&
                  mapCountForRoll(ROUTE_MAPS[0], 50) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[0], 99) == 4,
              "two-value expedition ranges must split evenly");
static_assert(mapCountForRoll(ROUTE_MAPS[2], 0) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[2], 24) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[2], 25) == 5 &&
                  mapCountForRoll(ROUTE_MAPS[2], 74) == 5 &&
                  mapCountForRoll(ROUTE_MAPS[2], 75) == 6 &&
                  mapCountForRoll(ROUTE_MAPS[2], 99) == 6,
              "three-value expedition ranges must use 25/50/25 weights");
static_assert(ENCOUNTER_COOLDOWN_STEP_COUNT == 5 && MAX_ENCOUNTERS_PER_MAP == 2,
              "explore encounter limits must match the design contract");
static_assert(cooldownAfterCompletedSteps(ENCOUNTER_COOLDOWN_STEP_COUNT, 4) == 1 &&
                  cooldownAfterCompletedSteps(ENCOUNTER_COOLDOWN_STEP_COUNT, 5) == 0 &&
                  encounterGateOpen(0, 1) &&
                  !encounterGateOpen(1, 0) &&
                  !encounterGateOpen(0, MAX_ENCOUNTERS_PER_MAP),
                  "encounter cooldown and per-map cap must gate random battles");
static_assert(encounterGateAllows(5, MAX_ENCOUNTERS_PER_MAP, true) &&
                  !encounterGateAllows(5, MAX_ENCOUNTERS_PER_MAP, false) &&
                  repelAllowsEncounter(true, true) &&
                  !repelAllowsEncounter(false, true),
              "honey must bypass the gate while repel only blocks random battles");
static_assert(canScheduleGuaranteedEncounter(12, 5) &&
                  guaranteedEncounterIndex(12, 5) == 8 &&
                  !canScheduleGuaranteedEncounter(6, 5),
              "guaranteed battles must use an interior point after cooldown");
static_assert(MAP_PICKUP_CHANCE == 6500,
              "long expeditions must keep the per-map pickup chance at 65 percent");
static_assert(ROUTE_MAPS[ROUTE_MAP_COUNT - 1].averageLevel == 60,
              "ancient waterfall valley must average level 60");
static_assert(ROUTE_MAPS[0].averageLevel - ROUTE_MAPS[0].depthSpread -
                  WILD_LEVEL_VARIANCE >= WILD_LEVEL_MIN,
              "first area shallow range must stay above level one");
static_assert(ROUTE_MAPS[ROUTE_MAP_COUNT - 1].averageLevel +
                  ROUTE_MAPS[ROUTE_MAP_COUNT - 1].depthSpread +
                  WILD_LEVEL_VARIANCE <= WILD_LEVEL_MAX,
              "deepest area level must stay within the wild level cap");
static_assert(depthLevelOffset(332, 4) == -4 && depthLevelOffset(333, 4) == 0 &&
                  depthLevelOffset(666, 4) == 0 && depthLevelOffset(667, 4) == 4,
              "exploration depth thresholds must cover three progress bands");
static_assert(static_cast<uint8_t>(ExploreScene::Area::GRASS_PATH) ==
                  ExploreMapGenerator::GRASS_PATH_AREA,
              "grass path area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::CREEK_SLOPE) ==
                  ExploreMapGenerator::CREEK_BRIDGE_SLOPE_AREA,
              "creek slope area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::TALL_GRASS_PARK) ==
                  ExploreMapGenerator::TALL_GRASS_PARK_AREA,
              "tall grass area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::FROST_CRYSTAL_CAVE) ==
                  ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA,
              "frost cave area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::MIST_FOREST_PATH) ==
                  ExploreMapGenerator::MIST_FOREST_PATH_AREA,
              "mist forest area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::ANCIENT_WATERFALL_VALLEY) ==
                  ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA,
              "ancient valley area index must follow menu order");
using RouteWorldPoint = ExploreRouteGeometry::WorldPoint;

RouteWorldPoint routePathPointWorld(const ExploreMapGenerator::Path& path,
                                    uint8_t index) {
    return ExploreRouteGeometry::pathPoint(path, index);
}

constexpr uint8_t routeFollowerTargetIndex(uint8_t leaderIndex) {
    return leaderIndex >= ROUTE_FOLLOWER_GAP_STEPS
        ? leaderIndex - ROUTE_FOLLOWER_GAP_STEPS
        : 0;
}

bool routeMonsterHealthy(const Game::MonsterRuntime& monster) {
    return !monster.fainted && monster.hpCur > 0;
}

uint8_t routeLeaderSlot(const Game::GameState& state) {
    bool firstHealthy = state.teamCount > 0 &&
                        routeMonsterHealthy(state.team[0]);
    bool secondHealthy = state.teamCount > 1 &&
                         routeMonsterHealthy(state.team[1]);
    return ExploreRunRules::leaderSlotForHealth(
        firstHealthy, secondHealthy);
}

bool hasHealthyRouteFollower(const Game::GameState& state) {
    bool firstHealthy = state.teamCount > 0 &&
                        routeMonsterHealthy(state.team[0]);
    bool secondHealthy = state.teamCount > 1 &&
                         routeMonsterHealthy(state.team[1]);
    return ExploreRunRules::showsHealthyFollower(
        firstHealthy, secondHealthy);
}

uint16_t routeStepDurationForSpecies(uint16_t speciesId) {
    PokemonSprites::WalkingAnimation animation{};
    uint8_t frameCount = 1;
    if (PokemonSprites::walkingAnimation(
            speciesId, PokemonSprites::WalkDirection::DOWN, animation) &&
        animation.frameCount > 0) {
        frameCount = animation.frameCount;
    }
    return PokemonMotion::routeStepDurationMs(
        PokemonMotion::behaviorForSpecies(speciesId), frameCount);
}

static_assert(routeFollowerTargetIndex(0) == 0 &&
                  routeFollowerTargetIndex(1) == 0 &&
                  routeFollowerTargetIndex(2) == 0 &&
                  routeFollowerTargetIndex(3) == 1,
              "route follower must remain two path steps behind the leader");

uint8_t routeDirectionForDelta(float dx, float dy, uint8_t fallback) {
    if (fabsf(dx) < 0.01f && fabsf(dy) < 0.01f) return fallback;
    if (fabsf(dx) >= fabsf(dy)) {
        return static_cast<uint8_t>(
            dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                       : PokemonSprites::WalkDirection::LEFT);
    }
    return static_cast<uint8_t>(
        dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                   : PokemonSprites::WalkDirection::UP);
}

uint16_t routeDurationForDistance(float fromX, float fromY, float toX, float toY,
                                  uint16_t stepDurationMs) {
    float stepCount = hypotf(toX - fromX, toY - fromY) / EXPLORE_TILE_SIZE;
    if (stepCount < 1.0f) stepCount = 1.0f;
    uint32_t duration = static_cast<uint32_t>(ceilf(stepCount * stepDurationMs));
    return static_cast<uint16_t>(MathUtil::min<uint32_t>(duration, 60000U));
}

const RouteMap& routeMap(uint8_t index) {
    return ROUTE_MAPS[index < ROUTE_MAP_COUNT ? index : 0];
}

uint8_t rollPickupId(const RouteMap& map) {
    uint16_t stepsToday = GameEngine::ins().gameState().stepsToday;
    uint16_t total = 0;
    for (uint8_t i = 0; i < map.pickupEntryCount; ++i) {
        if (!pickupAvailable(map.pickupTable[i].pickupId, stepsToday)) continue;
        total += map.pickupTable[i].weight;
    }
    if (total == 0) return PICKUP_NONE;

    uint16_t roll = static_cast<uint16_t>(GameRandom::random(0, total));
    for (uint8_t i = 0; i < map.pickupEntryCount; ++i) {
        if (!pickupAvailable(map.pickupTable[i].pickupId, stepsToday)) continue;
        if (roll < map.pickupTable[i].weight) return map.pickupTable[i].pickupId;
        roll -= map.pickupTable[i].weight;
    }
    return PICKUP_NONE;
}

static_assert(!pickupAvailable(PICKUP_RARE_CANDY, 4999) &&
                  pickupAvailable(PICKUP_RARE_CANDY, 5000) &&
                  pickupAvailable(PICKUP_POTION, 0),
              "only rare candy pickups require 5000 daily steps");

PokemonSprites::WalkDirection inwardDirection(ExploreMapGenerator::Edge edge) {
    switch (edge) {
    case ExploreMapGenerator::Edge::TOP: return PokemonSprites::WalkDirection::DOWN;
    case ExploreMapGenerator::Edge::RIGHT: return PokemonSprites::WalkDirection::LEFT;
    case ExploreMapGenerator::Edge::BOTTOM: return PokemonSprites::WalkDirection::UP;
    case ExploreMapGenerator::Edge::LEFT: return PokemonSprites::WalkDirection::RIGHT;
    }
    return PokemonSprites::WalkDirection::DOWN;
}

constexpr bool isFrostForegroundWallTile(uint16_t tileId) {
    return (tileId >= 4517 && tileId <= 4519) ||
           tileId == 4522 || tileId == 4523;
}

static_assert(isFrostForegroundWallTile(4517) &&
              isFrostForegroundWallTile(4518) &&
              isFrostForegroundWallTile(4519) &&
              isFrostForegroundWallTile(4522) &&
              isFrostForegroundWallTile(4523));
static_assert(!isFrostForegroundWallTile(4516) &&
              !isFrostForegroundWallTile(4520));

void drawGeneratedTileFallback(uint16_t tileId, int x, int y, uint8_t layer,
                               uint16_t fieldColor) {
    auto& canvas = PixelRenderer::canvas();
    if (tileId >= 4500 && tileId <= 4544) {
        if (layer == 0) {
            uint16_t color = PixelRenderer::rgb(181, 218, 232);
            if (tileId == 4511) color = PixelRenderer::rgb(203, 202, 218);
            else if (tileId == 4501) color = PixelRenderer::rgb(224, 235, 235);
            else if (tileId == 4504 || (tileId >= 4524 && tileId <= 4531)) {
                color = PixelRenderer::rgb(112, 217, 235);
            }
            canvas.fillRect(x, y, EXPLORE_TILE_SIZE, EXPLORE_TILE_SIZE, color);
        } else {
            canvas.fillTriangle(x + 5, y + 22, x + 13, y + 4, x + 21, y + 22,
                                PixelRenderer::rgb(112, 198, 232));
        }
        return;
    }
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
                              int cameraX, int cameraY, uint16_t fieldColor,
                              uint8_t animationFrame) {
    int firstX = MathUtil::max(0, cameraX / static_cast<int>(EXPLORE_TILE_SIZE));
    int firstY = MathUtil::max(0, cameraY / static_cast<int>(EXPLORE_TILE_SIZE));
    int lastX = MathUtil::min<int>(ExploreMapGenerator::WIDTH - 1,
                         (cameraX + Hal::DISPLAY_W - 1) / EXPLORE_TILE_SIZE);
    int lastY = MathUtil::min<int>(ExploreMapGenerator::HEIGHT - 1,
                         (cameraY + Hal::DISPLAY_H - 1) / EXPLORE_TILE_SIZE);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                uint16_t tileId = generated.layers[layer][tileY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0) continue;
                int x = tileX * EXPLORE_TILE_SIZE - cameraX;
                int y = tileY * EXPLORE_TILE_SIZE - cameraY;
                if (!GameAssets::drawExploreTile(tileId, x, y, animationFrame)) {
                    drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
}

void drawGeneratedMapForegroundViewport(
    const ExploreMapGenerator::Map& generated,
    int cameraX, int cameraY, uint16_t fieldColor,
    uint8_t animationFrame) {
    int firstX = MathUtil::max(0, cameraX / static_cast<int>(EXPLORE_TILE_SIZE));
    int firstY = MathUtil::max(0, cameraY / static_cast<int>(EXPLORE_TILE_SIZE));
    int lastX = MathUtil::min<int>(ExploreMapGenerator::WIDTH - 1,
                         (cameraX + Hal::DISPLAY_W - 1) / EXPLORE_TILE_SIZE);
    int lastY = MathUtil::min<int>(ExploreMapGenerator::HEIGHT - 1,
                         (cameraY + Hal::DISPLAY_H - 1) / EXPLORE_TILE_SIZE);
    constexpr uint8_t layer = 1;
    for (int tileY = firstY; tileY <= lastY; ++tileY) {
        for (int tileX = firstX; tileX <= lastX; ++tileX) {
            uint16_t tileId = generated.layers[layer]
                [tileY * ExploreMapGenerator::WIDTH + tileX];
            if (!isFrostForegroundWallTile(tileId)) continue;
            int x = tileX * EXPLORE_TILE_SIZE - cameraX;
            int y = tileY * EXPLORE_TILE_SIZE - cameraY;
            if (!GameAssets::drawExploreTile(tileId, x, y, animationFrame)) {
                drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
            }
        }
    }
}

const EncounterEntry* rollEncounterEntry(const RouteMap& map) {
    if (!map.encounters || map.encounterCount == 0) return nullptr;
    uint32_t total = 0;
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        total += map.encounters[i].weight;
    }
    if (total == 0) return nullptr;

    uint32_t roll = static_cast<uint32_t>(GameRandom::random(static_cast<long>(total)));
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        if (roll < map.encounters[i].weight) return &map.encounters[i];
        roll -= map.encounters[i].weight;
    }
    return nullptr;
}

// 活跃池内滚点：稀有成员按 weight × RARE_ROLL_BONUS 计（§7.4 / §7.9.3）
const ExplorePool::PoolEntry* rollPoolEntry(const ExplorePool::Pool& pool) {
    uint32_t total = ExplorePool::poolWeightTotal(pool);
    if (total == 0) return nullptr;

    uint32_t roll = static_cast<uint32_t>(GameRandom::random(static_cast<long>(total)));
    for (uint8_t i = 0; i < pool.count; ++i) {
        uint32_t weight = ExplorePool::rollWeightOf(pool.entries[i]);
        if (roll < weight) return &pool.entries[i];
        roll -= weight;
    }
    return nullptr;
}

uint8_t rollWildLevel(uint8_t minLevel, uint8_t maxLevel, uint8_t targetLevel) {
    if (minLevel < WILD_LEVEL_MIN) minLevel = WILD_LEVEL_MIN;
    if (maxLevel > WILD_LEVEL_MAX) maxLevel = WILD_LEVEL_MAX;
    if (maxLevel < minLevel) maxLevel = minLevel;
    if (targetLevel < minLevel) targetLevel = minLevel;
    if (targetLevel > maxLevel) targetLevel = maxLevel;

    uint8_t roll = static_cast<uint8_t>(GameRandom::random(0, 100));
    int16_t level = targetLevel;
    if (roll < 10) level -= 2;
    else if (roll < 30) --level;
    else if (roll >= 90) level += 2;
    else if (roll >= 70) ++level;
    if (level < minLevel) level = minLevel;
    if (level > maxLevel) level = maxLevel;
    return static_cast<uint8_t>(level);
}
}

void ExploreScene::onEnter() {
    auto& engine = GameEngine::ins();
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    phase = Phase::SELECT;
    adventureBondGain = 0;
    adventureBondSettled = false;
    progressionCancelledSpeciesId = 0;
    ProgressionUi::resetMoveLearnState(moveLearnState);
    areaCursor = 0;
    areaAnimCursor = 0.0f;
    areaBackgroundTraced = false;
    resultMessage = nullptr;
    defeatAwaitInput = false;
    clearFriendshipFlow();
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    walkStepResolutionPending = false;
    battleIsBoss = false;
    battleAllowsFriendship = true;
    battleFoodBond = 0;
    expeditionBossScheduled = false;
    expeditionNormalBossScheduled = false;
    naturalRunCompletionPending = false;
    normalBossPitySettled = false;
    expeditionPitySlotIndex = 0;
    expeditionBossSpeciesId = 0;
    expeditionBossLevel = 0;
    expeditionBossExperiencePercent = 100;
    expeditionSpecialKind = ExploreSpecial::Kind::NONE;
    specialRelocationHandled = false;
    specialChallengeYes = true;
    routeBossPending = false;
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugBattleMode = engine.consumeDebugBattleRequest();
    if (debugBattleMode) {
        activeArea = Area::GRASS_PATH;
        areaCursor = static_cast<uint8_t>(activeArea);
        mapBlocks[0] = 0;
        mapBlockCount = 1;
        currentMapBlock = 0;
        beginDebugEncounter();
        return;
    }
#endif
    if (engine.exploreTravelPhase() == ExploreTravelPhase::DEPARTING) {
        uint8_t area = engine.pendingExploreArea();
        if (area >= static_cast<uint8_t>(Area::COUNT)) area = 0;
        activeArea = static_cast<Area>(area);
        areaCursor = area;
        engine.markExploreActive();
        resetWalk();
    }
    if (phase == Phase::SELECT) loadAreaPreview();
}

void ExploreScene::onExit() {
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
}

Game::MonsterRuntime& ExploreScene::battlePlayerMonster() {
    auto& state = GameEngine::ins().gameState();
    uint8_t slot = battlePlayerSlot < state.teamCount ? battlePlayerSlot : 0;
    return state.team[slot];
}

const Game::MonsterRuntime& ExploreScene::battlePlayerMonster() const {
    const auto& state = GameEngine::ins().gameState();
    uint8_t slot = battlePlayerSlot < state.teamCount ? battlePlayerSlot : 0;
    return state.team[slot];
}

const Species& ExploreScene::battlePlayerSpecies() const {
    return GameEngine::ins().speciesFor(battlePlayerMonster());
}

SceneUpdateResult ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    if (exploreSubViewOpen) {
        return exploreSubView.update(nowMs, dtSeconds);
    }
    RenderDemand demand;
    bool areaCursorAnimating = false;
    bool areaCursorMoved = false;
    if (phase == Phase::SELECT) {
        uint8_t visibleAreaCount = ExploreItemProgression::visibleAreaCount(
            GameEngine::ins().gameState());
        float target = static_cast<float>(
            areaCursor < ROUTE_MAP_COUNT ? areaCursor : visibleAreaCount);
        float diff = target - areaAnimCursor;
        if (fabsf(diff) < 0.05f) {
            if (areaAnimCursor != target) {
                areaAnimCursor = target;
                demand.redraw();
                areaCursorMoved = true;
            }
        } else {
            areaAnimCursor += diff * AREA_CURSOR_LERP;
            if (fabsf(target - areaAnimCursor) < 0.05f) {
                areaAnimCursor = target;
            }
            demand.redraw();
            areaCursorMoved = true;
        }
        areaCursorAnimating =
            fabsf(target - areaAnimCursor) >= 0.05f;
        if (areaCursor < ROUTE_MAP_COUNT && areaPreviewPool.count > 0) {
            uint32_t visualCycle =
                (nowMs - areaPreviewStartedAt) / AREA_PREVIEW_CYCLE_MS;
            if (visualCycle != areaPreviewVisualCycle) {
                areaPreviewVisualCycle = visualCycle;
                demand.redraw();
            }
        }
    }

    // Keep synchronous LittleFS decode work out of the cursor transition.
    // The final cursor frame is rendered first, then preloading resumes.
    if (areaCursorMoved && areaPreviewLoadPending) {
        areaPreviewNextLoadAt = nowMs + PREVIEW_LOAD_AFTER_CURSOR_MS;
    }
    bool previewLoadWasDue = areaPreviewLoadPending &&
        static_cast<int32_t>(nowMs - areaPreviewNextLoadAt) >= 0;
    updateAreaPreviewLoading(nowMs);
    demand.changed(previewLoadWasDue);
    demand.changed(updateRouteMovement(nowMs));

    bool expWasActive = expAnimationActive;
    updateExpAnimation(nowMs);
    demand.changed(expWasActive != expAnimationActive);
    demand.changed(updateFoodThrow(nowMs));
    demand.changed(serviceBattleLog(nowMs));

    BattleSwitchStage switchStageBefore = battleSwitchStage;
    uint8_t playerSlotBefore = battlePlayerSlot;
    updateBattleSwitch(nowMs);
    demand.changed(switchStageBefore != battleSwitchStage ||
                   playerSlotBefore != battlePlayerSlot);

    Phase phaseBeforeTurn = phase;
    BattleTurnStage turnStageBefore = battleTurnStage;
    uint16_t playerHpBefore = battlePlayerMonster().hpCur;
    uint16_t wildHpBefore = wildHp;
    updateBattleTurn(nowMs);
    demand.changed(phaseBeforeTurn != phase ||
                   turnStageBefore != battleTurnStage ||
                   playerHpBefore != battlePlayerMonster().hpCur ||
                   wildHpBefore != wildHp);

    bool dynamic = phase == Phase::WALKING || phase == Phase::EXITING;
    if (phase == Phase::SELECT) {
        dynamic = areaCursorAnimating;
        if (areaCursor < ROUTE_MAP_COUNT && areaPreviewPool.count > 0) {
            uint32_t cycleElapsed =
                (nowMs - areaPreviewStartedAt) % AREA_PREVIEW_CYCLE_MS;
            dynamic = dynamic || cycleElapsed > AREA_PREVIEW_HOLD_MS;
        }
    }
    if (phase == Phase::ENCOUNTER || phase == Phase::FRIENDSHIP) {
        const auto& active = battlePlayerMonster();
        dynamic = dynamic || battleLogBusy() || expAnimationActive ||
                  battleSwitchStage != BattleSwitchStage::NONE ||
                  battleTurnStage != BattleTurnStage::IDLE ||
                  wildRuntime.majorStatus != Game::MajorStatus::NONE ||
                  active.majorStatus != Game::MajorStatus::NONE;
    }
    if (phase == Phase::EVOLUTION) {
        dynamic = dynamic || !ProgressionUi::evolutionAnimationComplete(
            GameEngine::ins().pendingEvolutionFromSpeciesId(),
            GameEngine::ins().pendingEvolutionToSpeciesId(),
            nowMs);
    }
    if (phase == Phase::EVOLUTION_CANCELLED) {
        dynamic = dynamic ||
            !ProgressionUi::evolutionCancellationComplete(nowMs);
    }

    demand.animate(dynamic);
    if (phase == Phase::SELECT && !dynamic &&
        areaCursor < ROUTE_MAP_COUNT && areaPreviewPool.count > 0) {
        uint32_t cycleElapsed =
            (nowMs - areaPreviewStartedAt) % AREA_PREVIEW_CYCLE_MS;
        demand.wakeIn(MathUtil::max<uint32_t>(
            1, AREA_PREVIEW_HOLD_MS - cycleElapsed + 1));
    }
    if (areaPreviewLoadPending) {
        uint32_t loadDelay =
            static_cast<int32_t>(nowMs - areaPreviewNextLoadAt) >= 0
                ? 1 : areaPreviewNextLoadAt - nowMs;
        demand.wakeIn(loadDelay);
    }
    return demand.result();
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    if (exploreSubViewOpen) {
        exploreSubView.onButton(event);
        MenuScene::BattleBagResult itemResult =
            exploreSubView.consumeBattleBagResult();
        if (itemResult != MenuScene::BattleBagResult::NONE) {
            exploreSubViewOpen = false;
            switch (itemResult) {
            case MenuScene::BattleBagResult::POTION:
                enqueueBattleLog(Ui::Bag::USED_POTION);
                break;
            case MenuScene::BattleBagResult::SUPER_POTION:
                enqueueBattleLog(Ui::Bag::USED_SUPER_POTION);
                break;
            case MenuScene::BattleBagResult::ANTIDOTE:
                enqueueBattleLog(Ui::Bag::USED_ANTIDOTE);
                break;
            case MenuScene::BattleBagResult::PARALYZE_HEAL:
                enqueueBattleLog(Ui::Bag::USED_PARALYZE_HEAL);
                break;
            case MenuScene::BattleBagResult::AWAKENING:
                enqueueBattleLog(Ui::Bag::USED_AWAKENING);
                break;
            case MenuScene::BattleBagResult::BURN_HEAL:
                enqueueBattleLog(Ui::Bag::USED_BURN_HEAL);
                break;
            case MenuScene::BattleBagResult::ICE_HEAL:
                enqueueBattleLog(Ui::Bag::USED_ICE_HEAL);
                break;
            case MenuScene::BattleBagResult::MAX_POTION:
                enqueueBattleLog(Ui::Bag::USED_MAX_POTION);
                break;
            case MenuScene::BattleBagResult::FULL_RESTORE:
                enqueueBattleLog(Ui::Bag::USED_FULL_RESTORE);
                break;
            case MenuScene::BattleBagResult::FULL_HEAL:
                enqueueBattleLog(Ui::Bag::USED_FULL_HEAL);
                break;
            case MenuScene::BattleBagResult::REVIVE:
                enqueueBattleLog(Ui::Bag::USED_REVIVE);
                break;
            case MenuScene::BattleBagResult::FOOD_THROWN:
                throwFood(exploreSubView.battleBagThrownFoodIndex());
                return true;
            default:
                break;
            }
            wildCounterattack();
            return true;
        }
        if (exploreSubView.exploreViewClosed()) {
            exploreSubViewOpen = false;
        }
        return true;
    }

    if (phase == Phase::ENDING) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::PRESSED) {
            completeExploreReturn(false);
        }
        return true;
    }

    if (phase == Phase::EXITING) return true;

    if (phase == Phase::PICKUP) {
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            resumeWalk();
        }
        return true;
    }

    if (phase == Phase::FRIENDSHIP) {
        if (event.action == BtnAction::LONG_PRESS) return true;
        if (event.action != BtnAction::PRESSED) return true;
        bool confirmationStep =
            friendshipStep == FriendshipStep::CONTACT_CONFIRM ||
            friendshipStep == FriendshipStep::TEAM_CONFIRM;
        if (event.btn == 1 && confirmationStep) {
            friendshipConfirmYes = !friendshipConfirmYes;
        } else if (event.btn == 0 && confirmationStep) {
            resolveFriendshipOffer();
        }
        return true;
    }

    if (phase == Phase::SPECIAL_PROMPT) {
        if (event.action == BtnAction::LONG_PRESS) return true;
        if (event.action != BtnAction::PRESSED) return true;
        if (event.btn == 1) {
            specialChallengeYes = !specialChallengeYes;
        } else if (event.btn == 0) {
            if (specialChallengeYes) {
                beginRouteBossEncounter();
            } else {
                routeBossPending = false;
                phase = Phase::WALKING;
            }
        }
        return true;
    }

    if ((phase == Phase::LEVEL_UP || phase == Phase::EVOLUTION ||
         phase == Phase::EVOLUTION_CANCELLED ||
         phase == Phase::LEARN_MOVE ||
         phase == Phase::MOVE_REPLACED) &&
        event.action != BtnAction::PRESSED) {
        return true;
    }
    if (phase == Phase::ENCOUNTER &&
        event.action == BtnAction::LONG_PRESS) {
        return true;
    }

    if (phase == Phase::WALKING && iceSliding &&
        (event.btn == 0 || event.btn == 1)) {
        return true;
    }

    if (phase == Phase::WALKING && exploreMenuOpen) {
        if (event.action == BtnAction::LONG_PRESS) return true;
        if (event.action != BtnAction::PRESSED) return false;
        if (event.btn == 0) {
            AppSceneFlow::ExploreMenuItem item =
                AppSceneFlow::exploreMenuEntry(exploreMenuCursor).item;
            if (item == AppSceneFlow::ExploreMenuItem::TEAM) {
                exploreSubView.openExploreTeamView();
                exploreSubViewOpen = true;
            } else if (item == AppSceneFlow::ExploreMenuItem::BAG) {
                exploreSubView.openExploreBagView();
                exploreSubViewOpen = true;
            } else if (item == AppSceneFlow::ExploreMenuItem::END) {
                requestExploreExit(false, false);
            } else {
                closeExploreMenu();
            }
            return true;
        }
        if (event.btn == 1) {
            exploreMenuCursor = (exploreMenuCursor + 1) % EXPLORE_MENU_ITEM_COUNT;
            return true;
        }
        return true;
    }

    if (phase == Phase::WALKING && event.btn == 1 &&
        event.action == BtnAction::PRESSED) {
        exploreMenuOpen = true;
        exploreMenuCursor = 0;
        exploreMenuOpenedAt = Hal::ins().millis();
        GameEngine::ins().completeTutorial(
            Game::TutorialStep::EXPLORE_MENU);
        return true;
    }

    if (phase != Phase::WALKING && (event.btn == 0 || event.btn == 1) &&
        event.action == BtnAction::LONG_PRESS) {
        requestExploreExit();
        return true;
    }

    if (event.action != BtnAction::PRESSED) return false;

    if (phase == Phase::SELECT) {
        uint8_t visibleAreaCount = ExploreItemProgression::visibleAreaCount(
            GameEngine::ins().gameState());
        if (event.btn == 0) {
            if (areaCursor >= static_cast<uint8_t>(Area::COUNT)) {
                GameEngine::ins().requestScene(SceneID::MENU);
            } else {
                if (GameEngine::ins().beginExploreDeparture(areaCursor)) {
                    activeArea = static_cast<Area>(areaCursor);
                }
            }
            return true;
        }
        if (event.btn == 1) {
            if (areaCursor >= static_cast<uint8_t>(Area::COUNT)) {
                areaCursor = 0;
                areaAnimCursor = 0.0f;
            } else if (areaCursor + 1 < visibleAreaCount) {
                ++areaCursor;
            } else {
                areaCursor = static_cast<uint8_t>(Area::COUNT);
            }
            loadAreaPreview();
            return true;
        }
    }

    if (phase == Phase::WALKING) {
        if (event.btn == 0) {
            beginAutoWalk();
            GameEngine::ins().completeTutorial(
                Game::TutorialStep::EXPLORE_WALK);
            return true;
        }
    }

    if (phase == Phase::ENCOUNTER) {
        if (battleLogBusy()) return true;
        if (defeatAwaitInput) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            if (debugBattleMode) returnToDebugMenu();
            else requestExploreExit(true);
#else
            requestExploreExit(true);
#endif
            return true;
        }
        if (event.btn == 0) {
            GameEngine::ins().completeTutorial(
                Game::TutorialStep::BATTLE_ACTION);
            AudioManager::ins().playSfx(SfxCue::UI_CONFIRM);
            if (battleCursor == 0) {
                attackWild();
            } else if (battleCursor == 1) {
                exploreSubView.openBattleBagView(
                    wild ? wild->name : nullptr, battlePlayerSlot);
                exploreSubViewOpen = true;
            } else if (battleCursor == 2) {
                switchBattleMonster();
            } else {
                fleeEncounter();
            }
            return true;
        }
        if (event.btn == 1) {
            AudioManager::ins().playSfx(SfxCue::UI_CURSOR);
            battleCursor = (battleCursor + 1) % 4;
            return true;
        }
    }

    if (phase == Phase::LEVEL_UP) {
        if (event.btn == 0) {
            GameEngine::ins().acknowledgePendingLevelUp();
            if (GameEngine::ins().hasPendingEvolution()) {
                phase = Phase::EVOLUTION;
            } else if (GameEngine::ins().hasPendingMoveLearn()) {
                ProgressionUi::resetMoveLearnState(moveLearnState);
                phase = Phase::LEARN_MOVE;
            } else {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::EVOLUTION) {
        if (event.btn == 1) {
            uint16_t retainedSpeciesId =
                GameEngine::ins().pendingEvolutionFromSpeciesId();
            uint16_t targetSpeciesId =
                GameEngine::ins().pendingEvolutionToSpeciesId();
            if (GameEngine::ins().cancelPendingEvolution()) {
                ProgressionUi::beginEvolutionCancellation(
                    retainedSpeciesId, targetSpeciesId,
                    Hal::ins().millis());
                progressionCancelledSpeciesId = retainedSpeciesId;
                phase = Phase::EVOLUTION_CANCELLED;
            }
            return true;
        }
        if (event.btn == 0) {
            uint32_t nowMs = Hal::ins().millis();
            if (!ProgressionUi::evolutionAnimationComplete(
                    GameEngine::ins().pendingEvolutionFromSpeciesId(),
                    GameEngine::ins().pendingEvolutionToSpeciesId(),
                    nowMs)) {
                return true;
            }
            GameEngine::ins().acknowledgePendingEvolution();
            ProgressionUi::resetEvolutionAnimation();
            if (GameEngine::ins().hasPendingEvolution()) {
                phase = Phase::EVOLUTION;
            } else if (GameEngine::ins().hasPendingMoveLearn()) {
                ProgressionUi::resetMoveLearnState(moveLearnState);
                phase = Phase::LEARN_MOVE;
            } else {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::EVOLUTION_CANCELLED) {
        if (event.btn == 0) {
            if (!ProgressionUi::evolutionCancellationComplete(
                    Hal::ins().millis())) {
                return true;
            }
            ProgressionUi::resetEvolutionAnimation();
            progressionCancelledSpeciesId = 0;
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::LEARN_MOVE) {
        if (ProgressionUi::handleMoveLearnInput(
                moveLearnState, event.btn)) {
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::MOVE_REPLACED) {
        if (event.btn == 0) {
            GameEngine::ins().acknowledgePendingMoveReplacement();
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::RESULT) {
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugBattleMode && (event.btn == 0 || event.btn == 1)) {
            returnToDebugMenu();
            return true;
        }
#endif
        if (event.btn == 0) {
            const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
            if (routeIndex + 1 >= path.pointCount) beginRouteExit();
            else {
                resumeWalk();
                beginAutoWalk();
            }
            return true;
        }
        if (event.btn == 1) {
            requestExploreExit();
            return true;
        }
    }

    return false;
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void ExploreScene::returnToDebugMenu() {
    debugBattleMode = false;
    GameEngine::ins().endDebugBattle();
}
#endif

void ExploreScene::beginRouteExit() {
    if (phase == Phase::EXITING || phase == Phase::ENDING) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (currentMapBlock + 1 == mapBlockCount &&
        routeIndex + 1 >= path.pointCount) {
        naturalRunCompletionPending = true;
    }

    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    routeTargetX = routeWorldX;
    routeTargetY = routeWorldY;
    switch (path.exit.edge) {
    case ExploreMapGenerator::Edge::TOP:
        routeTargetY = -ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::RIGHT:
        routeTargetX = EXPLORE_MAP_W + ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::BOTTOM:
        routeTargetY = EXPLORE_MAP_H + ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::LEFT:
        routeTargetX = -ROUTE_EXIT_MARGIN;
        break;
    }
    routeWalkDirection = routeDirectionForDelta(
        routeTargetX - routeFromX, routeTargetY - routeFromY, routeWalkDirection);

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFollowerTargetX = routeTargetX;
    routeFollowerTargetY = routeTargetY;
    routeFollowerWalkDirection = routeDirectionForDelta(
        routeFollowerTargetX - routeFollowerFromX,
        routeFollowerTargetY - routeFollowerFromY,
        routeFollowerWalkDirection);

    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    uint16_t leaderStepDuration = routeStepDurationForSpecies(
        engine.speciesFor(state.team[leaderSlot]).id);
    routeLeaderMoveDurationMs = routeDurationForDistance(
        routeFromX, routeFromY, routeTargetX, routeTargetY, leaderStepDuration);
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoving = false;
    if (hasHealthyRouteFollower(state)) {
        uint16_t followerStepDuration = routeStepDurationForSpecies(
            engine.speciesFor(state.team[1]).id);
        routeFollowerMoveDurationMs = routeDurationForDistance(
            routeFollowerFromX, routeFollowerFromY,
            routeFollowerTargetX, routeFollowerTargetY,
            followerStepDuration);
        routeFollowerMoving = true;
        routeMoveDurationMs = MathUtil::max<uint16_t>(
            routeLeaderMoveDurationMs,
            ROUTE_FOLLOWER_DELAY_MS + routeFollowerMoveDurationMs);
    } else {
        routeFollowerTargetX = routeFollowerFromX;
        routeFollowerTargetY = routeFollowerFromY;
    }

    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    routePickupAvailable = false;
    routeGuaranteedEncounterPending = false;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    routeMoveStarted = Hal::ins().millis();
    routeMoving = true;
    phase = Phase::EXITING;
}

void ExploreScene::requestExploreExit(bool fainted, bool showEndPrompt) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) {
        returnToDebugMenu();
        return;
    }
#endif
    if (phase == Phase::SELECT &&
        GameEngine::ins().exploreTravelPhase() != ExploreTravelPhase::ACTIVE) {
        if (GameEngine::ins().contactVisitExploring()) {
            GameEngine::ins().requestContactVisitFarewell();
            GameEngine::ins().requestScene(SceneID::MAIN);
        } else {
            GameEngine::ins().requestScene(SceneID::MENU);
        }
        return;
    }
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool returnFainted = fainted || mon.fainted || mon.hpCur == 0;
    routeMoving = false;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    if (returnFainted || !showEndPrompt) {
        exploreMenuOpen = false;
        exploreSubViewOpen = false;
        exploreMenuOpenedAt = 0;
        completeExploreReturn(returnFainted);
        return;
    }
    if (phase == Phase::ENDING) return;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    beginExploreEnding();
}

void ExploreScene::beginExploreEnding() {
    settleAdventureBond();
    phase = Phase::ENDING;
}

void ExploreScene::settleAdventureBond() {
    if (adventureBondSettled) return;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) return;
#endif
    adventureBondSettled = true;
    adventureBondGain = GameEngine::ins().grantAdventureBond(steps);
}

void ExploreScene::completeExploreReturn(bool fainted) {
    settleAdventureBond();
    settleNormalBossPity();
    GameEngine::ins().beginExploreReturn(fainted);
}

void ExploreScene::settleNormalBossPity() {
    if (normalBossPitySettled || !naturalRunCompletionPending) return;
    normalBossPitySettled = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) return;
#endif
    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    uint32_t currentSlot = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    bool changed = ExploreBossPity::syncSlot(state, currentSlot);
    if (currentSlot == expeditionPitySlotIndex &&
        !expeditionBossScheduled) {
        ExploreBossPity::increment(state, static_cast<uint8_t>(activeArea));
        changed = true;
    }
    if (changed) engine.markDirty(SaveUrgency::SOON);
}

void ExploreScene::closeExploreMenu() {
    if (!exploreMenuOpen) return;
    if (routeMoving) {
        routeMoveStarted += Hal::ins().millis() - exploreMenuOpenedAt;
    }
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
}

void ExploreScene::walk() {
    if (routeMoving) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routeIndex + 1 >= path.pointCount) {
        if (currentMapBlock + 1 < mapBlockCount) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
        } else {
            beginRouteExit();
        }
        return;
    }

    if (!iceSliding) {
        int8_t dx = 0;
        int8_t dy = 0;
        if (ExploreIceSlide::begins(
                generatedMap, path, routeIndex, dx, dy)) {
            iceSliding = true;
            iceSlideDx = dx;
            iceSlideDy = dy;
        }
    }

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    ++routeIndex;
    RouteWorldPoint target = routePathPointWorld(path, routeIndex);
    routeTargetX = target.x;
    routeTargetY = target.y;
    routeFollowerTargetX = routeFollowerFromX;
    routeFollowerTargetY = routeFollowerFromY;
    if (routeIndex >= ROUTE_FOLLOWER_GAP_STEPS) {
        RouteWorldPoint followerTarget = routePathPointWorld(
            path, routeFollowerTargetIndex(routeIndex));
        routeFollowerTargetX = followerTarget.x;
        routeFollowerTargetY = followerTarget.y;
    }
    float dx = routeTargetX - routeFromX;
    float dy = routeTargetY - routeFromY;
    routeWalkDirection = routeDirectionForDelta(dx, dy, routeWalkDirection);
    routeFollowerWalkDirection = routeDirectionForDelta(
        routeFollowerTargetX - routeFollowerFromX,
        routeFollowerTargetY - routeFollowerFromY,
        routeFollowerWalkDirection);
    routeFollowerMoving = fabsf(routeFollowerTargetX - routeFollowerFromX) >= 0.01f ||
                          fabsf(routeFollowerTargetY - routeFollowerFromY) >= 0.01f;
    routeMoveStarted = Hal::ins().millis();
    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    bool hasFollower = hasHealthyRouteFollower(state);
    routeLeaderMoveDurationMs = routeStepDurationForSpecies(
        engine.speciesFor(state.team[leaderSlot]).id);
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    if (iceSliding) {
        routeLeaderMoveDurationMs = ICE_SLIDE_STEP_MS;
        routeFollowerMoveDurationMs = ICE_SLIDE_STEP_MS;
        routeMoveDurationMs = ICE_SLIDE_STEP_MS;
        if (!hasFollower) {
            routeFollowerTargetX = routeFollowerFromX;
            routeFollowerTargetY = routeFollowerFromY;
            routeFollowerMoving = false;
        }
    } else if (hasFollower) {
        routeFollowerMoveDurationMs = routeStepDurationForSpecies(
            engine.speciesFor(state.team[1]).id);
        if (routeFollowerMoving) {
            routeMoveDurationMs = MathUtil::max<uint16_t>(
                routeLeaderMoveDurationMs,
                ROUTE_FOLLOWER_DELAY_MS + routeFollowerMoveDurationMs);
        }
    } else {
        routeFollowerTargetX = routeFollowerFromX;
        routeFollowerTargetY = routeFollowerFromY;
        routeFollowerMoving = false;
    }
    routeMoving = true;
    ++steps;
    GameEngine::ins().addWalkSteps(1);
}

void ExploreScene::beginAutoWalk() {
    if (phase != Phase::WALKING || routeMoving || autoWalkActive) return;
    autoWalkActive = true;
    walk();
}

bool ExploreScene::updateRouteMovement(uint32_t nowMs) {
    if (!routeMoving || exploreMenuOpen) return false;
    uint32_t elapsed = nowMs - routeMoveStarted;
    uint32_t completedAt = routeMoveStarted + routeMoveDurationMs;
    uint16_t leaderDurationMs = MathUtil::max<uint16_t>(1, routeLeaderMoveDurationMs);
    float leaderProgress = MathUtil::min(
        1.0f, elapsed / static_cast<float>(leaderDurationMs));
    const Game::GameState& state = GameEngine::ins().gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    const PokemonMotion::Behavior leaderMotion =
        PokemonMotion::behaviorForSpecies(
            GameEngine::ins().speciesFor(state.team[leaderSlot]).id);
    float leaderEased = phase == Phase::EXITING || iceSliding
        ? leaderProgress
        : PokemonMotion::stepPosition(leaderMotion, leaderProgress);
    routeWorldX = routeFromX + (routeTargetX - routeFromX) * leaderEased;
    routeWorldY = routeFromY + (routeTargetY - routeFromY) * leaderEased;

    if (routeFollowerMoving) {
        uint16_t followerDelayMs = iceSliding ? 0 : ROUTE_FOLLOWER_DELAY_MS;
        uint32_t followerElapsed = elapsed > followerDelayMs
            ? elapsed - followerDelayMs
            : 0;
        uint16_t followerDurationMs = MathUtil::max<uint16_t>(1, routeFollowerMoveDurationMs);
        float followerProgress = elapsed > followerDelayMs
            ? MathUtil::min(1.0f, followerElapsed / static_cast<float>(followerDurationMs))
            : 0.0f;
        const PokemonMotion::Behavior followerMotion =
            PokemonMotion::behaviorForSpecies(
                GameEngine::ins().speciesFor(state.team[1]).id);
        float followerEased = phase == Phase::EXITING || iceSliding
            ? followerProgress
            : PokemonMotion::stepPosition(followerMotion, followerProgress);
        routeFollowerWorldX = routeFollowerFromX +
                              (routeFollowerTargetX - routeFollowerFromX) * followerEased;
        routeFollowerWorldY = routeFollowerFromY +
                              (routeFollowerTargetY - routeFollowerFromY) * followerEased;
    }
    if (elapsed < routeMoveDurationMs) return true;

    routeWorldX = routeTargetX;
    routeWorldY = routeTargetY;
    routeFollowerWorldX = routeFollowerTargetX;
    routeFollowerWorldY = routeFollowerTargetY;
    routeMoving = false;
    routeFollowerMoving = false;
    if (phase == Phase::EXITING) {
        beginExploreEnding();
        return true;
    }
    if (!iceSliding && enterPendingProgression(Phase::WALKING)) {
        walkStepResolutionPending = true;
        return true;
    }
    finishCompletedWalkStep();
    if (routeMoving && (autoWalkActive || iceSliding) &&
        phase == Phase::WALKING &&
        static_cast<int32_t>(nowMs - completedAt) >= 0) {
        routeMoveStarted = completedAt;
    }
    return true;
}

void ExploreScene::finishCompletedWalkStep() {
    bool encounterBlockedThisStep = encounterCooldownSteps > 0;
    encounterCooldownSteps = cooldownAfterCompletedStep(encounterCooldownSteps);
    auto& engine = GameEngine::ins();
    bool repelActiveThisStep = engine.repelStepsRemaining() > 0;
    engine.tickRepelStep();
    recoverTeamForCompletedSteps();

    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (exploreMenuOpen) return;
    if (iceSliding) {
        if (ExploreIceSlide::continues(
                generatedMap, path, routeIndex,
                iceSlideDx, iceSlideDy)) {
            walk();
            return;
        }
        bool stoppedOnIce = ExploreIceSlide::indexIsSmoothIce(
            generatedMap, path, routeIndex);
        iceSliding = false;
        iceSlideDx = 0;
        iceSlideDy = 0;
        autoWalkActive = false;
        if (stoppedOnIce) return;
    }
    if (routeBossPending && routeIndex == routeBossIndex) {
        promptOrBeginRouteBoss();
        return;
    }
    if (collectRoutePickup()) {
        autoWalkActive = false;
        return;
    }
    if (routeIndex + 1 >= path.pointCount) {
        if (currentMapBlock + 1 < mapBlockCount) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
            if (phase == Phase::WALKING && autoWalkActive) walk();
        } else {
            beginRouteExit();
        }
        return;
    }
    if (routeBossPending && routeIndex + 1 == routeBossIndex) {
        autoWalkActive = false;
        return;
    }
    bool guaranteeEncounter = routeGuaranteedEncounterPending &&
                              routeIndex >= routeGuaranteedEncounterIndex;
    // 甜甜蜜：下一步必遇敌（保底标记等价物），遇敌闸门关闭时保留到再下一步。
    bool honeyActive = engine.honeyEncounterPending();
    if ((honeyActive || !encounterBlockedThisStep) &&
        rollRandomEncounter(guaranteeEncounter || honeyActive,
                            honeyActive, repelActiveThisStep)) {
        if (honeyActive) engine.clearHoneyEncounter();
        autoWalkActive = false;
        return;
    }
    if (autoWalkActive && phase == Phase::WALKING) walk();
}

void ExploreScene::recoverTeamForCompletedSteps() {
    if (!ExploreRunRules::isRecoveryStep(steps)) return;

    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    bool recovered = false;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& monster = state.team[slot];
        if (monster.fainted || monster.hpCur == 0 ||
            monster.hpMax == 0 || monster.hpCur >= monster.hpMax) {
            continue;
        }
        uint16_t amount = ExploreRunRules::recoveryAmount(monster.hpMax);
        monster.hpCur = static_cast<uint16_t>(MathUtil::min<uint32_t>(
            monster.hpMax, static_cast<uint32_t>(monster.hpCur) + amount));
        recovered = true;
    }
    if (recovered) engine.markDirty(SaveUrgency::DEFERRED);
}

void ExploreScene::finishProgression() {
    phase = progressionReturnPhase;
    if (phase != Phase::WALKING || !walkStepResolutionPending) return;
    walkStepResolutionPending = false;
    finishCompletedWalkStep();
}

bool ExploreScene::rollRandomEncounter(bool guaranteed, bool bypassGate,
                                       bool repelActiveThisStep) {
    if (!encounterGateAllows(
            encounterCooldownSteps, mapEncounterCount, bypassGate)) {
        return false;
    }
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    // 黄金喷雾生效中：非保底随机遇敌不触发（保底/BOSS 不受影响）。
    if (!repelAllowsEncounter(guaranteed, repelActiveThisStep)) return false;
    if (!guaranteed && GameRandom::random(0, 10000) >= map.encounterChance) return false;
    ++mapEncounterCount;
    encounterCooldownSteps = ENCOUNTER_COOLDOWN_STEP_COUNT;
    routeGuaranteedEncounterPending = false;
    rollEncounter();
    return true;
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    if (pickupId == PICKUP_COIN) {
        const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
        uint32_t coins = GameRandom::random(map.minCoin, (uint32_t)map.maxCoin + 1);
        GameEngine::ins().addCoins(coins);
        snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_COIN_FMT,
                 (unsigned long)coins);
        resultMessage = resultBuf;
        autoWalkActive = false;
        phase = Phase::PICKUP;
        return;
    }

    Game::ItemId itemId = Game::ItemId::COUNT;
    const char* itemName = nullptr;
    switch (pickupId) {
    case PICKUP_POTION:
        itemId = Game::ItemId::POTION;
        itemName = Ui::Explore::PICKUP_POTION;
        break;
    case PICKUP_SUPER_POTION:
        itemId = Game::ItemId::SUPER_POTION;
        itemName = Ui::Explore::PICKUP_SUPER_POTION;
        break;
    case PICKUP_ANTIDOTE:
        itemId = Game::ItemId::ANTIDOTE;
        itemName = Ui::Explore::PICKUP_ANTIDOTE;
        break;
    case PICKUP_RARE_CANDY:
        itemId = Game::ItemId::CANDY;
        itemName = Ui::Explore::PICKUP_CANDY;
        break;
    case PICKUP_MAX_POTION:
        itemId = Game::ItemId::MAX_POTION;
        itemName = Ui::MAX_POTION;
        break;
    case PICKUP_FULL_RESTORE:
        itemId = Game::ItemId::FULL_RESTORE;
        itemName = Ui::FULL_RESTORE;
        break;
    case PICKUP_FULL_HEAL:
        itemId = Game::ItemId::FULL_HEAL;
        itemName = Ui::FULL_HEAL;
        break;
    case PICKUP_REVIVE:
        itemId = Game::ItemId::REVIVE;
        itemName = Ui::REVIVE;
        break;
    case PICKUP_MAX_REPEL:
        itemId = Game::ItemId::MAX_REPEL;
        itemName = Ui::MAX_REPEL;
        break;
    case PICKUP_HONEY:
        itemId = Game::ItemId::HONEY;
        itemName = Ui::HONEY;
        break;
    case PICKUP_NUGGET:
        itemId = Game::ItemId::NUGGET;
        itemName = Ui::NUGGET;
        break;
    case PICKUP_BIG_PEARL:
        itemId = Game::ItemId::BIG_PEARL;
        itemName = Ui::BIG_PEARL;
        break;
    case PICKUP_STAR_PIECE:
        itemId = Game::ItemId::STAR_PIECE;
        itemName = Ui::STAR_PIECE;
        break;
    case PICKUP_HEART_SCALE:
        itemId = Game::ItemId::HEART_SCALE;
        itemName = Ui::HEART_SCALE;
        break;
    default:
        return;
    }
    bool stored = GameEngine::ins().addItem(itemId, 1);
    if (itemName) {
        if (stored) {
            snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_FMT,
                     itemName);
        } else if (routePickupFinalReward) {
            const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
            uint32_t coins = GameRandom::random(map.minCoin, (uint32_t)map.maxCoin + 1);
            GameEngine::ins().addCoins(coins);
            snprintf(resultBuf, sizeof(resultBuf),
                     Ui::Explore::PICKUP_COIN_FMT,
                     static_cast<unsigned long>(coins));
        } else {
            snprintf(resultBuf, sizeof(resultBuf), "%s", Ui::Shop::BAG_FULL);
        }
    }
    resultMessage = resultBuf;
    autoWalkActive = false;
    phase = Phase::PICKUP;
}

int8_t ExploreScene::currentDepthLevelOffset(uint8_t spread) const {
    if (mapBlockCount == 0 || currentMapBlock >= mapBlockCount) return 0;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    uint16_t routeLength = path.pointCount > 1 ? path.pointCount - 1 : 1;
    uint16_t localProgress = MathUtil::min<uint16_t>(
        1000, static_cast<uint32_t>(routeIndex) * 1000U / routeLength);
    uint16_t expeditionProgress = static_cast<uint16_t>(
        (static_cast<uint32_t>(currentMapBlock) * 1000U + localProgress) /
        mapBlockCount);
    return depthLevelOffset(expeditionProgress, spread);
}

void ExploreScene::beginEncounter(const Species& species, uint8_t level, bool boss) {
    GameEngine::ins().recordEncounteredSpecies(species.id);
    wild = &species;
    battleIsBoss = boss;
    battleAllowsFriendship = true;
    wildRuntime = GameEngine::ins().createMonster(wild->id, level);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    battleCursor = 0;
    battleFoodBond = 0;
    clearFriendshipFlow();
    defeatAwaitInput = false;
    battlePlayerSlot = routeLeaderSlot(GameEngine::ins().gameState());
    pendingBattleSwitchSlot = 0xFF;
    BattleSystem::resetVolatile(playerBattleState);
    BattleSystem::resetVolatile(wildBattleState);
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    forcedBattleEndPending = false;
    battleTurnController.reset();
    fleeAttempts = 0;
    autoWalkActive = false;
    phase = Phase::ENCOUNTER;
    AudioManager::ins().setMusic(
        boss ? MusicTrack::BATTLE_SPECIAL : MusicTrack::BATTLE);
    clearBattleLogs();
    enqueueBattleLog(battleIsBoss ? Ui::Explore::BOSS_APPEARED
                                  : Ui::Explore::WILD_APPEARED);
    BattleSystem::EffectResolution entryEffects;
    BattleSystem::applyEntryAbility(
        battlePlayerSpecies(), playerBattleState, *wild, wildBattleState,
        entryEffects);
    enqueueBattleEffectLogs(entryEffects, false);
    entryEffects = BattleSystem::EffectResolution{};
    BattleSystem::applyEntryAbility(
        *wild, wildBattleState, battlePlayerSpecies(), playerBattleState,
        entryEffects);
    enqueueBattleEffectLogs(entryEffects, true);
}

void ExploreScene::promptOrBeginRouteBoss() {
    if (!routeBossPending) return;
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE &&
        ExploreSpecial::configFor(expeditionSpecialKind).optional) {
        autoWalkActive = false;
        specialChallengeYes = true;
        phase = Phase::SPECIAL_PROMPT;
        return;
    }
    beginRouteBossEncounter();
}

void ExploreScene::beginRouteBossEncounter() {
    if (!routeBossPending || currentMapBlock + 1 != mapBlockCount) return;
    const Species* boss = findSpecies(expeditionBossSpeciesId);
    routeBossPending = false;
    if (!boss) {
        Platform::logf("[ExploreBoss] missing species=%u area=%u\n",
                      expeditionBossSpeciesId, mapBlocks[currentMapBlock]);
        return;
    }
    encounterCooldownSteps = ENCOUNTER_COOLDOWN_STEP_COUNT;
    Platform::logf("[ExploreBoss] encounter area=%u species=%u level=%u "
                  "exp=%u%% special=%u\n",
                  mapBlocks[currentMapBlock], expeditionBossSpeciesId,
                  expeditionBossLevel, expeditionBossExperiencePercent,
                  static_cast<unsigned>(expeditionSpecialKind));
    beginEncounter(*boss, expeditionBossLevel, true);
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE) {
        battleAllowsFriendship =
            ExploreSpecial::configFor(expeditionSpecialKind).allowsFriendship;
    }
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void ExploreScene::beginDebugEncounter() {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    const Species& opponent = count > 0
        ? table[static_cast<uint8_t>(GameRandom::random(0, count))]
        : starterSpecies();
    uint8_t level = rollWildLevel(
        WILD_LEVEL_MIN, WILD_LEVEL_MAX, GameEngine::ins().activeMonster().level);
    Platform::logf("[DebugBattle] opponent=%u level=%u\n", opponent.id, level);
    beginEncounter(opponent, level);
}
#endif

void ExploreScene::rollEncounter() {
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    // 趟内只在活跃池快照中滚点（§7.8）；快照为空时退回全表兜底
    const EncounterEntry* encounter = nullptr;
    if (activePool.count > 0) {
        const ExplorePool::PoolEntry* picked = rollPoolEntry(activePool);
        encounter = picked ? findEncounterEntry(map, picked->speciesId) : nullptr;
    } else {
        encounter = rollEncounterEntry(map);
    }
    const Species* opponent = encounter ? findSpecies(encounter->speciesId) : nullptr;
    if (!opponent) opponent = &starterSpecies();
    int16_t target = static_cast<int16_t>(map.averageLevel) +
                     currentDepthLevelOffset(map.depthSpread);
    uint8_t targetLevel = static_cast<uint8_t>(
        MathUtil::clamp(target, WILD_LEVEL_MIN, WILD_LEVEL_MAX));
    uint8_t wildLevel = encounter
        ? rollWildLevel(encounter->minLevel, encounter->maxLevel, targetLevel)
        : targetLevel;
    if (!encounter) {
        Platform::logf("[Explore] empty encounter table area=%u\n",
                      static_cast<unsigned>(activeArea));
    }
    beginEncounter(*opponent, wildLevel);
}

void ExploreScene::clearBattleLogs() {
    battleLogHead = 0;
    battleLogCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleResultPending = false;
    fleeExitPending = false;
    battleExpVisible = false;
    expAnimationPending = false;
    expAnimationActive = false;
    expAnimationFrom = 0;
    expAnimationTo = 0;
    expAnimationStarted = 0;
    battleTurnStage = BattleTurnStage::IDLE;
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleActionIndex = 0;
    battleActionAttackerWild = false;
    battleActionSelfHit = false;
    battleActionReleasingCharge = false;
    battleActionResult = BattleSystem::DamageResult{};
    battleActionCheck = BattleSystem::ActionCheckResult{};
    battleEffectResolution = BattleSystem::EffectResolution{};
    battleHpFrom = 0;
    battleHpTo = 0;
    battleActionStarted = 0;
    foodThrowActive = false;
    foodThrowIndex = 0;
    foodThrowStarted = 0;
    pendingBattleSwitchSlot = 0xFF;
    battleSwitchStage = BattleSwitchStage::NONE;
    battleSwitchStarted = 0;
    battleSwitchConsumesTurn = false;
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

bool ExploreScene::serviceBattleLog(uint32_t nowMs) {
    if (battleLogActive && (int32_t)(nowMs - battleLogUntil) < 0) return false;
    if (battleLogCount == 0) {
        bool playbackEnded = battleLogActive;
        bool changed = battleLogActive || battleLogVisibleCount > 0;
        battleLogActive = false;

        // Keep the two-line context visible while the player answers a
        // friendship question. The next queued message will scroll it out.
        if (phase == Phase::FRIENDSHIP) {
            switch (friendshipStep) {
            case FriendshipStep::CONTACT_INTRO:
                friendshipStep = FriendshipStep::CONTACT_CONFIRM;
                friendshipConfirmYes = true;
                return true;
            case FriendshipStep::CONTACT_ACQUIRED:
                if (GameEngine::ins().gameState().teamCount < Game::TEAM_CAP) {
                    friendshipStep = FriendshipStep::TEAM_CONFIRM;
                    friendshipConfirmYes = true;
                    return true;
                }
                break;
            case FriendshipStep::CONTACT_CONFIRM:
            case FriendshipStep::TEAM_CONFIRM:
                return playbackEnded;
            case FriendshipStep::TEAM_JOINED:
            case FriendshipStep::FINISHING:
                break;
            }
        }

        battleLogVisibleCount = 0;
        for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            battleLogVisible[i][0] = '\0';
        }
        if (fleeExitPending) {
            fleeExitPending = false;
            clearFriendshipFlow();
#if STICKMON_ENABLE_DEBUG_FEATURES
            if (debugBattleMode) returnToDebugMenu();
            else resumeWalk();
#else
            resumeWalk();
#endif
            return true;
        }
        if (battleResultPending) {
            battleResultPending = false;
            if (friendshipOfferPending) {
                clearFriendshipFlow();
                phase = Phase::FRIENDSHIP;
                char logBuf[BATTLE_LOG_LEN];
                snprintf(logBuf, sizeof(logBuf),
                         Ui::Explore::FRIEND_RECOGNIZES_FMT,
                         wild ? wild->name : "");
                enqueueBattleLog(logBuf);
                enqueueBattleLog(Ui::Explore::FRIEND_CONTACT_QUESTION);
                return true;
            }
            finishBattleVictoryFlow();
            return true;
        }
        if (phase == Phase::FRIENDSHIP) {
            switch (friendshipStep) {
            case FriendshipStep::CONTACT_ACQUIRED:
                clearFriendshipFlow();
                finishBattleVictoryFlow();
                return true;
            case FriendshipStep::TEAM_JOINED:
            case FriendshipStep::FINISHING:
                clearFriendshipFlow();
                finishBattleVictoryFlow();
                return true;
            case FriendshipStep::CONTACT_INTRO:
            case FriendshipStep::CONTACT_CONFIRM:
            case FriendshipStep::TEAM_CONFIRM:
                break;
            }
        }
        return changed;
    }

    uint8_t line = 0;
    if (battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        line = battleLogVisibleCount++;
    } else {
        for (uint8_t i = 1; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            strncpy(battleLogVisible[i - 1], battleLogVisible[i], BATTLE_LOG_LEN);
        }
        line = BATTLE_LOG_VISIBLE_CAP - 1;
    }
    uint8_t queueIndex = battleLogHead;
    strncpy(battleLogVisible[line], battleLogQueue[queueIndex], BATTLE_LOG_LEN - 1);
    battleLogVisible[line][BATTLE_LOG_LEN - 1] = '\0';
    BattleLogCue cue = battleLogCues[queueIndex];
    battleLogCues[queueIndex] = BattleLogCue::NONE;
    battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
    battleLogCount--;
    if (cue == BattleLogCue::EXP_GAIN) {
        startExpAnimation(nowMs);
    } else if (cue == BattleLogCue::LEVEL_UP) {
        AudioManager::ins().playSfx(SfxCue::LEVEL_UP);
    } else if (cue == BattleLogCue::MOVE_LEARNT) {
        AudioManager::ins().playSfx(SfxCue::MOVE_LEARNT);
    } else if (cue == BattleLogCue::FAINT) {
        AudioManager::ins().playSfx(SfxCue::FAINT);
    } else if (cue == BattleLogCue::CONTACT) {
        AudioManager::ins().playSfx(SfxCue::CONTACT);
    }
    battleLogActive = true;
    battleLogUntil = nowMs + 1000;
    return true;
}

bool ExploreScene::battleLogBusy() const {
    return battleLogActive || battleLogCount > 0 || battleResultPending ||
           fleeExitPending || expAnimationPending ||
           expAnimationActive || foodThrowActive ||
           battleSwitchStage != BattleSwitchStage::NONE ||
           battleTurnStage != BattleTurnStage::IDLE;
}

bool ExploreScene::battleLogPlaybackBusy() const {
    return battleLogActive || battleLogCount > 0;
}

void ExploreScene::prepareExpAnimation(uint32_t fromExp, uint32_t toExp) {
    expAnimationFrom = fromExp;
    expAnimationTo = toExp;
    expAnimationStarted = 0;
    expAnimationActive = false;
    expAnimationPending = toExp > fromExp;
}

void ExploreScene::startExpAnimation(uint32_t nowMs) {
    battleExpVisible = true;
    AudioManager::ins().playSfx(SfxCue::EXP_GAIN);
    if (!expAnimationPending) return;
    expAnimationPending = false;
    expAnimationActive = true;
    expAnimationStarted = nowMs;
}

void ExploreScene::updateExpAnimation(uint32_t nowMs) {
    if (!expAnimationActive || nowMs < expAnimationStarted) return;
    if (nowMs - expAnimationStarted >= EXP_ANIMATION_MS) {
        expAnimationActive = false;
        AudioManager::ins().playSfx(SfxCue::EXP_FULL);
    }
}

uint32_t ExploreScene::battleExpForRender(uint32_t nowMs) const {
    if (expAnimationPending) return expAnimationFrom;
    if (!expAnimationActive || nowMs <= expAnimationStarted) {
        return expAnimationActive ? expAnimationFrom : battlePlayerMonster().exp;
    }
    uint32_t elapsed = MathUtil::min<uint32_t>(EXP_ANIMATION_MS, nowMs - expAnimationStarted);
    uint64_t distance = static_cast<uint64_t>(expAnimationTo - expAnimationFrom) * elapsed;
    return expAnimationFrom + static_cast<uint32_t>(distance / EXP_ANIMATION_MS);
}

bool ExploreScene::enterPendingProgression(Phase returnPhase) {
    progressionReturnPhase = returnPhase;
    if (GameEngine::ins().hasPendingLevelUp()) {
        phase = Phase::LEVEL_UP;
        return true;
    }
    if (GameEngine::ins().hasPendingEvolution()) {
        phase = Phase::EVOLUTION;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveReplacement()) {
        phase = Phase::MOVE_REPLACED;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveLearn()) {
        ProgressionUi::resetMoveLearnState(moveLearnState);
        phase = Phase::LEARN_MOVE;
        return true;
    }
    return false;
}

void ExploreScene::enqueueBattleProgressionLogs(uint8_t teamSlot) {
    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return;
    const Species& species = engine.speciesFor(state.team[teamSlot]);
    char logBuf[BATTLE_LOG_LEN];

    if (engine.hasPendingLevelUp()) {
        uint8_t level = engine.pendingLevelUpLevel();
        engine.acknowledgePendingLevelUp();
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::LEVEL_UP_LOG_FMT,
                 species.name, level);
        enqueueBattleLog(logBuf, BattleLogCue::LEVEL_UP);
    }

    while (engine.hasPendingMoveLearn()) {
        uint8_t learnerSlot = engine.pendingMoveLearnSlot();
        if (learnerSlot >= state.teamCount || learnerSlot >= Game::TEAM_CAP) {
            engine.resolvePendingMoveLearn(false);
            continue;
        }
        if (engine.pendingMoveLearnNeedsReplacement()) {
            break;
        }
        const Species& learnerSpecies = engine.speciesFor(state.team[learnerSlot]);
        const MoveInfo* move = findMove(engine.pendingMoveLearnId());
        bool learned = engine.resolvePendingMoveLearn(true);
        if (!learned || !move) continue;
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_LEARNED_LOG_FMT,
                 learnerSpecies.name, move->name);
        enqueueBattleLog(logBuf, BattleLogCue::MOVE_LEARNT);
    }
}

void ExploreScene::enqueueBattleEffectLogs(const BattleSystem::EffectResolution& effects,
                                           bool attackerWild) {
    if (!wild) return;
    const Species& playerSpecies = battlePlayerSpecies();
    auto statusLabel = [](Game::MajorStatus status) -> const char* {
        switch (status) {
        case Game::MajorStatus::POISON: return Ui::Status::STATUS_POISON;
        case Game::MajorStatus::TOXIC: return Ui::Status::STATUS_TOXIC;
        case Game::MajorStatus::PARALYSIS: return Ui::Status::STATUS_PARALYSIS;
        case Game::MajorStatus::SLEEP: return Ui::Status::STATUS_SLEEP;
        case Game::MajorStatus::BURN: return Ui::Status::STATUS_BURN;
        case Game::MajorStatus::FREEZE: return Ui::Status::STATUS_FREEZE;
        default: return Ui::Status::STATUS_OK;
        }
    };

    char logBuf[BATTLE_LOG_LEN];
    for (uint8_t index = 0; index < effects.count; ++index) {
        const BattleSystem::EffectOutcome& outcome = effects.outcomes[index];
        bool targetWild = outcome.target == MoveEffectTarget::ATTACKER
            ? attackerWild
            : !attackerWild;
        const char* targetName = targetWild ? wild->name : playerSpecies.name;
        switch (outcome.kind) {
        case BattleSystem::EffectOutcomeKind::STATUS_APPLIED:
        case BattleSystem::EffectOutcomeKind::YAWN_SLEEP:
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         targetName, abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::STATUS_APPLIED_FMT,
                     targetName, statusLabel(outcome.status));
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_FAILED:
            enqueueBattleLog(Ui::Explore::NO_EFFECT);
            break;
        case BattleSystem::EffectOutcomeKind::CONFUSED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::FLINCHED:
            break;
        case BattleSystem::EffectOutcomeKind::BOUND:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BOUND_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STAT_CHANGED: {
            uint8_t statIndex = static_cast<uint8_t>(outcome.stat);
            if (statIndex >= static_cast<uint8_t>(BattleStat::COUNT)) break;
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         outcome.ability == AbilityId::INTIMIDATE
                             ? (attackerWild ? wild->name : playerSpecies.name)
                             : targetName,
                         abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), outcome.stageDelta > 0
                         ? Ui::Explore::STAT_ROSE_FMT
                         : Ui::Explore::STAT_FELL_FMT,
                     targetName, Ui::Explore::STAT_NAMES[statIndex]);
            enqueueBattleLog(logBuf);
            break;
        }
        case BattleSystem::EffectOutcomeKind::DRAINED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::DRAINED_FMT, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::RECOIL:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::RECOIL_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::HEALED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::HEALED_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::CURED:
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         targetName, abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CURED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_CLEARED:
        case BattleSystem::EffectOutcomeKind::BIND_ENDED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BIND_ENDED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::YAWNED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::YAWNED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_DAMAGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::STATUS_DAMAGE_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_DAMAGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BIND_DAMAGE_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::ABILITY_ACTIVATED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                     targetName, abilityName(outcome.ability));
            enqueueBattleLog(logBuf);
            if (outcome.amount > 0) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABSORB_HEAL_FMT,
                         targetName, outcome.amount);
                enqueueBattleLog(logBuf);
            }
            break;
        }
    }
}

void ExploreScene::updateBattleTurn(uint32_t nowMs) {
    if (phase != Phase::ENCOUNTER) return;
    switch (battleTurnStage) {
    case BattleTurnStage::IDLE:
        return;
    case BattleTurnStage::WAIT_ACTION_START:
        if (!battleLogPlaybackBusy()) beginBattleAction();
        return;
    case BattleTurnStage::ANIMATING:
        if (nowMs - battleActionStarted < BATTLE_ACTION_MS) return;
        applyBattleDamage();
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    case BattleTurnStage::WAIT_ACTION_LOGS:
        if (!battleLogPlaybackBusy()) finishBattleAction();
        return;
    case BattleTurnStage::WAIT_END_TURN_LOGS:
        if (!battleLogPlaybackBusy()) finishBattleEndTurn();
        return;
    }
}

void ExploreScene::beginBattleAction() {
    if (!wild || battleActionIndex >= battleTurnPlan.count) {
        battleTurnStage = BattleTurnStage::IDLE;
        return;
    }

    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    const BattleTurnController::Action& plannedAction =
        battleTurnPlan.actions[battleActionIndex];
    battleActionAttackerWild =
        plannedAction.side == BattleTurnController::Side::WILD;
    battleActionSelfHit = false;
    battleActionReleasingCharge = false;
    battleActionResult = BattleSystem::DamageResult{};
    battleActionCheck = BattleSystem::ActionCheckResult{};
    battleEffectResolution = BattleSystem::EffectResolution{};
    wildRuntime.hpCur = wildHp;

    Game::MonsterRuntime& attacker = battleActionAttackerWild ? wildRuntime : activeMon;
    const Species& attackerSpecies = battleActionAttackerWild ? *wild : activeSpecies;
    BattleSystem::BattleActorState& attackerState = battleActionAttackerWild
        ? wildBattleState : playerBattleState;
    BattleSystem::BattleActorState& defenderState = battleActionAttackerWild
        ? playerBattleState : wildBattleState;
    uint8_t sideIndex = battleActionAttackerWild ? 1 : 0;
    uint8_t specialSlot = plannedAction.specialSlot;
    Game::MoveId moveId = plannedAction.moveId;
    if (BattleSystem::isChargingMove(attackerState)) {
        battleActionReleasingCharge = true;
    }
    const MoveInfo* move = findMove(moveId);
    const char* attackerName = attackerSpecies.name;
    char logBuf[BATTLE_LOG_LEN];

    uint8_t opponentIndex = sideIndex == 0 ? 1 : 0;
    const BattleTurnController::Action* opponentAction =
        battleTurnPlan.actionFor(battleActionAttackerWild
            ? BattleTurnController::Side::PLAYER
            : BattleTurnController::Side::WILD);
    Game::MoveId opponentMoveId = opponentAction ? opponentAction->moveId : 0;
    const MoveInfo* opponentMove = findMove(opponentMoveId);
    battleActionCheck = BattleSystem::checkAction(
        attacker, attackerSpecies, attackerState, moveId,
        opponentMove && opponentMove->power > 0 &&
            opponentMove->damageClass != DamageClass::STATUS);
    if (!battleActionAttackerWild) engine.markDirty(SaveUrgency::DEFERRED);
    if (battleActionCheck.wokeUp) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WOKE_UP_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }
    if (battleActionCheck.thawed) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::THAWED_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }
    if (battleActionCheck.confusionEnded) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSION_ENDED_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }

    if (!battleActionCheck.canAct()) {
        if (battleActionReleasingCharge) {
            BattleSystem::clearChargingMove(attackerState);
            battleActionReleasingCharge = false;
        }
        switch (battleActionCheck.blockReason) {
        case BattleSystem::ActionBlockReason::FLINCH:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::FLINCHED_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::SLEEP:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ASLEEP_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::FREEZE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::FROZEN_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::PARALYSIS:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::PARALYZED_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSION_HIT_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::RECHARGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::RECHARGING_FMT,
                     attackerName);
            break;
        case BattleSystem::ActionBlockReason::MOVE_FAILED:
            snprintf(logBuf, sizeof(logBuf), "%s", Ui::Explore::MOVE_FAILED);
            break;
        default:
            snprintf(logBuf, sizeof(logBuf), "%s", battleActionAttackerWild
                         ? Ui::Explore::WILD_CANNOT_MOVE
                         : Ui::Explore::CANNOT_MOVE);
            break;
        }
        enqueueBattleLog(logBuf);
        if (battleActionCheck.blockReason == BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT &&
            battleActionCheck.selfDamage > 0) {
            battleActionSelfHit = true;
            battleActionResult.damage = battleActionCheck.selfDamage;
            battleHpFrom = attacker.hpCur;
            battleHpTo = battleActionResult.damage >= battleHpFrom
                ? 0
                : battleHpFrom - battleActionResult.damage;
            battleActionStarted = Hal::ins().millis();
            battleTurnStage = BattleTurnStage::ANIMATING;
        } else {
            battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        }
        return;
    }

    if (!battleActionReleasingCharge &&
        BattleSystem::moveRequiresCharge(moveId)) {
        BattleSystem::beginChargingMove(attackerState, moveId, specialSlot);
        if (moveId == 76) {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::SOLAR_BEAM_CHARGE_FMT,
                     attackerName);
        } else {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_CHARGE_FMT,
                     attackerName);
        }
        enqueueBattleLog(logBuf);
        if (move && (move->flags & MOVE_FLAG_CHARGE_DEFENSE)) {
            uint8_t defense = static_cast<uint8_t>(BattleStat::DEFENSE);
            int8_t before = attackerState.statStages[defense];
            attackerState.statStages[defense] = std::min<int8_t>(6, before + 1);
            if (attackerState.statStages[defense] != before) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::STAT_ROSE_FMT,
                         attackerName, Ui::Explore::STAT_NAMES[defense]);
                enqueueBattleLog(logBuf);
            }
        }
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionReleasingCharge) {
        BattleSystem::clearChargingMove(attackerState);
    }

    Game::MonsterRuntime& defender = battleActionAttackerWild ? activeMon : wildRuntime;
    const Species& defenderSpecies = battleActionAttackerWild ? activeSpecies : *wild;
    BattleSystem::DamageContext damageContext;
    damageContext.attackerMovesSecond = battleActionIndex > 0;
    damageContext.defenderDamagedThisTurn = battleTurnDamaged[opponentIndex];
    damageContext.defenderMoveIsDamaging = opponentMove && opponentMove->power > 0 &&
        opponentMove->damageClass != DamageClass::STATUS;
    damageContext.allowForceWildEnd = !battleActionAttackerWild;
    battleActionResult = BattleSystem::calcBasicDamage(
        attacker, attackerSpecies, defender, defenderSpecies, specialSlot,
        attackerState, defenderState, damageContext);
    move = findMove(battleActionResult.moveId);
    if (battleActionAttackerWild) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
    } else {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name, move ? move->name : Ui::Status::MOVE_UNKNOWN);
    }
    enqueueBattleLog(logBuf);

    if (battleActionResult.failed && move) {
        BattleSystem::recordMoveResult(
            attackerState, attacker, attackerSpecies, *move, false, specialSlot);
        enqueueBattleLog(Ui::Explore::MOVE_FAILED);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }

    if (battleActionResult.missed) {
        if (move) BattleSystem::recordMoveResult(
            attackerState, attacker, attackerSpecies, *move, false, specialSlot);
        enqueueBattleLog(Ui::Explore::MOVE_MISSED);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionResult.effectiveness == 0) {
        if (move) BattleSystem::recordMoveResult(
            attackerState, attacker, attackerSpecies, *move, true, specialSlot);
        if (!battleActionResult.absorbed &&
            battleActionResult.activatedAbility != AbilityId::NONE) {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                     defenderSpecies.name,
                     abilityName(battleActionResult.activatedAbility));
            enqueueBattleLog(logBuf);
        }
        if (battleActionResult.absorbed) {
            uint16_t beforeHp = defender.hpCur;
            BattleSystem::EffectResolution absorb = BattleSystem::applyAbsorbAbility(
                battleActionResult,
                defender, defenderSpecies,
                defenderState);
            if (battleActionAttackerWild) activeMon.hpCur = defender.hpCur;
            else {
                wildHp = defender.hpCur;
                wildRuntime.hpCur = wildHp;
            }
            enqueueBattleEffectLogs(absorb, battleActionAttackerWild);
            if (defender.hpCur != beforeHp) engine.markDirty(SaveUrgency::DEFERRED);
        }
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionResult.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
    if (battleActionResult.effectiveness > 100) {
        enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
    } else if (battleActionResult.effectiveness < 100) {
        enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
    }

    battleHpFrom = battleActionAttackerWild ? activeMon.hpCur : wildHp;
    battleHpTo = battleActionResult.damage >= battleHpFrom
        ? 0
        : battleHpFrom - battleActionResult.damage;
    if (battleActionResult.damage == 0) {
        applyBattleDamage();
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    snprintf(logBuf, sizeof(logBuf), battleActionAttackerWild
                 ? Ui::Explore::WILD_DAMAGE_FMT
                 : Ui::Explore::DAMAGE_FMT,
             battleActionResult.damage);
    enqueueBattleLog(logBuf);
    if (battleActionResult.hitCount > 1) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::HIT_COUNT_FMT,
                 battleActionResult.hitCount);
        enqueueBattleLog(logBuf);
    }
    battleActionStarted = Hal::ins().millis();
    battleTurnStage = BattleTurnStage::ANIMATING;
}

void ExploreScene::applyBattleDamage() {
    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    wildRuntime.hpCur = wildHp;

    if (battleActionSelfHit) {
        AudioManager::ins().playSfx(SfxCue::DAMAGE_NORMAL);
        if (battleActionAttackerWild) {
            wildHp = battleHpTo;
            wildRuntime.hpCur = wildHp;
            battleTurnDamaged[1] = true;
        } else {
            activeMon.hpCur = battleHpTo;
            battleTurnDamaged[0] = true;
            engine.markDirty(SaveUrgency::DEFERRED);
        }
        return;
    }

    if (battleActionResult.damage > 0) {
        SfxCue damageCue = battleActionResult.effectiveness > 100
            ? SfxCue::DAMAGE_SUPER
            : (battleActionResult.effectiveness < 100
                ? SfxCue::DAMAGE_WEAK : SfxCue::DAMAGE_NORMAL);
        AudioManager::ins().playSfx(damageCue);
        if (battleActionAttackerWild) {
            activeMon.hpCur = battleHpTo;
            battleTurnDamaged[0] = true;
        } else {
            wildHp = battleHpTo;
            wildRuntime.hpCur = wildHp;
            battleTurnDamaged[1] = true;
        }
    }

    const MoveInfo* move = findMove(battleActionResult.moveId);
    if (move) {
        bool defenderCanStillAct = battleTurnPlan.hasActionAfter(
            battleActionIndex,
            battleActionAttackerWild ? BattleTurnController::Side::PLAYER
                                     : BattleTurnController::Side::WILD);
        uint16_t actualDamage = battleHpFrom >= battleHpTo ? battleHpFrom - battleHpTo : 0;
        if (battleActionAttackerWild) {
            battleEffectResolution = BattleSystem::applyMoveEffects(
                *move, wildRuntime, *wild, wildBattleState,
                activeMon, activeSpecies, playerBattleState,
                actualDamage, defenderCanStillAct);
        } else {
            battleEffectResolution = BattleSystem::applyMoveEffects(
                *move, activeMon, activeSpecies, playerBattleState,
                wildRuntime, *wild, wildBattleState,
                actualDamage, defenderCanStillAct);
        }
        wildHp = wildRuntime.hpCur;
        enqueueBattleEffectLogs(battleEffectResolution, battleActionAttackerWild);

        BattleSystem::EffectResolution abilityEffects =
            BattleSystem::applyPostDamageAbilities(
                *move,
                battleActionAttackerWild ? wildRuntime : activeMon,
                battleActionAttackerWild ? *wild : activeSpecies,
                battleActionAttackerWild ? wildBattleState : playerBattleState,
                battleActionAttackerWild ? activeMon : wildRuntime,
                battleActionAttackerWild ? activeSpecies : *wild,
                battleActionAttackerWild ? playerBattleState : wildBattleState,
                actualDamage);
        wildHp = wildRuntime.hpCur;
        enqueueBattleEffectLogs(abilityEffects, battleActionAttackerWild);

        bool rampageEnded = BattleSystem::recordMoveResult(
            battleActionAttackerWild ? wildBattleState : playerBattleState,
            battleActionAttackerWild ? wildRuntime : activeMon,
            battleActionAttackerWild ? *wild : activeSpecies,
            *move, true, battleActionResult.specialSlot);
        wildHp = wildRuntime.hpCur;
        if (rampageEnded) {
            char logBuf[BATTLE_LOG_LEN];
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::RAMPAGE_CONFUSED_FMT,
                     battleActionAttackerWild ? wild->name : activeSpecies.name);
            enqueueBattleLog(logBuf);
        }
        if (battleActionResult.sturdyActivated) {
            char logBuf[BATTLE_LOG_LEN];
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                     battleActionAttackerWild ? activeSpecies.name : wild->name,
                     abilityName(AbilityId::STURDY));
            enqueueBattleLog(logBuf);
        }
        if (battleActionResult.forceWildEnd && wildHp > 0) {
            forcedBattleEndPending = true;
            enqueueBattleLog(Ui::Explore::DRAGON_TAIL_END);
        }
    }

    if (!battleActionAttackerWild && move) {
        uint8_t moveSlot = battleActionResult.special
            ? static_cast<uint8_t>(battleActionResult.specialSlot + 1)
            : 0;
        if (moveSlot > 0 && moveSlot < Game::MOVE_SLOT_COUNT &&
            activeMon.moveProficiency[moveSlot] < Game::MOVE_PROFICIENCY_MAX) {
            activeMon.moveProficiency[moveSlot]++;
        }
    }
    engine.markDirty(SaveUrgency::DEFERRED);
}

void ExploreScene::finishBattleAction() {
    bool playerFainted = battlePlayerMonster().hpCur == 0;
    bool wildFainted = wildHp == 0;
    if (playerFainted || wildFainted) {
        battleTurnStage = BattleTurnStage::IDLE;
        battleTurnPlan = BattleTurnController::TurnPlan{};
        battleActionIndex = 0;
        if (playerFainted) finishPlayerFaint();
        else finishWildFaint();
        return;
    }
    if (forcedBattleEndPending) {
        forcedBattleEndPending = false;
        clearFriendshipFlow();
        finishRoamingEncounter();
        resumeWalk();
        return;
    }

    ++battleActionIndex;
    if (battleActionIndex < battleTurnPlan.count) {
        battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    } else {
        resolveBattleEndTurn();
    }
}

void ExploreScene::resolveBattleEndTurn() {
    auto& engine = GameEngine::ins();
    BattleSystem::EffectResolution playerEffects = BattleSystem::resolveEndTurn(
        battlePlayerMonster(), battlePlayerSpecies(), playerBattleState);
    for (uint8_t index = 0; index < playerEffects.count; ++index) {
        playerEffects.outcomes[index].target = MoveEffectTarget::ATTACKER;
    }
    enqueueBattleEffectLogs(playerEffects, false);

    wildRuntime.hpCur = wildHp;
    BattleSystem::EffectResolution wildEffects = BattleSystem::resolveEndTurn(
        wildRuntime, *wild, wildBattleState);
    wildHp = wildRuntime.hpCur;
    for (uint8_t index = 0; index < wildEffects.count; ++index) {
        wildEffects.outcomes[index].target = MoveEffectTarget::ATTACKER;
    }
    enqueueBattleEffectLogs(wildEffects, true);
    engine.markDirty(SaveUrgency::DEFERRED);

    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    if (battleLogPlaybackBusy()) {
        battleTurnStage = BattleTurnStage::WAIT_END_TURN_LOGS;
    } else {
        finishBattleEndTurn();
    }
}

void ExploreScene::finishBattleEndTurn() {
    battleTurnStage = BattleTurnStage::IDLE;
    if (wildHp == 0) {
        finishWildFaint();
    } else if (battlePlayerMonster().hpCur == 0) {
        finishPlayerFaint();
    } else if (BattleSystem::isChargingMove(playerBattleState)) {
        beginChargedBattleTurn();
    } else if (playerBattleState.lockedMoveId != 0) {
        beginChargedBattleTurn();
    }
}

void ExploreScene::beginChargedBattleTurn() {
    if (!wild || wildHp == 0 || battlePlayerMonster().hpCur == 0) return;
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    battleTurnPlan = battleTurnController.planAiTurn(
        activeMon, activeSpecies, playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::markFirstSpecialVictory() {
    if (!ExploreSpecial::isFirstBoss(expeditionSpecialKind)) return;
    uint8_t bit = ExploreSpecial::defeatedBit(expeditionSpecialKind);
    if (bit == 0) return;

    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    if ((state.specialBossDefeatedMask & bit) != 0) return;
    state.specialBossDefeatedMask =
        static_cast<uint8_t>(state.specialBossDefeatedMask | bit);
    engine.markDirty(SaveUrgency::IMMEDIATE);
}

void ExploreScene::finishRoamingEncounter() {
    if (specialRelocationHandled ||
        !ExploreSpecial::isRoaming(expeditionSpecialKind)) {
        return;
    }
    int8_t index = ExploreSpecial::roamingIndex(expeditionSpecialKind);
    if (index < 0 || index >= ExploreSpecial::ROAMER_COUNT) return;

    auto& engine = GameEngine::ins();
    ++engine.gameState().roamingRerollCounts[index];
    specialRelocationHandled = true;
    engine.markDirty(SaveUrgency::IMMEDIATE);
}

void ExploreScene::finishWildFaint() {
    if (!wild) return;
    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    uint8_t activeSlot = battlePlayerSlot;
    uint16_t expGain = BattleSystem::experienceReward(*wild, wildRuntime.level);
    if (battleIsBoss) {
        expGain = BattleSystem::scaledExperienceReward(
            expGain, expeditionBossExperiencePercent);
    }
    uint8_t reserveSlot = 0xFF;
    const Game::GameState& state = engine.gameState();
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == activeSlot) continue;
        const Game::MonsterRuntime& teammate = state.team[slot];
        if (!teammate.fainted && teammate.hpCur > 0) {
            reserveSlot = slot;
            break;
        }
    }
    BattleSystem::ExperienceAwards expAwards = BattleSystem::experienceAwards(
        expGain, reserveSlot != 0xFF);
    uint16_t reserveExpGain = expAwards.reserve;
    uint16_t activeExpGain = expAwards.active;
    uint32_t expBefore = activeMon.exp;
    engine.grantEffortToTeamMember(activeSlot, *wild);
    if (reserveSlot != 0xFF) {
        engine.grantEffortToTeamMember(reserveSlot, *wild);
    }
    uint32_t activeExpAwarded =
        engine.addExperienceToTeamMember(activeSlot, activeExpGain);
    prepareExpAnimation(expBefore, battlePlayerMonster().exp);
    battleResultPending = true;
    enqueueBattleLog(Ui::Explore::BATTLE_WIN);
    if (battleIsBoss) enqueueBattleLog(Ui::Explore::BOSS_DEFEATED);
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::EXP_GAIN_FMT,
             static_cast<unsigned>(activeExpAwarded));
    enqueueBattleLog(logBuf, BattleLogCue::EXP_GAIN);
    enqueueBattleProgressionLogs(activeSlot);

    if (reserveSlot != 0xFF && reserveExpGain > 0) {
        uint32_t awarded = engine.addExperienceToTeamMember(
            reserveSlot, reserveExpGain);
        if (awarded > 0) {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::SHARED_EXP_GAIN_FMT,
                     engine.speciesFor(state.team[reserveSlot]).name,
                     static_cast<unsigned>(awarded));
            enqueueBattleLog(logBuf);
            enqueueBattleProgressionLogs(reserveSlot);
        }
    }

    uint8_t coinReward = ExploreBoss::victoryCoinReward(battleIsBoss);
    if (coinReward > 0) {
        engine.addCoins(coinReward);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::COIN_GAIN_FMT,
                 static_cast<unsigned>(coinReward));
        enqueueBattleLog(logBuf);
    }
    markFirstSpecialVictory();
    finishRoamingEncounter();
    Game::FriendshipService::OfferResult friendshipOffer =
        Game::FriendshipService::evaluateOffer(
            state, *wild, wildRuntime, battleIsBoss,
            battleAllowsFriendship, battleFoodBond,
#if STICKMON_ENABLE_DEBUG_FEATURES
            debugBattleMode
#else
            false
#endif
        );
    friendshipOfferPending = friendshipOffer.offered;

    // 栖息地轮换：击败区域头目 → 该区域重抽计数 +1 并立即重建活跃池（§7.2）
    if (battleIsBoss) {
        uint8_t bossArea = mapBlocks[currentMapBlock];
        if (bossArea < Game::EXPLORE_AREA_COUNT) {
            uint8_t& rerollCount =
                engine.gameState().explorePoolRerollCounts[bossArea];
            rerollCount = rerollCount == UINT8_MAX
                ? 1 : static_cast<uint8_t>(rerollCount + 1);
            if (expeditionNormalBossScheduled) {
                ExploreBossPity::resetArea(engine.gameState(), bossArea);
            }
            engine.markDirty(SaveUrgency::DEFERRED);
            snapshotActivePool();
        }
    }

    // 按物种独立累计失败层数；加成已叠加在最终结交概率上（§7.9.4）。
    if (friendshipOffer.eligible && !friendshipOfferPending) {
        Game::FriendshipService::recordFailure(
            engine.gameState(), wild->id);
        engine.markDirty(SaveUrgency::DEFERRED);
    }
}

void ExploreScene::attackWild() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    auto& activeMon = battlePlayerMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }

    const Species& activeSpecies = battlePlayerSpecies();
    battleTurnPlan = battleTurnController.planAiTurn(
        activeMon, activeSpecies, playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::wildCounterattack() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    const auto& activeMon = battlePlayerMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }
    battleTurnPlan = battleTurnController.planWildOnly(
        activeMon, battlePlayerSpecies(), playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::throwFood(uint8_t foodIndex) {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE ||
        foodThrowActive) {
        return;
    }
    if (foodIndex >= Game::ROOM_FOOD_COUNT) foodIndex = 0;

    foodThrowActive = true;
    foodThrowIndex = foodIndex;
    foodThrowStarted = Hal::ins().millis();
    AudioManager::ins().playSfx(SfxCue::THROW);

    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::FOOD_THROW_FMT,
             wild->name, Ui::Room::FOOD_NAMES[foodIndex]);
    enqueueBattleLog(logBuf);
}

bool ExploreScene::updateFoodThrow(uint32_t nowMs) {
    if (!foodThrowActive) return false;
    if (phase != Phase::ENCOUNTER || !wild || wildHp == 0) {
        foodThrowActive = false;
        return true;
    }
    if (nowMs - foodThrowStarted < FOOD_THROW_DURATION_MS) return false;

    foodThrowActive = false;
    finishFoodThrow();
    return true;
}

void ExploreScene::finishFoodThrow() {
    if (!wild || wildHp == 0) return;

    FoodTuning::ThrowClass throwClass =
        FriendshipSystem::classifyFoodThrow(foodThrowIndex, wildRuntime.nature);
    bool accepted = FriendshipSystem::acceptsFoodThrow(
        battleIsBoss, throwClass, static_cast<uint8_t>(GameRandom::random(0, 100)));
    char logBuf[BATTLE_LOG_LEN];
    if (!accepted) {
        snprintf(logBuf, sizeof(logBuf),
                 throwClass == FoodTuning::ThrowClass::DISLIKED
                     ? Ui::Explore::FOOD_REFUSED_DISLIKED_FMT
                     : Ui::Explore::FOOD_REFUSED_FMT,
                 wild->name);
        enqueueBattleLog(logBuf);
        wildCounterattack();
        return;
    }

    battleFoodBond = FriendshipSystem::addFoodBond(
        battleFoodBond, FriendshipSystem::throwBondGain(throwClass));
    snprintf(logBuf, sizeof(logBuf),
             throwClass == FoodTuning::ThrowClass::LIKED
                 ? Ui::Explore::FOOD_ACCEPTED_LIKED_FMT
                 : Ui::Explore::FOOD_ACCEPTED_FMT,
             wild->name);
    enqueueBattleLog(logBuf);
    resolveBattleEndTurn();
}

void ExploreScene::switchBattleMonster() {
    const Game::GameState& state = GameEngine::ins().gameState();
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = state.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            beginBattleSwitch(slot, true);
            return;
        }
    }
    enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
}

void ExploreScene::beginBattleSwitch(uint8_t slot, bool consumesTurn) {
    const auto& state = GameEngine::ins().gameState();
    if (battleSwitchStage != BattleSwitchStage::NONE ||
        slot == battlePlayerSlot || slot >= state.teamCount ||
        slot >= Game::TEAM_CAP ||
        state.team[slot].fainted || state.team[slot].hpCur == 0) {
        if (consumesTurn) enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
        return;
    }

    pendingBattleSwitchSlot = slot;
    battleSwitchConsumesTurn = consumesTurn;
    battleSwitchStage = BattleSwitchStage::RETREATING;
    battleSwitchStarted = Hal::ins().millis();
    battleCursor = 0;
}

void ExploreScene::updateBattleSwitch(uint32_t nowMs) {
    if (battleSwitchStage == BattleSwitchStage::NONE ||
        nowMs - battleSwitchStarted < BATTLE_SWITCH_PHASE_MS) {
        return;
    }

    if (battleSwitchStage == BattleSwitchStage::RETREATING) {
        uint8_t slot = pendingBattleSwitchSlot;
        const auto& state = GameEngine::ins().gameState();
        if (slot == 0xFF || slot >= state.teamCount ||
            slot >= Game::TEAM_CAP || state.team[slot].fainted ||
            state.team[slot].hpCur == 0) {
            bool consumedTurn = battleSwitchConsumesTurn;
            pendingBattleSwitchSlot = 0xFF;
            battleSwitchConsumesTurn = false;
            battleSwitchStage = BattleSwitchStage::NONE;
            if (consumedTurn) enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
            else defeatAwaitInput = true;
            return;
        }
        battlePlayerSlot = slot;
        battleTurnController.resetPlayerAi();
        pendingBattleSwitchSlot = 0xFF;
        BattleSystem::resetVolatile(playerBattleState);
        BattleSystem::EffectResolution entryEffects;
        BattleSystem::applyEntryAbility(
            battlePlayerSpecies(), playerBattleState, *wild, wildBattleState,
            entryEffects);
        enqueueBattleEffectLogs(entryEffects, false);
        battleSwitchStage = BattleSwitchStage::ENTERING;
        battleSwitchStarted = nowMs;
        return;
    }

    bool consumedTurn = battleSwitchConsumesTurn;
    battleSwitchConsumesTurn = false;
    battleSwitchStage = BattleSwitchStage::NONE;

    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::SWITCH_IN_FMT,
             battlePlayerSpecies().name);
    enqueueBattleLog(logBuf);
    if (consumedTurn) wildCounterattack();
}

int ExploreScene::battleSwitchOffsetX(uint32_t nowMs) const {
    if (battleSwitchStage == BattleSwitchStage::NONE) return 0;
    float progress = MathUtil::min<uint32_t>(
        BATTLE_SWITCH_PHASE_MS, nowMs - battleSwitchStarted) /
        static_cast<float>(BATTLE_SWITCH_PHASE_MS);
    if (battleSwitchStage == BattleSwitchStage::RETREATING) {
        return -static_cast<int>(
            roundf(BATTLE_SWITCH_TRAVEL_X * progress * progress));
    }
    float remaining = 1.0f - progress;
    return -static_cast<int>(
        roundf(BATTLE_SWITCH_TRAVEL_X * remaining * remaining));
}

uint16_t ExploreScene::battleHpForRender(bool wildSide, uint16_t currentHp,
                                         uint32_t nowMs) const {
    bool targetIsWild = battleActionSelfHit
        ? battleActionAttackerWild
        : !battleActionAttackerWild;
    if (battleTurnStage != BattleTurnStage::ANIMATING || wildSide != targetIsWild) {
        return currentHp;
    }
    uint32_t elapsed = nowMs - battleActionStarted;
    uint32_t drainStart = BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS;
    if (elapsed <= drainStart) return battleHpFrom;
    if (elapsed >= BATTLE_ACTION_MS) return battleHpTo;
    uint32_t drainElapsed = elapsed - drainStart;
    uint32_t drained = static_cast<uint32_t>(battleHpFrom - battleHpTo) * drainElapsed /
                       BATTLE_HP_DRAIN_MS;
    return battleHpFrom - drained;
}

int ExploreScene::battleHitShakeX(bool wildSide, uint32_t nowMs) const {
    bool targetIsWild = battleActionSelfHit
        ? battleActionAttackerWild
        : !battleActionAttackerWild;
    if (battleTurnStage != BattleTurnStage::ANIMATING || wildSide != targetIsWild) return 0;
    uint32_t elapsed = nowMs - battleActionStarted;
    if (elapsed < BATTLE_HIT_DELAY_MS ||
        elapsed >= BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS) {
        return 0;
    }
    static constexpr int8_t OFFSETS[] = {-3, 4, -4, 3, -2, 2, 0};
    uint32_t local = elapsed - BATTLE_HIT_DELAY_MS;
    size_t index = MathUtil::min<size_t>(sizeof(OFFSETS) - 1,
                               local * sizeof(OFFSETS) / BATTLE_HIT_SHAKE_MS);
    return OFFSETS[index];
}

void ExploreScene::finishPlayerFaint() {
    if (defeatAwaitInput) return;
    auto& engine = GameEngine::ins();
    const char* faintedName = battlePlayerSpecies().name;
    uint32_t loss =
        engine.applyFaintPenaltyToTeamMember(battlePlayerSlot);
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::FAINTED_FMT,
             faintedName);
    enqueueBattleLog(logBuf, BattleLogCue::FAINT);
    snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, (unsigned long)loss);
    enqueueBattleLog(resultBuf);
    resultMessage = resultBuf;
    clearFriendshipFlow();

    const Game::GameState& state = engine.gameState();
    pendingBattleSwitchSlot = 0xFF;
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = state.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            pendingBattleSwitchSlot = slot;
            break;
        }
    }

    if (pendingBattleSwitchSlot != 0xFF) {
        beginBattleSwitch(pendingBattleSwitchSlot, false);
        return;
    }

    enqueueBattleLog(Ui::Explore::BATTLE_LOST);
    finishRoamingEncounter();
    defeatAwaitInput = true;
}

void ExploreScene::finishBattleVictoryFlow() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) {
        phase = Phase::RESULT;
        resultMessage = Ui::Explore::BATTLE_WIN;
        enterPendingProgression(Phase::RESULT);
        return;
    }
#endif
    resumeWalk();
    enterPendingProgression(Phase::WALKING);
}

void ExploreScene::clearFriendshipFlow() {
    friendshipOfferPending = false;
    friendshipConfirmYes = true;
    friendshipStep = FriendshipStep::CONTACT_INTRO;
    friendshipContactIndex = 0xFF;
}

void ExploreScene::resolveFriendshipOffer() {
    auto& engine = GameEngine::ins();
    if (!wild) {
        clearFriendshipFlow();
        finishBattleVictoryFlow();
        return;
    }

    switch (friendshipStep) {
    case FriendshipStep::CONTACT_INTRO:
        return;
    case FriendshipStep::CONTACT_CONFIRM: {
        if (!friendshipConfirmYes) {
            clearFriendshipFlow();
            finishBattleVictoryFlow();
            return;
        }
        uint8_t contactSlot = 0xFF;
#if STICKMON_ENABLE_DEBUG_FEATURES
        uint8_t metArea = debugBattleMode
            ? Game::MET_AREA_UNKNOWN
            : mapBlocks[currentMapBlock];
#else
        uint8_t metArea = mapBlocks[currentMapBlock];
#endif
        if (!engine.recordFriendContact(wildRuntime, metArea, &contactSlot)) {
            friendshipStep = FriendshipStep::FINISHING;
            enqueueBattleLog(Ui::Explore::FRIEND_CONTACTS_FULL);
            return;
        }
        friendshipContactIndex = contactSlot;
        friendshipStep = FriendshipStep::CONTACT_ACQUIRED;
        // 结交成功只清零当前物种，其他物种的累积不受影响。
        {
            Game::GameState& save = engine.gameState();
            int8_t pityIndex = FriendshipPity::indexFor(wild->id);
            if (pityIndex >= 0 &&
                save.friendshipPityFailCounts[pityIndex] != 0) {
                Game::FriendshipService::recordSuccess(save, wild->id);
                engine.markDirty(SaveUrgency::DEFERRED);
            }
        }
        snprintf(resultBuf, sizeof(resultBuf),
                 Ui::Explore::FRIEND_CONTACT_ACQUIRED_FMT, wild->name);
        enqueueBattleLog(resultBuf, BattleLogCue::CONTACT);
        if (engine.gameState().teamCount < Game::TEAM_CAP) {
            enqueueBattleLog(Ui::Explore::FRIEND_TEAM_QUESTION);
        }
        return;
    }
    case FriendshipStep::CONTACT_ACQUIRED:
        return;
    case FriendshipStep::TEAM_CONFIRM:
        if (!friendshipConfirmYes) {
            clearFriendshipFlow();
            finishBattleVictoryFlow();
            return;
        }
        if (friendshipContactIndex != 0xFF &&
            engine.inviteContactToTeam(friendshipContactIndex) ==
                ContactInviteResult::JOINED) {
            friendshipContactIndex = 0xFF;
            initializeRouteFollowerPosition(true);
            friendshipStep = FriendshipStep::TEAM_JOINED;
            snprintf(resultBuf, sizeof(resultBuf),
                     Ui::Explore::FRIEND_TEAM_JOINED_FMT, wild->name);
            enqueueBattleLog(resultBuf);
            return;
        }
        friendshipStep = FriendshipStep::FINISHING;
        enqueueBattleLog(Ui::Storage::TEAM_FULL_TOAST);
        return;
    case FriendshipStep::TEAM_JOINED:
    case FriendshipStep::FINISHING:
        return;
    }
}

void ExploreScene::fleeEncounter() {
    if (!wild) return;
    if (!BattleSystem::canFlee(playerBattleState)) {
        enqueueBattleLog(Ui::Explore::BOUND_CANNOT_FLEE);
        wildCounterattack();
        return;
    }
    const auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    uint16_t activeSpeed = BattleSystem::effectiveSpeed(
        activeMon, activeSpecies, playerBattleState);
    uint16_t wildSpeed = BattleSystem::effectiveSpeed(
        wildRuntime, *wild, wildBattleState);
    bool escaped = wildSpeed == 0 || activeSpeed >= wildSpeed;
    if (!escaped) {
        fleeAttempts++;
        uint32_t odds = ((uint32_t)activeSpeed * 128) / wildSpeed + 30UL * fleeAttempts;
        escaped = odds >= 256 || GameRandom::random(0, 256) < odds;
    }

    if (!escaped) {
        enqueueBattleLog(Ui::Explore::FLEE_FAILED);
        wildCounterattack();
        return;
    }

    clearFriendshipFlow();
    finishRoamingEncounter();
    fleeExitPending = true;
    enqueueBattleLog(Ui::Explore::FLEE_SUCCESS);
}

void ExploreScene::snapshotActivePool() {
    // 活跃池由 (时段序号, 区域ID, 重抽计数) 派生种子确定性重建，不进存档（§7.6）
    serviceAreaPoolCache(0);
    uint8_t areaIndex = static_cast<uint8_t>(activeArea);
    if (areaIndex >= ROUTE_MAP_COUNT) {
        activePool.count = 0;
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }
    activePool = buildAreaPool(areaIndex);

    memset(areaPreloadSpeciesIds, 0, sizeof(areaPreloadSpeciesIds));
    areaPreloadSpeciesCount = 0;
    for (uint8_t i = 0; i < activePool.count; ++i) {
        uint16_t speciesId = activePool.entries[i].speciesId;
        bool duplicate = false;
        for (uint8_t j = 0; j < areaPreloadSpeciesCount; ++j) {
            if (areaPreloadSpeciesIds[j] == speciesId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && speciesId != 0) {
            areaPreloadSpeciesIds[areaPreloadSpeciesCount++] = speciesId;
        }
    }
    PokemonSprites::setDynamicSceneSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount);
    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 0);
    areaPreviewNextLoadAt = Hal::ins().millis() + 1;
}

bool ExploreScene::ownsSpecies(uint16_t speciesId) const {
    const Game::GameState& state = GameEngine::ins().gameState();
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        if (state.team[i].speciesId == speciesId) return true;
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        if (state.storage[i].speciesId == speciesId) return true;
    }
    return false;
}

ExploreSpecial::Kind ExploreScene::specialKindForArea(uint8_t area) const {
    const auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint32_t slotIndex = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    return ExploreSpecial::kindForArea(
        area,
        state.specialBossDefeatedMask,
        slotIndex,
        state.roamingRerollCounts,
        ownsSpecies(ExploreSpecial::MEW));
}

void ExploreScene::resetWalk() {
    phase = Phase::WALKING;
    generateMapBlocks();
    snapshotActivePool();
    currentMapBlock = 0;
    steps = 0;
    mapEncounterCount = 0;
    encounterCooldownSteps = 0;
    if (!resetRouteSegment()) {
        requestExploreExit(false, false);
        return;
    }
    resumeWalk();
}

void ExploreScene::resumeWalk() {
    phase = Phase::WALKING;
    AudioManager::ins().setMusic(MusicTrack::EXPLORE);
    battleIsBoss = false;
    wild = nullptr;
    wildHp = wildHpMax = 0;
    battleResultPending = false;
    fleeExitPending = false;
    battleExpVisible = false;
    clearFriendshipFlow();
    resultMessage = nullptr;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    routeMoving = false;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
}

void ExploreScene::initializeRouteFollowerPosition(bool useTrailPosition) {
    routeFollowerWalkDirection = routeWalkDirection;
    routeFollowerVisualWalkDirection = routeFollowerWalkDirection;
    routeFollowerWorldX = routeWorldX;
    routeFollowerWorldY = routeWorldY;

    if (generatedMap.pathCount > 0 &&
        currentRoutePath < generatedMap.pathCount &&
        useTrailPosition &&
        routeIndex >= ROUTE_FOLLOWER_GAP_STEPS) {
        const ExploreMapGenerator::Path& path =
            generatedMap.paths[currentRoutePath];
        RouteWorldPoint trail = routePathPointWorld(
            path, routeFollowerTargetIndex(routeIndex));
        routeFollowerWorldX = trail.x;
        routeFollowerWorldY = trail.y;
        routeFollowerWalkDirection = routeDirectionForDelta(
            routeWorldX - routeFollowerWorldX,
            routeWorldY - routeFollowerWorldY,
            routeWalkDirection);
        routeFollowerVisualWalkDirection = routeFollowerWalkDirection;
    } else {
        PokemonSprites::WalkDirection direction =
            static_cast<PokemonSprites::WalkDirection>(routeWalkDirection);
        if (direction == PokemonSprites::WalkDirection::UP ||
            direction == PokemonSprites::WalkDirection::DOWN) {
            float candidate = routeWorldX + ROUTE_FOLLOWER_START_OFFSET;
            if (candidate > EXPLORE_MAP_W - EXPLORE_TILE_SIZE * 0.5f) {
                candidate = routeWorldX - ROUTE_FOLLOWER_START_OFFSET;
            }
            routeFollowerWorldX = candidate;
        } else {
            float candidate = routeWorldY + ROUTE_FOLLOWER_START_OFFSET;
            if (candidate > EXPLORE_MAP_H - EXPLORE_TILE_SIZE * 0.5f) {
                candidate = routeWorldY - ROUTE_FOLLOWER_START_OFFSET;
            }
            routeFollowerWorldY = candidate;
        }
    }

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFollowerTargetX = routeFollowerWorldX;
    routeFollowerTargetY = routeFollowerWorldY;
    routeFollowerMoving = false;
}

bool ExploreScene::resetRouteSegment() {
    uint8_t mapIndex = mapBlocks[currentMapBlock];
    uint32_t mapSeed = ExploreMapGenerator::deriveSeed(
        expeditionSeed, currentMapBlock, mapIndex);
    bool generated = false;
    for (uint8_t attempt = 0;
         attempt < sizeof(MAP_GENERATION_RETRY_SALTS) /
                       sizeof(MAP_GENERATION_RETRY_SALTS[0]);
         ++attempt) {
        uint32_t candidateSeed = mapSeed ^ MAP_GENERATION_RETRY_SALTS[attempt];
        if (candidateSeed == 0) candidateSeed = MAP_GENERATION_SAFE_SEED;
        if (!ExploreMapGenerator::generate(
                candidateSeed, pendingEntryEdge, mapIndex, generatedMap)) {
            continue;
        }
        generated = true;
        break;
    }
    if (!generated) {
        generated = ExploreMapGenerator::generate(
            MAP_GENERATION_SAFE_SEED, pendingEntryEdge, mapIndex, generatedMap);
    }
    if (!generated) {
        Platform::logf("[ExploreMap] generation failed block=%u area=%u entry=%u\n",
                      currentMapBlock, mapIndex,
                      static_cast<unsigned>(pendingEntryEdge));
        return false;
    }
    Platform::logf("[ExploreMap] block=%u area=%u seed=%08lx fingerprint=%08lx "
                  "entry=%u coast=%u forest=%u creek=%u cliff=%u waterfall=%u\n",
                  currentMapBlock,
                  mapIndex,
                  static_cast<unsigned long>(generatedMap.seed),
                  static_cast<unsigned long>(ExploreMapGenerator::fingerprint(generatedMap)),
                  static_cast<unsigned>(generatedMap.entry.edge),
                  generatedMap.hasCoast ? 1 : 0,
                  generatedMap.hasForest ? 1 : 0,
                  generatedMap.hasCreek ? 1 : 0,
                  generatedMap.hasCliff ? 1 : 0,
                  generatedMap.hasWaterfall ? 1 : 0);
    mapEncounterCount = 0;
    prepareMapRoutes();
    if (generatedMap.pathCount == 0) return false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    routeIndex = 0;
    routeMoving = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    routeWalkDirection = static_cast<uint8_t>(inwardDirection(generatedMap.entry.edge));
    routeVisualWalkDirection = routeWalkDirection;
    mapTargetSteps = MathUtil::max<uint16_t>(1, path.pointCount - 1);
    RouteWorldPoint start = routePathPointWorld(path, 0);
    routeWorldX = start.x;
    routeWorldY = start.y;
    routeFromX = routeTargetX = routeWorldX;
    routeFromY = routeTargetY = routeWorldY;
    initializeRouteFollowerPosition(false);
    placeRouteBoss();
    placeRoutePickup();
    return true;
}

void ExploreScene::generateMapBlocks() {
    uint8_t selectedMap = static_cast<uint8_t>(activeArea);
    if (selectedMap >= ROUTE_MAP_COUNT) selectedMap = 0;
    const RouteMap& map = routeMap(selectedMap);
    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    uint32_t currentPitySlot = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    if (ExploreBossPity::syncSlot(state, currentPitySlot)) {
        engine.markDirty(SaveUrgency::SOON);
    }
    expeditionPitySlotIndex = currentPitySlot;
    uint8_t pityMisses = state.normalBossMissCount[selectedMap];
    mapBlockCount = ExploreBossPity::requiresGuaranteedEligibleRun(pityMisses)
        ? map.maxMapCount
        : mapCountForRoll(
              map, static_cast<uint8_t>(GameRandom::random(0, 100)));
    bool bossLengthEligible = ExploreRunRules::allowsRegionalBoss(
        mapBlockCount, map.maxMapCount);
    expeditionSpecialKind = bossLengthEligible
        ? specialKindForArea(selectedMap)
        : ExploreSpecial::Kind::NONE;
    specialRelocationHandled = false;
    expeditionNormalBossScheduled = false;
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE) {
        ExploreSpecial::Config special =
            ExploreSpecial::configFor(expeditionSpecialKind);
        expeditionBossScheduled = true;
        expeditionBossSpeciesId = special.speciesId;
        expeditionBossLevel = ExploreSpecial::encounterLevel(
            expeditionSpecialKind,
            ExploreBoss::configForArea(selectedMap).level);
        expeditionBossExperiencePercent = special.experiencePercent;
    } else if (bossLengthEligible) {
        expeditionBossScheduled =
            GameRandom::random(0, ExploreBoss::SPAWN_ROLL_MAX) <
            ExploreBossPity::chanceForMisses(pityMisses);
        expeditionNormalBossScheduled = expeditionBossScheduled;
        expeditionBossSpeciesId = expeditionBossScheduled
            ? ExploreBoss::speciesForRoll(
                  selectedMap,
                  static_cast<uint32_t>(
                      GameRandom::random(0, ExploreBoss::CANDIDATE_COUNT)))
            : 0;
        const ExploreBoss::Config& normal =
            ExploreBoss::configForArea(selectedMap);
        expeditionBossLevel = normal.level;
        expeditionBossExperiencePercent = normal.experiencePercent;
    } else {
        expeditionBossScheduled = false;
        expeditionBossSpeciesId = 0;
        expeditionBossLevel = 0;
        expeditionBossExperiencePercent = 100;
    }
    for (uint8_t i = 0; i < MAP_BLOCK_CAP; ++i) {
        mapBlocks[i] = i < mapBlockCount ? selectedMap : 0xFF;
    }
    expeditionSeed = static_cast<uint32_t>(GameRandom::random(1, 0x7FFFFFFF));
    pendingEntryEdge = static_cast<ExploreMapGenerator::Edge>((expeditionSeed >> 8) & 0x03);
    currentRoutePath = 0;
    activeExitMask = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    Platform::logf("[ExploreRun] area=%u maps=%u maxMaps=%u bossEligible=%u "
                  "boss=%u normalBoss=%u pity=%u chance=%u bossSpecies=%u "
                  "bossLevel=%u special=%u seed=%08lx\n",
                  selectedMap, mapBlockCount,
                  map.maxMapCount,
                  bossLengthEligible ? 1 : 0,
                  expeditionBossScheduled ? 1 : 0,
                  expeditionNormalBossScheduled ? 1 : 0,
                  pityMisses,
                  ExploreBossPity::chanceForMisses(pityMisses),
                  expeditionBossSpeciesId,
                  expeditionBossLevel,
                  static_cast<unsigned>(expeditionSpecialKind),
                  static_cast<unsigned long>(expeditionSeed));
}

void ExploreScene::prepareMapRoutes() {
    uint8_t pathCount = MathUtil::min<uint8_t>(MAP_EXIT_CAP, generatedMap.pathCount);
    activeExitMask = 0;
    currentRoutePath = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    if (pathCount == 0) return;

    bool hasNextMap = currentMapBlock + 1 < mapBlockCount;
    for (uint8_t pathId = 0; pathId < pathCount; ++pathId) {
        activeExitMask = static_cast<uint8_t>(activeExitMask | (1U << pathId));
        if (hasNextMap) exitNextMaps[pathId] = mapBlocks[currentMapBlock + 1];
    }
    if (expeditionBossScheduled && currentMapBlock + 1 == mapBlockCount) {
        uint8_t bossPaths[MAP_EXIT_CAP];
        uint8_t bossPathCount = 0;
        for (uint8_t pathId = 0; pathId < pathCount; ++pathId) {
            if (ExploreBoss::canPlaceOnPath(generatedMap.paths[pathId].pointCount)) {
                bossPaths[bossPathCount++] = pathId;
            }
        }
        if (bossPathCount > 0) {
            currentRoutePath = bossPaths[GameRandom::random(0, bossPathCount)];
            return;
        }
    }
    currentRoutePath = GameRandom::random(0, pathCount);
}

void ExploreScene::placeRouteBoss() {
    routeBossIndex = 0;
    routeBossPending = false;
    if (!expeditionBossScheduled || expeditionBossSpeciesId == 0 ||
        currentMapBlock + 1 != mapBlockCount) {
        return;
    }

    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (!ExploreBoss::canPlaceOnPath(path.pointCount)) {
        Platform::logf("[ExploreBoss] no placement block=%u path=%u points=%u\n",
                      currentMapBlock, currentRoutePath, path.pointCount);
        return;
    }

    uint8_t preferred = ExploreBoss::routeIndex(path.pointCount);
    routeBossIndex = ExploreIceSlide::nearestNonIceIndex(
        generatedMap, path, preferred, 1,
        static_cast<uint8_t>(path.pointCount - 2));
    if (routeBossIndex == ExploreIceSlide::INVALID_INDEX) {
        Platform::logf("[ExploreBoss] no non-ice placement block=%u path=%u points=%u\n",
                      currentMapBlock, currentRoutePath, path.pointCount);
        routeBossIndex = 0;
        return;
    }
    routeBossPending = true;
    Platform::logf("[ExploreBoss] placed area=%u path=%u index=%u species=%u "
                  "level=%u exp=%u%%\n",
                  mapBlocks[currentMapBlock], currentRoutePath, routeBossIndex,
                  expeditionBossSpeciesId, expeditionBossLevel,
                  expeditionBossExperiencePercent);
}

void ExploreScene::placeRoutePickup() {
    routePickupAvailable = false;
    routePickupFinalReward = false;
    routePickupIndex = 0;
    routePickupItem = PICKUP_NONE;
    routeGuaranteedEncounterIndex = 0;
    routeGuaranteedEncounterPending = false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (path.pointCount < 2) return;
    if (routeBossPending) {
        Platform::logf("[ExploreEvent] block=%u type=boss index=%u\n",
                      currentMapBlock, routeBossIndex);
        return;
    }

    if (ExploreRunRules::shouldPlaceFinalReward(
            currentMapBlock, mapBlockCount, routeBossPending)) {
        routePickupFinalReward = true;
        uint8_t preferred = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        uint8_t last = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, preferred, 1, last);
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) {
            routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
                generatedMap, path, preferred, 1,
                static_cast<uint8_t>(path.pointCount - 1));
        }
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        routePickupItem = rollPickupId(
            routeMap(mapBlocks[currentMapBlock]));
        if (routePickupItem == PICKUP_NONE) routePickupItem = PICKUP_COIN;
        routePickupAvailable = true;
        Platform::logf("[ExploreEvent] block=%u type=final-reward index=%u item=%u\n",
                      currentMapBlock, routePickupIndex, routePickupItem);
        return;
    }

    if (path.pointCount < 3) {
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, static_cast<uint8_t>(path.pointCount - 1),
            1, static_cast<uint8_t>(path.pointCount - 1));
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]));
        routePickupAvailable = routePickupItem != PICKUP_NONE;
        return;
    }

    bool choosePickup = GameRandom::random(0, 10000) < MAP_PICKUP_CHANCE;
    if (!choosePickup &&
        canScheduleGuaranteedEncounter(path.pointCount, encounterCooldownSteps)) {
        uint8_t first = static_cast<uint8_t>(encounterCooldownSteps + 1);
        uint8_t last = static_cast<uint8_t>(path.pointCount - 2);
        routeGuaranteedEncounterIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path,
            guaranteedEncounterIndex(path.pointCount, encounterCooldownSteps),
            first, last);
        if (routeGuaranteedEncounterIndex != ExploreIceSlide::INVALID_INDEX) {
            routeGuaranteedEncounterPending = true;
            Platform::logf("[ExploreEvent] block=%u type=battle index=%u cooldown=%u\n",
                          currentMapBlock, routeGuaranteedEncounterIndex,
                          encounterCooldownSteps);
            return;
        }
    }

    uint8_t first = MathUtil::max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = MathUtil::min<uint8_t>(path.pointCount - 2, path.pointCount * 3 / 4);
    if (first > last) first = last = path.pointCount / 2;
    uint8_t preferred = static_cast<uint8_t>(GameRandom::random(first, last + 1));
    routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
        generatedMap, path, preferred, first, last);
    if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) {
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 2));
    }
    if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
    routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]));
    routePickupAvailable = routePickupItem != PICKUP_NONE;
    Platform::logf("[ExploreEvent] block=%u type=item index=%u cooldown=%u\n",
                  currentMapBlock, routePickupIndex, encounterCooldownSteps);
}

bool ExploreScene::collectRoutePickup() {
    if (!routePickupAvailable || routeIndex != routePickupIndex) return false;
    routePickupAvailable = false;
    resolvePickup(routePickupItem);
    return true;
}

void ExploreScene::advanceMapBlock(uint8_t nextMap) {
    if (currentMapBlock + 1 >= mapBlockCount) {
        requestExploreExit();
        return;
    }
    if (nextMap >= ROUTE_MAP_COUNT || nextMap != mapBlocks[currentMapBlock + 1]) {
        requestExploreExit();
        return;
    }
    pendingEntryEdge = ExploreMapGenerator::opposite(
        generatedMap.paths[currentRoutePath].exit.edge);
    ++currentMapBlock;
    if (!resetRouteSegment()) {
        requestExploreExit(false, false);
        return;
    }
    phase = Phase::WALKING;
    resultMessage = nullptr;
}

void ExploreScene::render() {
    if (exploreSubViewOpen) {
        exploreSubView.render();
        return;
    }
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(11, 22, 27));

    switch (phase) {
    case Phase::SELECT: renderAreaMenu(); break;
    case Phase::WALKING: renderWalking(); break;
    case Phase::ENCOUNTER: renderEncounter(); break;
    case Phase::LEVEL_UP:
        ProgressionUi::renderLevelUp(GameEngine::ins().pendingLevelUpLevel());
        break;
    case Phase::EVOLUTION:
        ProgressionUi::renderEvolution(
            GameEngine::ins().pendingEvolutionFromSpeciesId(),
            GameEngine::ins().pendingEvolutionToSpeciesId(),
            Hal::ins().millis());
        break;
    case Phase::EVOLUTION_CANCELLED:
        ProgressionUi::renderEvolutionCancelled(
            progressionCancelledSpeciesId, Hal::ins().millis());
        break;
    case Phase::LEARN_MOVE:
        ProgressionUi::renderMoveLearn(moveLearnState);
        break;
    case Phase::MOVE_REPLACED: ProgressionUi::renderMoveReplacement(); break;
    case Phase::FRIENDSHIP:
        renderEncounter();
        renderFriendshipPrompt();
        break;
    case Phase::SPECIAL_PROMPT:
        renderWalking();
        renderSpecialPrompt();
        break;
    case Phase::PICKUP:
        renderWalking();
        renderPickupPrompt();
        break;
    case Phase::RESULT: renderResult(); break;
    case Phase::EXITING: renderWalking(); break;
    case Phase::ENDING:
        renderWalking();
        renderEndPrompt();
        break;
    }
    renderTutorial();
}

void ExploreScene::renderTutorial() {
    GameEngine& engine = GameEngine::ins();
    if (!engine.gameState().oobeDone || exploreSubViewOpen) return;

    if (phase == Phase::WALKING && !exploreMenuOpen) {
        if (!engine.tutorialComplete(Game::TutorialStep::EXPLORE_WALK)) {
            TutorialOverlay::draw(
                TutorialOverlay::Button::A, Ui::Tutorial::EXPLORE_WALK);
        } else if (!engine.tutorialComplete(
                       Game::TutorialStep::EXPLORE_MENU)) {
            TutorialOverlay::draw(
                TutorialOverlay::Button::B, Ui::Tutorial::EXPLORE_MENU);
        }
        return;
    }

}

ExplorePool::Pool ExploreScene::buildAreaPool(uint8_t areaIndex) {
    if (areaIndex >= ROUTE_MAP_COUNT) return ExplorePool::Pool{};

    const RouteMap& map = routeMap(areaIndex);
    ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES];
    uint8_t sourceCount = buildPoolSource(
        map, source, ExplorePool::MAX_SOURCE_ENTRIES);
    const auto& engine = GameEngine::ins();
    uint32_t slotIndex = ExplorePool::slotIndexFor(engine.gameMinutesTotal());
    uint8_t rerollCount =
        engine.gameState().explorePoolRerollCounts[areaIndex];
    return ExplorePool::buildPool(
        source, sourceCount,
        ExplorePool::mixSeed(slotIndex, areaIndex, rerollCount));
}

uint8_t ExploreScene::collectAreaPoolSpecies(uint16_t* speciesIds,
                                             uint8_t capacity,
                                             uint8_t priorityArea) {
    if (!speciesIds || capacity == 0) return 0;
    const auto& state = GameEngine::ins().gameState();
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(state);
    uint8_t count = 0;
    if (priorityArea < ROUTE_MAP_COUNT && priorityArea <= unlockedArea) {
        count = ExplorePool::appendUniqueSpecies(
            buildAreaPool(priorityArea), speciesIds, count, capacity);
    }
    for (uint8_t area = 0;
         area < ROUTE_MAP_COUNT && area <= unlockedArea; ++area) {
        if (area == priorityArea) continue;
        count = ExplorePool::appendUniqueSpecies(
            buildAreaPool(area), speciesIds, count, capacity);
    }
    return count;
}

bool ExploreScene::serviceAreaPoolCache(uint8_t loadBudget) {
    static uint32_t cachedSlotIndex = UINT32_MAX;
    static uint8_t cachedRerollCounts[Game::EXPLORE_AREA_COUNT] = {};
    static bool cacheSignatureValid = false;
    static uint16_t speciesIds[AREA_PRELOAD_CAP] = {};
    static uint8_t speciesCount = 0;
    static uint8_t firstPoolSpeciesCount = 0;
    static bool firstPoolReadyLogged = false;
    static bool allPoolsReadyLogged = false;

    auto& engine = GameEngine::ins();
    uint32_t slotIndex =
        ExplorePool::slotIndexFor(engine.gameMinutesTotal());
    const uint8_t* rerollCounts =
        engine.gameState().explorePoolRerollCounts;
    bool signatureChanged =
        !cacheSignatureValid || slotIndex != cachedSlotIndex;
    if (!signatureChanged) {
        for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
            if (rerollCounts[area] != cachedRerollCounts[area]) {
                signatureChanged = true;
                break;
            }
        }
    }
    if (signatureChanged) {
        memset(speciesIds, 0, sizeof(speciesIds));
        firstPoolSpeciesCount = buildAreaPool(0).count;
        speciesCount = collectAreaPoolSpecies(
            speciesIds, AREA_PRELOAD_CAP, 0);
        cachedSlotIndex = slotIndex;
        for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
            cachedRerollCounts[area] = rerollCounts[area];
        }
        cacheSignatureValid = true;
        firstPoolReadyLogged = false;
        allPoolsReadyLogged = false;
        Platform::logf(
            "[ExplorePreload] pools reset slot=%lu species=%u rerolls=%u,%u,%u,%u,%u,%u\n",
            static_cast<unsigned long>(slotIndex), speciesCount,
            cachedRerollCounts[0], cachedRerollCounts[1],
            cachedRerollCounts[2], cachedRerollCounts[3],
            cachedRerollCounts[4], cachedRerollCounts[5]);
    }

    PokemonSprites::setPinnedDynamicSpecies(
        speciesIds, speciesCount);
    bool ready = PokemonSprites::preloadDynamicSpecies(
        speciesIds, speciesCount, loadBudget);
    bool firstPoolReady = PokemonSprites::preloadDynamicSpecies(
        speciesIds, firstPoolSpeciesCount, 0);
    if (firstPoolReady && !firstPoolReadyLogged) {
        firstPoolReadyLogged = true;
        Platform::logf(
            "[ExplorePreload] first_pool ready slot=%lu reroll=%u species=%u\n",
            static_cast<unsigned long>(slotIndex),
            cachedRerollCounts[0], firstPoolSpeciesCount);
    }
    if (ready && !allPoolsReadyLogged) {
        allPoolsReadyLogged = true;
        const auto& stats = PokemonSprites::cacheStats();
        Platform::logf(
            "[ExplorePreload] all_pools ready slot=%lu species=%u decoded=%lu free=%lu\n",
            static_cast<unsigned long>(slotIndex), speciesCount,
            static_cast<unsigned long>(stats.decodedBytes),
            static_cast<unsigned long>(stats.freePsram));
    } else if (!ready) {
        allPoolsReadyLogged = false;
    }
    return ready;
}

void ExploreScene::loadAreaPreview() {
    serviceAreaPoolCache(0);
    memset(areaPreviewFrames, 0, sizeof(areaPreviewFrames));
    memset(areaPreviewSpeciesIds, 0, sizeof(areaPreviewSpeciesIds));
    memset(areaPreloadSpeciesIds, 0, sizeof(areaPreloadSpeciesIds));
    areaPreloadSpeciesCount = 0;
    areaPreviewPool = ExplorePool::Pool{};
    areaPreviewStartedAt = Hal::ins().millis();
    areaPreviewVisualCycle = UINT32_MAX;
    areaPreviewNextLoadAt = areaPreviewStartedAt + 80;
    areaPreviewLoadPending = false;
    if (areaCursor >= ROUTE_MAP_COUNT) {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }
    if (!ExploreItemProgression::isAreaUnlocked(
            areaCursor, GameEngine::ins().gameState())) {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }

    // 预览展示当前时段种子的活跃池全成员：所见即本趟可遭遇（§7.1/§7.5）
    areaPreviewPool = buildAreaPool(areaCursor);
    for (uint8_t i = 0; i < areaPreviewPool.count; ++i) {
        areaPreviewSpeciesIds[i] = areaPreviewPool.entries[i].speciesId;
    }
    PokemonSprites::setDynamicSceneSpecies(
        areaPreviewSpeciesIds, areaPreviewPool.count);

    // Current-area members lead the queue. All six active pools remain pinned,
    // so changing the cursor never evicts a preview that was already loaded.
    areaPreloadSpeciesCount = collectAreaPoolSpecies(
        areaPreloadSpeciesIds, AREA_PRELOAD_CAP, areaCursor);
    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 0);
    areaPreviewStartedAt = Hal::ins().millis();
    areaPreviewNextLoadAt =
        areaPreviewStartedAt + PREVIEW_LOAD_AFTER_CURSOR_MS;
    refreshAreaPreviewFrames();
}

void ExploreScene::updateAreaPreviewLoading(uint32_t nowMs) {
    if (!areaPreviewLoadPending ||
        static_cast<int32_t>(nowMs - areaPreviewNextLoadAt) < 0) {
        return;
    }

    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 1);
    if (phase == Phase::SELECT && areaCursor < ROUTE_MAP_COUNT) {
        refreshAreaPreviewFrames();
    }
    bool currentPoolReady = PokemonSprites::preloadDynamicSpecies(
        areaPreviewSpeciesIds, areaPreviewPool.count, 0);
    areaPreviewNextLoadAt = nowMs +
        (currentPoolReady ? PREVIEW_BACKGROUND_LOAD_INTERVAL_MS : 1);
}

void ExploreScene::refreshAreaPreviewFrames() {
    for (uint8_t i = 0; i < areaPreviewPool.count; ++i) {
        uint16_t speciesId = areaPreviewPool.entries[i].speciesId;
        areaPreviewFrames[i] = PokemonSprites::findCachedSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
        if (!areaPreviewFrames[i]) {
            areaPreviewFrames[i] = PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::ICON_0);
        }
    }
}

void ExploreScene::renderAreaMenu() {
    auto& c = PixelRenderer::canvas();
    bool backgroundDrawn = GameAssets::drawBattleBackground(
        GameAssets::Kind::EXPLORE_MENU_BACKGROUND);
    if (!areaBackgroundTraced) {
        areaBackgroundTraced = true;
        Platform::logf(
            "[ExploreMenuBg] kind=%u drawn=%u assets=%u cursor=%u phase=%u\n",
            static_cast<unsigned>(GameAssets::Kind::EXPLORE_MENU_BACKGROUND),
            backgroundDrawn ? 1U : 0U, GameAssets::available() ? 1U : 0U,
            areaCursor, static_cast<unsigned>(phase));
    }
    if (!backgroundDrawn) {
        c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0x0000);
    }
    static constexpr int LEFT_W = 70;
    static constexpr int CENTER_Y = Hal::DISPLAY_H / 2;
    static constexpr int AREA_SPACING = 34;
    static constexpr int PREVIEW_CENTER_X =
        LEFT_W + (Hal::DISPLAY_W - LEFT_W) / 2;
    // 活跃池轮播（§7.5）：三帧可见，右帧先转到中间，整体向左轮转
    static constexpr int PREVIEW_CENTER_Y =
        26 + (Hal::DISPLAY_H - 26) / 2;
    static constexpr int PREVIEW_GAP = 10;
    uint8_t visibleAreaCount = ExploreItemProgression::visibleAreaCount(
        GameEngine::ins().gameState());
    uint8_t count = static_cast<uint8_t>(visibleAreaCount + 1);
    c.setClipRect(0, 0, LEFT_W, Hal::DISPLAY_H);
    for (uint8_t i = 0; i < count; ++i) {
        float offset = static_cast<float>(i) - areaAnimCursor;
        if (fabsf(offset) > 2.25f) continue;
        int y = CENTER_Y + static_cast<int>(roundf(offset * AREA_SPACING));
        bool active = fabsf(offset) < 0.5f;
        uint8_t areaIndex = i < visibleAreaCount ? i : ROUTE_MAP_COUNT;
        const char* name = areaIndex < ROUTE_MAP_COUNT
            ? routeMap(areaIndex).name : Ui::BACK;
        bool unlocked = areaIndex >= ROUTE_MAP_COUNT ||
            ExploreItemProgression::isAreaUnlocked(
                areaIndex, GameEngine::ins().gameState());
        bool specialActive =
            unlocked && areaIndex < ROUTE_MAP_COUNT &&
            specialKindForArea(areaIndex) != ExploreSpecial::Kind::NONE;
        uint16_t color = !unlocked
            ? PixelRenderer::rgb(82, 88, 96)
            : specialActive
            ? PixelRenderer::rgb(72, 220, 255)
            : (active
                ? PixelRenderer::rgb(255, 216, 72)
                : PixelRenderer::rgb(156, 164, 176));
        if (active) {
            c.fillRect(5, y - 6, 3, 12,
                       unlocked
                           ? PixelRenderer::rgb(255, 216, 72)
                           : PixelRenderer::rgb(82, 88, 96));
        }
        int textX = (LEFT_W - uiTextWidth(name)) / 2 + 4;
        PixelRenderer::text(textX, y - 8, name, color, 1);
    }
    c.clearClipRect();

    c.drawFastVLine(LEFT_W, 4, Hal::DISPLAY_H - 8,
                    PixelRenderer::rgb(241, 242, 232));
    // 池内有稀有成员时标题换成「大量出现!」提示玩家时间窗口（§7.5）
    bool selectedAreaUnlocked = areaCursor < ROUTE_MAP_COUNT &&
        ExploreItemProgression::isAreaUnlocked(
            areaCursor, GameEngine::ins().gameState());
    const char* title = areaCursor < ROUTE_MAP_COUNT &&
                                !selectedAreaUnlocked
                            ? Ui::Explore::AREA_LOCKED
                            : areaCursor < ROUTE_MAP_COUNT &&
                                ExplorePool::poolHasRare(areaPreviewPool)
                            ? Ui::Explore::MASS_OUTBREAK
                            : Ui::Explore::HABITAT_MONSTERS;
    int titleX = PREVIEW_CENTER_X - uiTextWidth(title) / 2;
    PixelRenderer::text(titleX, 4, title, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(LEFT_W + 8, 24, Hal::DISPLAY_W - LEFT_W - 16,
                    PixelRenderer::rgb(241, 242, 232));

    if (areaCursor >= ROUTE_MAP_COUNT) return;
    if (!selectedAreaUnlocked) {
        const char* message = Ui::Explore::DEFEAT_PREVIOUS_BOSS;
        int messageX = PREVIEW_CENTER_X - uiTextWidth(message) / 2;
        PixelRenderer::text(messageX, PREVIEW_CENTER_Y - 8, message,
                            PixelRenderer::rgb(156, 164, 176), 1);
        return;
    }

    // 轮播活跃池全成员；稀有成员以黑色剪影隐藏身份（§7.5）
    const uint8_t poolCount = areaPreviewPool.count;
    if (poolCount == 0) return;
    uint32_t elapsed = Hal::ins().millis() - areaPreviewStartedAt;
    // 整体右往左轮播:右帧先转到中间,新帧从右侧滑入
    // 初次展示按池顺序排成 0、1、2，避免环形上一项把末尾稀有剪影放到最左侧。
    uint8_t currentPreview = static_cast<uint8_t>(
        (elapsed / AREA_PREVIEW_CYCLE_MS + 1) % poolCount);
    uint8_t nextPreview =
        static_cast<uint8_t>((currentPreview + 1) % poolCount);
    uint8_t prevPreview = static_cast<uint8_t>(
        (currentPreview + poolCount - 1) % poolCount);
    uint32_t cycleElapsed = elapsed % AREA_PREVIEW_CYCLE_MS;
    float progress = cycleElapsed <= AREA_PREVIEW_HOLD_MS
        ? 0.0f
        : (cycleElapsed - AREA_PREVIEW_HOLD_MS) /
              static_cast<float>(AREA_PREVIEW_MOVE_MS);
    progress = MathUtil::min(1.0f, progress);
    progress = progress * progress * (3.0f - 2.0f * progress);

    auto frameWidth = [](const PokemonSprites::SpriteFrame* frame) {
        return frame ? static_cast<int>(frame->width) : 0;
    };
    c.setClipRect(LEFT_W + 1, 26,
                  Hal::DISPLAY_W - LEFT_W - 1,
                  Hal::DISPLAY_H - 26);
    if (progress <= 0.0f) {
        // 静止:中间帧锚定在面板中心,两侧按实际宽度留 10px 缝
        const PokemonSprites::SpriteFrame* cur = areaPreviewFrames[currentPreview];
        const PokemonSprites::SpriteFrame* prev = areaPreviewFrames[prevPreview];
        const PokemonSprites::SpriteFrame* next = areaPreviewFrames[nextPreview];
        int leftX = PREVIEW_CENTER_X - frameWidth(cur) / 2 - PREVIEW_GAP -
                    frameWidth(prev) / 2;
        int rightX = PREVIEW_CENTER_X + frameWidth(cur) / 2 + PREVIEW_GAP +
                     frameWidth(next) / 2;
        drawPreviewMember(areaPreviewPool, prevPreview, prev, leftX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(areaPreviewPool, currentPreview, cur,
                          PREVIEW_CENTER_X, PREVIEW_CENTER_Y);
        drawPreviewMember(areaPreviewPool, nextPreview, next, rightX,
                          PREVIEW_CENTER_Y);
    } else {
        // 切换:右帧滑到中间,中间帧滑到左槽,新帧从右侧滑入
        // (间隔按实际宽度留 10px)
        const PokemonSprites::SpriteFrame* cur = areaPreviewFrames[currentPreview];
        const PokemonSprites::SpriteFrame* next = areaPreviewFrames[nextPreview];
        uint8_t enteringPreview =
            static_cast<uint8_t>((nextPreview + 1) % poolCount);
        const PokemonSprites::SpriteFrame* entering =
            areaPreviewFrames[enteringPreview];
        int curW = frameWidth(cur);
        int nextW = frameWidth(next);
        int enteringW = frameWidth(entering);
        // 旧槽位(中间为当前帧)
        int nextOldX = PREVIEW_CENTER_X + curW / 2 + PREVIEW_GAP + nextW / 2;
        int curOldX = PREVIEW_CENTER_X;
        int enteringOldX = Hal::DISPLAY_W + enteringW / 2 + 4;
        // 新槽位(右帧成为中间帧)
        int nextNewX = PREVIEW_CENTER_X;
        int curNewX = PREVIEW_CENTER_X - nextW / 2 - PREVIEW_GAP - curW / 2;
        int enteringNewX =
            PREVIEW_CENTER_X + nextW / 2 + PREVIEW_GAP + enteringW / 2;
        int nextX = nextOldX +
                    static_cast<int>(roundf((nextNewX - nextOldX) * progress));
        int curX = curOldX +
                   static_cast<int>(roundf((curNewX - curOldX) * progress));
        int enteringX = enteringOldX +
                        static_cast<int>(
                            roundf((enteringNewX - enteringOldX) * progress));
        drawPreviewMember(areaPreviewPool, currentPreview, cur, curX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(areaPreviewPool, nextPreview, next, nextX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(areaPreviewPool, enteringPreview, entering,
                          enteringX, PREVIEW_CENTER_Y);
    }
    c.clearClipRect();
}

void ExploreScene::renderWalking() {
    auto& c = PixelRenderer::canvas();
    int cameraX = MathUtil::clamp(static_cast<int>(roundf(routeWorldX)) - Hal::DISPLAY_W / 2,
                            0, static_cast<int>(EXPLORE_MAP_W) - Hal::DISPLAY_W);
    int cameraY = MathUtil::clamp(static_cast<int>(roundf(routeWorldY)) - Hal::DISPLAY_H / 2,
                            0, static_cast<int>(EXPLORE_MAP_H) - Hal::DISPLAY_H);
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    uint8_t mapAnimationFrame = static_cast<uint8_t>((Hal::ins().millis() / 140U) % 8U);
    drawGeneratedMapViewport(
        generatedMap, cameraX, cameraY, map.fieldColor, mapAnimationFrame);

    renderRoutePickup(cameraX, cameraY);

    bool bossBehindTeam = false;
    if (routeBossPending) {
        const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
        if (routeBossIndex < path.pointCount) {
            bossBehindTeam =
                routePathPointWorld(path, routeBossIndex).y <= routeWorldY;
        }
    }
    if (bossBehindTeam) renderRouteBoss(cameraX, cameraY);

    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    const Species& activeSpecies = engine.speciesFor(state.team[leaderSlot]);
    if (hasHealthyRouteFollower(state)) {
        const Species& followerSpecies = engine.speciesFor(state.team[1]);
        if (routeFollowerWorldY <= routeWorldY) {
            drawRouteMonster(followerSpecies, routeFollowerWorldX,
                             routeFollowerWorldY, routeFollowerWalkDirection,
                             true, cameraX, cameraY);
            drawRouteMonster(activeSpecies, routeWorldX, routeWorldY,
                             routeWalkDirection, false, cameraX, cameraY);
        } else {
            drawRouteMonster(activeSpecies, routeWorldX, routeWorldY,
                             routeWalkDirection, false, cameraX, cameraY);
            drawRouteMonster(followerSpecies, routeFollowerWorldX,
                             routeFollowerWorldY, routeFollowerWalkDirection,
                             true, cameraX, cameraY);
        }
    } else {
        drawRouteMonster(activeSpecies, routeWorldX, routeWorldY,
                         routeWalkDirection, false, cameraX, cameraY);
    }
    if (routeBossPending && !bossBehindTeam) renderRouteBoss(cameraX, cameraY);

    drawGeneratedMapForegroundViewport(
        generatedMap, cameraX, cameraY, map.fieldColor, mapAnimationFrame);

    const uint16_t panel = PixelRenderer::rgb(8, 10, 14);
    int hudW = MathUtil::max(60, uiTextWidth(map.name) + 12);
    int hudX = Hal::DISPLAY_W - hudW - 4;
    PixelRenderer::fillRectAlpha(
        hudX, 2, hudW, 22, panel, EXPLORE_HUD_ALPHA);
    c.drawRect(hudX, 2, hudW, 22, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(
        hudX + 6, 5, map.name, PixelRenderer::rgb(245, 246, 232), 1);
    if (exploreMenuOpen) renderExploreMenu();
}

void ExploreScene::renderRouteBoss(int cameraX, int cameraY) {
    if (!routeBossPending) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routeBossIndex >= path.pointCount) return;

    const Species* species = findSpecies(expeditionBossSpeciesId);
    if (!species) return;

    RouteWorldPoint anchor = routePathPointWorld(path, routeBossIndex);
    float anchorX = anchor.x;
    float anchorY = anchor.y;
    float tangentX = 0.0f;
    float tangentY = 1.0f;
    if (routeBossIndex + 1 < path.pointCount) {
        RouteWorldPoint next = routePathPointWorld(path, routeBossIndex + 1);
        tangentX = next.x - anchorX;
        tangentY = next.y - anchorY;
    }
    float tangentLength = hypotf(tangentX, tangentY);
    if (tangentLength > 0.01f) {
        tangentX /= tangentLength;
        tangentY /= tangentLength;
    }

    uint32_t nowMs = Hal::ins().millis();
    uint32_t patrolPhase = nowMs % ROUTE_BOSS_PATROL_CYCLE_MS;
    bool movingOut = patrolPhase >= ROUTE_BOSS_PATROL_OUT_START_MS &&
                     patrolPhase < ROUTE_BOSS_PATROL_OUT_END_MS;
    bool movingBack = patrolPhase >= ROUTE_BOSS_PATROL_RETURN_START_MS &&
                      patrolPhase < ROUTE_BOSS_PATROL_RETURN_END_MS;
    bool patrolMoving = movingOut || movingBack;
    float patrolOffset = ROUTE_BOSS_PATROL_DISTANCE *
                         routeBossPatrolPermille(nowMs) / 1000.0f;
    float worldX = anchorX + tangentX * patrolOffset;
    float worldY = anchorY + tangentY * patrolOffset;

    uint8_t walkDirection = static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN);
    if (routeBossPatrolFacesOutward(patrolPhase)) {
        walkDirection = routeDirectionForDelta(tangentX, tangentY, walkDirection);
    } else {
        walkDirection = routeDirectionForDelta(-tangentX, -tangentY, walkDirection);
    }

    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            species->id,
            static_cast<PokemonSprites::WalkDirection>(walkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (patrolMoving) {
            uint32_t moveStarted = movingOut ? ROUTE_BOSS_PATROL_OUT_START_MS
                                             : ROUTE_BOSS_PATROL_RETURN_START_MS;
            uint16_t moveDuration = static_cast<uint16_t>(
                movingOut
                    ? ROUTE_BOSS_PATROL_OUT_END_MS - ROUTE_BOSS_PATROL_OUT_START_MS
                    : ROUTE_BOSS_PATROL_RETURN_END_MS - ROUTE_BOSS_PATROL_RETURN_START_MS);
            uint32_t moveElapsed = patrolPhase - moveStarted;
            frameIndex = PokemonMotion::movementFrame(
                PokemonMotion::behaviorForSpecies(species->id),
                animation.frameCount, moveElapsed, moveElapsed, moveDuration);
        }
        frame = PokemonSprites::findSpeciesSprite(
            species->id,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        flipX = animation.flipX;
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            species->id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (!frame) return;

    constexpr float scale = 0.8f;
    int drawW = MathUtil::max<int>(1, static_cast<int>(roundf(frame->width * scale)));
    int drawH = MathUtil::max<int>(1, static_cast<int>(roundf(frame->height * scale)));
    int centerX = static_cast<int>(roundf(worldX)) - cameraX;
    float airOffsetY = routeAirOffsetY(species->id, Hal::ins().millis(), scale);
    int centerY = static_cast<int>(roundf(worldY - airOffsetY)) - cameraY;
    int drawX = centerX - drawW / 2;
    int drawY = centerY - drawH / 2;
    if (drawX + drawW < -8 || drawX >= Hal::DISPLAY_W + 8 ||
        drawY + drawH < -16 || drawY >= Hal::DISPLAY_H + 8) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    // Keep the shadow at the unshifted ground anchor while the body floats.
    int shadowY = static_cast<int>(roundf(worldY)) - cameraY + drawH / 2 - 10;
    c.fillEllipse(centerX, shadowY, 10, 3,
                  PixelRenderer::rgb(68, 87, 74));
    PokemonSprites::drawFrameScaled(frame, drawX, drawY, scale, flipX);
}

void ExploreScene::drawRouteMonster(const Species& species, float worldX,
                                    float worldY, uint8_t walkDirection,
                                    bool follower, int cameraX, int cameraY) {
    auto& c = PixelRenderer::canvas();
    const PokemonSprites::SpriteFrame* marker = nullptr;
    bool markerFlipX = false;
    int8_t poseOffsetX = 0;
    int8_t poseOffsetY = 0;
    const PokemonMotion::Behavior motion =
        PokemonMotion::behaviorForSpecies(species.id);
    uint8_t* visualWalkDirection = follower
        ? &routeFollowerVisualWalkDirection
        : &routeVisualWalkDirection;
    bool animationActive = false;
    uint32_t movementElapsedMs = 0;
    uint32_t stepElapsedMs = 0;
    uint16_t stepDurationMs = 1;

    if (routeMoving && (!follower || routeFollowerMoving)) {
        uint32_t animationNow = exploreMenuOpen ? exploreMenuOpenedAt : Hal::ins().millis();
        uint32_t elapsed = animationNow - routeMoveStarted;
        uint16_t followerDelayMs = iceSliding ? 0 : ROUTE_FOLLOWER_DELAY_MS;
        if (!follower || elapsed > followerDelayMs) {
            if (follower) elapsed -= followerDelayMs;
            animationActive = true;
            stepDurationMs = MathUtil::max<uint16_t>(
                1, follower ? routeFollowerMoveDurationMs
                            : routeLeaderMoveDurationMs);
            stepElapsedMs = elapsed;
            movementElapsedMs = elapsed;
            if (phase == Phase::EXITING) {
                stepDurationMs = MathUtil::max<uint16_t>(
                    1, routeStepDurationForSpecies(species.id));
                stepElapsedMs %= stepDurationMs;
            } else {
                stepElapsedMs = MathUtil::min<uint32_t>(stepElapsedMs, stepDurationMs - 1);
                uint32_t completedSteps = routeIndex > 0 ? routeIndex - 1 : 0;
                if (follower) {
                    uint8_t followerTargetIndex = routeFollowerTargetIndex(routeIndex);
                    completedSteps = followerTargetIndex > 0
                        ? followerTargetIndex - 1
                        : 0;
                }
                movementElapsedMs = completedSteps * motion.stepDurationMs +
                                    stepElapsedMs;
            }
        }
    }

    if (motion.mode != PokemonMotion::Mode::SLITHER || !animationActive) {
        *visualWalkDirection = walkDirection;
    } else {
        uint16_t cycleMs = PokemonMotion::cycleDurationMs(
            motion, PokemonMotion::PlaybackContext::ROUTE);
        uint8_t motionPhase = PokemonMotion::slitherPhaseIndex(
            movementElapsedMs, cycleMs);
        if (PokemonMotion::slitherDirectionChangeSafe(motionPhase)) {
            *visualWalkDirection = walkDirection;
        }
    }

    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            species.id,
            static_cast<PokemonSprites::WalkDirection>(*visualWalkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (animationActive) {
            if (motion.mode == PokemonMotion::Mode::SLITHER) {
                uint8_t registrationDirection = 0;
                switch (static_cast<PokemonSprites::WalkDirection>(
                            *visualWalkDirection)) {
                case PokemonSprites::WalkDirection::DOWN:
                    registrationDirection = 0;
                    break;
                case PokemonSprites::WalkDirection::LEFT:
                    registrationDirection = 2;
                    break;
                case PokemonSprites::WalkDirection::UP:
                    registrationDirection = 4;
                    break;
                case PokemonSprites::WalkDirection::RIGHT:
                    registrationDirection = 6;
                    break;
                }
                PokemonMotion::Pose pose = PokemonMotion::slitherPose(
                    motion, animation.frameCount, movementElapsedMs,
                    PokemonMotion::cycleDurationMs(
                        motion, PokemonMotion::PlaybackContext::ROUTE),
                    registrationDirection);
                frameIndex = pose.frameIndex;
                poseOffsetX = pose.offsetX;
                poseOffsetY = pose.offsetY;
            } else {
                frameIndex = PokemonMotion::movementFrame(
                    motion, animation.frameCount, movementElapsedMs,
                    stepElapsedMs, stepDurationMs);
            }
        }
        marker = PokemonSprites::findSpeciesSprite(
            species.id,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        markerFlipX = animation.flipX;
    }
    if (!marker) {
        poseOffsetX = 0;
        poseOffsetY = 0;
        marker = PokemonSprites::findSpeciesSprite(
            species.id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (marker) {
        float scale = 0.8f;
        int markerW = static_cast<int>(marker->width * scale);
        int markerH = static_cast<int>(marker->height * scale);
        int markerX = static_cast<int>(roundf(worldX)) - cameraX - markerW / 2;
        float airOffsetY = routeAirOffsetY(species.id, Hal::ins().millis(), scale);
        int markerY = static_cast<int>(roundf(worldY - airOffsetY)) - cameraY - markerH / 2;
        int shadowY = static_cast<int>(roundf(worldY)) - cameraY + markerH / 2 - 10;
        c.fillEllipse(markerX + markerW / 2, shadowY,
                      10, 3,
                      PixelRenderer::rgb(68, 87, 74));
        markerX += static_cast<int>(roundf(poseOffsetX * scale));
        markerY += static_cast<int>(roundf(poseOffsetY * scale));
        PokemonSprites::drawFrameScaled(marker, markerX, markerY, scale, markerFlipX);
    }
}

void ExploreScene::renderRoutePickup(int cameraX, int cameraY) {
    if (!routePickupAvailable) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routePickupIndex >= path.pointCount) return;

    RouteWorldPoint point = routePathPointWorld(path, routePickupIndex);
    int x = static_cast<int>(roundf(point.x)) - cameraX;
    int y = static_cast<int>(roundf(point.y)) - cameraY +
            ROUTE_PICKUP_VISUAL_OFFSET_Y;
    if (x < -13 || x >= Hal::DISPLAY_W + 13 ||
        y < -13 || y >= Hal::DISPLAY_H + 13) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 6, 7, 2, PixelRenderer::rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::EXPLORE_PICKUP_BALL, x, y - 4)) {
        return;
    }
    if (GameAssets::drawCentered(
            GameAssets::Kind::ITEM_POKE_BALL, x, y - 4, 0.62f)) {
        return;
    }
    c.fillCircle(x, y - 4, 7, PixelRenderer::rgb(224, 69, 65));
    for (int row = 0; row <= 6; ++row) {
        int halfWidth = static_cast<int>(
            sqrtf(49.0f - static_cast<float>(row * row)));
        c.drawFastHLine(x - halfWidth, y - 4 + row,
                        halfWidth * 2 + 1, 0xFFFF);
    }
    c.drawCircle(x, y - 4, 7, PixelRenderer::rgb(35, 39, 44));
    c.drawFastHLine(x - 7, y - 4, 14, PixelRenderer::rgb(35, 39, 44));
    c.fillCircle(x, y - 4, 2, 0xFFFF);
    c.drawCircle(x, y - 4, 2, PixelRenderer::rgb(35, 39, 44));
}

void ExploreScene::renderExploreMenu() {
    static_assert(EXPLORE_MENU_ITEM_COUNT ==
                      sizeof(Ui::Explore::SIDE_MENU_ITEMS) /
                          sizeof(Ui::Explore::SIDE_MENU_ITEMS[0]),
                  "explore side menu labels must match the menu item count");
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_W = 60;
    static constexpr int PANEL_X = Hal::DISPLAY_W - PANEL_W;
    static constexpr int PANEL_Y = 0;
    static constexpr int PANEL_H = Hal::DISPLAY_H;
    static constexpr int ROW_H = 30;
    const uint16_t background = PixelRenderer::rgb(20, 25, 32);
    const uint16_t border = PixelRenderer::rgb(190, 200, 205);
    const uint16_t active = PixelRenderer::rgb(255, 216, 72);
    const uint16_t inactive = PixelRenderer::rgb(235, 239, 232);

    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, background);
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, border);
    for (uint8_t i = 0; i < EXPLORE_MENU_ITEM_COUNT; ++i) {
        int y = PANEL_Y + 9 + i * ROW_H;
        bool selected = i == exploreMenuCursor;
        if (selected) c.fillRect(PANEL_X + 5, y, 3, 18, active);
        PixelRenderer::text(PANEL_X + 13, y, Ui::Explore::SIDE_MENU_ITEMS[i],
                            selected ? active : inactive, 1);
    }
}

void ExploreScene::renderEncounter() {
    auto& c = PixelRenderer::canvas();
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    if (!GameAssets::drawBattleBackground(map.battleBackground)) {
        c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(197, 220, 192));
        c.fillRect(0, 78, Hal::DISPLAY_W, 24, PixelRenderer::rgb(228, 225, 190));
    }

    uint32_t nowMs = Hal::ins().millis();
    if (wild) {
        drawMonsterSprite(*wild,
                          BattleLayout::WILD_X, BattleLayout::WILD_GROUND_Y,
                          BattleLayout::WILD_MAX_W, BattleLayout::WILD_MAX_H,
                          false, battleHitShakeX(true, nowMs));
        if (wildHp > 0) {
            drawBattleConditionEffects(BattleLayout::WILD_X,
                                       BattleLayout::WILD_GROUND_Y,
                                       wildRuntime.majorStatus,
                                       wildBattleState, nowMs);
        }
    }
    int playerOffsetX =
        battleHitShakeX(false, nowMs) + battleSwitchOffsetX(nowMs);
    drawMonsterSprite(battlePlayerSpecies(),
                      BattleLayout::PLAYER_X, BattleLayout::PLAYER_GROUND_Y,
                      BattleLayout::PLAYER_MAX_W, BattleLayout::PLAYER_MAX_H,
                      true, playerOffsetX);
    const auto& activeMonster = battlePlayerMonster();
    if (activeMonster.hpCur > 0) {
        drawBattleConditionEffects(BattleLayout::PLAYER_X + playerOffsetX,
                                   BattleLayout::PLAYER_GROUND_Y,
                                   activeMonster.majorStatus,
                                   playerBattleState, nowMs);
    }
    renderFoodThrow();
    renderBattleHud();
    renderCommandBox();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (GameEngine::ins().debugBattleDrawBoundsVisible()) {
        c.drawRect(BattleLayout::WILD_X - BattleLayout::WILD_MAX_W / 2,
                   BattleLayout::WILD_GROUND_Y - BattleLayout::WILD_MAX_H,
                   BattleLayout::WILD_MAX_W, BattleLayout::WILD_MAX_H,
                   PixelRenderer::rgb(255, 0, 0));
        c.drawRect(BattleLayout::PLAYER_X - BattleLayout::PLAYER_MAX_W / 2,
                   BattleLayout::PLAYER_GROUND_Y - BattleLayout::PLAYER_MAX_H,
                   BattleLayout::PLAYER_MAX_W, BattleLayout::PLAYER_MAX_H,
                   PixelRenderer::rgb(0, 220, 255));
    }
#endif
}

void ExploreScene::renderFoodThrow() {
    if (!foodThrowActive) return;

    uint32_t elapsed = Hal::ins().millis() - foodThrowStarted;
    float progress = MathUtil::min<uint32_t>(elapsed, FOOD_THROW_DURATION_MS) /
                     static_cast<float>(FOOD_THROW_DURATION_MS);
    float inverse = 1.0f - progress;
    float x = FOOD_THROW_START_X +
              (FOOD_THROW_END_X - FOOD_THROW_START_X) * progress;
    float y = FOOD_THROW_START_Y +
              (FOOD_THROW_END_Y - FOOD_THROW_START_Y) * progress -
              FOOD_THROW_ARC_HEIGHT * 4.0f * progress * inverse;
    GameAssets::Kind kind = GameAssets::itemKind(
        Game::itemIdForFoodIndex(foodThrowIndex));
    if (!GameAssets::drawCentered(
            kind, static_cast<int>(roundf(x)), static_cast<int>(roundf(y)),
            0.55f)) {
        PixelRenderer::canvas().fillCircle(
            static_cast<int>(roundf(x)), static_cast<int>(roundf(y)), 4,
            PixelRenderer::rgb(240, 174, 76));
    }
}

void ExploreScene::renderResult() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(42, 42, 156, 58, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(42, 42, 156, 58, PixelRenderer::rgb(95, 110, 126));
    const char* result = resultMessage && resultMessage[0]
        ? resultMessage
        : (wild ? wild->name : nullptr);
    if (result) {
        PixelRenderer::text(58, 67, result,
                            PixelRenderer::rgb(241, 242, 232), 1);
    }
}

void ExploreScene::renderFriendshipPrompt() {
    if (friendshipStep != FriendshipStep::CONTACT_CONFIRM &&
        friendshipStep != FriendshipStep::TEAM_CONFIRM) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_W = 60;
    static constexpr int PANEL_X = Hal::DISPLAY_W - PANEL_W;
    static constexpr int PANEL_Y = 34;
    static constexpr int PANEL_H = 67;
    static constexpr int ROW_H = 28;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(20, 25, 32));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(190, 200, 205));

    static constexpr const char* ITEMS[] = {
        Ui::Explore::FRIEND_YES,
        Ui::Explore::FRIEND_NO,
    };
    for (uint8_t i = 0; i < 2; ++i) {
        bool selected = friendshipConfirmYes == (i == 0);
        int y = PANEL_Y + 7 + i * ROW_H;
        if (selected) {
            c.fillRect(PANEL_X + 5, y, 3, 18,
                       PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(
            PANEL_X + 17, y, ITEMS[i],
            selected ? PixelRenderer::rgb(255, 216, 72)
                     : PixelRenderer::rgb(235, 239, 232),
            1);
    }
}

void ExploreScene::renderSpecialPrompt() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 22;
    static constexpr int PANEL_Y = 24;
    static constexpr int PANEL_W = 196;
    static constexpr int PANEL_H = 88;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(241, 242, 232));

    const Species* species = findSpecies(expeditionBossSpeciesId);
    char line[48] = {};
    snprintf(line, sizeof(line), Ui::Explore::SPECIAL_FOUND_FMT,
             species ? species->name : "");
    PixelRenderer::text(
        PANEL_X + (PANEL_W - uiTextWidth(line)) / 2,
        PANEL_Y + 12, line, PixelRenderer::rgb(72, 220, 255), 1);
    PixelRenderer::text(
        PANEL_X + (PANEL_W -
                   uiTextWidth(Ui::Explore::SPECIAL_CHALLENGE_QUESTION)) / 2,
        PANEL_Y + 36, Ui::Explore::SPECIAL_CHALLENGE_QUESTION,
        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t challengeColor = specialChallengeYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t bypassColor = specialChallengeYes
        ? PixelRenderer::rgb(156, 164, 176)
        : PixelRenderer::rgb(255, 216, 72);
    PixelRenderer::text(PANEL_X + 45, PANEL_Y + 61,
                        Ui::Explore::SPECIAL_CHALLENGE, challengeColor, 1);
    PixelRenderer::text(PANEL_X + 130, PANEL_Y + 61,
                        Ui::Explore::SPECIAL_BYPASS, bypassColor, 1);
}

void ExploreScene::renderPickupPrompt() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 36;
    static constexpr int PANEL_Y = 40;
    static constexpr int PANEL_W = 168;
    static constexpr int PANEL_H = 54;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(241, 242, 232));
    if (resultMessage && resultMessage[0]) {
        PixelRenderer::text(PANEL_X + 14, PANEL_Y + 19, resultMessage,
                            PixelRenderer::rgb(255, 216, 72), 1);
    }
}

void ExploreScene::renderEndPrompt() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 34;
    static constexpr int PANEL_Y = 35;
    static constexpr int PANEL_W = 172;
    static constexpr int PANEL_H = 66;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(241, 242, 232));
    PixelRenderer::text(88, PANEL_Y + 13, Ui::Explore::RESULT_END,
                        PixelRenderer::rgb(255, 216, 72), 1);
    int returnY = PANEL_Y + 39;
    if (adventureBondGain > 0) {
        char bondBuf[24];
        snprintf(bondBuf, sizeof(bondBuf), Ui::Explore::BOND_GAIN_FMT,
                 adventureBondGain);
        PixelRenderer::text(
            (Hal::DISPLAY_W - uiTextWidth(bondBuf)) / 2,
            PANEL_Y + 33, bondBuf, PixelRenderer::rgb(255, 154, 181), 1);
        returnY = PANEL_Y + 50;
    }
    PixelRenderer::text(64, returnY, Ui::Explore::ANY_KEY_RETURN,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

void ExploreScene::drawMonsterSprite(const Species& species, int x, int groundY,
                                     int maxWidth, int maxHeight, bool back,
                                     int spriteOffsetX) {
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, back ? PokemonSprites::SpriteKind::BACK : PokemonSprites::SpriteKind::FRONT);
    if (!frame) return;

    uint8_t w = FlashStorage::readByte(&frame->width);
    uint8_t h = FlashStorage::readByte(&frame->height);
    float scale = 1.0f;
    if (w * scale > maxWidth) scale = static_cast<float>(maxWidth) / w;
    if (h * scale > maxHeight) scale = static_cast<float>(maxHeight) / h;

    int drawW = MathUtil::max<int>(1, static_cast<int>(roundf(w * scale)));
    int drawH = MathUtil::max<int>(1, static_cast<int>(roundf(h * scale)));
    int drawX = x - drawW / 2 + spriteOffsetX;
    int drawY = groundY - drawH;

    if (scale < 0.999f || scale > 1.001f) {
        PokemonSprites::drawFrameScaled(frame, drawX, drawY, scale);
    } else {
        PokemonSprites::drawFrame(frame, drawX, drawY);
    }
}

void ExploreScene::renderBattleHud() {
    auto& c = PixelRenderer::canvas();
    char buf[24];
    auto drawHpWithStatus = [](int statusX, int statusY, int hpX, int hpY, int hpWidth,
                               uint16_t hp, uint16_t hpMax, Game::MajorStatus status) {
        GameAssets::Kind statusAsset = GameAssets::statusKind(status);
        if (statusAsset != GameAssets::Kind::COUNT) {
            GameAssets::draw(statusAsset, statusX, statusY);
        }
        PixelRenderer::bar(hpX, hpY, hpWidth, 6,
                           hpMax ? (hp * 100 / hpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112),
                           PixelRenderer::rgb(59, 70, 59));
    };

    if (wild) {
        uint16_t shownWildHp = battleHpForRender(true, wildHp, Hal::ins().millis());
        drawBattleDarkText(6, 5, wild->name);
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, wildRuntime.level);
        drawBattleAsciiRightAligned(114, 5, buf);
        drawHpWithStatus(12, 20, 28, 23, 74, shownWildHp, wildHpMax,
                         wildRuntime.majorStatus);
    }

    const auto& active = battlePlayerMonster();
    const Species& species = battlePlayerSpecies();
    bool showVictoryExp = wild && wildHp == 0 && battleResultPending &&
                          battleExpVisible;
    bool holdPreRewardLevel = wild && wildHp == 0 && battleResultPending &&
                              expAnimationPending;
    uint32_t shownExp = showVictoryExp
        ? battleExpForRender(Hal::ins().millis())
        : (holdPreRewardLevel ? expAnimationFrom : active.exp);
    uint8_t shownLevel = (showVictoryExp || holdPreRewardLevel)
        ? levelForExp(species.growthRate, shownExp)
        : active.level;

    drawBattleDarkText(120, BattleLayout::PLAYER_NAME_Y, species.name);
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, shownLevel);
    drawBattleAsciiRightAligned(228, BattleLayout::PLAYER_NAME_Y, buf);

    if (!showVictoryExp) {
        uint16_t shownPlayerHp = battleHpForRender(
            false, active.hpCur, Hal::ins().millis());
        drawHpWithStatus(128, 82, 144, 85, 86, shownPlayerHp, active.hpMax,
                         active.majorStatus);
        return;
    }

    uint32_t levelFloor = minimumExpForLevel(species.growthRate, shownLevel);
    uint32_t levelCeiling = shownLevel >= Game::LEVEL_MAX
        ? levelFloor
        : minimumExpForLevel(species.growthRate, shownLevel + 1);
    uint8_t expPercent = shownLevel >= Game::LEVEL_MAX
        ? 100
        : static_cast<uint8_t>(MathUtil::min<uint32_t>(
            100,
            (shownExp - levelFloor) * 100UL / MathUtil::max<uint32_t>(1, levelCeiling - levelFloor)));
    drawBattleCompactExpLabel(
        BattleLayout::EXP_LABEL_X, BattleLayout::EXP_LABEL_Y,
        PixelRenderer::rgb(35, 86, 126));
    c.fillRect(BattleLayout::EXP_BAR_X, BattleLayout::EXP_BAR_Y,
               BattleLayout::EXP_BAR_W, BattleLayout::EXP_BAR_H,
               PixelRenderer::rgb(38, 51, 59));
    c.fillRect(BattleLayout::EXP_BAR_X + 1, BattleLayout::EXP_BAR_Y + 1,
               BattleLayout::EXP_BAR_W - 2, BattleLayout::EXP_BAR_H - 2,
               PixelRenderer::rgb(184, 198, 194));
    int expFill = (BattleLayout::EXP_BAR_W - 2) * expPercent / 100;
    if (expFill > 0) {
        c.fillRect(BattleLayout::EXP_BAR_X + 1, BattleLayout::EXP_BAR_Y + 1,
                   expFill, BattleLayout::EXP_BAR_H - 2,
                   PixelRenderer::rgb(35, 118, 184));
    }
}

void ExploreScene::renderCommandBox() {
    auto& c = PixelRenderer::canvas();
    PixelRenderer::fillRectAlpha(
        0, BattleLayout::FOOTER_Y, Hal::DISPLAY_W, BattleLayout::FOOTER_H,
        PixelRenderer::rgb(204, 204, 204), BATTLE_FOOTER_ALPHA);
    c.drawRect(0, BattleLayout::FOOTER_Y, Hal::DISPLAY_W, BattleLayout::FOOTER_H,
               PixelRenderer::rgb(74, 91, 75));

    if (battleLogActive || battleLogVisibleCount > 0) {
        for (uint8_t i = 0; i < battleLogVisibleCount; ++i) {
            drawBattleFooterDarkText(12, 101 + i * 16, battleLogVisible[i]);
        }
        return;
    }
    if (phase != Phase::ENCOUNTER) return;
    if (defeatAwaitInput) {
        drawBattleFooterDarkText(72, 109, Ui::Explore::ANY_KEY_RETURN);
        return;
    }
    if (battleTurnStage != BattleTurnStage::IDLE ||
        battleSwitchStage != BattleSwitchStage::NONE) {
        return;
    }

    static constexpr int commandY = 109;
    static constexpr int xs[] = {18, 74, 126, 190};
    static constexpr const char* items[] = {
        Ui::Explore::CMD_BATTLE,
        Ui::Explore::CMD_BAG,
        Ui::Explore::CMD_SWITCH,
        Ui::Explore::CMD_FLEE,
    };
    for (uint8_t i = 0; i < 4; ++i) {
        if (battleCursor == i) {
            drawBattleFooterDarkText(xs[i] - 10, commandY, ">");
        }
        drawBattleFooterDarkText(xs[i], commandY, items[i]);
    }
}
