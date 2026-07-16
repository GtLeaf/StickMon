#include "scenes/ExploreScene.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "assets/GameAssets.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/ProgressionUi.h"
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

struct PickupWeights {
    uint16_t ball;
    uint16_t coin;
    uint16_t potion;
    uint16_t superPotion;
    uint16_t antidote;
    uint16_t candy;
};

struct EncounterEntry {
    uint16_t speciesId;
    uint8_t weight;
    uint8_t minLevel;
    uint8_t maxLevel;
};

constexpr uint16_t pickupWeightTotal(const PickupWeights& weights) {
    return weights.ball + weights.coin + weights.potion + weights.superPotion +
           weights.antidote + weights.candy;
}

template <size_t N>
constexpr uint16_t encounterWeightTotal(const EncounterEntry (&entries)[N], size_t index = 0) {
    return index == N ? 0 : entries[index].weight + encounterWeightTotal(entries, index + 1);
}

template <size_t N>
constexpr bool encounterLevelRangesValid(const EncounterEntry (&entries)[N], size_t index = 0) {
    return index == N ||
           (entries[index].minLevel >= 1 &&
            entries[index].minLevel <= entries[index].maxLevel &&
            entries[index].maxLevel <= Game::LEVEL_MAX &&
            encounterLevelRangesValid(entries, index + 1));
}

static constexpr uint8_t WILD_LEVEL_MIN = 1;
static constexpr uint8_t WILD_LEVEL_MAX = Game::LEVEL_MAX;
static constexpr uint8_t WILD_LEVEL_VARIANCE = 2;
static constexpr uint16_t DEPTH_MIDDLE_START_PERMILLE = 333;
static constexpr uint16_t DEPTH_DEEP_START_PERMILLE = 667;
static constexpr uint8_t ENCOUNTER_COOLDOWN_STEP_COUNT = 5;
static constexpr uint8_t MAX_ENCOUNTERS_PER_MAP = 2;
static constexpr uint16_t MAP_PICKUP_CHANCE = 6500;
static constexpr uint32_t MAP_GENERATION_SAFE_SEED = 1;
static constexpr uint32_t MAP_GENERATION_RETRY_SALTS[] = {
    0,
    0x6D2B79F5U,
    0x9E3779B9U,
    0x85EBCA6BU,
};

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

static constexpr Game::ItemId CAPTURE_BALLS[] = {
    Game::ItemId::POKE_BALL,
    Game::ItemId::GREAT_BALL,
    Game::ItemId::HEAVY_BALL,
    Game::ItemId::TIMER_BALL,
};
static constexpr uint8_t CAPTURE_BALL_COUNT = sizeof(CAPTURE_BALLS) / sizeof(CAPTURE_BALLS[0]);
static constexpr uint32_t CAPTURE_ANIMATION_MS = 2200;
static constexpr uint32_t EXP_ANIMATION_MS = 900;
static constexpr uint32_t BATTLE_HIT_DELAY_MS = 260;
static constexpr uint32_t BATTLE_HIT_SHAKE_MS = 280;
static constexpr uint32_t BATTLE_HP_DRAIN_MS = 420;
static constexpr uint32_t BATTLE_ACTION_MS =
    BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS + BATTLE_HP_DRAIN_MS;
static constexpr uint8_t BATTLE_WIN_COIN_REWARD = 10;
static constexpr uint8_t EXP_SHARE_PERCENT = 50;
static constexpr uint8_t EXPLORE_MAP_TILES_W = 16;
static constexpr uint8_t EXPLORE_MAP_TILES_H = 12;
static constexpr uint16_t EXPLORE_TILE_SIZE = 26;
static constexpr uint16_t EXPLORE_MAP_W = EXPLORE_MAP_TILES_W * EXPLORE_TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_H = EXPLORE_MAP_TILES_H * EXPLORE_TILE_SIZE;
static constexpr float ROUTE_FOLLOWER_START_OFFSET = EXPLORE_TILE_SIZE;
static constexpr uint16_t ROUTE_FOLLOWER_DELAY_MS = 120;
static constexpr uint8_t ROUTE_FOLLOWER_GAP_STEPS = 2;
static constexpr int ROUTE_PICKUP_VISUAL_OFFSET_Y = 6;
static constexpr float ROUTE_EXIT_MARGIN = EXPLORE_TILE_SIZE * 2.0f;
static constexpr uint8_t EXPLORE_HUD_ALPHA = 150;
static constexpr int BATTLE_ASCII_ADVANCE = 8;
static constexpr uint8_t BATTLE_FOOTER_ALPHA = 153;

struct BattleLayout {
    static constexpr int FOOTER_Y = 99;
    static constexpr int FOOTER_H = Hal::DISPLAY_H - FOOTER_Y;

    static constexpr int WILD_X = 178;
    static constexpr int WILD_GROUND_Y = 62;
    static constexpr int WILD_MAX_W = 100;
    static constexpr int WILD_MAX_H = 58;
    static constexpr int WILD_MIN_TARGET_H = 42;
    static constexpr int WILD_MAX_UPSCALE_PERCENT = 150;

    static constexpr int PLAYER_X = 58;
    static constexpr int PLAYER_GROUND_Y = 97;
    static constexpr int PLAYER_MAX_W = 100;
    static constexpr int PLAYER_MAX_H = 62;
};

void drawBattleText(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::textOutlined(
        x, y, value, color, PixelRenderer::rgb(255, 255, 255), 1);
}

void drawBattleDarkText(int x, int y, const char* value) {
    drawBattleText(x, y, value, PixelRenderer::rgb(25, 31, 40));
}

void drawBattleAsciiRightAligned(int rightX, int y, const char* value) {
    drawBattleDarkText(rightX - static_cast<int>(strlen(value)) * BATTLE_ASCII_ADVANCE,
                       y, value);
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
    PickupWeights pickupWeights;
    const EncounterEntry* encounters;
    uint8_t encounterCount;
    uint16_t fieldColor;
    uint16_t accentColor;
};

static constexpr EncounterEntry GRASS_PATH_ENCOUNTERS[] = {
    {10, 27, 1, 6},    // Caterpie (before Lv.7 evolution)
    {161, 25, 1, 9},   // Sentret
    {16, 22, 2, 9},    // Pidgey
    {261, 15, 2, 9},   // Poochyena
    {172, 8, 3, 9},    // Pichu
    {133, 2, 3, 9},    // Eevee
    {1, 1, 3, 9},      // Bulbasaur
};

static constexpr EncounterEntry CREEK_SLOPE_ENCOUNTERS[] = {
    {194, 23, 7, 17},  // Wooper
    {298, 16, 7, 15},  // Azurill
    {183, 10, 8, 17},  // Marill
    {278, 19, 7, 17},  // Wingull
    {129, 16, 7, 17},  // Magikarp (before Lv.20 evolution)
    {74, 12, 8, 17},   // Geodude
    {16, 2, 7, 17},    // Pidgey
    {147, 1, 10, 17},  // Dratini
    {7, 1, 7, 15},     // Squirtle (before Lv.16 evolution)
};

static constexpr EncounterEntry TALL_GRASS_PARK_ENCOUNTERS[] = {
    {12, 34, 17, 27},  // Butterfree
    {285, 25, 17, 22}, // Shroomish (before Lv.23 evolution)
    {25, 15, 17, 27},  // Pikachu
    {162, 12, 17, 27}, // Furret
    {281, 8, 20, 29},  // Kirlia (before Lv.30 evolution)
    {133, 3, 17, 27},  // Eevee
    {123, 2, 17, 27},  // Scyther
    {2, 1, 17, 31},    // Ivysaur (before Lv.32 evolution)
};

static constexpr EncounterEntry MIST_FOREST_PATH_ENCOUNTERS[] = {
    {94, 27, 41, 53},  // Gengar
    {169, 23, 41, 53}, // Crobat
    {286, 18, 41, 53}, // Breloom
    {262, 15, 41, 53}, // Mightyena
    {282, 9, 41, 53},  // Gardevoir
    {212, 4, 41, 53},  // Scizor
    {197, 4, 41, 53},  // Umbreon
};

