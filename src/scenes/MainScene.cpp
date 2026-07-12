#include "scenes/MainScene.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/HudAssets.h"
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
static constexpr uint16_t PMD_IDLE_FRAME_MS = 520;
static constexpr uint16_t PMD_WALKING_FRAME_MS = 170;
static constexpr uint16_t PMD_SLEEPING_FRAME_MS = 700;
static constexpr float PMD_MOVING_SPEED_EPSILON = 1.0f;
static constexpr float PMD_SHORT_MOVE_DISTANCE = 14.0f;
static constexpr float DEBUG_TILT_DEADZONE = 0.08f;
static constexpr float DEBUG_TILT_MAX = 0.62f;
static constexpr float DEBUG_TILT_SPEED = 58.0f;
static constexpr float CAMERA_FOCUS_Y = 84.0f;
static constexpr float FOOD_FEED_OFFSET_X = 12.0f;
static constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
static constexpr float BED_APPROACH_TOLERANCE_X = 18.0f;
static constexpr float BED_APPROACH_TOLERANCE_Y = 14.0f;
static constexpr float BED_SLEEP_TOLERANCE_X = 2.5f;
static constexpr float BED_SLEEP_TOLERANCE_Y = 6.0f;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MIN_MS = 2600;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MAX_MS = 6500;
static constexpr uint16_t DAY_WAKE_DELAY_MIN_MS = 650;
static constexpr uint16_t DAY_WAKE_DELAY_MAX_MS = 1300;
static constexpr uint16_t FEED_BITE_DELAY_MIN_MS = 700;
static constexpr uint16_t FEED_BITE_DELAY_MAX_MS = 1300;
static constexpr uint16_t FEED_BITE_INTERVAL_MIN_MS = 1000;
static constexpr uint16_t FEED_BITE_INTERVAL_MAX_MS = 1600;
static constexpr uint16_t FEED_SESSION_MIN_MS = 3200;
static constexpr uint16_t FEED_SESSION_MAX_MS = 5600;
static constexpr uint16_t POST_FEED_AWAKE_MIN_MS = 7000;
static constexpr uint16_t POST_FEED_AWAKE_MAX_MS = 13000;
static constexpr uint8_t FEED_CONTINUE_SATIETY = 82;
static constexpr uint16_t MIND_UPDATE_MS = 400;
static constexpr uint16_t MOVE_STUCK_MS = 1600;
static constexpr float MOVE_PROGRESS_EPSILON = 0.45f;
static constexpr int NAV_CELL_PX = 8;
static constexpr uint8_t NAV_MAX_COLS = 32;
static constexpr uint8_t NAV_MAX_ROWS = 32;
static constexpr uint16_t NAV_MAX_NODES = NAV_MAX_COLS * NAV_MAX_ROWS;

void* gNavScratch = nullptr;
int16_t* gNavParent = nullptr;
uint16_t* gNavQueue = nullptr;

bool ensureNavScratch() {
    if (gNavScratch) return true;
    size_t parentBytes = sizeof(int16_t) * NAV_MAX_NODES;
    size_t queueBytes = sizeof(uint16_t) * NAV_MAX_NODES;
    size_t totalBytes = parentBytes + queueBytes;
    gNavScratch = psramFound() ? ps_malloc(totalBytes) : malloc(totalBytes);
    if (!gNavScratch) return false;
    gNavParent = static_cast<int16_t*>(gNavScratch);
    gNavQueue = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(gNavScratch) + parentBytes);
    return true;
}

RoomResource& room() {
    RoomResource::ins().begin();
    return RoomResource::ins();
}

uint16_t roomHeight() { return room().height(); }
int16_t roomOriginY() { return room().roomY(); }
int16_t roomWalkMinX() { return room().walkMinX(); }
int16_t roomWalkMinY() { return room().walkMinY(); }
int16_t roomWalkMaxX() { return room().walkMaxX(); }
int16_t roomWalkMaxY() { return room().walkMaxY(); }
int16_t roomBedMinX() { return room().bedMinX(); }
int16_t roomBedMinY() { return room().bedMinY(); }
int16_t roomBedMaxX() { return room().bedMaxX(); }
int16_t roomBedMaxY() { return room().bedMaxY(); }
float foodCenterX() { return (float)room().foodX(); }
float foodCenterY() { return (float)room().foodY(); }
float foodFeedX() { return foodCenterX() + FOOD_FEED_OFFSET_X; }
float foodFeedY() { return foodCenterY() + FOOD_FEED_OFFSET_Y; }
float bedCenterX() { return (float)room().bedX(); }
float bedCenterY() { return (float)room().bedY(); }
float bedSleepX() { return (float)room().bedX(); }
float bedSleepY() { return (float)room().bedY(); }

enum class PmdMotionMode : uint8_t {
    LOOP,
    START_HOLD_END,
    PINGPONG,
};

struct PmdSpriteConfig {
    uint16_t speciesId;
    uint8_t idleFrames;
    uint16_t idleFrameMs;
    uint8_t walkingFrames;
    uint8_t sleepingFrames;
    PmdMotionMode motionMode;
    PokemonSprites::SpriteKind idleBase;
    PokemonSprites::SpriteKind walkingBase;
    PokemonSprites::SpriteKind sleepingBase;
    bool mirrorRightDirections;
    float airHeight;
    float bobAmplitude;
};

