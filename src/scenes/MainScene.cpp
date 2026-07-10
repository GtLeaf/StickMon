#include "scenes/MainScene.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/HudAssets.h"
#include "assets/PokemonSprites.h"
#include "assets/RoomAssets.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"
extern "C" {
#include "third_party/uzlib/uzlib.h"
}

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
static constexpr float FOOD_CENTER_X = (float)RoomAssets::ROOM_FOOD_X;
static constexpr float FOOD_CENTER_Y = (float)RoomAssets::ROOM_FOOD_Y;
static constexpr float FOOD_FEED_OFFSET_X = 12.0f;
static constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
static constexpr float FOOD_FEED_X = FOOD_CENTER_X + FOOD_FEED_OFFSET_X;
static constexpr float FOOD_FEED_Y = FOOD_CENTER_Y + FOOD_FEED_OFFSET_Y;
static constexpr float BED_CENTER_X = (float)RoomAssets::ROOM_BED_X;
static constexpr float BED_CENTER_Y = (float)RoomAssets::ROOM_BED_Y;
static constexpr float BED_SLEEP_X = (float)RoomAssets::ROOM_BED_X;
static constexpr float BED_SLEEP_Y = (float)RoomAssets::ROOM_BED_Y;
static constexpr float BED_APPROACH_TOLERANCE_X = 18.0f;
static constexpr float BED_APPROACH_TOLERANCE_Y = 14.0f;
static constexpr float BED_SLEEP_TOLERANCE_X = 2.5f;
static constexpr float BED_SLEEP_TOLERANCE_Y = 6.0f;
static constexpr uint32_t FEED_REQUEST_TIMEOUT_MS = 18000;
static constexpr uint32_t NIGHT_FEED_REQUEST_TIMEOUT_MS = 45000;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MIN_MS = 2600;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MAX_MS = 6500;
static constexpr uint16_t FEED_BITE_DELAY_MIN_MS = 700;
static constexpr uint16_t FEED_BITE_DELAY_MAX_MS = 1300;
static constexpr uint16_t FEED_SESSION_MIN_MS = 3200;
static constexpr uint16_t FEED_SESSION_MAX_MS = 5600;
static constexpr uint16_t POST_FEED_AWAKE_MIN_MS = 7000;
static constexpr uint16_t POST_FEED_AWAKE_MAX_MS = 13000;
static constexpr uint8_t DAY_AUTO_FEED_HUNGER = 55;
static constexpr uint8_t NIGHT_AUTO_FEED_HUNGER = 30;
static constexpr uint32_t ROOM_BUFFER_PIXELS =
    (uint32_t)RoomAssets::STANDARD_ROOM_W * (uint32_t)RoomAssets::STANDARD_ROOM_H;

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
    float bottom = (float)(RoomAssets::STANDARD_ROOM_Y + RoomAssets::STANDARD_ROOM_H);
    float maxY = bottom - (float)Hal::DISPLAY_H;
    return maxY > 0.0f ? maxY : 0.0f;
}

float cameraForWorldY(float worldY) {
    return clampf(worldY - CAMERA_FOCUS_Y, 0.0f, roomMaxCameraY());
}

int16_t roomPointX(uint8_t index) {
    return (int16_t)pgm_read_word(&RoomAssets::ROOM_WALK_POLYGON[index].x);
}

int16_t roomPointY(uint8_t index) {
    return (int16_t)pgm_read_word(&RoomAssets::ROOM_WALK_POLYGON[index].y);
}

int16_t bedPointX(uint8_t index) {
    return (int16_t)pgm_read_word(&RoomAssets::ROOM_BED_POLYGON[index].x);
}

int16_t bedPointY(uint8_t index) {
    return (int16_t)pgm_read_word(&RoomAssets::ROOM_BED_POLYGON[index].y);
}