static constexpr EncounterEntry ANCIENT_WATERFALL_VALLEY_ENCOUNTERS[] = {
    {76, 29, 53, 67},  // Golem
    {169, 22, 53, 67}, // Crobat
    {323, 21, 53, 67}, // Camerupt
    {94, 13, 53, 67},  // Gengar
    {195, 8, 53, 67},  // Quagsire
    {212, 4, 53, 67},  // Scizor
    {149, 2, 53, 67},  // Dragonite
    {6, 1, 53, 67},    // Charizard
};

static constexpr EncounterEntry FROST_CRYSTAL_CAVE_ENCOUNTERS[] = {
    {361, 56, 28, 41}, // Snorunt (before Lv.42 evolution)
    {42, 18, 28, 41},  // Golbat
    {75, 14, 28, 41},  // Graveler
    {93, 8, 28, 41},   // Haunter
    {282, 4, 30, 42},  // Gardevoir
};

static_assert(encounterWeightTotal(GRASS_PATH_ENCOUNTERS) == 100,
              "grass path encounter weights must sum to 100");
static_assert(encounterWeightTotal(CREEK_SLOPE_ENCOUNTERS) == 100,
              "creek slope encounter weights must sum to 100");
static_assert(encounterWeightTotal(TALL_GRASS_PARK_ENCOUNTERS) == 100,
              "tall grass park encounter weights must sum to 100");
static_assert(encounterWeightTotal(MIST_FOREST_PATH_ENCOUNTERS) == 100,
              "mist forest path encounter weights must sum to 100");
static_assert(encounterWeightTotal(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS) == 100,
              "ancient waterfall valley encounter weights must sum to 100");
static_assert(encounterWeightTotal(FROST_CRYSTAL_CAVE_ENCOUNTERS) == 100,
              "frost crystal cave encounter weights must sum to 100");
static_assert(encounterLevelRangesValid(GRASS_PATH_ENCOUNTERS) &&
                  encounterLevelRangesValid(CREEK_SLOPE_ENCOUNTERS) &&
                  encounterLevelRangesValid(TALL_GRASS_PARK_ENCOUNTERS) &&
                  encounterLevelRangesValid(FROST_CRYSTAL_CAVE_ENCOUNTERS) &&
                  encounterLevelRangesValid(MIST_FOREST_PATH_ENCOUNTERS) &&
                  encounterLevelRangesValid(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS),
              "explore encounter levels must stay within the global level range");

#define ENTRY_COUNT(entriesValue) \
    static_cast<uint8_t>(sizeof(entriesValue) / sizeof(entriesValue[0]))

static constexpr RouteMap ROUTE_MAPS[] = {
    {
        Ui::Explore::GRASS_PATH,
        Ui::Explore::AREA_DESCS[0],
        GameAssets::Kind::BATTLE_BG_GRASS,
        5,
        2,
        3,
        4,
        500,
        {250, 250, 167, 42, 83, 42},
        GRASS_PATH_ENCOUNTERS,
        ENTRY_COUNT(GRASS_PATH_ENCOUNTERS),
        0x2227,
        0x5EEE,
    },
    {
        Ui::Explore::CREEK_SLOPE,
        Ui::Explore::AREA_DESCS[1],
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        12,
        3,
        4,
        5,
        600,
        {152, 254, 254, 101, 118, 51},
        CREEK_SLOPE_ENCOUNTERS,
        ENTRY_COUNT(CREEK_SLOPE_ENCOUNTERS),
        0x224A,
        0x4D38,
    },
    {
        Ui::Explore::TALL_GRASS_PARK,
        Ui::Explore::AREA_DESCS[2],
        GameAssets::Kind::BATTLE_BG_GRASS,
        22,
        3,
        4,
        6,
        700,
        {260, 240, 180, 100, 100, 80},
        TALL_GRASS_PARK_ENCOUNTERS,
        ENTRY_COUNT(TALL_GRASS_PARK_ENCOUNTERS),
        0x2A66,
        0x8EA9,
    },
    {
        Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::AREA_DESCS[3],
        GameAssets::Kind::BATTLE_BG_SNOW,
        34,
        4,
        5,
        7,
        900,
        {91, 137, 160, 183, 114, 91},
        FROST_CRYSTAL_CAVE_ENCOUNTERS,
        ENTRY_COUNT(FROST_CRYSTAL_CAVE_ENCOUNTERS),
        0xB6DB,
        0x5D7F,
    },
    {
        Ui::Explore::MIST_FOREST_PATH,
        Ui::Explore::AREA_DESCS[4],
        GameAssets::Kind::BATTLE_BG_DEEP_FOREST,
        47,
        4,
        6,
        8,
        1100,
        {130, 183, 183, 183, 183, 52},
        MIST_FOREST_PATH_ENCOUNTERS,
        ENTRY_COUNT(MIST_FOREST_PATH_ENCOUNTERS),
        0x1945,
        0x644D,
    },
    {
        Ui::Explore::ANCIENT_WATERFALL_VALLEY,
        Ui::Explore::AREA_DESCS[5],
        GameAssets::Kind::BATTLE_BG_RIVERSIDE,
        60,
        5,
        7,
        9,
        1300,
        {118, 176, 147, 206, 147, 88},
        ANCIENT_WATERFALL_VALLEY_ENCOUNTERS,
        ENTRY_COUNT(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS),
        0x1987,
        0x54F7,
    },
};
#undef ENTRY_COUNT

static constexpr uint8_t ROUTE_MAP_COUNT = sizeof(ROUTE_MAPS) / sizeof(ROUTE_MAPS[0]);

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
            pickupWeightTotal(maps[index].pickupWeights) > 0 &&
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
float routeWorldCoordinate(uint8_t tile) {
    return tile * EXPLORE_TILE_SIZE + EXPLORE_TILE_SIZE * 0.5f;
}

constexpr uint8_t routeFollowerTargetIndex(uint8_t leaderIndex) {
    return leaderIndex >= ROUTE_FOLLOWER_GAP_STEPS
        ? leaderIndex - ROUTE_FOLLOWER_GAP_STEPS
        : 0;
}

bool hasHealthyRouteFollower(const Game::GameState& state) {
    if (state.teamCount <= 1) return false;
    const Game::MonsterRuntime& follower = state.team[1];
    return !follower.fainted && follower.hpCur > 0;
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
    return static_cast<uint16_t>(min<uint32_t>(duration, 60000U));
}

const RouteMap& routeMap(uint8_t index) {
    return ROUTE_MAPS[index < ROUTE_MAP_COUNT ? index : 0];
}