static constexpr PmdSpriteConfig PMD_SPRITE_CONFIGS[] = {
    {1, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BULBASAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::BULBASAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::BULBASAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {2, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::IVYSAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::IVYSAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::IVYSAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {3, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::VENUSAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::VENUSAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::VENUSAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {4, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARMANDER_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARMANDER_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARMANDER_SLEEPING_0, true, 0.0f, 0.0f},
    {5, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARMELEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARMELEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARMELEON_SLEEPING_0, true, 0.0f, 0.0f},
    {6, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARIZARD_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARIZARD_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARIZARD_SLEEPING_0, true, 0.0f, 0.0f},
    {7, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SQUIRTLE_IDLE_FRONT_0, PokemonSprites::SpriteKind::SQUIRTLE_WALKING_FRONT_0, PokemonSprites::SpriteKind::SQUIRTLE_SLEEPING_0, true, 0.0f, 0.0f},
    {8, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::WARTORTLE_IDLE_FRONT_0, PokemonSprites::SpriteKind::WARTORTLE_WALKING_FRONT_0, PokemonSprites::SpriteKind::WARTORTLE_SLEEPING_0, false, 0.0f, 0.0f},
    {9, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BLASTOISE_IDLE_FRONT_0, PokemonSprites::SpriteKind::BLASTOISE_WALKING_FRONT_0, PokemonSprites::SpriteKind::BLASTOISE_SLEEPING_0, true, 0.0f, 0.0f},
    {16, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEY_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEY_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEY_SLEEPING_0, true, 0.0f, 0.0f},
    {17, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEOTTO_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEOTTO_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEOTTO_SLEEPING_0, true, 0.0f, 0.0f},
    {18, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEOT_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEOT_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEOT_SLEEPING_0, true, 0.0f, 0.0f},
    {25, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIKACHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIKACHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIKACHU_SLEEPING_0, true, 0.0f, 0.0f},
    {26, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::RAICHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::RAICHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::RAICHU_SLEEPING_0, false, 0.0f, 0.0f},
    {92, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GASTLY_IDLE_FRONT_0, PokemonSprites::SpriteKind::GASTLY_WALKING_FRONT_0, PokemonSprites::SpriteKind::GASTLY_SLEEPING_0, true, 12.0f, 2.0f},
    {93, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::START_HOLD_END, PokemonSprites::SpriteKind::HAUNTER_IDLE_FRONT_0, PokemonSprites::SpriteKind::HAUNTER_WALKING_FRONT_0, PokemonSprites::SpriteKind::HAUNTER_SLEEPING_0, true, 10.0f, 2.0f},
    {94, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GENGAR_IDLE_FRONT_0, PokemonSprites::SpriteKind::GENGAR_WALKING_FRONT_0, PokemonSprites::SpriteKind::GENGAR_SLEEPING_0, true, 0.0f, 0.0f},
    {123, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SCYTHER_IDLE_FRONT_0, PokemonSprites::SpriteKind::SCYTHER_WALKING_FRONT_0, PokemonSprites::SpriteKind::SCYTHER_SLEEPING_0, true, 0.0f, 0.0f},
    {129, 2, PMD_IDLE_FRAME_MS, 1, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MAGIKARP_IDLE_FRONT_0, PokemonSprites::SpriteKind::MAGIKARP_WALKING_FRONT_0, PokemonSprites::SpriteKind::MAGIKARP_SLEEPING_0, false, 0.0f, 0.0f},
    {130, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GYARADOS_IDLE_FRONT_0, PokemonSprites::SpriteKind::GYARADOS_WALKING_FRONT_0, PokemonSprites::SpriteKind::GYARADOS_SLEEPING_0, false, 0.0f, 0.0f},
    {133, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::EEVEE_IDLE_FRONT_0, PokemonSprites::SpriteKind::EEVEE_WALKING_FRONT_0, PokemonSprites::SpriteKind::EEVEE_SLEEPING_0, false, 0.0f, 0.0f},
    {134, 1, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::VAPOREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::VAPOREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::VAPOREON_SLEEPING_0, false, 0.0f, 0.0f},
    {135, 1, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::JOLTEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::JOLTEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::JOLTEON_SLEEPING_0, false, 0.0f, 0.0f},
    {136, 3, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::FLAREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::FLAREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::FLAREON_SLEEPING_0, false, 0.0f, 0.0f},
    {196, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::ESPEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::ESPEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::ESPEON_SLEEPING_0, true, 0.0f, 0.0f},
    {197, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::UMBREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::UMBREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::UMBREON_SLEEPING_0, true, 0.0f, 0.0f},
    {143, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SNORLAX_IDLE_FRONT_0, PokemonSprites::SpriteKind::SNORLAX_WALKING_FRONT_0, PokemonSprites::SpriteKind::SNORLAX_SLEEPING_0, true, 0.0f, 0.0f},
    {147, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::PINGPONG, PokemonSprites::SpriteKind::DRATINI_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRATINI_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRATINI_SLEEPING_0, false, 0.0f, 0.0f},
    {148, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::PINGPONG, PokemonSprites::SpriteKind::DRAGONAIR_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRAGONAIR_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRAGONAIR_SLEEPING_0, false, 0.0f, 0.0f},
    {149, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::DRAGONITE_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRAGONITE_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRAGONITE_SLEEPING_0, false, 0.0f, 0.0f},
    {151, 3, 360, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MEW_IDLE_FRONT_0, PokemonSprites::SpriteKind::MEW_WALKING_FRONT_0, PokemonSprites::SpriteKind::MEW_SLEEPING_0, false, 14.0f, 3.0f},
    {161, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SENTRET_IDLE_FRONT_0, PokemonSprites::SpriteKind::SENTRET_WALKING_FRONT_0, PokemonSprites::SpriteKind::SENTRET_SLEEPING_0, true, 0.0f, 0.0f},
    {162, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::FURRET_IDLE_FRONT_0, PokemonSprites::SpriteKind::FURRET_WALKING_FRONT_0, PokemonSprites::SpriteKind::FURRET_SLEEPING_0, true, 0.0f, 0.0f},
    {261, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::POOCHYENA_IDLE_FRONT_0, PokemonSprites::SpriteKind::POOCHYENA_WALKING_FRONT_0, PokemonSprites::SpriteKind::POOCHYENA_SLEEPING_0, true, 0.0f, 0.0f},
    {262, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MIGHTYENA_IDLE_FRONT_0, PokemonSprites::SpriteKind::MIGHTYENA_WALKING_FRONT_0, PokemonSprites::SpriteKind::MIGHTYENA_SLEEPING_0, true, 0.0f, 0.0f},
    {278, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::WINGULL_IDLE_FRONT_0, PokemonSprites::SpriteKind::WINGULL_WALKING_FRONT_0, PokemonSprites::SpriteKind::WINGULL_SLEEPING_0, true, 0.0f, 0.0f},
    {279, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PELIPPER_IDLE_FRONT_0, PokemonSprites::SpriteKind::PELIPPER_WALKING_FRONT_0, PokemonSprites::SpriteKind::PELIPPER_SLEEPING_0, true, 0.0f, 0.0f},
    {172, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PICHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::PICHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::PICHU_SLEEPING_0, true, 0.0f, 0.0f},
    {212, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SCIZOR_IDLE_FRONT_0, PokemonSprites::SpriteKind::SCIZOR_WALKING_FRONT_0, PokemonSprites::SpriteKind::SCIZOR_SLEEPING_0, true, 0.0f, 0.0f},
    {380, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::LATIAS_IDLE_FRONT_0, PokemonSprites::SpriteKind::LATIAS_WALKING_FRONT_0, PokemonSprites::SpriteKind::LATIAS_SLEEPING_0, true, 18.0f, 2.0f},
    {381, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::LATIOS_IDLE_FRONT_0, PokemonSprites::SpriteKind::LATIOS_WALKING_FRONT_0, PokemonSprites::SpriteKind::LATIOS_SLEEPING_0, true, 18.0f, 2.0f},
};

const PmdSpriteConfig* pmdSpriteConfigForSpecies(uint16_t speciesId) {
    for (const auto& config : PMD_SPRITE_CONFIGS) {
        if (config.speciesId == speciesId) return &config;
    }
    return nullptr;
}

uint8_t rgb565R(uint16_t c) {
    return (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
}

uint8_t rgb565G(uint16_t c) {
    return (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
}

uint8_t rgb565B(uint16_t c) {
    return (uint8_t)((c & 0x1F) * 255 / 31);
}

uint16_t blendRgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint8_t inv = 255 - alpha;
    uint8_t r = (uint8_t)((rgb565R(src) * alpha + rgb565R(dst) * inv) / 255);
    uint8_t g = (uint8_t)((rgb565G(src) * alpha + rgb565G(dst) * inv) / 255);
    uint8_t b = (uint8_t)((rgb565B(src) * alpha + rgb565B(dst) * inv) / 255);
    return PixelRenderer::rgb(r, g, b);
}

void fillRectAlpha(int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
    auto& c = PixelRenderer::canvas();
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        for (int px = x; px < x + w; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillRadialLightAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    auto& c = PixelRenderer::canvas();
    int minX = centerX - radiusX;
    int maxX = centerX + radiusX;
    int minY = centerY - radiusY;
    int maxY = centerY + radiusY;
    for (int py = minY; py <= maxY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        float dy = (float)(py - centerY) / (float)radiusY;
        for (int px = minX; px <= maxX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            float dx = (float)(px - centerX) / (float)radiusX;
            float distSq = dx * dx + dy * dy;
            if (distSq > 1.0f) continue;
            uint8_t alpha = (uint8_t)(maxAlpha * (1.0f - distSq));
            if (alpha == 0) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillSoftEllipseAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t maxAlpha) {
    fillRadialLightAlpha(centerX, centerY, radiusX, radiusY, color, maxAlpha);
}

void fillEllipseAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t alpha) {
    if (radiusX <= 0 || radiusY <= 0 || alpha == 0) return;
    auto& c = PixelRenderer::canvas();
    int minX = centerX - radiusX;
    int maxX = centerX + radiusX;
    int minY = centerY - radiusY;
    int maxY = centerY + radiusY;
    for (int py = minY; py <= maxY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        float dy = (float)(py - centerY) / (float)radiusY;
        for (int px = minX; px <= maxX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            float dx = (float)(px - centerX) / (float)radiusX;
            if (dx * dx + dy * dy > 1.0f) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillTopDownLightAlpha(int height, uint16_t color, uint8_t maxAlpha) {
    if (height <= 0 || maxAlpha == 0) return;
    if (height > Hal::DISPLAY_H) height = Hal::DISPLAY_H;
    auto& c = PixelRenderer::canvas();
    for (int py = 0; py < height; ++py) {
        float t = 1.0f - (float)py / (float)height;
        uint8_t alpha = (uint8_t)(maxAlpha * t * t);
        if (alpha == 0) continue;
        for (int px = 0; px < Hal::DISPLAY_W; ++px) {
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

float roomMaxCameraY() {
    float bottom = (float)(roomOriginY() + roomHeight());
    float maxY = bottom - (float)Hal::DISPLAY_H;
    return maxY > 0.0f ? maxY : 0.0f;
}

float cameraForWorldY(float worldY) {
    return clampf(worldY - CAMERA_FOCUS_Y, 0.0f, roomMaxCameraY());
}

int16_t roomPointX(uint8_t index) {
    return room().walkPoint(index).x;
}

int16_t roomPointY(uint8_t index) {
    return room().walkPoint(index).y;
}

int16_t bedPointX(uint8_t index) {
    return room().bedPoint(index).x;
}

int16_t bedPointY(uint8_t index) {
    return room().bedPoint(index).y;
}

bool roomWalkContains(float x, float y) {
    uint8_t count = room().walkPolygonCount();
    if (count < 3) return true;

    bool inside = false;
    uint8_t j = count - 1;
    for (uint8_t i = 0; i < count; ++i) {
        float xi = (float)roomPointX(i);
        float yi = (float)roomPointY(i);
        float xj = (float)roomPointX(j);
        float yj = (float)roomPointY(j);
        bool crosses = ((yi > y) != (yj > y)) &&
                       (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (crosses) inside = !inside;
        j = i;
    }
    return inside;
}

bool roomBedContains(float x, float y) {
    uint8_t count = room().bedPolygonCount();
    if (count < 3) return false;

    bool inside = false;
    uint8_t j = count - 1;
    for (uint8_t i = 0; i < count; ++i) {
        float xi = (float)bedPointX(i);
        float yi = (float)bedPointY(i);
        float xj = (float)bedPointX(j);
        float yj = (float)bedPointY(j);
        bool crosses = ((yi > y) != (yj > y)) &&
                       (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (crosses) inside = !inside;
        j = i;
    }
    return inside;
}

bool randomRoomWalkPoint(float& x, float& y) {
    for (uint8_t tries = 0; tries < 48; ++tries) {
        float px = (float)random(roomWalkMinX(), roomWalkMaxX() + 1);
        float py = (float)random(roomWalkMinY(), roomWalkMaxY() + 1);
        if (roomWalkContains(px, py)) {
            x = px;
            y = py;
            return true;
        }
    }

    float centerX = (roomWalkMinX() + roomWalkMaxX()) * 0.5f;
    float centerY = (roomWalkMinY() + roomWalkMaxY()) * 0.5f;
    if (roomWalkContains(centerX, centerY)) {
        x = centerX;
        y = centerY;
        return true;
    }

    if (room().walkPolygonCount() > 0) {
        x = (float)roomPointX(0);
        y = (float)roomPointY(0);
        return true;
    }
    x = centerX;
    y = centerY;
    return false;
}

bool randomRoomWalkPointNear(float centerX, float centerY, float radiusX, float radiusY, float& x, float& y) {
    int spanX = (int)roundf(radiusX);
    int spanY = (int)roundf(radiusY);
    if (spanX < 1) spanX = 1;
    if (spanY < 1) spanY = 1;
    for (uint8_t tries = 0; tries < 32; ++tries) {
        float px = clampf(centerX + (float)random(-spanX, spanX + 1),
                          (float)roomWalkMinX(),
                          (float)roomWalkMaxX());
        float py = clampf(centerY + (float)random(-spanY, spanY + 1),
                          (float)roomWalkMinY(),
                          (float)roomWalkMaxY());
        if (roomWalkContains(px, py)) {
            x = px;
            y = py;
            return true;
        }
    }
    return randomRoomWalkPoint(x, y);
}

float pmdFloatYOffset(const PmdSpriteConfig* config, uint32_t nowMs) {
    if (!config || config->airHeight <= 0.0f) return 0.0f;
    float phase = (float)(nowMs % 1600UL) * 0.00392699f;
    return config->airHeight + sinf(phase) * config->bobAmplitude;
}

uint8_t pmdWalkingPlaybackFrameCount(const PmdSpriteConfig* config, bool longMove) {
    if (!config || config->walkingFrames == 0) return 1;
    if (config->motionMode == PmdMotionMode::PINGPONG && config->walkingFrames == 3) return longMove ? 3 : 2;
    if (config->motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 2) return 2;
    return config->walkingFrames;
}

bool pmdWalkingPlaybackLoops(const PmdSpriteConfig* config) {
    return !config || config->motionMode == PmdMotionMode::LOOP;
}

uint8_t pmdWalkingSpriteFrameIndex(const PmdSpriteConfig* config, uint8_t playbackFrame, bool longMove) {
    if (!config || config->walkingFrames == 0) return 0;
    if (config->motionMode == PmdMotionMode::PINGPONG && config->walkingFrames == 3) {
        uint8_t maxFrame = longMove ? 2 : 1;
        return playbackFrame > maxFrame ? maxFrame : playbackFrame;
    }
    if (config->motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) {
        return playbackFrame == 0 ? 0 : 1;
    }
    return playbackFrame % config->walkingFrames;
}

uint8_t pmdStoppingPlaybackFrameCount(const PmdSpriteConfig* config) {
    if (config && config->motionMode == PmdMotionMode::PINGPONG && config->walkingFrames >= 2) return 2;
    return 1;
}

uint8_t pmdStoppingSpriteFrameIndex(const PmdSpriteConfig* config, uint8_t playbackFrame) {
    if (!config || config->walkingFrames == 0) return 0;
    if (config->motionMode == PmdMotionMode::PINGPONG && config->walkingFrames >= 2) {
        static constexpr uint8_t SEQUENCE[] = {1, 0};
        return SEQUENCE[playbackFrame < 2 ? playbackFrame : 1];
    }
    if (config->motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) {
        return (uint8_t)(config->walkingFrames - 1);
    }
    return 0;
}

bool mainSceneIsNight() {
    uint16_t minutes = GameEngine::ins().gameMinutesOfDay();
    return minutes < 6 * 60 || minutes >= 18 * 60;
}

void drawHungerIcon(int x, int y, uint8_t hunger) {
    if (hunger == 0) return;
    auto& c = PixelRenderer::canvas();
    uint8_t visibleRows = (uint8_t)(((uint16_t)HudAssets::HUNGER_ICON_H * hunger + 99) / 100);
    if (visibleRows > HudAssets::HUNGER_ICON_H) visibleRows = HudAssets::HUNGER_ICON_H;
    uint8_t hiddenRows = HudAssets::HUNGER_ICON_H - visibleRows;

    static constexpr int8_t CUT_JITTER[HudAssets::HUNGER_ICON_W] = {
        -1, 0, 1, 0, 2, 1, 0, -1, 1, 0, 2, 0, -1, 1, 0, 2, 1, 0,
    };

    const uint32_t total = (uint32_t)HudAssets::HUNGER_ICON_W * (uint32_t)HudAssets::HUNGER_ICON_H;
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < HudAssets::HUNGER_ICON_RLE_LEN && pixel < total) {
        uint16_t token = pgm_read_word(&HudAssets::HUNGER_ICON_RLE[idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < HudAssets::HUNGER_ICON_RLE_LEN && pixel < total; ++i, ++pixel) {
            uint16_t color = pgm_read_word(&HudAssets::HUNGER_ICON_RLE[idx++]);
            uint8_t row = (uint8_t)(pixel / HudAssets::HUNGER_ICON_W);
            uint8_t col = (uint8_t)(pixel % HudAssets::HUNGER_ICON_W);
            int cutRow = hiddenRows == 0 ? 0 : (int)hiddenRows + CUT_JITTER[col];
            if (cutRow < 0) cutRow = 0;
            if (cutRow > HudAssets::HUNGER_ICON_H) cutRow = HudAssets::HUNGER_ICON_H;
            if (row < cutRow) continue;
            c.drawPixel(x + col, y + row, color);
        }
    }
}
}

void MainScene::onEnter() {
    active = &GameEngine::ins().activeSpecies();
    uint32_t nowMs = Hal::ins().millis();
    behaviorProfile = behaviorProfileFor(*active, GameEngine::ins().activeMonster());
    mind.reset(nowMs);
    nextMindUpdateMs = nowMs;
    restoreViewState(nowMs);
    bool mayBeAtBed = aiMode == AiMode::RESTING || aiMode == AiMode::WAKING || aiMode == AiMode::LEAVING_BED;
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(mayBeAtBed && (aiMode == AiMode::LEAVING_BED || monsterAtBedSleepPose()))) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
    }
    velocityX = 0.0f;
    velocityY = 0.0f;
    cameraY = cameraForWorldY(monsterY);
    pmdLongMove = false;
    pmdFrameStartedMs = nowMs;
    clearMoveRoute();
    if (aiMode == AiMode::WANDER || aiMode == AiMode::SEEK_FOOD ||
        aiMode == AiMode::SEEK_BED || aiMode == AiMode::LEAVING_BED) {
        if (!buildMoveRoute(targetX, targetY)) aiMode = AiMode::IDLE;
    }
    if (nextAiDecisionMs == 0) nextAiDecisionMs = nowMs;
}

void MainScene::onExit() {
    persistViewState(Hal::ins().millis());
}

void MainScene::restoreViewState(uint32_t nowMs) {
    const MainSceneViewState& saved = GameEngine::ins().mainSceneViewState();
    if (!saved.valid || !active || saved.speciesId != active->id) {
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        pmdAction = PmdAction::IDLE;
        pmdDirection = PmdDirection::FRONT;
        pmdFrame = 0;
        facingRight = true;
        nextAiDecisionMs = nowMs;
        postFeedAwakeUntilMs = 0;
        return;
    }

    monsterX = saved.monsterX;
    monsterY = saved.monsterY;
    targetX = saved.targetX;
    targetY = saved.targetY;
    facingRight = saved.facingRight;
    pmdDirection = saved.pmdDirection <= (uint8_t)PmdDirection::DOWN_RIGHT
        ? (PmdDirection)saved.pmdDirection
        : PmdDirection::FRONT;
    pmdFrame = saved.pmdFrame;
    aiMode = saved.aiMode <= (uint8_t)AiMode::RESTING
        ? (AiMode)saved.aiMode
        : AiMode::IDLE;
    if (aiMode == AiMode::TURNING) aiMode = AiMode::IDLE;
    if (aiMode == AiMode::WAKING) aiMode = AiMode::RESTING;
    if (aiMode == AiMode::FEEDING) aiMode = AiMode::IDLE;
    pmdAction = aiMode == AiMode::RESTING ? PmdAction::SLEEPING : PmdAction::IDLE;
    nextAiDecisionMs = nowMs + saved.nextDecisionRemainingMs;
    postFeedAwakeUntilMs = saved.postFeedAwakeRemainingMs == 0
        ? 0
        : nowMs + saved.postFeedAwakeRemainingMs;
}

void MainScene::persistViewState(uint32_t nowMs) {
    if (!active) return;
    MainSceneViewState saved;
    saved.valid = true;
    saved.speciesId = active->id;
    saved.monsterX = monsterX;
    saved.monsterY = monsterY;
    saved.targetX = targetX;
    saved.targetY = targetY;
    AiMode storedMode = aiMode;
    if (storedMode == AiMode::TURNING) storedMode = turnNextMode;
    if (storedMode == AiMode::WAKING) storedMode = AiMode::RESTING;
    if (storedMode == AiMode::FEEDING) storedMode = AiMode::IDLE;
    saved.aiMode = (uint8_t)storedMode;
    saved.pmdAction = (uint8_t)pmdAction;
    saved.pmdDirection = (uint8_t)pmdDirection;
    saved.pmdFrame = pmdFrame;
    saved.facingRight = facingRight;
    saved.nextDecisionRemainingMs = (int32_t)(nextAiDecisionMs - nowMs) > 0
        ? nextAiDecisionMs - nowMs
        : 0;
    saved.postFeedAwakeRemainingMs = (int32_t)(postFeedAwakeUntilMs - nowMs) > 0
        ? postFeedAwakeUntilMs - nowMs
        : 0;
    GameEngine::ins().saveMainSceneViewState(saved);
}

void MainScene::update(uint32_t nowMs, float dtSeconds) {
    active = &GameEngine::ins().activeSpecies();
    updateMonsterAi(nowMs, dtSeconds);
    updateCamera();
    updatePmdSpriteState(nowMs);
    uint8_t levelUp = 0;
    if (GameEngine::ins().consumePendingLevelUp(levelUp)) {
        snprintf(toastBuffer, sizeof(toastBuffer), Ui::Common::LEVEL_UP_FMT, levelUp);
        toast = toastBuffer;
        toastUntil = nowMs + 1400;
    }
    bool combo = Hal::ins().btnA_raw() && Hal::ins().btnB_raw();
    if (!combo) {
        comboStartMs = 0;
        comboSaved = false;
    } else if (comboStartMs == 0) {
        comboStartMs = nowMs;
    } else if (!comboSaved && nowMs - comboStartMs >= 800) {
        GameEngine::ins().saveNow();
        toast = Ui::Common::SAVED;
        toastUntil = nowMs + 1200;
        comboSaved = true;
    }
}

void MainScene::updateCamera() {
    cameraY = cameraForWorldY(monsterY);
}

int16_t MainScene::worldToScreenY(float worldY) const {
    return (int16_t)roundf(worldY - cameraY);
}

float MainScene::walkBoundaryOffsetY() const {
    uint8_t frameH = 42;
    if (const PokemonSprites::SpriteFrame* frame = movementBoundsFrame()) {
        frameH = pgm_read_byte(&frame->height);
    }
    return (float)constrain((int)(frameH * 0.42f), 16, 32);
}

float MainScene::walkFootprintRadiusX() const {
    uint8_t frameW = 38;
    if (const PokemonSprites::SpriteFrame* frame = movementBoundsFrame()) {
        frameW = pgm_read_byte(&frame->width);
    }
    return (float)constrain((int)(frameW * 0.24f), 7, 16);
}

float MainScene::walkFootprintRadiusY() const {
    uint8_t frameH = 42;
    if (const PokemonSprites::SpriteFrame* frame = movementBoundsFrame()) {
        frameH = pgm_read_byte(&frame->height);
    }
    return (float)constrain((int)(frameH * 0.10f), 4, 8);
}

bool MainScene::monsterFootprintInsideWalkArea(float x, float y) const {
    float footY = y + walkBoundaryOffsetY();
    float rx = walkFootprintRadiusX();
    float ry = walkFootprintRadiusY();
    float diagX = rx * 0.70f;
    float diagY = ry * 0.70f;

    return roomWalkContains(x, footY) &&
           roomWalkContains(x - rx, footY) &&
           roomWalkContains(x + rx, footY) &&
           roomWalkContains(x, footY - ry) &&
           roomWalkContains(x - diagX, footY - diagY) &&
           roomWalkContains(x + diagX, footY - diagY);
}

bool MainScene::randomMonsterCenterWalkPoint(float& x, float& y) const {
    float offsetY = walkBoundaryOffsetY();
    float rx = walkFootprintRadiusX();
    for (uint8_t tries = 0; tries < 64; ++tries) {
        float px = (float)random(roomWalkMinX(), roomWalkMaxX() + 1);
        float py = (float)random(roomWalkMinY(), roomWalkMaxY() + 1);
        float centerY = py - offsetY;
        if (px - rx < (float)roomWalkMinX() ||
            px + rx > (float)roomWalkMaxX()) {
            continue;
        }
        if (monsterFootprintInsideWalkArea(px, centerY)) {
            x = px;
            y = centerY;
            return true;
        }
    }

    for (int py = roomWalkMinY(); py <= roomWalkMaxY(); py += 4) {
        for (int px = roomWalkMinX(); px <= roomWalkMaxX(); px += 4) {
            float centerY = (float)py - offsetY;
            if (monsterFootprintInsideWalkArea((float)px, centerY)) {
                x = (float)px;
                y = centerY;
                return true;
            }
        }
    }

    float px = 0.0f;
    float py = 0.0f;
    if (!randomRoomWalkPoint(px, py)) return false;
    x = px;
    y = py - offsetY;
    return monsterFootprintInsideWalkArea(x, y);
}

bool MainScene::pathSegmentInsideWalkArea(float fromX, float fromY, float toX, float toY) const {
    float dx = toX - fromX;
    float dy = toY - fromY;
    float distance = sqrtf(dx * dx + dy * dy);
    uint16_t steps = (uint16_t)ceilf(distance / 3.0f);
    if (steps == 0) return monsterFootprintInsideWalkArea(toX, toY);
    bool enteredWalkArea = monsterFootprintInsideWalkArea(fromX, fromY);
    if (!enteredWalkArea && !monsterAtBedSleepPose()) return false;
    for (uint16_t i = 1; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        bool inside = monsterFootprintInsideWalkArea(fromX + dx * t, fromY + dy * t);
        if (inside) {
            enteredWalkArea = true;
        } else if (enteredWalkArea) {
            return false;
        }
    }
    return enteredWalkArea;
}

void MainScene::clearMoveRoute() {
    moveRouteCount = 0;
    moveRouteIndex = 0;
}

bool MainScene::currentWaypoint(float& x, float& y) const {
    if (moveRouteIndex >= moveRouteCount) return false;
    x = moveRouteX[moveRouteIndex];
    y = moveRouteY[moveRouteIndex];
    return true;
}

bool MainScene::buildMoveRoute(float goalX, float goalY) {
    clearMoveRoute();
    if (!monsterFootprintInsideWalkArea(goalX, goalY)) return false;
    if (pathSegmentInsideWalkArea(monsterX, monsterY, goalX, goalY)) {
        moveRouteX[0] = goalX;
        moveRouteY[0] = goalY;
        moveRouteCount = 1;
        return true;
    }
    if (!ensureNavScratch()) return false;

    float offsetY = walkBoundaryOffsetY();
    float minX = (float)roomWalkMinX();
    float minY = (float)roomWalkMinY() - offsetY;
    float maxX = (float)roomWalkMaxX();
    float maxY = (float)roomWalkMaxY() - offsetY;
    uint8_t cols = (uint8_t)min<int>(NAV_MAX_COLS, (int)ceilf((maxX - minX) / NAV_CELL_PX) + 1);
    uint8_t rows = (uint8_t)min<int>(NAV_MAX_ROWS, (int)ceilf((maxY - minY) / NAV_CELL_PX) + 1);
    uint16_t nodeCount = (uint16_t)cols * rows;
    if (cols < 2 || rows < 2 || nodeCount > NAV_MAX_NODES) return false;

    auto nodeX = [&](uint16_t node) {
        uint8_t col = (uint8_t)(node % cols);
        return minX + (float)col * NAV_CELL_PX;
    };
    auto nodeY = [&](uint16_t node) {
        uint8_t row = (uint8_t)(node / cols);
        return minY + (float)row * NAV_CELL_PX;
    };

    int16_t startNode = -1;
    int16_t goalNode = -1;
    float bestStart = 1000000.0f;
    float bestGoal = 1000000.0f;
    for (uint16_t node = 0; node < nodeCount; ++node) {
        float x = nodeX(node);
        float y = nodeY(node);
        if (!monsterFootprintInsideWalkArea(x, y)) continue;
        float startDx = x - monsterX;
        float startDy = y - monsterY;
        float startDist = startDx * startDx + startDy * startDy;
        if (startDist < bestStart && pathSegmentInsideWalkArea(monsterX, monsterY, x, y)) {
            bestStart = startDist;
            startNode = (int16_t)node;
        }
        float goalDx = x - goalX;
        float goalDy = y - goalY;
        float goalDist = goalDx * goalDx + goalDy * goalDy;
        if (goalDist < bestGoal && pathSegmentInsideWalkArea(x, y, goalX, goalY)) {
            bestGoal = goalDist;
            goalNode = (int16_t)node;
        }
    }
    if (startNode < 0 || goalNode < 0) return false;

    for (uint16_t node = 0; node < nodeCount; ++node) gNavParent[node] = -2;
    uint16_t queueRead = 0;
    uint16_t queueWrite = 0;
    gNavQueue[queueWrite++] = (uint16_t)startNode;
    gNavParent[startNode] = -1;
    static constexpr int8_t DIRS[8][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
    };
    while (queueRead < queueWrite && gNavParent[goalNode] == -2) {
        uint16_t node = gNavQueue[queueRead++];
        int col = node % cols;
        int row = node / cols;
        float fromX = nodeX(node);
        float fromY = nodeY(node);
        for (const auto& dir : DIRS) {
            int nextCol = col + dir[0];
            int nextRow = row + dir[1];
            if (nextCol < 0 || nextCol >= cols || nextRow < 0 || nextRow >= rows) continue;
            uint16_t next = (uint16_t)(nextRow * cols + nextCol);
            if (gNavParent[next] != -2) continue;
            float nextX = nodeX(next);
            float nextY = nodeY(next);
            if (!monsterFootprintInsideWalkArea(nextX, nextY) ||
                !pathSegmentInsideWalkArea(fromX, fromY, nextX, nextY)) {
                continue;
            }
            gNavParent[next] = (int16_t)node;
            if (queueWrite < NAV_MAX_NODES) gNavQueue[queueWrite++] = next;
        }
    }
    if (gNavParent[goalNode] == -2) return false;

    uint16_t pathCount = 0;
    for (int16_t node = goalNode; node >= 0 && pathCount < NAV_MAX_NODES;
         node = gNavParent[node]) {
        gNavQueue[pathCount++] = (uint16_t)node;
    }
    if (pathCount == 0) return false;

    float fromX = monsterX;
    float fromY = monsterY;
    int cursor = (int)pathCount - 1;
    while (cursor > 0 && moveRouteCount + 1 < MOVE_ROUTE_CAP) {
        int selected = cursor - 1;
        for (int candidate = 0; candidate < cursor; ++candidate) {
            uint16_t node = gNavQueue[candidate];
            if (pathSegmentInsideWalkArea(fromX, fromY, nodeX(node), nodeY(node))) {
                selected = candidate;
                break;
            }
        }
        uint16_t node = gNavQueue[selected];
        fromX = nodeX(node);
        fromY = nodeY(node);
        moveRouteX[moveRouteCount] = fromX;
        moveRouteY[moveRouteCount] = fromY;
        moveRouteCount++;
        cursor = selected;
    }
    if (!pathSegmentInsideWalkArea(fromX, fromY, goalX, goalY) || moveRouteCount >= MOVE_ROUTE_CAP) {
        clearMoveRoute();
        return false;
    }
    moveRouteX[moveRouteCount] = goalX;
    moveRouteY[moveRouteCount] = goalY;
    moveRouteCount++;
    return true;
}

void MainScene::updateStuckWatchdog(uint32_t nowMs, float distanceToWaypoint) {
    if (distanceToWaypoint + MOVE_PROGRESS_EPSILON < lastWaypointDistance) {
        lastWaypointDistance = distanceToWaypoint;
        lastMoveProgressMs = nowMs;
        return;
    }
    if (nowMs - lastMoveProgressMs < MOVE_STUCK_MS) return;

    stuckRecoveryCount++;
    Serial.printf("[MonsterAI] stuck mode=%u count=%u pos=%.1f,%.1f target=%.1f,%.1f\n",
                  (unsigned)aiMode, stuckRecoveryCount, monsterX, monsterY, targetX, targetY);
    if (stuckRecoveryCount <= 2 && buildMoveRoute(targetX, targetY)) {
        lastWaypointDistance = 1000000.0f;
        lastMoveProgressMs = nowMs;
        return;
    }
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::IDLE;
    nextAiDecisionMs = nowMs + random(1200, 2601);
    lastMoveProgressMs = nowMs;
}

bool MainScene::chooseFoodApproachPose(float& x, float& y) const {
    float offsetY = walkBoundaryOffsetY();
    float bestDistance = 1000000.0f;
    bool found = false;
    for (int dy = -6; dy <= 6; ++dy) {
        for (int dx = -8; dx <= 8; ++dx) {
            float footX = foodFeedX() + dx;
            float footY = foodFeedY() + dy;
            float candidateY = footY - offsetY;
            if (!monsterFootprintInsideWalkArea(footX, candidateY)) continue;
            float distance = (float)(dx * dx + dy * dy);
            if (!found || distance < bestDistance) {
                x = footX;
                y = candidateY;
                bestDistance = distance;
                found = true;
            }
        }
    }
    return found;
}

bool MainScene::monsterCanUseBedSleepPose(float x, float y) const {
    float footY = y + walkBoundaryOffsetY();
    return roomBedContains(x, footY) ||
           (x >= (float)roomBedMinX() + 12.0f &&
            x <= (float)roomBedMaxX() - 12.0f &&
            footY >= (float)roomBedMinY() + 7.0f &&
            footY <= (float)roomBedMaxY() - 7.0f);
}

bool MainScene::chooseBedApproachPose(float& x, float& y) const {
    static constexpr float CANDIDATES[][2] = {
        {0.0f, 7.0f},
        {-7.0f, 6.0f},
        {7.0f, 6.0f},
        {-10.0f, 3.0f},
        {10.0f, 3.0f},
        {0.0f, 10.0f},
    };

    float offsetY = walkBoundaryOffsetY();
    for (uint8_t i = 0; i < sizeof(CANDIDATES) / sizeof(CANDIDATES[0]); ++i) {
        float candidateX = bedSleepX() + CANDIDATES[i][0];
        float candidateY = bedSleepY() + CANDIDATES[i][1] - offsetY;
        if (monsterFootprintInsideWalkArea(candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }

    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestDistSq = 1000000.0f;
    bool found = false;
    for (int py = roomWalkMinY(); py <= roomWalkMaxY(); py += 3) {
        for (int px = roomWalkMinX(); px <= roomWalkMaxX(); px += 3) {
            float centerY = (float)py - offsetY;
            if (!monsterFootprintInsideWalkArea((float)px, centerY)) continue;
            float dx = (float)px - bedSleepX();
            float dy = (float)py - bedSleepY();
            float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq) {
                bestX = (float)px;
                bestY = centerY;
                bestDistSq = distSq;
                found = true;
            }
        }
    }
    if (!found) return false;
    x = bestX;
    y = bestY;
    return true;
}

bool MainScene::chooseBedSleepPose(float& x, float& y) const {
    static constexpr float CANDIDATES[][2] = {
        {0.0f, 0.0f},
        {-3.0f, -1.0f},
        {3.0f, -1.0f},
        {0.0f, -3.0f},
        {-2.0f, 2.0f},
        {2.0f, 2.0f},
    };

    float offsetY = walkBoundaryOffsetY();
    for (uint8_t i = 0; i < sizeof(CANDIDATES) / sizeof(CANDIDATES[0]); ++i) {
        float candidateX = bedSleepX() + CANDIDATES[i][0];
        float candidateY = bedSleepY() + CANDIDATES[i][1] - offsetY;
        if (monsterCanUseBedSleepPose(candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }
    x = bedSleepX();
    y = bedSleepY() - offsetY;
    return true;
}

void MainScene::updateMonsterAi(uint32_t nowMs, float dtSeconds) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (mon.fainted || mon.hpCur == 0 || (mon.statusBits & Game::STATUS_SLEEP)) {
        if (!monsterAtBedSleepPose()) snapMonsterToBed();
        clearMoveRoute();
        feedingConsumed = false;
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::RESTING;
        return;
    }

    bool debugTilt = GameEngine::ins().debugTiltControlEnabled();
    updateMind(nowMs);
    bool bedRestActive = !debugTilt &&
        (aiMode == AiMode::RESTING || aiMode == AiMode::WAKING || aiMode == AiMode::LEAVING_BED);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(bedRestActive && (aiMode == AiMode::LEAVING_BED || monsterAtBedSleepPose()))) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
        targetX = monsterX;
        targetY = monsterY;
        clearMoveRoute();
        aiMode = AiMode::IDLE;
    }

    if (debugTilt) {
        if (aiMode != AiMode::IDLE && aiMode != AiMode::WANDER) {
            clearMoveRoute();
            aiMode = AiMode::IDLE;
        }
        float prevX = monsterX;
        float prevY = monsterY;
        updateDebugTiltControl(nowMs, dtSeconds);
        float walkOffsetY = walkBoundaryOffsetY();
        monsterX = clampf(monsterX, (float)roomWalkMinX(), (float)roomWalkMaxX());
        monsterY = clampf(monsterY, (float)roomWalkMinY() - walkOffsetY,
                          (float)roomWalkMaxY() - walkOffsetY);
        if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
            monsterX = prevX;
            monsterY = prevY;
            velocityX = 0.0f;
            velocityY = 0.0f;
        }
        return;
    }

    if (aiMode == AiMode::FEEDING) {
        updateFeeding(nowMs);
        return;
    }

    if (aiMode == AiMode::WAKING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        if ((int32_t)(nowMs - stateUntilMs) < 0) return;
        if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
            if (chooseBedApproachPose(targetX, targetY)) {
                beginMovement(AiMode::LEAVING_BED, nowMs);
            } else {
                randomMonsterCenterWalkPoint(monsterX, monsterY);
                aiMode = AiMode::IDLE;
                nextAiDecisionMs = nowMs + 700;
            }
            return;
        }
        if (wakingForFood && GameEngine::ins().bowlHasFood() && mon.satiety < FEED_CONTINUE_SATIETY) {
            setFoodTarget(nowMs);
        } else {
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + random(700, 1401);
        }
        return;
    }

    if (aiMode == AiMode::TURNING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        if ((int32_t)(nowMs - stateUntilMs) < 0) return;
        pmdDirection = turnTargetDirection;
        aiMode = turnNextMode;
        lastMoveProgressMs = nowMs;
        lastWaypointDistance = 1000000.0f;
        return;
    }

    if (aiMode == AiMode::RESTING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        bool wantsFood = mind.topDesire() == MonsterDesire::EAT &&
                         GameEngine::ins().bowlHasFood() &&
                         mon.satiety < FEED_CONTINUE_SATIETY;
        if (wantsFood) {
            beginWaking(nowMs, true);
        } else if (!monsterNeedsBedRest()) {
            beginWaking(nowMs, false);
        }
        return;
    }

    if (aiMode == AiMode::IDLE && (int32_t)(nowMs - nextAiDecisionMs) >= 0) {
        chooseAiGoal(nowMs);
    }
    if (aiMode == AiMode::IDLE || aiMode == AiMode::TURNING || aiMode == AiMode::WAKING ||
        aiMode == AiMode::FEEDING || aiMode == AiMode::RESTING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        return;
    }

    float waypointX = targetX;
    float waypointY = targetY;
    if (!currentWaypoint(waypointX, waypointY) && !buildMoveRoute(targetX, targetY)) {
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + 1200;
        return;
    }
    currentWaypoint(waypointX, waypointY);

    float prevX = monsterX;
    float prevY = monsterY;
    float dx = waypointX - monsterX;
    float dy = waypointY - monsterY;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = (aiMode == AiMode::SEEK_FOOD || aiMode == AiMode::SEEK_BED ||
                   aiMode == AiMode::LEAVING_BED) ? 19.0f : 10.5f;
    if (mon.mood < 40 || mon.satiety < 20) speed *= 0.72f;
    speed *= behaviorProfile.moveSpeedScale;
    float step = speed * dtSeconds;
    if (dist < 1.2f || step >= dist) {
        monsterX = waypointX;
        monsterY = waypointY;
        moveRouteIndex++;
        if (moveRouteIndex >= moveRouteCount) {
            finishMovement(nowMs);
            return;
        }
        float nextX = moveRouteX[moveRouteIndex];
        float nextY = moveRouteY[moveRouteIndex];
        PmdDirection nextDirection = pmdDirectionForVelocity(nextX - monsterX, nextY - monsterY);
        uint8_t directionDelta = (uint8_t)abs((int)nextDirection - (int)pmdDirection);
        if (directionDelta > 4) directionDelta = 8 - directionDelta;
        if (directionDelta >= 2) beginTurn(aiMode, nextDirection, nowMs);
        return;
    }

    velocityX = dx / dist * speed;
    velocityY = dy / dist * speed * 0.75f;
    monsterX += velocityX * dtSeconds;
    monsterY += velocityY * dtSeconds;
    if (fabsf(dx) > 8.0f) facingRight = dx > 0.0f;

    float walkOffsetY = walkBoundaryOffsetY();
    monsterX = clampf(monsterX, (float)roomWalkMinX(), (float)roomWalkMaxX());
    monsterY = clampf(monsterY, (float)roomWalkMinY() - walkOffsetY,
                      (float)roomWalkMaxY() - walkOffsetY);
    bool wasInsideWalkArea = monsterFootprintInsideWalkArea(prevX, prevY);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(aiMode == AiMode::LEAVING_BED && !wasInsideWalkArea)) {
        monsterX = prevX;
        monsterY = prevY;
        velocityX = 0.0f;
        velocityY = 0.0f;
        if (!buildMoveRoute(targetX, targetY)) {
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + random(1200, 2601);
        }
        return;
    }
    updateStuckWatchdog(nowMs, sqrtf((waypointX - monsterX) * (waypointX - monsterX) +
                                     (waypointY - monsterY) * (waypointY - monsterY)));
}

bool MainScene::monsterNearFood() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - foodFeedX()) < 9.0f && fabsf(walkY - foodFeedY()) < 7.0f;
}

bool MainScene::monsterNearBed() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - bedCenterX()) <= BED_APPROACH_TOLERANCE_X &&
           fabsf(walkY - bedCenterY()) <= BED_APPROACH_TOLERANCE_Y;
}

bool MainScene::monsterAtBedSleepPose() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - bedSleepX()) <= BED_SLEEP_TOLERANCE_X &&
           fabsf(walkY - bedSleepY()) <= BED_SLEEP_TOLERANCE_Y;
}

bool MainScene::monsterNeedsBedRest() const {
    uint32_t nowMs = Hal::ins().millis();
    if ((int32_t)(nowMs - postFeedAwakeUntilMs) < 0) return false;
    return mind.topDesire() == MonsterDesire::REST;
}

void MainScene::updateMind(uint32_t nowMs) {
    if ((int32_t)(nowMs - nextMindUpdateMs) < 0) return;
    mind.update(GameEngine::ins().activeMonster(), mainSceneIsNight(),
                GameEngine::ins().bowlHasFood(), nowMs);
    nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
}

void MainScene::beginMovement(AiMode mode, uint32_t nowMs) {
    if (!buildMoveRoute(targetX, targetY)) {
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + random(1200, 2601);
        return;
    }
    float waypointX = targetX;
    float waypointY = targetY;
    currentWaypoint(waypointX, waypointY);
    PmdDirection direction = pmdDirectionForVelocity(waypointX - monsterX, waypointY - monsterY);
    uint8_t directionDelta = (uint8_t)abs((int)direction - (int)pmdDirection);
    if (directionDelta > 4) directionDelta = 8 - directionDelta;
    if (directionDelta >= 2) {
        beginTurn(mode, direction, nowMs);
    } else {
        aiMode = mode;
    }
    lastWaypointDistance = 1000000.0f;
    lastMoveProgressMs = nowMs;
    stuckRecoveryCount = 0;
}

void MainScene::beginTurn(AiMode nextMode, PmdDirection direction, uint32_t nowMs) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    turnNextMode = nextMode;
    turnTargetDirection = direction;
    stateUntilMs = nowMs + behaviorProfile.turnPauseMs;
    aiMode = AiMode::TURNING;
}

void MainScene::beginWaking(uint32_t nowMs, bool forFood) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    wakingForFood = forFood;
    uint16_t delayMs = mainSceneIsNight()
        ? (uint16_t)random(NIGHT_FEED_WAKE_DELAY_MIN_MS, NIGHT_FEED_WAKE_DELAY_MAX_MS + 1)
        : (uint16_t)random(DAY_WAKE_DELAY_MIN_MS, DAY_WAKE_DELAY_MAX_MS + 1);
    stateUntilMs = nowMs + delayMs;
    aiMode = AiMode::WAKING;
}