bool roomWalkContains(float x, float y) {
    uint8_t count = RoomAssets::ROOM_WALK_POLYGON_COUNT;
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
    uint8_t count = RoomAssets::ROOM_BED_POLYGON_COUNT;
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
        float px = (float)random(RoomAssets::ROOM_WALK_MIN_X, RoomAssets::ROOM_WALK_MAX_X + 1);
        float py = (float)random(RoomAssets::ROOM_WALK_MIN_Y, RoomAssets::ROOM_WALK_MAX_Y + 1);
        if (roomWalkContains(px, py)) {
            x = px;
            y = py;
            return true;
        }
    }

    float centerX = (RoomAssets::ROOM_WALK_MIN_X + RoomAssets::ROOM_WALK_MAX_X) * 0.5f;
    float centerY = (RoomAssets::ROOM_WALK_MIN_Y + RoomAssets::ROOM_WALK_MAX_Y) * 0.5f;
    if (roomWalkContains(centerX, centerY)) {
        x = centerX;
        y = centerY;
        return true;
    }

    if (RoomAssets::ROOM_WALK_POLYGON_COUNT > 0) {
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
                          (float)RoomAssets::ROOM_WALK_MIN_X,
                          (float)RoomAssets::ROOM_WALK_MAX_X);
        float py = clampf(centerY + (float)random(-spanY, spanY + 1),
                          (float)RoomAssets::ROOM_WALK_MIN_Y,
                          (float)RoomAssets::ROOM_WALK_MAX_Y);
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