uint8_t rollPickupId(const PickupWeights& weights) {
    bool candyAvailable = GameEngine::ins().gameState().stepsToday >= 5000;
    uint16_t total = weights.ball + weights.coin + weights.potion +
                     weights.superPotion + weights.antidote +
                     (candyAvailable ? weights.candy : 0);
    if (total == 0) return PICKUP_BALL;

    uint16_t roll = static_cast<uint16_t>(random(0, total));
    if (roll < weights.ball) return PICKUP_BALL;
    roll -= weights.ball;
    if (roll < weights.coin) return PICKUP_COIN;
    roll -= weights.coin;
    if (roll < weights.potion) return PICKUP_POTION;
    roll -= weights.potion;
    if (roll < weights.superPotion) return PICKUP_SUPER_POTION;
    roll -= weights.superPotion;
    if (roll < weights.antidote) return PICKUP_ANTIDOTE;
    return candyAvailable ? PICKUP_RARE_CANDY : PICKUP_BALL;
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
                if (!GameAssets::drawExploreTile(tileId, x, y, animationFrame)) {
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

uint8_t rollWildLevel(uint8_t minLevel, uint8_t maxLevel, uint8_t targetLevel) {
    if (minLevel < WILD_LEVEL_MIN) minLevel = WILD_LEVEL_MIN;
    if (maxLevel > WILD_LEVEL_MAX) maxLevel = WILD_LEVEL_MAX;
    if (maxLevel < minLevel) maxLevel = minLevel;
    if (targetLevel < minLevel) targetLevel = minLevel;
    if (targetLevel > maxLevel) targetLevel = maxLevel;

    uint8_t roll = static_cast<uint8_t>(random(0, 100));
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
    phase = Phase::SELECT;
    areaCursor = 0;
    resultMessage = nullptr;
    defeatAwaitInput = false;
    captureMenuOpen = false;
    captureAnimationActive = false;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    autoWalkActive = false;
    walkStepResolutionPending = false;
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
    if (engine.exploreTravelPhase() == ExploreTravelPhase::DEPARTING) {
        uint8_t area = engine.pendingExploreArea();
        if (area >= static_cast<uint8_t>(Area::COUNT)) area = 0;
        activeArea = static_cast<Area>(area);
        areaCursor = area;
        engine.markExploreActive();
        resetWalk();
    }
}

void ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    if (exploreSubViewOpen) {
        exploreSubView.update(nowMs, dtSeconds);
        return;
    }
    updateRouteMovement(nowMs);
    updateExpAnimation(nowMs);
    serviceBattleLog(nowMs);
    updateBattleTurn(nowMs);
    updateCaptureAnimation(nowMs);
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    if (exploreSubViewOpen) {
        exploreSubView.onButton(event);
        if (exploreSubView.exploreViewClosed()) {
            exploreSubViewOpen = false;
        }
        return true;
    }

    if (phase == Phase::ENDING) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::PRESSED) {
            GameEngine::ins().beginExploreReturn(false);
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

    if ((phase == Phase::LEVEL_UP || phase == Phase::LEARN_MOVE || phase == Phase::ENCOUNTER) &&
        event.action == BtnAction::LONG_PRESS) {
        return true;
    }

    if (phase == Phase::WALKING && exploreMenuOpen) {
        if (event.action == BtnAction::LONG_PRESS) return true;
        if (event.action != BtnAction::PRESSED) return false;
        if (event.btn == 0) {
            if (exploreMenuCursor == 0) {
                exploreSubView.openExploreTeamView();
                exploreSubViewOpen = true;
            } else if (exploreMenuCursor == 1) {
                exploreSubView.openExploreBagView();
                exploreSubViewOpen = true;
            } else if (exploreMenuCursor == 2) {
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
        return true;
    }

    if (phase != Phase::WALKING && (event.btn == 0 || event.btn == 1) &&
        event.action == BtnAction::LONG_PRESS) {
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
            beginAutoWalk();
            return true;
        }
    }

    if (phase == Phase::ENCOUNTER) {
        if (captureAnimationActive) return true;
        if (battleLogBusy()) return true;
        if (defeatAwaitInput) {
            if (debugBattleMode) returnToDebugMenu();
            else requestExploreExit(true);
            return true;
        }
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
            return true;
        }
        if (event.btn == 1) {
            battleCursor = (battleCursor + 1) % 3;
            return true;
        }
    }

    if (phase == Phase::LEVEL_UP) {
        if (event.btn == 0) {
            GameEngine::ins().acknowledgePendingLevelUp();
            if (GameEngine::ins().hasPendingMoveLearn()) {
                learnCursor = 0;
                phase = Phase::LEARN_MOVE;
            } else {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::LEARN_MOVE) {
        if (event.btn == 0) {
            GameEngine::ins().resolvePendingMoveLearn(learnCursor == 0);
            if (GameEngine::ins().hasPendingMoveLearn()) {
                learnCursor = 0;
            } else {
                finishProgression();
            }
            return true;
        }
        if (event.btn == 1) {
            learnCursor = (learnCursor + 1) % 2;
            return true;
        }
    }

    if (phase == Phase::RESULT) {
        if (debugBattleMode && (event.btn == 0 || event.btn == 1)) {
            returnToDebugMenu();
            return true;
        }
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

void ExploreScene::returnToDebugMenu() {
    debugBattleMode = false;
    GameEngine::ins().endDebugBattle();
}

void ExploreScene::beginRouteExit() {
    if (phase == Phase::EXITING || phase == Phase::ENDING) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];

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
    uint16_t leaderStepDuration = PokemonMotion::behaviorForSpecies(
        engine.activeSpecies().id).stepDurationMs;
    routeLeaderMoveDurationMs = routeDurationForDistance(
        routeFromX, routeFromY, routeTargetX, routeTargetY, leaderStepDuration);
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoving = false;
    const Game::GameState& state = engine.gameState();
    if (hasHealthyRouteFollower(state)) {
        uint16_t followerStepDuration = PokemonMotion::behaviorForSpecies(
            engine.speciesFor(state.team[1]).id).stepDurationMs;
        routeFollowerMoveDurationMs = routeDurationForDistance(
            routeFollowerFromX, routeFollowerFromY,
            routeFollowerTargetX, routeFollowerTargetY,
            followerStepDuration);
        routeFollowerMoving = true;
        routeMoveDurationMs = max<uint16_t>(
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
    routeMoveStarted = Hal::ins().millis();
    routeMoving = true;
    phase = Phase::EXITING;
}

void ExploreScene::requestExploreExit(bool fainted, bool showEndPrompt) {
    if (debugBattleMode) {
        returnToDebugMenu();
        return;
    }
    if (phase == Phase::SELECT &&
        GameEngine::ins().exploreTravelPhase() != ExploreTravelPhase::ACTIVE) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return;
    }
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool returnFainted = fainted || mon.fainted || mon.hpCur == 0;
    routeMoving = false;
    autoWalkActive = false;
    if (returnFainted || !showEndPrompt) {
        exploreMenuOpen = false;
        exploreSubViewOpen = false;
        exploreMenuOpenedAt = 0;
        GameEngine::ins().beginExploreReturn(returnFainted);
        return;
    }
    if (phase == Phase::ENDING) return;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    phase = Phase::ENDING;
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

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    ++routeIndex;
    ExploreMapGenerator::Point target = path.points[routeIndex];
    routeTargetX = routeWorldCoordinate(target.x);
    routeTargetY = routeWorldCoordinate(target.y);
    routeFollowerTargetX = routeFollowerFromX;
    routeFollowerTargetY = routeFollowerFromY;
    if (routeIndex >= ROUTE_FOLLOWER_GAP_STEPS) {
        const ExploreMapGenerator::Point& followerTarget =
            path.points[routeFollowerTargetIndex(routeIndex)];
        routeFollowerTargetX = routeWorldCoordinate(followerTarget.x);
        routeFollowerTargetY = routeWorldCoordinate(followerTarget.y);
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
    routeLeaderMoveDurationMs = PokemonMotion::behaviorForSpecies(
        engine.activeSpecies().id).stepDurationMs;
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    const Game::GameState& state = engine.gameState();
    if (hasHealthyRouteFollower(state)) {
        routeFollowerMoveDurationMs = PokemonMotion::behaviorForSpecies(
            engine.speciesFor(state.team[1]).id).stepDurationMs;
        if (routeFollowerMoving) {
            routeMoveDurationMs = max<uint16_t>(
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

void ExploreScene::updateRouteMovement(uint32_t nowMs) {
    if (!routeMoving || exploreMenuOpen) return;
    uint32_t elapsed = nowMs - routeMoveStarted;
    uint16_t leaderDurationMs = max<uint16_t>(1, routeLeaderMoveDurationMs);
    float leaderProgress = min(
        1.0f, elapsed / static_cast<float>(leaderDurationMs));
    const PokemonMotion::Behavior leaderMotion =
        PokemonMotion::behaviorForSpecies(GameEngine::ins().activeSpecies().id);
    float leaderEased = phase == Phase::EXITING
        ? leaderProgress
        : PokemonMotion::stepPosition(leaderMotion, leaderProgress);
    routeWorldX = routeFromX + (routeTargetX - routeFromX) * leaderEased;
    routeWorldY = routeFromY + (routeTargetY - routeFromY) * leaderEased;

    if (routeFollowerMoving) {
        uint32_t followerElapsed = elapsed > ROUTE_FOLLOWER_DELAY_MS
            ? elapsed - ROUTE_FOLLOWER_DELAY_MS
            : 0;
        uint16_t followerDurationMs = max<uint16_t>(1, routeFollowerMoveDurationMs);
        float followerProgress = elapsed > ROUTE_FOLLOWER_DELAY_MS
            ? min(1.0f, followerElapsed / static_cast<float>(followerDurationMs))
            : 0.0f;
        const Game::GameState& state = GameEngine::ins().gameState();
        const PokemonMotion::Behavior followerMotion =
            PokemonMotion::behaviorForSpecies(
                GameEngine::ins().speciesFor(state.team[1]).id);
        float followerEased = phase == Phase::EXITING
            ? followerProgress
            : PokemonMotion::stepPosition(followerMotion, followerProgress);
        routeFollowerWorldX = routeFollowerFromX +
                              (routeFollowerTargetX - routeFollowerFromX) * followerEased;
        routeFollowerWorldY = routeFollowerFromY +
                              (routeFollowerTargetY - routeFollowerFromY) * followerEased;
    }
    if (elapsed < routeMoveDurationMs) return;

    routeWorldX = routeTargetX;
    routeWorldY = routeTargetY;
    routeFollowerWorldX = routeFollowerTargetX;
    routeFollowerWorldY = routeFollowerTargetY;
    routeMoving = false;
    routeFollowerMoving = false;
    if (phase == Phase::EXITING) {
        phase = Phase::ENDING;
        return;
    }
    if (enterPendingProgression(Phase::WALKING)) {
        walkStepResolutionPending = true;
        return;
    }
    finishCompletedWalkStep();
}

void ExploreScene::finishCompletedWalkStep() {
    bool encounterBlockedThisStep = encounterCooldownSteps > 0;
    encounterCooldownSteps = cooldownAfterCompletedStep(encounterCooldownSteps);

    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (exploreMenuOpen) return;
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
    bool guaranteeEncounter = routeGuaranteedEncounterPending &&
                              routeIndex >= routeGuaranteedEncounterIndex;
    if (!encounterBlockedThisStep && rollRandomEncounter(guaranteeEncounter)) {
        autoWalkActive = false;
        return;
    }
    if (autoWalkActive && phase == Phase::WALKING) walk();
}

void ExploreScene::finishProgression() {
    phase = progressionReturnPhase;
    if (phase != Phase::WALKING || !walkStepResolutionPending) return;
    walkStepResolutionPending = false;
    finishCompletedWalkStep();
}

bool ExploreScene::rollRandomEncounter(bool guaranteed) {
    if (!encounterGateOpen(encounterCooldownSteps, mapEncounterCount)) return false;
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    if (!guaranteed && random(0, 10000) >= map.encounterChance) return false;
    ++mapEncounterCount;
    encounterCooldownSteps = ENCOUNTER_COOLDOWN_STEP_COUNT;
    routeGuaranteedEncounterPending = false;
    rollEncounter();
    return true;
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    const char* itemName = nullptr;
    bool stored = true;
    switch (pickupId) {
    case PICKUP_BALL:
        stored = GameEngine::ins().addBalls(1);
        itemName = Ui::Explore::PICKUP_BALL;
        break;
    case PICKUP_COIN: {
        uint8_t level = GameEngine::ins().activeMonster().level;
        uint32_t upper = 10 + min<uint8_t>(40, level);
        uint32_t coins = random(10, upper + 1);
        GameEngine::ins().addCoins(coins);
        snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_COIN_FMT,
                 (unsigned long)coins);
        break;
    }
    case PICKUP_POTION:
        stored = GameEngine::ins().addPotion(1);
        itemName = Ui::Explore::PICKUP_POTION;
        break;
    case PICKUP_SUPER_POTION:
        stored = GameEngine::ins().addSuperPotion(1);
        itemName = Ui::Explore::PICKUP_SUPER_POTION;
        break;
    case PICKUP_ANTIDOTE:
        stored = GameEngine::ins().addAntidote(1);
        itemName = Ui::Explore::PICKUP_ANTIDOTE;
        break;
    case PICKUP_RARE_CANDY:
        stored = GameEngine::ins().addCandy(1);
        itemName = Ui::Explore::PICKUP_CANDY;
        break;
    default:
        return;
    }
    if (itemName) {
        if (stored) {
            snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_FMT,
                     itemName);
        } else {
            snprintf(resultBuf, sizeof(resultBuf), "%s", Ui::Shop::BAG_FULL);
        }
    }
    resultMessage = resultBuf;
    lastCaptureSuccess = false;
    autoWalkActive = false;
    phase = Phase::PICKUP;
}

int8_t ExploreScene::currentDepthLevelOffset(uint8_t spread) const {
    if (mapBlockCount == 0 || currentMapBlock >= mapBlockCount) return 0;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    uint16_t routeLength = path.pointCount > 1 ? path.pointCount - 1 : 1;
    uint16_t localProgress = min<uint16_t>(
        1000, static_cast<uint32_t>(routeIndex) * 1000U / routeLength);
    uint16_t expeditionProgress = static_cast<uint16_t>(
        (static_cast<uint32_t>(currentMapBlock) * 1000U + localProgress) /
        mapBlockCount);
    return depthLevelOffset(expeditionProgress, spread);
}

void ExploreScene::beginEncounter(const Species& species, uint8_t level) {
    wild = &species;
    wildRuntime = GameEngine::ins().createMonster(wild->id, level);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    battleCursor = 0;
    battleTurns = 0;
    captureMenuOpen = false;
    captureAnimationActive = false;
    defeatAwaitInput = false;
    pendingBattleSwitchSlot = 0xFF;
    BattleSystem::resetVolatile(playerBattleState);
    BattleSystem::resetVolatile(wildBattleState);
    fleeAttempts = 0;
    autoWalkActive = false;
    phase = Phase::ENCOUNTER;
    clearBattleLogs();
    enqueueBattleLog(Ui::Explore::WILD_APPEARED);
}

void ExploreScene::beginDebugEncounter() {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    const Species& opponent = count > 0
        ? table[static_cast<uint8_t>(random(0, count))]
        : starterSpecies();
    uint8_t level = rollWildLevel(
        WILD_LEVEL_MIN, WILD_LEVEL_MAX, GameEngine::ins().activeMonster().level);
    Serial.printf("[DebugBattle] opponent=%u level=%u\n", opponent.id, level);
    beginEncounter(opponent, level);
}

void ExploreScene::rollEncounter() {
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    const EncounterEntry& encounter = rollEncounterEntry(map);
    const Species* opponent = findSpecies(encounter.speciesId);
    if (!opponent) opponent = &speciesTable()[0];
    int16_t target = static_cast<int16_t>(map.averageLevel) +
                     currentDepthLevelOffset(map.depthSpread);
    uint8_t targetLevel = static_cast<uint8_t>(
        constrain(target, WILD_LEVEL_MIN, WILD_LEVEL_MAX));
    uint8_t wildLevel = rollWildLevel(
        encounter.minLevel, encounter.maxLevel, targetLevel);
    beginEncounter(*opponent, wildLevel);
}

void ExploreScene::clearBattleLogs() {
    battleLogHead = 0;
    battleLogCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleResultPending = false;
    battleExpVisible = false;
    expAnimationPending = false;
    expAnimationActive = false;
    expAnimationFrom = 0;
    expAnimationTo = 0;
    expAnimationStarted = 0;
    battleTurnStage = BattleTurnStage::IDLE;
    battleActionCount = 0;
    battleActionIndex = 0;
    battleActionAttackerWild = false;
    battleActionSelfHit = false;
    battleActionResult = BattleSystem::DamageResult{};
    battleActionCheck = BattleSystem::ActionCheckResult{};
    battleEffectResolution = BattleSystem::EffectResolution{};
    battleTurnSpecialSlots[0] = BattleSystem::SPECIAL_SLOT_NONE;
    battleTurnSpecialSlots[1] = BattleSystem::SPECIAL_SLOT_NONE;
    battleHpFrom = 0;
    battleHpTo = 0;
    battleActionStarted = 0;
    pendingBattleSwitchSlot = 0xFF;
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
            if (debugBattleMode) {
                phase = Phase::RESULT;
                resultMessage = Ui::Explore::BATTLE_WIN;
            } else {
                resumeWalk();
            }
        }
        return;
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
    } else if (cue == BattleLogCue::TEAM_SWITCH) {
        activatePendingBattleSwitch();
    }
    battleLogActive = true;
    battleLogUntil = nowMs + 1000;
}

bool ExploreScene::battleLogBusy() const {
    return captureAnimationActive || battleLogActive || battleLogCount > 0 ||
           battleResultPending || expAnimationPending || expAnimationActive ||
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

bool ExploreScene::enterPendingProgression(Phase returnPhase) {
    progressionReturnPhase = returnPhase;
    if (GameEngine::ins().hasPendingLevelUp()) {
        phase = Phase::LEVEL_UP;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveLearn()) {
        learnCursor = 0;
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
        enqueueBattleLog(logBuf);
    }

    while (engine.hasPendingMoveLearn()) {
        uint8_t learnerSlot = engine.pendingMoveLearnSlot();
        if (learnerSlot >= state.teamCount || learnerSlot >= Game::TEAM_CAP) {
            engine.resolvePendingMoveLearn(false);
            continue;
        }
        const Species& learnerSpecies = engine.speciesFor(state.team[learnerSlot]);
        const MoveInfo* move = findMove(engine.pendingMoveLearnId());
        bool learned = engine.resolvePendingMoveLearn(true);
        if (!learned || !move) continue;
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_LEARNED_LOG_FMT,
                 learnerSpecies.name, move->name);
        enqueueBattleLog(logBuf);
    }
}

void ExploreScene::enqueueBattleEffectLogs(const BattleSystem::EffectResolution& effects,
                                           bool attackerWild) {
    if (!wild) return;
    const Species& playerSpecies = GameEngine::ins().activeSpecies();
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
    if (!wild || battleActionIndex >= battleActionCount) {
        battleTurnStage = BattleTurnStage::IDLE;
        return;
    }

    auto& engine = GameEngine::ins();
    auto& activeMon = engine.activeMonster();
    const Species& activeSpecies = engine.activeSpecies();
    battleActionAttackerWild = battleActionOrder[battleActionIndex];
    battleActionSelfHit = false;
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
    uint8_t specialSlot = battleTurnSpecialSlots[sideIndex];
    Game::MoveId moveId = BattleSystem::moveIdForAction(
        attacker, attackerSpecies, specialSlot);
    const char* attackerName = attackerSpecies.name;
    char logBuf[BATTLE_LOG_LEN];

    battleActionCheck = BattleSystem::checkAction(
        attacker, attackerSpecies, attackerState, moveId);
    if (!battleActionAttackerWild) engine.markDirty(false);
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

    const Game::MonsterRuntime& defender = battleActionAttackerWild ? activeMon : wildRuntime;
    const Species& defenderSpecies = battleActionAttackerWild ? activeSpecies : *wild;
    battleActionResult = BattleSystem::calcBasicDamage(
        attacker, attackerSpecies, defender, defenderSpecies, specialSlot,
        attackerState, defenderState);
    const MoveInfo* move = findMove(battleActionResult.moveId);
    if (battleActionAttackerWild) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
    } else {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name, move ? move->name : Ui::Status::MOVE_UNKNOWN);
    }
    enqueueBattleLog(logBuf);

    if (battleActionResult.missed) {
        enqueueBattleLog(Ui::Explore::MOVE_MISSED);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionResult.effectiveness == 0) {
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
    auto& activeMon = engine.activeMonster();
    wildRuntime.hpCur = wildHp;

    if (battleActionSelfHit) {
        if (battleActionAttackerWild) {
            wildHp = battleHpTo;
            wildRuntime.hpCur = wildHp;
        } else {
            activeMon.hpCur = battleHpTo;
            engine.markDirty(false);
        }
        return;
    }

    if (battleActionResult.damage > 0) {
        if (battleActionAttackerWild) {
            activeMon.hpCur = battleHpTo;
        } else {
            wildHp = battleHpTo;
            wildRuntime.hpCur = wildHp;
        }
    }

    const MoveInfo* move = findMove(battleActionResult.moveId);
    if (move) {
        bool defenderCanStillAct = false;
        for (uint8_t index = battleActionIndex + 1; index < battleActionCount; ++index) {
            if (battleActionOrder[index] != battleActionAttackerWild) {
                defenderCanStillAct = true;
                break;
            }
        }
        uint16_t actualDamage = battleHpFrom >= battleHpTo ? battleHpFrom - battleHpTo : 0;
        if (battleActionAttackerWild) {
            battleEffectResolution = BattleSystem::applyMoveEffects(
                *move, wildRuntime, *wild, wildBattleState,
                activeMon, engine.activeSpecies(), playerBattleState,
                actualDamage, defenderCanStillAct);
        } else {
            battleEffectResolution = BattleSystem::applyMoveEffects(
                *move, activeMon, engine.activeSpecies(), playerBattleState,
                wildRuntime, *wild, wildBattleState,
                actualDamage, defenderCanStillAct);
        }
        wildHp = wildRuntime.hpCur;
        enqueueBattleEffectLogs(battleEffectResolution, battleActionAttackerWild);
    }

    if (!battleActionAttackerWild && move) {
        uint8_t moveSlot = battleActionResult.special
            ? static_cast<uint8_t>(battleActionResult.specialSlot + 1)
            : 0;
        if (moveSlot < Game::MOVE_SLOT_COUNT &&
            activeMon.moveProficiency[moveSlot] < Game::MOVE_PROFICIENCY_MAX) {
            activeMon.moveProficiency[moveSlot]++;
        }
    }
    engine.markDirty(false);
}

void ExploreScene::finishBattleAction() {
    bool playerFainted = GameEngine::ins().activeMonster().hpCur == 0;
    bool wildFainted = wildHp == 0;
    if (playerFainted || wildFainted) {
        battleTurnStage = BattleTurnStage::IDLE;
        battleActionCount = 0;
        battleActionIndex = 0;
        if (wildFainted) finishWildFaint();
        else finishPlayerFaint();
        return;
    }

    ++battleActionIndex;
    if (battleActionIndex < battleActionCount) {
        battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    } else {
        resolveBattleEndTurn();
    }
}

void ExploreScene::resolveBattleEndTurn() {
    auto& engine = GameEngine::ins();
    BattleSystem::EffectResolution playerEffects = BattleSystem::resolveEndTurn(
        engine.activeMonster(), engine.activeSpecies(), playerBattleState);
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
    engine.markDirty(false);

    battleActionCount = 0;
    battleActionIndex = 0;
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
    } else if (GameEngine::ins().activeMonster().hpCur == 0) {
        finishPlayerFaint();
    }
}

void ExploreScene::finishWildFaint() {
    if (!wild) return;
    auto& engine = GameEngine::ins();
    auto& activeMon = engine.activeMonster();
    uint16_t expGain = BattleSystem::experienceReward(*wild, wildRuntime.level);
    uint8_t reserveSlot = 0xFF;
    const Game::GameState& state = engine.gameState();
    for (uint8_t slot = 1; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        const Game::MonsterRuntime& teammate = state.team[slot];
        if (!teammate.fainted && teammate.hpCur > 0) {
            reserveSlot = slot;
            break;
        }
    }
    uint16_t reserveExpGain = reserveSlot == 0xFF
        ? 0
        : static_cast<uint32_t>(expGain) * EXP_SHARE_PERCENT / 100;
    uint16_t activeExpGain = expGain - reserveExpGain;
    uint32_t expBefore = activeMon.exp;
    engine.grantEffortFrom(*wild);
    if (reserveSlot != 0xFF) {
        engine.grantEffortToTeamMember(reserveSlot, *wild);
    }
    uint32_t activeExpAwarded = engine.addExperienceToTeamMember(0, activeExpGain);
    prepareExpAnimation(expBefore, engine.activeMonster().exp);
    battleResultPending = true;
    enqueueBattleLog(Ui::Explore::BATTLE_WIN);
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::EXP_GAIN_FMT,
             static_cast<unsigned>(activeExpAwarded));
    enqueueBattleLog(logBuf, BattleLogCue::EXP_GAIN);
    enqueueBattleProgressionLogs(0);

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

    // Save every experience update together with the final coin reward.
    engine.addCoins(BATTLE_WIN_COIN_REWARD);
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::COIN_GAIN_FMT,
             BATTLE_WIN_COIN_REWARD);
    enqueueBattleLog(logBuf);
    lastCaptureSuccess = false;
}

void ExploreScene::attackWild() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }

    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    battleTurnSpecialSlots[0] = BattleSystem::rollSpecialMoveSlot(activeMon);
    battleTurnSpecialSlots[1] = BattleSystem::rollSpecialMoveSlot(wildRuntime);
    Game::MoveId playerMove = BattleSystem::moveIdForAction(
        activeMon, activeSpecies, battleTurnSpecialSlots[0]);
    Game::MoveId wildMove = BattleSystem::moveIdForAction(
        wildRuntime, *wild, battleTurnSpecialSlots[1]);
    int8_t playerPriority = BattleSystem::movePriority(playerMove);
    int8_t wildPriority = BattleSystem::movePriority(wildMove);
    uint16_t playerSpeed = BattleSystem::effectiveSpeed(
        activeMon, activeSpecies, playerBattleState);
    uint16_t wildSpeed = BattleSystem::effectiveSpeed(
        wildRuntime, *wild, wildBattleState);
    bool playerFirst = playerPriority > wildPriority ||
                       (playerPriority == wildPriority &&
                        (playerSpeed > wildSpeed ||
                         (playerSpeed == wildSpeed && random(0, 2) == 0)));
    battleActionOrder[0] = !playerFirst;
    battleActionOrder[1] = playerFirst;
    battleActionCount = 2;
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    if (battleTurns < 255) battleTurns++;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::wildCounterattack() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    const auto& activeMon = GameEngine::ins().activeMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }
    battleTurnSpecialSlots[0] = BattleSystem::SPECIAL_SLOT_NONE;
    battleTurnSpecialSlots[1] = BattleSystem::rollSpecialMoveSlot(wildRuntime);
    battleActionOrder[0] = true;
    battleActionCount = 1;
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
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
    size_t index = min<size_t>(sizeof(OFFSETS) - 1,
                               local * sizeof(OFFSETS) / BATTLE_HIT_SHAKE_MS);
    return OFFSETS[index];
}