void MainScene::enterResting(uint32_t nowMs) {
    snapMonsterToBed();
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::RESTING;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + 2000;
    mind.onRested(nowMs);
}

void MainScene::finishMovement(uint32_t nowMs) {
    AiMode completedMode = aiMode;
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    if (completedMode == AiMode::SEEK_FOOD && GameEngine::ins().bowlHasFood() &&
        GameEngine::ins().activeMonster().satiety < FEED_CONTINUE_SATIETY && monsterNearFood()) {
        enterFeeding(nowMs);
        return;
    }
    if (completedMode == AiMode::SEEK_BED) {
        enterResting(nowMs);
        return;
    }
    if (completedMode == AiMode::LEAVING_BED) {
        if (wakingForFood && GameEngine::ins().bowlHasFood() &&
            GameEngine::ins().activeMonster().satiety < FEED_CONTINUE_SATIETY) {
            setFoodTarget(nowMs);
        } else {
            aiMode = AiMode::IDLE;
            targetX = monsterX;
            targetY = monsterY;
            nextAiDecisionMs = nowMs + random(700, 1401);
        }
        return;
    }
    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
    mind.onActivity(nowMs);
}

void MainScene::setFoodTarget(uint32_t nowMs) {
    targetX = foodFeedX();
    targetY = foodFeedY() - walkBoundaryOffsetY();
    if (!monsterFootprintInsideWalkArea(targetX, targetY)) {
        if (!chooseFoodApproachPose(targetX, targetY)) {
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + 2200;
            return;
        }
    }
    beginMovement(AiMode::SEEK_FOOD, nowMs);
}