float pmdMoveSpeedScale(const Species* species) {
    return species && species->id == 129 ? 0.35f : 1.0f;
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

uint16_t* gRoomBuffer = nullptr;
bool gRoomBufferValid = false;
bool gRoomBufferNight = false;

bool ensureRoomBuffer() {
    if (gRoomBuffer) return true;
    if (!psramFound()) return false;
    size_t bytes = (size_t)ROOM_BUFFER_PIXELS * sizeof(uint16_t);
    gRoomBuffer = (uint16_t*)ps_malloc(bytes);
    gRoomBufferValid = false;
    return gRoomBuffer != nullptr;
}

bool inflateRawDeflate(const uint8_t* compressed, uint32_t compressedSize, uint8_t* out, uint32_t outSize) {
    TINF_DATA d;
    memset(&d, 0, sizeof(d));
    uzlib_init();
    uzlib_uncompress_init(&d, nullptr, 0);
    d.source = compressed;
    d.source_limit = compressed + compressedSize;
    d.dest_start = out;
    d.dest = out;
    d.dest_limit = out + outSize;

    int result = TINF_OK;
    while (d.dest < d.dest_limit) {
        result = uzlib_uncompress(&d);
        if (result == TINF_DONE) break;
        if (result != TINF_OK) return false;
    }
    return result == TINF_DONE || d.dest == d.dest_limit;
}

bool decodeRoomBaseToBuffer(uint16_t* out) {
    if (!out) return false;
    const uint32_t decodedBytes = RoomAssets::STANDARD_ROOM_BASE_RAW_BYTES;
    if (decodedBytes != ROOM_BUFFER_PIXELS * sizeof(uint16_t)) return false;

    uint8_t* compressed = psramFound()
        ? (uint8_t*)ps_malloc(RoomAssets::STANDARD_ROOM_BASE_COMPRESSED_LEN)
        : (uint8_t*)malloc(RoomAssets::STANDARD_ROOM_BASE_COMPRESSED_LEN);
    if (!compressed) return false;

    for (uint32_t i = 0; i < RoomAssets::STANDARD_ROOM_BASE_COMPRESSED_LEN; ++i) {
        compressed[i] = pgm_read_byte(&RoomAssets::STANDARD_ROOM_BASE_COMPRESSED[i]);
    }

    bool ok = inflateRawDeflate(
        compressed,
        RoomAssets::STANDARD_ROOM_BASE_COMPRESSED_LEN,
        (uint8_t*)out,
        decodedBytes
    );
    free(compressed);
    return ok;
}

void applyNightPatchToBuffer(uint16_t* out) {
    if (!out || RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUN_COUNT == 0) return;
    for (uint32_t runIndex = 0; runIndex < RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUN_COUNT; ++runIndex) {
        const RoomAssets::RoomPatchRun* run = &RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUNS[runIndex];
        uint16_t y = pgm_read_word(&run->y);
        uint16_t x = pgm_read_word(&run->x);
        uint16_t len = pgm_read_word(&run->len);
        uint32_t colorOffset = pgm_read_dword(&run->colorOffset);
        uint32_t dst = (uint32_t)y * RoomAssets::STANDARD_ROOM_W + x;
        if (dst >= ROOM_BUFFER_PIXELS) continue;
        uint32_t maxLen = ROOM_BUFFER_PIXELS - dst;
        if (len > maxLen) len = (uint16_t)maxLen;
        for (uint16_t i = 0; i < len && colorOffset + i < RoomAssets::STANDARD_ROOM_NIGHT_PATCH_PIXEL_COUNT; ++i) {
            out[dst + i] = pgm_read_word(&RoomAssets::STANDARD_ROOM_NIGHT_PATCH_PIXELS[colorOffset + i]);
        }
    }
}

bool prepareRoomBuffer(bool night) {
    if (!ensureRoomBuffer()) return false;
    if (gRoomBufferValid && gRoomBufferNight == night) return true;
    if (!decodeRoomBaseToBuffer(gRoomBuffer)) return false;
    if (night) applyNightPatchToBuffer(gRoomBuffer);
    gRoomBufferNight = night;
    gRoomBufferValid = true;
    return true;
}

void drawRoomBuffer(float cameraY) {
    if (!gRoomBuffer) return;
    int16_t roomScreenY = RoomAssets::STANDARD_ROOM_Y - (int16_t)roundf(cameraY);
    int16_t srcY = roomScreenY < 0 ? (int16_t)-roomScreenY : 0;
    int16_t dstY = roomScreenY > 0 ? roomScreenY : 0;
    int16_t drawH = (int16_t)RoomAssets::STANDARD_ROOM_H - srcY;
    int16_t screenRemaining = Hal::DISPLAY_H - dstY;
    if (drawH > screenRemaining) drawH = screenRemaining;
    if (drawH <= 0) return;

    PixelRenderer::canvas().pushImage(
        0,
        dstY,
        RoomAssets::STANDARD_ROOM_W,
        drawH,
        &gRoomBuffer[(uint32_t)srcY * RoomAssets::STANDARD_ROOM_W]
    );
}

void applyNightPatchToCanvas(float cameraY) {
    if (RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUN_COUNT == 0) return;
    auto& c = PixelRenderer::canvas();
    int16_t roomScreenY = RoomAssets::STANDARD_ROOM_Y - (int16_t)roundf(cameraY);
    for (uint32_t runIndex = 0; runIndex < RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUN_COUNT; ++runIndex) {
        const RoomAssets::RoomPatchRun* run = &RoomAssets::STANDARD_ROOM_NIGHT_PATCH_RUNS[runIndex];
        int16_t y = (int16_t)pgm_read_word(&run->y);
        int16_t screenY = roomScreenY + y;
        if (screenY < 0 || screenY >= Hal::DISPLAY_H) continue;

        int16_t x = (int16_t)pgm_read_word(&run->x);
        uint16_t len = pgm_read_word(&run->len);
        uint32_t colorOffset = pgm_read_dword(&run->colorOffset);
        for (uint16_t i = 0; i < len && colorOffset + i < RoomAssets::STANDARD_ROOM_NIGHT_PATCH_PIXEL_COUNT; ++i) {
            int16_t screenX = x + (int16_t)i;
            if (screenX < 0 || screenX >= Hal::DISPLAY_W) continue;
            uint16_t color = pgm_read_word(&RoomAssets::STANDARD_ROOM_NIGHT_PATCH_PIXELS[colorOffset + i]);
            c.drawPixel(screenX, screenY, color);
        }
    }
}

void drawRoomFallback(float cameraY, bool night) {
    (void)cameraY;
    (void)night;
    PixelRenderer::clear(PixelRenderer::rgb(16, 18, 24));
}

void drawRoomCached(float cameraY, bool night) {
    if (prepareRoomBuffer(night)) {
        drawRoomBuffer(cameraY);
    } else {
        drawRoomFallback(cameraY, night);
    }
}

void drawHungerIcon(int x, int y, uint8_t hunger) {
    if (hunger == 0) return;
    auto& c = PixelRenderer::canvas();
    uint8_t visibleRows = (uint8_t)(((uint16_t)HudAssets::HUNGER_ICON_H * hunger + 99) / 100);
    if (visibleRows > HudAssets::HUNGER_ICON_H) visibleRows = HudAssets::HUNGER_ICON_H;

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
            if (row >= visibleRows) continue;
            uint8_t col = (uint8_t)(pixel % HudAssets::HUNGER_ICON_W);
            c.drawPixel(x + col, y + row, color);
        }
    }
}
}