void ExploreScene::finishPlayerFaint() {
    if (defeatAwaitInput) return;
    auto& engine = GameEngine::ins();
    const char* faintedName = engine.activeSpecies().name;
    uint32_t loss = engine.applyActiveFaintPenalty();
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::FAINTED_FMT,
             faintedName);
    enqueueBattleLog(logBuf);
    snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, (unsigned long)loss);
    enqueueBattleLog(resultBuf);
    resultMessage = resultBuf;
    lastCaptureSuccess = false;
    captureMenuOpen = false;

    const Game::GameState& state = engine.gameState();
    pendingBattleSwitchSlot = 0xFF;
    for (uint8_t slot = 1; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        const Game::MonsterRuntime& candidate = state.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            pendingBattleSwitchSlot = slot;
            break;
        }
    }

    if (pendingBattleSwitchSlot != 0xFF) {
        const Species& replacement = engine.speciesFor(state.team[pendingBattleSwitchSlot]);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::SWITCH_IN_FMT, replacement.name);
        enqueueBattleLog(logBuf, BattleLogCue::TEAM_SWITCH);
        return;
    }

    enqueueBattleLog(Ui::Explore::BATTLE_LOST);
    defeatAwaitInput = true;
}

void ExploreScene::activatePendingBattleSwitch() {
    if (pendingBattleSwitchSlot == 0xFF) return;
    uint8_t slot = pendingBattleSwitchSlot;
    pendingBattleSwitchSlot = 0xFF;
    if (GameEngine::ins().moveTeamMemberToFront(slot)) {
        BattleSystem::resetVolatile(playerBattleState);
        battleCursor = 0;
        return;
    }

    defeatAwaitInput = true;
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
        lastCaptureSuccess = debugBattleMode
            ? GameEngine::ins().recordCapture(wildRuntime)
            : GameEngine::ins().recordCapture(wildRuntime, mapBlocks[currentMapBlock]);
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
    if (!BattleSystem::canFlee(playerBattleState)) {
        enqueueBattleLog(Ui::Explore::BOUND_CANNOT_FLEE);
        wildCounterattack();
        return;
    }
    const auto& activeMon = GameEngine::ins().activeMonster();
    const Species& activeSpecies = GameEngine::ins().activeSpecies();
    uint16_t activeSpeed = BattleSystem::effectiveSpeed(
        activeMon, activeSpecies, playerBattleState);
    uint16_t wildSpeed = BattleSystem::effectiveSpeed(
        wildRuntime, *wild, wildBattleState);
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
    wild = nullptr;
    wildHp = wildHpMax = 0;
    battleResultPending = false;
    battleExpVisible = false;
    captureMenuOpen = false;
    captureAnimationActive = false;
    resultMessage = nullptr;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    routeMoving = false;
    autoWalkActive = false;
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
        Serial.printf("[ExploreMap] generation failed block=%u area=%u entry=%u\n",
                      currentMapBlock, mapIndex,
                      static_cast<unsigned>(pendingEntryEdge));
        return false;
    }
    Serial.printf("[ExploreMap] block=%u area=%u seed=%08lx fingerprint=%08lx "
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
    routeWalkDirection = static_cast<uint8_t>(inwardDirection(generatedMap.entry.edge));
    mapTargetSteps = max<uint16_t>(1, path.pointCount - 1);
    ExploreMapGenerator::Point start = path.points[0];
    routeWorldX = routeWorldCoordinate(start.x);
    routeWorldY = routeWorldCoordinate(start.y);
    routeFromX = routeTargetX = routeWorldX;
    routeFromY = routeTargetY = routeWorldY;
    routeFollowerWalkDirection = routeWalkDirection;
    routeFollowerWorldX = routeWorldX;
    routeFollowerWorldY = routeWorldY;
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
    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFollowerTargetX = routeFollowerWorldX;
    routeFollowerTargetY = routeFollowerWorldY;
    routeFollowerMoving = false;
    placeRoutePickup();
    return true;
}