void MainScene::setBedTarget(uint32_t nowMs) {
    if (!chooseBedApproachPose(targetX, targetY)) {
        randomMonsterCenterWalkPoint(targetX, targetY);
    }
    beginMovement(AiMode::SEEK_BED, nowMs);
}

void MainScene::snapMonsterToBed() {
    if (!chooseBedSleepPose(monsterX, monsterY)) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
    }
    targetX = monsterX;
    targetY = monsterY;
}

void MainScene::enterFeeding(uint32_t nowMs) {
    feedingConsumed = false;
    feedingBiteMs = nowMs + (uint32_t)random(FEED_BITE_DELAY_MIN_MS, FEED_BITE_DELAY_MAX_MS + 1);
    feedingUntilMs = nowMs + (uint32_t)random(FEED_SESSION_MIN_MS, FEED_SESSION_MAX_MS + 1);
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;
    PmdDirection foodDirection = pmdDirectionForVelocity(
        foodCenterX() - monsterX,
        foodCenterY() - (monsterY + walkBoundaryOffsetY()));
    facingRight = foodCenterX() > monsterX;
    uint8_t directionDelta = (uint8_t)abs((int)foodDirection - (int)pmdDirection);
    if (directionDelta > 4) directionDelta = 8 - directionDelta;
    if (directionDelta >= 2) {
        beginTurn(AiMode::FEEDING, foodDirection, nowMs);
    } else {
        pmdDirection = foodDirection;
        aiMode = AiMode::FEEDING;
    }
}