void MainScene::onEnter() {
    active = &GameEngine::ins().activeSpecies();
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
    }
    targetX = monsterX;
    targetY = monsterY;
    velocityX = 0.0f;
    velocityY = 0.0f;
    cameraY = cameraForWorldY(monsterY);
    pmdAction = PmdAction::IDLE;
    pmdDirection = PmdDirection::FRONT;
    pmdFrame = 0;
    pmdLongMove = false;
    pmdFrameStartedMs = Hal::ins().millis();
    nextAiDecisionMs = 0;
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
    if (const PokemonSprites::SpriteFrame* frame = currentMonsterFrame()) {
        frameH = pgm_read_byte(&frame->height);
    }
    return (float)constrain((int)(frameH * 0.42f), 16, 32);
}

float MainScene::walkFootprintRadiusX() const {
    uint8_t frameW = 38;
    if (const PokemonSprites::SpriteFrame* frame = currentMonsterFrame()) {
        frameW = pgm_read_byte(&frame->width);
    }
    return (float)constrain((int)(frameW * 0.24f), 7, 16);
}

float MainScene::walkFootprintRadiusY() const {
    uint8_t frameH = 42;
    if (const PokemonSprites::SpriteFrame* frame = currentMonsterFrame()) {
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
        float px = (float)random(RoomAssets::ROOM_WALK_MIN_X, RoomAssets::ROOM_WALK_MAX_X + 1);
        float py = (float)random(RoomAssets::ROOM_WALK_MIN_Y, RoomAssets::ROOM_WALK_MAX_Y + 1);
        float centerY = py - offsetY;
        if (px - rx < (float)RoomAssets::ROOM_WALK_MIN_X ||
            px + rx > (float)RoomAssets::ROOM_WALK_MAX_X) {
            continue;
        }
        if (monsterFootprintInsideWalkArea(px, centerY)) {
            x = px;
            y = centerY;
            return true;
        }
    }

    for (int py = RoomAssets::ROOM_WALK_MIN_Y; py <= RoomAssets::ROOM_WALK_MAX_Y; py += 4) {
        for (int px = RoomAssets::ROOM_WALK_MIN_X; px <= RoomAssets::ROOM_WALK_MAX_X; px += 4) {
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

bool MainScene::randomMonsterCenterWalkPointNear(float centerX, float centerY, float radiusX, float radiusY,
                                                 float& x, float& y) const {
    float offsetY = walkBoundaryOffsetY();
    int spanX = (int)roundf(radiusX);
    int spanY = (int)roundf(radiusY);
    if (spanX < 1) spanX = 1;
    if (spanY < 1) spanY = 1;
    for (uint8_t tries = 0; tries < 48; ++tries) {
        float px = clampf(centerX + (float)random(-spanX, spanX + 1),
                          (float)RoomAssets::ROOM_WALK_MIN_X,
                          (float)RoomAssets::ROOM_WALK_MAX_X);
        float py = clampf(centerY + (float)random(-spanY, spanY + 1),
                          (float)RoomAssets::ROOM_WALK_MIN_Y,
                          (float)RoomAssets::ROOM_WALK_MAX_Y);
        float candidateY = py - offsetY;
        if (monsterFootprintInsideWalkArea(px, candidateY)) {
            x = px;
            y = candidateY;
            return true;
        }
    }
    return randomMonsterCenterWalkPoint(x, y);
}

bool MainScene::monsterCanUseBedSleepPose(float x, float y) const {
    float footY = y + walkBoundaryOffsetY();
    return roomBedContains(x, footY) ||
           (x >= (float)RoomAssets::ROOM_BED_MIN_X + 12.0f &&
            x <= (float)RoomAssets::ROOM_BED_MAX_X - 12.0f &&
            footY >= (float)RoomAssets::ROOM_BED_MIN_Y + 7.0f &&
            footY <= (float)RoomAssets::ROOM_BED_MAX_Y - 7.0f);
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
        float candidateX = BED_SLEEP_X + CANDIDATES[i][0];
        float candidateY = BED_SLEEP_Y + CANDIDATES[i][1] - offsetY;
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
    for (int py = RoomAssets::ROOM_WALK_MIN_Y; py <= RoomAssets::ROOM_WALK_MAX_Y; py += 3) {
        for (int px = RoomAssets::ROOM_WALK_MIN_X; px <= RoomAssets::ROOM_WALK_MAX_X; px += 3) {
            float centerY = (float)py - offsetY;
            if (!monsterFootprintInsideWalkArea((float)px, centerY)) continue;
            float dx = (float)px - BED_SLEEP_X;
            float dy = (float)py - BED_SLEEP_Y;
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
        float candidateX = BED_SLEEP_X + CANDIDATES[i][0];
        float candidateY = BED_SLEEP_Y + CANDIDATES[i][1] - offsetY;
        if (monsterCanUseBedSleepPose(candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }
    x = BED_SLEEP_X;
    y = BED_SLEEP_Y - offsetY;
    return true;
}

void MainScene::updateMonsterAi(uint32_t nowMs, float dtSeconds) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (mon.fainted || mon.hpCur == 0 || (mon.statusBits & Game::STATUS_SLEEP)) {
        snapMonsterToBed();
        pendingFeed = false;
        feedingConsumed = false;
        feedingBiteTried = false;
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::IDLE;
        return;
    }

    bool debugTilt = GameEngine::ins().debugTiltControlEnabled();
    bool bedRestActive = !debugTilt && monsterNeedsBedRest() && !pendingFeed;
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(bedRestActive && monsterAtBedSleepPose())) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
        targetX = monsterX;
        targetY = monsterY;
    }

    if (!debugTilt && aiMode == AiMode::FEEDING) {
        updateFeeding(nowMs);
        return;
    }
    if (!debugTilt) {
        updatePendingFeed(nowMs);
        if (aiMode == AiMode::FEEDING) return;
        bedRestActive = monsterNeedsBedRest() && !pendingFeed;
        if (bedRestActive) {
            if (monsterAtBedSleepPose()) {
                velocityX = 0.0f;
                velocityY = 0.0f;
                aiMode = AiMode::IDLE;
                targetX = monsterX;
                targetY = monsterY;
                nextAiDecisionMs = nowMs + 2000;
                return;
            }
            if (monsterNearBed()) {
                snapMonsterToBed();
                velocityX = 0.0f;
                velocityY = 0.0f;
                aiMode = AiMode::IDLE;
                nextAiDecisionMs = nowMs + 2000;
                return;
            }
            setBedTarget(nowMs);
        }
    }
    if (!debugTilt && (int32_t)(nowMs - nextAiDecisionMs) >= 0) {
        chooseAiGoal(nowMs);
    }

    float prevX = monsterX;
    float prevY = monsterY;
    if (debugTilt) {
        updateDebugTiltControl(nowMs, dtSeconds);
    } else if (aiMode == AiMode::IDLE) {
        velocityX = 0.0f;
        velocityY = 0.0f;
    } else {
        float dx = targetX - monsterX;
        float dy = targetY - monsterY;
        float dist = sqrtf(dx * dx + dy * dy);
        float speed = (aiMode == AiMode::SEEK_FOOD || aiMode == AiMode::SEEK_BED) ? 19.0f : 10.5f;
        if (mon.mood < 40 || mon.satiety < 20) speed *= 0.72f;
        speed *= pmdMoveSpeedScale(active);
        float step = speed * dtSeconds;
        if (dist < 1.2f || step >= dist) {
            monsterX = targetX;
            monsterY = targetY;
            aiMode = AiMode::IDLE;
            velocityX = 0.0f;
            velocityY = 0.0f;
            nextAiDecisionMs = nowMs + random(1800, 4201);
            updatePendingFeed(nowMs);
        } else {
            velocityX = dx / dist * speed;
            velocityY = dy / dist * speed * 0.75f;
            monsterX += velocityX * dtSeconds;
            monsterY += velocityY * dtSeconds;
            if (fabsf(dx) > 8.0f) facingRight = dx > 0.0f;
        }
    }

    float walkOffsetY = walkBoundaryOffsetY();
    monsterX = clampf(monsterX, (float)RoomAssets::ROOM_WALK_MIN_X, (float)RoomAssets::ROOM_WALK_MAX_X);
    monsterY = clampf(monsterY, (float)RoomAssets::ROOM_WALK_MIN_Y - walkOffsetY,
                      (float)RoomAssets::ROOM_WALK_MAX_Y - walkOffsetY);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
        if (monsterFootprintInsideWalkArea(prevX, prevY)) {
            monsterX = prevX;
            monsterY = prevY;
        } else {
            randomMonsterCenterWalkPoint(monsterX, monsterY);
        }
        targetX = monsterX;
        targetY = monsterY;
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + random(1200, 2601);
    }
}

bool MainScene::monsterNearFood() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - FOOD_FEED_X) < 9.0f && fabsf(walkY - FOOD_FEED_Y) < 7.0f;
}