void ExploreScene::generateMapBlocks() {
    uint8_t selectedMap = static_cast<uint8_t>(activeArea);
    if (selectedMap >= ROUTE_MAP_COUNT) selectedMap = 0;
    const RouteMap& map = routeMap(selectedMap);
    mapBlockCount = mapCountForRoll(map, static_cast<uint8_t>(random(0, 100)));
    for (uint8_t i = 0; i < MAP_BLOCK_CAP; ++i) {
        mapBlocks[i] = i < mapBlockCount ? selectedMap : 0xFF;
    }
    expeditionSeed = static_cast<uint32_t>(random(1, 0x7FFFFFFF));
    pendingEntryEdge = static_cast<ExploreMapGenerator::Edge>((expeditionSeed >> 8) & 0x03);
    currentRoutePath = 0;
    activeExitMask = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    Serial.printf("[ExploreRun] area=%u maps=%u seed=%08lx\n",
                  selectedMap, mapBlockCount,
                  static_cast<unsigned long>(expeditionSeed));
}

void ExploreScene::prepareMapRoutes() {
    uint8_t pathCount = min<uint8_t>(MAP_EXIT_CAP, generatedMap.pathCount);
    activeExitMask = 0;
    currentRoutePath = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    if (pathCount == 0) return;

    bool hasNextMap = currentMapBlock + 1 < mapBlockCount;
    for (uint8_t pathId = 0; pathId < pathCount; ++pathId) {
        activeExitMask = static_cast<uint8_t>(activeExitMask | (1U << pathId));
        if (hasNextMap) exitNextMaps[pathId] = mapBlocks[currentMapBlock + 1];
    }
    currentRoutePath = random(0, pathCount);
}