void MainScene::updateFeeding(uint32_t nowMs) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if ((int32_t)(nowMs - feedingBiteMs) >= 0 &&
        GameEngine::ins().bowlHasFood() && mon.satiety < FEED_CONTINUE_SATIETY) {
        bool fed = GameEngine::ins().consumeBowlFood();
        feedingConsumed = feedingConsumed || fed;
        feedingBiteMs = nowMs + (uint32_t)random(FEED_BITE_INTERVAL_MIN_MS,
                                                   FEED_BITE_INTERVAL_MAX_MS + 1);
        if (!fed) feedingUntilMs = nowMs + 600;
    }
    if (!GameEngine::ins().bowlHasFood() || mon.satiety >= FEED_CONTINUE_SATIETY) {
        if ((int32_t)(feedingUntilMs - (nowMs + 700)) > 0) feedingUntilMs = nowMs + 700;
    }

    if ((int32_t)(nowMs - feedingUntilMs) < 0) return;

    if (feedingConsumed) {
        postFeedAwakeUntilMs = nowMs + (uint32_t)random(POST_FEED_AWAKE_MIN_MS, POST_FEED_AWAKE_MAX_MS + 1);
    }
    feedingConsumed = false;
    aiMode = AiMode::IDLE;
    nextAiDecisionMs = nowMs + random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
    mind.onAte(nowMs);
}