bool MainScene::monsterNearBed() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - BED_CENTER_X) <= BED_APPROACH_TOLERANCE_X &&
           fabsf(walkY - BED_CENTER_Y) <= BED_APPROACH_TOLERANCE_Y;
}

bool MainScene::monsterAtBedSleepPose() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - BED_SLEEP_X) <= BED_SLEEP_TOLERANCE_X &&
           fabsf(walkY - BED_SLEEP_Y) <= BED_SLEEP_TOLERANCE_Y;
}

bool MainScene::monsterNeedsBedRest() const {
    uint32_t nowMs = Hal::ins().millis();
    if ((int32_t)(nowMs - postFeedAwakeUntilMs) < 0) return false;
    return mainSceneIsNight();
}

void MainScene::setFoodTarget(uint32_t nowMs) {
    targetX = FOOD_FEED_X;
    targetY = FOOD_FEED_Y - walkBoundaryOffsetY();
    if (!monsterFootprintInsideWalkArea(targetX, targetY)) {
        if (!randomMonsterCenterWalkPointNear(FOOD_FEED_X, FOOD_FEED_Y, 8.0f, 5.0f, targetX, targetY)) {
            randomMonsterCenterWalkPoint(targetX, targetY);
        }
    }
    aiMode = AiMode::SEEK_FOOD;
    nextAiDecisionMs = nowMs + 1200;
}