void ExploreScene::placeRoutePickup() {
    routePickupAvailable = false;
    routePickupIndex = 0;
    routePickupItem = PICKUP_NONE;
    routeGuaranteedEncounterIndex = 0;
    routeGuaranteedEncounterPending = false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (path.pointCount < 2) return;

    if (path.pointCount < 3) {
        routePickupIndex = path.pointCount - 1;
        routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]).pickupWeights);
        routePickupAvailable = routePickupItem != PICKUP_NONE;
        return;
    }

    bool choosePickup = random(0, 10000) < MAP_PICKUP_CHANCE;
    if (!choosePickup &&
        canScheduleGuaranteedEncounter(path.pointCount, encounterCooldownSteps)) {
        routeGuaranteedEncounterIndex = guaranteedEncounterIndex(
            path.pointCount, encounterCooldownSteps);
        routeGuaranteedEncounterPending = true;
        Serial.printf("[ExploreEvent] block=%u type=battle index=%u cooldown=%u\n",
                      currentMapBlock, routeGuaranteedEncounterIndex,
                      encounterCooldownSteps);
        return;
    }

    uint8_t first = max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = min<uint8_t>(path.pointCount - 2, path.pointCount * 3 / 4);
    if (first > last) first = last = path.pointCount / 2;
    routePickupIndex = static_cast<uint8_t>(random(first, last + 1));
    routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]).pickupWeights);
    routePickupAvailable = routePickupItem != PICKUP_NONE;
    Serial.printf("[ExploreEvent] block=%u type=item index=%u cooldown=%u\n",
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
    case Phase::LEARN_MOVE: ProgressionUi::renderMoveLearn(learnCursor); break;
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
    uint8_t mapAnimationFrame = static_cast<uint8_t>((Hal::ins().millis() / 140U) % 8U);
    drawGeneratedMapViewport(
        generatedMap, cameraX, cameraY, map.fieldColor, mapAnimationFrame);

    renderRoutePickup(cameraX, cameraY);

    auto& engine = GameEngine::ins();
    const Species& activeSpecies = engine.activeSpecies();
    const Game::GameState& state = engine.gameState();
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

    const uint16_t panel = PixelRenderer::rgb(8, 10, 14);
    PixelRenderer::fillRectAlpha(156, 2, 80, 22, panel, EXPLORE_HUD_ALPHA);
    c.drawRect(156, 2, 80, 22, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(162, 5, map.name, PixelRenderer::rgb(245, 246, 232), 1);
    if (exploreMenuOpen) renderExploreMenu();
}

void ExploreScene::drawRouteMonster(const Species& species, float worldX,
                                    float worldY, uint8_t walkDirection,
                                    bool follower, int cameraX, int cameraY) {
    auto& c = PixelRenderer::canvas();
    const PokemonSprites::SpriteFrame* marker = nullptr;
    bool markerFlipX = false;
    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            species.id,
            static_cast<PokemonSprites::WalkDirection>(walkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (routeMoving && (!follower || routeFollowerMoving)) {
            uint32_t animationNow = exploreMenuOpen ? exploreMenuOpenedAt : Hal::ins().millis();
            uint32_t elapsed = animationNow - routeMoveStarted;
            if (!follower || elapsed > ROUTE_FOLLOWER_DELAY_MS) {
                if (follower) elapsed -= ROUTE_FOLLOWER_DELAY_MS;
                const PokemonMotion::Behavior motion =
                    PokemonMotion::behaviorForSpecies(species.id);
                uint16_t stepDurationMs = max<uint16_t>(
                    1, follower ? routeFollowerMoveDurationMs
                                : routeLeaderMoveDurationMs);
                uint32_t stepElapsedMs = elapsed;
                uint32_t movementElapsedMs = elapsed;
                if (phase == Phase::EXITING) {
                    stepDurationMs = max<uint16_t>(1, motion.stepDurationMs);
                    stepElapsedMs %= stepDurationMs;
                } else {
                    stepElapsedMs = min<uint32_t>(stepElapsedMs, stepDurationMs - 1);
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
        marker = PokemonSprites::findSpeciesSprite(
            species.id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (marker) {
        float scale = 0.8f;
        int markerW = static_cast<int>(marker->width * scale);
        int markerH = static_cast<int>(marker->height * scale);
        int markerX = static_cast<int>(roundf(worldX)) - cameraX - markerW / 2;
        int markerY = static_cast<int>(roundf(worldY)) - cameraY - markerH / 2;
        c.fillEllipse(markerX + markerW / 2, markerY + markerH - 10,
                      10, 3,
                      PixelRenderer::rgb(68, 87, 74));
        PokemonSprites::drawFrameScaled(marker, markerX, markerY, scale, markerFlipX);
    }
}

void ExploreScene::renderRoutePickup(int cameraX, int cameraY) {
    if (!routePickupAvailable) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routePickupIndex >= path.pointCount) return;

    const ExploreMapGenerator::Point& point = path.points[routePickupIndex];
    int x = static_cast<int>(routeWorldCoordinate(point.x)) - cameraX;
    int y = static_cast<int>(routeWorldCoordinate(point.y)) - cameraY +
            ROUTE_PICKUP_VISUAL_OFFSET_Y;
    if (x < -12 || x >= Hal::DISPLAY_W + 12 ||
        y < -12 || y >= Hal::DISPLAY_H + 12) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 5, 6, 2, PixelRenderer::rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::ITEM_POKE_BALL, x, y - 4, 0.5f)) {
        return;
    }
    c.fillCircle(x, y - 4, 6, PixelRenderer::rgb(224, 69, 65));
    c.fillRect(x - 5, y - 4, 10, 5, 0xFFFF);
    c.drawFastHLine(x - 6, y - 4, 12, PixelRenderer::rgb(35, 39, 44));
    c.fillCircle(x, y - 4, 2, 0xFFFF);
}

void ExploreScene::renderExploreMenu() {
    static_assert(EXPLORE_MENU_ITEM_COUNT ==
                      sizeof(Ui::Explore::SIDE_MENU_ITEMS) /
                          sizeof(Ui::Explore::SIDE_MENU_ITEMS[0]),
                  "explore side menu labels must match the menu item count");
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 156;
    static constexpr int PANEL_Y = 0;
    static constexpr int PANEL_W = Hal::DISPLAY_W - PANEL_X;
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
        if (selected) c.fillRect(PANEL_X + 6, y, 4, 18, active);
        PixelRenderer::text(PANEL_X + 17, y, Ui::Explore::SIDE_MENU_ITEMS[i],
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
    uint32_t captureElapsed = captureAnimationActive ? nowMs - captureAnimationStarted : 0;
    bool hideWild = captureAnimationActive && captureElapsed >= 560 &&
                    (captureOutcome || captureElapsed < 1780);
    if (wild && !hideWild) {
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
    drawMonsterSprite(GameEngine::ins().activeSpecies(),
                      BattleLayout::PLAYER_X, BattleLayout::PLAYER_GROUND_Y,
                      BattleLayout::PLAYER_MAX_W, BattleLayout::PLAYER_MAX_H,
                      true, battleHitShakeX(false, nowMs));
    const auto& activeMonster = GameEngine::ins().activeMonster();
    if (activeMonster.hpCur > 0) {
        drawBattleConditionEffects(BattleLayout::PLAYER_X,
                                   BattleLayout::PLAYER_GROUND_Y,
                                   activeMonster.majorStatus,
                                   playerBattleState, nowMs);
    }
    if (captureAnimationActive) renderCaptureAnimation();
    renderBattleHud();
    renderCommandBox();
}

void ExploreScene::renderCaptureAnimation() {
    if (!captureAnimationActive) return;
    uint32_t elapsed = Hal::ins().millis() - captureAnimationStarted;
    int ballX = BattleLayout::WILD_X;
    int ballY = BattleLayout::WILD_GROUND_Y - 8;
    GameAssets::Kind kind = GameAssets::ballFrameKind(captureBall, 0);

    if (elapsed < 520) {
        float progress = elapsed / 520.0f;
        ballX = static_cast<int>(BattleLayout::PLAYER_X +
                                 (BattleLayout::WILD_X - BattleLayout::PLAYER_X) * progress);
        ballY = static_cast<int>(75 +
                                 ((BattleLayout::WILD_GROUND_Y - 26) - 75) * progress -
                                 34.0f * 4.0f * progress * (1.0f - progress));
        kind = GameAssets::ballFrameKind(captureBall, (elapsed / 55) % 8);
    } else if (elapsed < 760) {
        ballY = BattleLayout::WILD_GROUND_Y - 26;
        kind = GameAssets::ballOpenKind(captureBall);
        GameAssets::drawCentered(GameAssets::Kind::BALL_BURST_STAR,
                                 BattleLayout::WILD_X,
                                 BattleLayout::WILD_GROUND_Y - 27, 1.0f);
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

void ExploreScene::renderResult() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(42, 42, 156, 58, PixelRenderer::rgb(35, 42, 50));
    c.drawRect(42, 42, 156, 58, PixelRenderer::rgb(95, 110, 126));
    if (lastCaptureSuccess) {
        PixelRenderer::text(58, 58, Ui::Explore::CAPTURE_SUCCESS,
                            PixelRenderer::rgb(92, 222, 112), 1);
        const char* capturedName = resultMessage && resultMessage[0]
            ? resultMessage
            : (wild ? wild->name : nullptr);
        if (capturedName) {
            PixelRenderer::text(58, 78, capturedName,
                                PixelRenderer::rgb(255, 216, 72), 1);
        }
    } else {
        const char* result = resultMessage && resultMessage[0]
            ? resultMessage
            : (wild ? wild->name : nullptr);
        if (result) {
            PixelRenderer::text(58, 67, result,
                                PixelRenderer::rgb(241, 242, 232), 1);
        }
    }
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
    PixelRenderer::text(64, PANEL_Y + 39, Ui::Explore::ANY_KEY_RETURN,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

void ExploreScene::drawMonsterSprite(const Species& species, int x, int groundY,
                                     int maxWidth, int maxHeight, bool back,
                                     int spriteOffsetX) {
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, back ? PokemonSprites::SpriteKind::BACK : PokemonSprites::SpriteKind::FRONT);
    if (!frame) return;

    uint8_t w = pgm_read_byte(&frame->width);
    uint8_t h = pgm_read_byte(&frame->height);
    float scale = 1.0f;
    if (!back && h < BattleLayout::WILD_MIN_TARGET_H) {
        scale = min(static_cast<float>(BattleLayout::WILD_MIN_TARGET_H) / h,
                    BattleLayout::WILD_MAX_UPSCALE_PERCENT / 100.0f);
    }
    if (w * scale > maxWidth) scale = static_cast<float>(maxWidth) / w;
    if (h * scale > maxHeight) scale = static_cast<float>(maxHeight) / h;

    int drawW = max<int>(1, static_cast<int>(roundf(w * scale)));
    int drawH = max<int>(1, static_cast<int>(roundf(h * scale)));
    int drawX = x - drawW / 2 + spriteOffsetX;
    int drawY = groundY - drawH;
    int shadowRadiusX = constrain(drawW * 3 / 8, 10, maxWidth / 2);
    int shadowRadiusY = constrain(drawH / 12, 3, 6);

    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, groundY - 1, shadowRadiusX, shadowRadiusY,
                  PixelRenderer::rgb(23, 27, 34));
    if (scale < 0.999f || scale > 1.001f) {
        PokemonSprites::drawFrameScaled(frame, drawX, drawY, scale);
    } else {
        PokemonSprites::drawFrame(frame, drawX, drawY);
    }
}

void ExploreScene::renderBattleHud() {
    auto& c = PixelRenderer::canvas();
    char buf[24];
    auto drawStatus = [](int x, int y, Game::MajorStatus status) {
        const char* label = nullptr;
        uint16_t color = PixelRenderer::rgb(239, 128, 85);
        switch (status) {
        case Game::MajorStatus::POISON: label = Ui::Status::STATUS_POISON; break;
        case Game::MajorStatus::TOXIC: label = Ui::Status::STATUS_TOXIC; break;
        case Game::MajorStatus::PARALYSIS:
            label = Ui::Status::STATUS_PARALYSIS;
            color = PixelRenderer::rgb(224, 180, 52);
            break;
        case Game::MajorStatus::SLEEP:
            label = Ui::Status::STATUS_SLEEP;
            color = PixelRenderer::rgb(112, 151, 219);
            break;
        case Game::MajorStatus::BURN: label = Ui::Status::STATUS_BURN; break;
        case Game::MajorStatus::FREEZE:
            label = Ui::Status::STATUS_FREEZE;
            color = PixelRenderer::rgb(80, 176, 210);
            break;
        default: break;
        }
        if (label) drawBattleText(x, y, label, color);
    };

    if (wild) {
        uint16_t shownWildHp = battleHpForRender(true, wildHp, Hal::ins().millis());
        drawBattleDarkText(6, 5, wild->name);
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, wildRuntime.level);
        drawBattleAsciiRightAligned(124, 5, buf);
        PixelRenderer::bar(12, 23, 90, 6,
                           wildHpMax ? (shownWildHp * 100 / wildHpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112), PixelRenderer::rgb(59, 70, 59));
        drawStatus(104, 18, wildRuntime.majorStatus);
    }

    const auto& active = GameEngine::ins().activeMonster();
    const Species& species = GameEngine::ins().activeSpecies();
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

    drawBattleDarkText(120, 66, species.name);
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, shownLevel);
    drawBattleAsciiRightAligned(238, 66, buf);

    if (!showVictoryExp) {
        uint16_t shownPlayerHp = battleHpForRender(
            false, active.hpCur, Hal::ins().millis());
        PixelRenderer::bar(128, 85, 102, 6,
                           active.hpMax ? (shownPlayerHp * 100 / active.hpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112),
                           PixelRenderer::rgb(59, 70, 59));
        drawStatus(96, 80, active.majorStatus);
        return;
    }

    uint32_t levelFloor = minimumExpForLevel(species.growthRate, shownLevel);
    uint32_t levelCeiling = shownLevel >= Game::LEVEL_MAX
        ? levelFloor
        : minimumExpForLevel(species.growthRate, shownLevel + 1);
    uint8_t expPercent = shownLevel >= Game::LEVEL_MAX
        ? 100
        : static_cast<uint8_t>(min<uint32_t>(
            100,
            (shownExp - levelFloor) * 100UL / max<uint32_t>(1, levelCeiling - levelFloor)));
    drawBattleText(118, 80, Ui::Explore::EXP_LABEL,
                   PixelRenderer::rgb(35, 86, 126));
    c.fillRect(149, 85, 81, 6, PixelRenderer::rgb(38, 51, 59));
    c.fillRect(150, 86, 79, 4, PixelRenderer::rgb(184, 198, 194));
    int expFill = 79 * expPercent / 100;
    if (expFill > 0) {
        c.fillRect(150, 86, expFill, 4, PixelRenderer::rgb(35, 118, 184));
    }
}

void ExploreScene::renderCommandBox() {
    auto& c = PixelRenderer::canvas();
    PixelRenderer::fillRectAlpha(
        0, BattleLayout::FOOTER_Y, Hal::DISPLAY_W, BattleLayout::FOOTER_H,
        PixelRenderer::rgb(204, 204, 204), BATTLE_FOOTER_ALPHA);
    c.drawRect(0, BattleLayout::FOOTER_Y, Hal::DISPLAY_W, BattleLayout::FOOTER_H,
               PixelRenderer::rgb(74, 91, 75));

    if (phase != Phase::ENCOUNTER) return;
    if (captureAnimationActive) {
        drawBattleDarkText(82, 109, Ui::Explore::CAPTURING);
        return;
    }
    if (captureMenuOpen) {
        if (captureCursor >= CAPTURE_BALL_COUNT) {
            drawBattleDarkText(100, 101, Ui::BACK);
        } else {
            char line[32];
            snprintf(line, sizeof(line), Ui::Explore::BALL_SELECT_FMT,
                     captureBallName(captureCursor),
                     GameEngine::ins().itemCount(CAPTURE_BALLS[captureCursor]));
            drawBattleDarkText(56, 101, line);
        }
        drawBattleText(60, 117, Ui::Explore::BALL_SELECT_HINT,
                       PixelRenderer::rgb(74, 91, 75));
        return;
    }
    if (battleLogActive || battleLogVisibleCount > 0) {
        for (uint8_t i = 0; i < battleLogVisibleCount; ++i) {
            drawBattleDarkText(12, 101 + i * 16, battleLogVisible[i]);
        }
        return;
    }
    if (defeatAwaitInput) {
        drawBattleDarkText(72, 109, Ui::Explore::ANY_KEY_RETURN);
        return;
    }
    if (battleTurnStage != BattleTurnStage::IDLE) return;

    static constexpr int xs[] = {30, 104, 178};
    static constexpr int ys[] = {109, 109, 109};
    static constexpr const char* items[] = {
        Ui::Explore::CMD_ATTACK,
        Ui::Explore::CMD_BAG,
        Ui::Explore::CMD_FLEE,
    };
    for (uint8_t i = 0; i < 3; ++i) {
        if (battleCursor == i) drawBattleDarkText(xs[i] - 14, ys[i], ">");
        drawBattleDarkText(xs[i], ys[i], items[i]);
    }
}