void MainScene::updateDebugTiltControl(uint32_t nowMs, float dtSeconds) {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!Hal::ins().readAccel(ax, ay, az)) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        return;
    }

    auto applyDeadzone = [](float value) {
        if (fabsf(value) < DEBUG_TILT_DEADZONE) return 0.0f;
        return clampf(value, -DEBUG_TILT_MAX, DEBUG_TILT_MAX) / DEBUG_TILT_MAX;
    };

    // StickS3 is rendered in landscape rotation=1. Map raw IMU axes to screen
    // coordinates so positive input means right/down on the room canvas.
    float inputX = applyDeadzone(-ax);
    float inputY = applyDeadzone(ay);
    float inputLength = sqrtf(inputX * inputX + inputY * inputY);
    if (inputLength > 1.0f) {
        inputX /= inputLength;
        inputY /= inputLength;
    }

    velocityX = inputX * DEBUG_TILT_SPEED;
    velocityY = inputY * DEBUG_TILT_SPEED * 0.75f;
    if (fabsf(velocityX) < PMD_MOVING_SPEED_EPSILON &&
        fabsf(velocityY) < PMD_MOVING_SPEED_EPSILON) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        return;
    }

    monsterX += velocityX * dtSeconds;
    monsterY += velocityY * dtSeconds;
    targetX = monsterX + velocityX * 0.25f;
    targetY = monsterY + velocityY * 0.25f;
    aiMode = AiMode::WANDER;
    nextAiDecisionMs = nowMs + 1000;
    if (fabsf(velocityX) > 8.0f) facingRight = velocityX > 0.0f;
}

void MainScene::updatePmdSpriteState(uint32_t nowMs) {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config) return;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool sleeping = (mon.statusBits & Game::STATUS_SLEEP) != 0 ||
                    mon.fainted ||
                    mon.hpCur == 0 ||
                    aiMode == AiMode::RESTING ||
                    (aiMode == AiMode::WAKING && (int32_t)(nowMs - stateUntilMs) < 0);
    float speedSq = velocityX * velocityX + velocityY * velocityY;
    bool moving = speedSq > PMD_MOVING_SPEED_EPSILON * PMD_MOVING_SPEED_EPSILON;
    PmdAction nextAction = sleeping ? PmdAction::SLEEPING : (moving ? PmdAction::WALKING : PmdAction::IDLE);
    PmdDirection nextDirection = moving ? pmdDirectionForVelocity(velocityX, velocityY) : pmdDirection;
    auto resetMoveLength = [&]() {
        float dx = targetX - monsterX;
        float dy = targetY - monsterY;
        pmdLongMove = sqrtf(dx * dx + dy * dy) > PMD_SHORT_MOVE_DISTANCE;
    };

    if (pmdAction == PmdAction::STOPPING) {
        if (nextAction == PmdAction::WALKING || nextAction == PmdAction::SLEEPING) {
            pmdAction = nextAction;
            pmdDirection = nextDirection;
            pmdFrame = 0;
            if (nextAction == PmdAction::WALKING) resetMoveLength();
            pmdFrameStartedMs = nowMs;
            return;
        }
        uint8_t frameCount = pmdStoppingPlaybackFrameCount(config);
        while (nowMs - pmdFrameStartedMs >= PMD_WALKING_FRAME_MS) {
            if (pmdFrame + 1 < frameCount) {
                pmdFrame++;
                pmdFrameStartedMs += PMD_WALKING_FRAME_MS;
            } else {
                pmdAction = PmdAction::IDLE;
                pmdFrame = 0;
                pmdLongMove = false;
                pmdFrameStartedMs = nowMs;
                break;
            }
        }
        return;
    }

    if (pmdAction == PmdAction::WALKING &&
        nextAction == PmdAction::IDLE &&
        ((config->motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) ||
         (config->motionMode == PmdMotionMode::PINGPONG && config->walkingFrames >= 2))) {
        pmdAction = PmdAction::STOPPING;
        pmdFrame = 0;
        pmdFrameStartedMs = nowMs;
        return;
    }

    if (nextAction != pmdAction || nextDirection != pmdDirection) {
        pmdAction = nextAction;
        pmdDirection = nextDirection;
        pmdFrame = 0;
        if (nextAction == PmdAction::WALKING) resetMoveLength();
        pmdFrameStartedMs = nowMs;
        return;
    }

    uint16_t frameMs = config->idleFrameMs;
    uint8_t frameCount = config->idleFrames;
    bool loops = true;
    if (pmdAction == PmdAction::WALKING) {
        frameMs = PMD_WALKING_FRAME_MS;
        frameCount = pmdWalkingPlaybackFrameCount(config, pmdLongMove);
        loops = pmdWalkingPlaybackLoops(config);
    } else if (pmdAction == PmdAction::SLEEPING) {
        frameMs = PMD_SLEEPING_FRAME_MS;
        frameCount = config->sleepingFrames;
    }
    if (frameCount == 0) frameCount = 1;
    while (nowMs - pmdFrameStartedMs >= frameMs) {
        if (loops) {
            pmdFrame = (uint8_t)((pmdFrame + 1) % frameCount);
        } else if (pmdFrame + 1 < frameCount) {
            pmdFrame++;
        }
        pmdFrameStartedMs += frameMs;
    }
}