void MainScene::setBedTarget(uint32_t nowMs) {
    if (!chooseBedApproachPose(targetX, targetY)) {
        randomMonsterCenterWalkPoint(targetX, targetY);
    }
    aiMode = AiMode::SEEK_BED;
    nextAiDecisionMs = nowMs + 1400;
}

void MainScene::snapMonsterToBed() {
    if (!chooseBedSleepPose(monsterX, monsterY)) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
    }
    targetX = monsterX;
    targetY = monsterY;
}

void MainScene::updatePendingFeed(uint32_t nowMs) {
    if (!pendingFeed) return;
    if (GameEngine::ins().foodCount() == 0 || (int32_t)(nowMs - pendingFeedUntilMs) >= 0) {
        pendingFeed = false;
        return;
    }
    if ((int32_t)(nowMs - pendingFeedReadyMs) < 0) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        return;
    }
    if (!monsterNearFood()) {
        setFoodTarget(nowMs);
        return;
    }

    enterFeeding(nowMs);
}

void MainScene::enterFeeding(uint32_t nowMs) {
    pendingFeed = false;
    feedingConsumed = false;
    feedingBiteTried = false;
    feedingBiteMs = nowMs + (uint32_t)random(FEED_BITE_DELAY_MIN_MS, FEED_BITE_DELAY_MAX_MS + 1);
    feedingUntilMs = nowMs + (uint32_t)random(FEED_SESSION_MIN_MS, FEED_SESSION_MAX_MS + 1);
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::FEEDING;
    targetX = monsterX;
    targetY = monsterY;
    pmdDirection = FOOD_CENTER_X < monsterX ? PmdDirection::LEFT : PmdDirection::RIGHT;
    facingRight = FOOD_CENTER_X > monsterX;
}

void MainScene::updateFeeding(uint32_t nowMs) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;

    if (!feedingBiteTried && (int32_t)(nowMs - feedingBiteMs) >= 0) {
        bool fed = GameEngine::ins().consumeFood();
        toast = fed ? Ui::Menu::FEED_TOAST : Ui::Menu::NO_FOOD;
        toastUntil = nowMs + 1200;
        feedingBiteTried = true;
        feedingConsumed = fed;
        if (!fed) {
            feedingUntilMs = nowMs + 600;
        }
    }

    if ((int32_t)(nowMs - feedingUntilMs) < 0) return;

    if (feedingConsumed) {
        postFeedAwakeUntilMs = nowMs + (uint32_t)random(POST_FEED_AWAKE_MIN_MS, POST_FEED_AWAKE_MAX_MS + 1);
    }
    feedingConsumed = false;
    feedingBiteTried = false;
    aiMode = AiMode::IDLE;
    nextAiDecisionMs = nowMs + random(1800, 4201);
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
    bool feedWakeReady = pendingFeed && (int32_t)(nowMs - pendingFeedReadyMs) >= 0;
    bool feedMovementActive = aiMode == AiMode::SEEK_FOOD ||
                              aiMode == AiMode::FEEDING ||
                              feedWakeReady;
    bool postFeedAwake = (int32_t)(nowMs - postFeedAwakeUntilMs) < 0;
    bool sleeping = (mon.statusBits & Game::STATUS_SLEEP) != 0 ||
                    mon.fainted ||
                    mon.hpCur == 0 ||
                    (mainSceneIsNight() && monsterAtBedSleepPose() && !feedMovementActive && !postFeedAwake);
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
    uint8_t autoFeedHunger = mainSceneIsNight() ? NIGHT_AUTO_FEED_HUNGER : DAY_AUTO_FEED_HUNGER;
    bool hungry = mon.satiety < autoFeedHunger && GameEngine::ins().foodCount() > 0;
    if (hungry && !monsterNearFood()) {
        setFoodTarget(nowMs);
        nextAiDecisionMs = nowMs + random(2600, 5201);
        return;
    }

    if (random(0, 100) < 62) {
        aiMode = AiMode::IDLE;
        targetX = monsterX;
        targetY = monsterY;
        nextAiDecisionMs = nowMs + random(2200, 6801);
        return;
    }

    aiMode = AiMode::WANDER;
    float walkOffsetY = walkBoundaryOffsetY();
    for (uint8_t tries = 0; tries < 18; ++tries) {
        float candidateX = clampf(monsterX + (float)random(-28, 29),
                                  (float)RoomAssets::ROOM_WALK_MIN_X,
                                  (float)RoomAssets::ROOM_WALK_MAX_X);
        float candidateY = clampf(monsterY + (float)random(-16, 17),
                                  (float)RoomAssets::ROOM_WALK_MIN_Y - walkOffsetY,
                                  (float)RoomAssets::ROOM_WALK_MAX_Y - walkOffsetY);
        if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) continue;
        if (fabsf(candidateX - monsterX) < 8.0f && fabsf(candidateY - monsterY) < 4.0f) continue;
        targetX = candidateX;
        targetY = candidateY;
        nextAiDecisionMs = nowMs + random(3200, 7601);
        return;
    }
    randomMonsterCenterWalkPoint(targetX, targetY);
    nextAiDecisionMs = nowMs + random(3200, 7601);
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
        if (GameEngine::ins().foodCount() == 0) {
            toast = Ui::Menu::NO_FOOD;
            toastUntil = nowMs + 1200;
            return true;
        }
        pendingFeed = true;
        bool restingAtNight = mainSceneIsNight() && monsterAtBedSleepPose();
        pendingFeedReadyMs = restingAtNight
            ? nowMs + (uint32_t)random(NIGHT_FEED_WAKE_DELAY_MIN_MS, NIGHT_FEED_WAKE_DELAY_MAX_MS + 1)
            : nowMs;
        pendingFeedUntilMs = nowMs + (restingAtNight ? NIGHT_FEED_REQUEST_TIMEOUT_MS : FEED_REQUEST_TIMEOUT_MS);
        if ((int32_t)(nowMs - pendingFeedReadyMs) >= 0) setFoodTarget(nowMs);
        updatePendingFeed(nowMs);
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
    drawRoomCached(cameraY, mainSceneIsNight());
}

void MainScene::drawFloor() {
}

void MainScene::drawFood() {
    if (GameEngine::ins().foodCount() == 0) return;
    auto& c = PixelRenderer::canvas();
    uint8_t foodIndex = GameEngine::ins().selectedFoodIndex();
    uint16_t foodColor = foodIndex == 1 ? PixelRenderer::rgb(255, 138, 112)
                                        : PixelRenderer::rgb(245, 180, 87);
    uint16_t garnishColor = foodIndex == 1 ? PixelRenderer::rgb(255, 216, 72)
                                           : PixelRenderer::rgb(92, 151, 80);
    int cx = (int)FOOD_CENTER_X;
    c.fillEllipse(cx, worldToScreenY(FOOD_CENTER_Y + 3.0f), 10, 4, PixelRenderer::rgb(122, 96, 76));
    c.fillEllipse(cx, worldToScreenY(FOOD_CENTER_Y), 8, 3, foodColor);
    c.fillCircle(cx - 4, worldToScreenY(FOOD_CENTER_Y - 2.0f), 2, garnishColor);
    c.fillCircle(cx + 3, worldToScreenY(FOOD_CENTER_Y - 1.0f), 2, PixelRenderer::rgb(178, 79, 57));
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

    uint8_t count = RoomAssets::ROOM_WALK_POLYGON_COUNT;
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
    if (!toast || Hal::ins().millis() > toastUntil) return;
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