MainScene::PmdDirection MainScene::pmdDirectionForVelocity(float vx, float vy) const {
    float degrees = atan2f(vy, vx) * 57.2957795f;
    if (degrees >= -22.5f && degrees < 22.5f) return PmdDirection::RIGHT;
    if (degrees >= 22.5f && degrees < 67.5f) return PmdDirection::DOWN_RIGHT;
    if (degrees >= 67.5f && degrees < 112.5f) return PmdDirection::FRONT;
    if (degrees >= 112.5f && degrees < 157.5f) return PmdDirection::DOWN_LEFT;
    if (degrees >= -67.5f && degrees < -22.5f) return PmdDirection::UP_RIGHT;
    if (degrees >= -112.5f && degrees < -67.5f) return PmdDirection::BACK;
    if (degrees >= -157.5f && degrees < -112.5f) return PmdDirection::UP_LEFT;
    return PmdDirection::LEFT;
}

PokemonSprites::SpriteKind MainScene::pmdSpriteKind() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config) return PokemonSprites::SpriteKind::FRONT;

    if (pmdAction == PmdAction::SLEEPING) {
        uint16_t base = static_cast<uint16_t>(config->sleepingBase);
        return static_cast<PokemonSprites::SpriteKind>(base + (pmdFrame % config->sleepingFrames));
    }

    uint16_t directionIndex = pmdDirectionFrameIndex();
    if (pmdAction == PmdAction::IDLE) {
        uint16_t base = static_cast<uint16_t>(config->idleBase);
        return static_cast<PokemonSprites::SpriteKind>(base + directionIndex * config->idleFrames + (pmdFrame % config->idleFrames));
    }

    uint8_t walkingFrame = pmdAction == PmdAction::STOPPING
        ? pmdStoppingSpriteFrameIndex(config, pmdFrame)
        : pmdWalkingSpriteFrameIndex(config, pmdFrame, pmdLongMove);
    uint16_t base = static_cast<uint16_t>(config->walkingBase);
    return static_cast<PokemonSprites::SpriteKind>(base + directionIndex * config->walkingFrames + walkingFrame);
}

const PokemonSprites::SpriteFrame* MainScene::currentMonsterFrame() const {
    if (!active) return nullptr;
    if (pmdSpriteConfigForSpecies(active->id)) {
        return PokemonSprites::findSpeciesSprite(active->id, pmdSpriteKind());
    }
    return PokemonSprites::findSpeciesSprite(active->id, PokemonSprites::SpriteKind::FRONT);
}

const PokemonSprites::SpriteFrame* MainScene::movementBoundsFrame() const {
    if (!active) return nullptr;
    if (const PmdSpriteConfig* config = pmdSpriteConfigForSpecies(active->id)) {
        return PokemonSprites::findSpeciesSprite(active->id, config->idleBase);
    }
    return PokemonSprites::findSpeciesSprite(active->id, PokemonSprites::SpriteKind::FRONT);
}

uint16_t MainScene::pmdDirectionFrameIndex() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config || !config->mirrorRightDirections) return static_cast<uint16_t>(pmdDirection);

    switch (pmdDirection) {
    case PmdDirection::FRONT: return 0;
    case PmdDirection::DOWN_LEFT: return 1;
    case PmdDirection::LEFT: return 2;
    case PmdDirection::UP_LEFT: return 3;
    case PmdDirection::BACK: return 4;
    case PmdDirection::UP_RIGHT: return 3;
    case PmdDirection::RIGHT: return 2;
    case PmdDirection::DOWN_RIGHT: return 1;
    }
    return 0;
}

bool MainScene::pmdDirectionFlipX() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config || !config->mirrorRightDirections) return false;
    return pmdDirection == PmdDirection::UP_RIGHT ||
           pmdDirection == PmdDirection::RIGHT ||
           pmdDirection == PmdDirection::DOWN_RIGHT;
}

void MainScene::chooseAiGoal(uint32_t nowMs) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    switch (mind.topDesire()) {
    case MonsterDesire::EAT:
        if (GameEngine::ins().bowlHasFood() && mon.satiety < FEED_CONTINUE_SATIETY) {
            if (monsterNearFood()) enterFeeding(nowMs);
            else setFoodTarget(nowMs);
            return;
        }
        break;
    case MonsterDesire::REST:
        if (monsterNeedsBedRest()) {
            if (monsterNearBed()) enterResting(nowMs);
            else setBedTarget(nowMs);
            return;
        }
        break;
    case MonsterDesire::WANDER: {
        float walkOffsetY = walkBoundaryOffsetY();
        for (uint8_t tries = 0; tries < 18; ++tries) {
            int radiusX = behaviorProfile.wanderRadiusX;
            int radiusY = behaviorProfile.wanderRadiusY;
            float candidateX = clampf(monsterX + (float)random(-radiusX, radiusX + 1),
                                      (float)roomWalkMinX(),
                                      (float)roomWalkMaxX());
            float candidateY = clampf(monsterY + (float)random(-radiusY, radiusY + 1),
                                      (float)roomWalkMinY() - walkOffsetY,
                                      (float)roomWalkMaxY() - walkOffsetY);
            if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) continue;
            if (fabsf(candidateX - monsterX) < 8.0f && fabsf(candidateY - monsterY) < 4.0f) continue;
            targetX = candidateX;
            targetY = candidateY;
            beginMovement(AiMode::WANDER, nowMs);
            return;
        }
        break;
    }
    case MonsterDesire::STARE:
        if (random(100) < 32) {
            int direction = (int)pmdDirection + (random(2) == 0 ? -1 : 1);
            if (direction < 0) direction += 8;
            if (direction >= 8) direction -= 8;
            nextAiDecisionMs = nowMs + random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
            beginTurn(AiMode::IDLE, (PmdDirection)direction, nowMs);
            mind.onActivity(nowMs);
            return;
        }
        break;
    }

    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
    if (mind.topDesire() != MonsterDesire::STARE) {
        mind.onActivity(nowMs);
    }
}

void MainScene::render() {
    int16_t depthZ = (int16_t)(monsterY - 78.0f);
    RenderItem items[] = {
        {0, &MainScene::drawBackground},
        {10, &MainScene::drawFloor},
        {18, &MainScene::drawFood},
        {(int16_t)(20 + depthZ), &MainScene::drawShadow},
        {(int16_t)(30 + depthZ), &MainScene::drawMonster},
        {(int16_t)(40 + depthZ), &MainScene::drawStateEffect},
        {85, &MainScene::drawNightOverlay},
        {88, &MainScene::drawWalkBoundary},
        {90, &MainScene::drawHud},
        {100, &MainScene::drawToast},
    };
    sortAndDraw(items, sizeof(items) / sizeof(items[0]));
}

bool MainScene::onButton(const ButtonEvent& event) {
    if (event.action != BtnAction::PRESSED && event.action != BtnAction::LONG_PRESS) {
        return false;
    }

    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        GameEngine::ins().petMonster();
        toast = Ui::Menu::PET_TOAST;
        toastUntil = Hal::ins().millis() + 1200;
        return true;
    }

    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        uint32_t nowMs = Hal::ins().millis();
        FoodPlacementResult result = GameEngine::ins().placeSelectedFoodInBowl();
        switch (result) {
        case FoodPlacementResult::ADDED:
            toast = Ui::Menu::FOOD_ADDED;
            nextMindUpdateMs = nowMs;
            if (aiMode == AiMode::IDLE && (int32_t)(nextAiDecisionMs - (nowMs + 1200)) > 0) {
                nextAiDecisionMs = nowMs + 1200;
            }
            break;
        case FoodPlacementResult::BOWL_FULL:
            toast = Ui::Menu::FOOD_FULL;
            break;
        case FoodPlacementResult::DIFFERENT_FOOD:
            toast = Ui::Menu::FOOD_MIXED;
            break;
        case FoodPlacementResult::NO_STOCK:
        default:
            toast = Ui::Menu::NO_FOOD;
            break;
        }
        toastUntil = nowMs + 1200;
        return true;
    }

    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    return false;
}

void MainScene::drawBackground() {
    PixelRenderer::clear(PixelRenderer::rgb(5, 6, 18));
    RoomRenderer::draw(cameraY, mainSceneIsNight());
}

void MainScene::drawFloor() {
}

void MainScene::drawFood() {
    if (!GameEngine::ins().bowlHasFood()) return;
    auto& c = PixelRenderer::canvas();
    uint8_t foodIndex = GameEngine::ins().bowlFoodIndex();
    uint16_t foodColor = foodIndex == 1 ? PixelRenderer::rgb(255, 138, 112)
                                        : PixelRenderer::rgb(245, 180, 87);
    uint16_t garnishColor = foodIndex == 1 ? PixelRenderer::rgb(255, 216, 72)
                                           : PixelRenderer::rgb(92, 151, 80);
    int cx = (int)foodCenterX();
    c.fillEllipse(cx, worldToScreenY(foodCenterY() + 3.0f), 10, 4, PixelRenderer::rgb(122, 96, 76));
    c.fillEllipse(cx, worldToScreenY(foodCenterY()), 8, 3, foodColor);
    c.fillCircle(cx - 4, worldToScreenY(foodCenterY() - 2.0f), 2, garnishColor);
    c.fillCircle(cx + 3, worldToScreenY(foodCenterY() - 1.0f), 2, PixelRenderer::rgb(178, 79, 57));
}

void MainScene::drawShadow() {
    bool night = mainSceneIsNight();
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    bool floating = config && config->airHeight > 0.0f;
    uint8_t frameW = 38;
    uint8_t frameH = 42;
    if (const PokemonSprites::SpriteFrame* frame = currentMonsterFrame()) {
        frameW = pgm_read_byte(&frame->width);
        frameH = pgm_read_byte(&frame->height);
    }

    int rx = constrain((int)(frameW * (floating ? 0.25f : 0.44f)), floating ? 10 : 16, floating ? 22 : 40);
    int ry = constrain((int)(frameH * (floating ? 0.055f : 0.14f)), floating ? 3 : 6, floating ? 6 : 14);
    if (night) {
        rx = (rx * 11 + 5) / 10;
        ry = (ry * 11 + 5) / 10;
    }

    int shadowY = worldToScreenY(monsterY + constrain((int)(frameH * 0.42f), 16, 32));
    uint16_t shadowColor = night ? PixelRenderer::rgb(18, 16, 24) : PixelRenderer::rgb(36, 29, 24);
    uint8_t outerAlpha = night ? (floating ? 84 : 122) : (floating ? 68 : 116);
    uint8_t coreAlpha = night ? (floating ? 0 : 92) : (floating ? 0 : 86);
    fillSoftEllipseAlpha((int)monsterX, shadowY, rx, ry, shadowColor, outerAlpha);
    if (!floating) {
        fillEllipseAlpha((int)monsterX, shadowY, max(5, rx / 2), max(2, ry / 2), shadowColor, coreAlpha);
    }
}

void MainScene::drawMonster() {
    auto& c = PixelRenderer::canvas();
    int x = (int)monsterX;
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int y = worldToScreenY(monsterY - pmdFloatYOffset(config, Hal::ins().millis()));
    if (active && pmdSpriteConfigForSpecies(active->id) && drawPmdMonster(x, y)) {
        return;
    }

    const PokemonSprites::SpriteFrame* frame = currentMonsterFrame();
    if (frame) {
        uint8_t w = pgm_read_byte(&frame->width);
        uint8_t h = pgm_read_byte(&frame->height);
        if (PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2, facingRight)) {
            return;
        }
    }
    c.fillRect(x - 19, y - 25, 38, 42, active->colorA);
    c.fillRect(x - 12, y - 17, 24, 25, active->colorB);
    c.fillCircle(x - 7, y - 8, 2, PixelRenderer::rgb(24, 30, 38));
    c.fillCircle(x + 7, y - 8, 2, PixelRenderer::rgb(24, 30, 38));
    c.drawLine(x - 5, y + 4, x + 5, y + 4, PixelRenderer::rgb(24, 30, 38));
}

bool MainScene::drawPmdMonster(int x, int y) {
    if (!active) return false;
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(active->id, pmdSpriteKind());
    if (!frame) return false;

    uint8_t w = pgm_read_byte(&frame->width);
    uint8_t h = pgm_read_byte(&frame->height);
    if (PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2,
                                  pmdAction == PmdAction::SLEEPING ? false : pmdDirectionFlipX())) {
        return true;
    }
    return false;
}

void MainScene::drawStateEffect() {
    if (GameEngine::ins().moodValue() < 50) return;
    auto& c = PixelRenderer::canvas();
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int x = (int)monsterX + 18;
    int y = worldToScreenY(monsterY - pmdFloatYOffset(config, Hal::ins().millis())) - 31;
    c.fillCircle(x, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillCircle(x + 5, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillTriangle(x - 3, y + 2, x + 8, y + 2, x + 2, y + 9, PixelRenderer::rgb(255, 103, 135));
}

void MainScene::drawNightOverlay() {
    bool night = mainSceneIsNight();
    uint8_t lightSource = GameEngine::ins().debugLightSourceIndex();
    if (lightSource == 0) return;

    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int followX = (int)monsterX;
    int followY = worldToScreenY(monsterY - pmdFloatYOffset(config, Hal::ins().millis()) - 10.0f);
    int lightX = followX;
    int lightY = followY;
    switch (lightSource) {
    case 1:
        lightX = 44;
        lightY = 30;
        break;
    case 2:
        lightX = Hal::DISPLAY_W / 2;
        lightY = 20;
        break;
    case 3:
        lightX = Hal::DISPLAY_W - 44;
        lightY = 30;
        break;
    case 4:
        lightX = 34;
        lightY = Hal::DISPLAY_H / 2;
        break;
    case 5:
        lightX = Hal::DISPLAY_W - 34;
        lightY = Hal::DISPLAY_H / 2;
        break;
    default:
        break;
    }

    if (night) {
        int coreRadiusX = 58;
        int coreRadiusY = 42;
        int haloRadiusX = 112;
        int haloRadiusY = 76;
        fillRadialLightAlpha(lightX, lightY, coreRadiusX, coreRadiusY,
                             PixelRenderer::rgb(255, 210, 128), 88);
        fillRadialLightAlpha(lightX, lightY, haloRadiusX, haloRadiusY,
                             PixelRenderer::rgb(255, 151, 92), 30);
    } else {
        fillRadialLightAlpha(lightX, lightY, 68, 46,
                             PixelRenderer::rgb(255, 226, 150), 30);
        fillRadialLightAlpha(lightX, lightY, 118, 76,
                             PixelRenderer::rgb(255, 176, 96), 10);
    }

    if (lightSource != 0) {
        auto& c = PixelRenderer::canvas();
        c.fillCircle(lightX, lightY, 3, PixelRenderer::rgb(255, 236, 158));
        c.drawCircle(lightX, lightY, 5, PixelRenderer::rgb(255, 169, 79));
    }
}

void MainScene::drawWalkBoundary() {
    if (!GameEngine::ins().debugWalkBoundaryVisible()) return;

    uint8_t count = room().walkPolygonCount();
    if (count < 2) return;

    auto& c = PixelRenderer::canvas();
    uint16_t outline = PixelRenderer::rgb(0, 0, 0);
    uint16_t red = PixelRenderer::rgb(255, 32, 32);

    int16_t firstX = roomPointX(0);
    int16_t firstY = worldToScreenY((float)roomPointY(0));
    int16_t prevX = firstX;
    int16_t prevY = firstY;
    for (uint8_t i = 1; i <= count; ++i) {
        uint8_t index = i == count ? 0 : i;
        int16_t x = roomPointX(index);
        int16_t y = worldToScreenY((float)roomPointY(index));

        c.drawLine(prevX - 1, prevY, x - 1, y, outline);
        c.drawLine(prevX + 1, prevY, x + 1, y, outline);
        c.drawLine(prevX, prevY - 1, x, y - 1, outline);
        c.drawLine(prevX, prevY + 1, x, y + 1, outline);
        c.drawLine(prevX, prevY, x, y, red);

        c.fillCircle(x, y, 2, red);
        prevX = x;
        prevY = y;
    }

    uint16_t routeColor = PixelRenderer::rgb(255, 216, 72);
    int routePrevX = (int)roundf(monsterX);
    int routePrevY = worldToScreenY(monsterY);
    for (uint8_t i = moveRouteIndex; i < moveRouteCount; ++i) {
        int routeX = (int)roundf(moveRouteX[i]);
        int routeY = worldToScreenY(moveRouteY[i]);
        c.drawLine(routePrevX, routePrevY, routeX, routeY, routeColor);
        c.fillCircle(routeX, routeY, 2, routeColor);
        routePrevX = routeX;
        routePrevY = routeY;
    }
    c.drawCircle((int)roundf(targetX), worldToScreenY(targetY), 4,
                 PixelRenderer::rgb(64, 210, 255));
    char debugText[24];
    snprintf(debugText, sizeof(debugText), "AI:%u D:%u R:%u B:%u",
             (unsigned)aiMode, (unsigned)mind.topDesire(), (unsigned)moveRouteCount,
             (unsigned)GameEngine::ins().bowlFoodCount());
    PixelRenderer::text(2, 2, debugText, 0xFFFF, 1);
}

void MainScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 168;
    static constexpr int PANEL_Y = 2;
    static constexpr int PANEL_W = 68;
    static constexpr int PANEL_H = 38;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    fillRectAlpha(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(8, 10, 14), 150);
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(72, 83, 98));

    char clock[8];
    uint16_t gameMinutes = GameEngine::ins().gameMinutesOfDay();
    snprintf(clock, sizeof(clock), "%02u:%02u", gameMinutes / 60, gameMinutes % 60);
    PixelRenderer::text(PANEL_X + 20, PANEL_Y + 3, clock, PixelRenderer::rgb(245, 246, 232));

    uint8_t hunger = GameEngine::ins().hungerValue();
    if (hunger > 0) {
        drawHungerIcon(PANEL_X + 4, PANEL_Y + 19, hunger);
    }

    uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
    if (mon.fainted || mon.hpCur == 0 || mon.hpMax == 0) hpPct = 0;
    int barX = PANEL_X + 30;
    int barY = PANEL_Y + 25;
    int barW = 30;
    c.fillRect(barX, barY, barW, 6, PixelRenderer::rgb(39, 45, 50));
    int fillW = ((barW - 2) * hpPct) / 100;
    if (fillW > 0) c.fillRect(barX + 1, barY + 1, fillW, 4, PixelRenderer::rgb(92, 222, 112));
    c.drawRect(barX, barY, barW, 6, PixelRenderer::rgb(245, 246, 232));
}

void MainScene::drawToast() {
    if (!toast) return;
    if ((int32_t)(Hal::ins().millis() - toastUntil) >= 0) {
        toast = nullptr;
        return;
    }
    auto& c = PixelRenderer::canvas();
    c.fillRect(68, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(76, 8, toast, PixelRenderer::rgb(255, 255, 255));
}

void MainScene::sortAndDraw(RenderItem* items, uint8_t count) {
    std::sort(items, items + count, [](const RenderItem& a, const RenderItem& b) {
        return a.z < b.z;
    });
    for (uint8_t i = 0; i < count; ++i) {
        (this->*items[i].draw)();
    }
}
