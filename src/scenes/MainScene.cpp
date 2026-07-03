#include "scenes/MainScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/PokemonSprites.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

namespace {
static constexpr uint16_t PMD_IDLE_FRAME_MS = 520;
static constexpr uint16_t PMD_WALKING_FRAME_MS = 170;
static constexpr uint16_t PMD_SLEEPING_FRAME_MS = 700;
static constexpr float PMD_MOVING_SPEED_EPSILON = 1.0f;
static constexpr float PMD_SHORT_MOVE_DISTANCE = 14.0f;

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

uint16_t moodColor(uint8_t mood) {
    if (mood > 66) return PixelRenderer::rgb(92, 222, 112);
    if (mood > 33) return PixelRenderer::rgb(255, 216, 72);
    return PixelRenderer::rgb(239, 85, 85);
}

uint16_t hungerColor(uint8_t hunger) {
    if (hunger > 50) return PixelRenderer::rgb(92, 222, 112);
    if (hunger > 20) return PixelRenderer::rgb(255, 216, 72);
    return PixelRenderer::rgb(239, 85, 85);
}

float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
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

void drawHeartIcon(int centerX, int centerY, uint8_t tier, uint16_t color) {
    auto& c = PixelRenderer::canvas();
    if (tier >= 3) {
        static constexpr uint8_t H = 10;
        static constexpr uint8_t W = 11;
        static constexpr uint16_t MASK[H] = {
            0b00000000000,
            0b01100001100,
            0b11110011110,
            0b11111111110,
            0b11111111110,
            0b01111111100,
            0b00111111000,
            0b00011110000,
            0b00001100000,
            0b00000100000,
        };
        int x = centerX - W / 2;
        int y = centerY - H / 2;
        for (uint8_t row = 0; row < H; ++row) {
            for (uint8_t col = 0; col < W; ++col) {
                if (MASK[row] & (1 << (W - 1 - col))) c.drawPixel(x + col, y + row, color);
            }
        }
        return;
    }

    if (tier == 2) {
        static constexpr uint8_t H = 9;
        static constexpr uint8_t W = 9;
        static constexpr uint16_t MASK[H] = {
            0b000000000,
            0b011000110,
            0b111101111,
            0b111111111,
            0b111111111,
            0b011111110,
            0b001111100,
            0b000111000,
            0b000010000,
        };
        int x = centerX - W / 2;
        int y = centerY - H / 2;
        for (uint8_t row = 0; row < H; ++row) {
            for (uint8_t col = 0; col < W; ++col) {
                if (MASK[row] & (1 << (W - 1 - col))) c.drawPixel(x + col, y + row, color);
            }
        }
        return;
    }

    static constexpr uint8_t H = 7;
    static constexpr uint8_t W = 7;
    static constexpr uint8_t MASK[H] = {
        0b0000000,
        0b0101010,
        0b1111110,
        0b1111110,
        0b0111100,
        0b0011000,
        0b0001000,
    };
    int x = centerX - W / 2;
    int y = centerY - H / 2;
    for (uint8_t row = 0; row < H; ++row) {
        for (uint8_t col = 0; col < W; ++col) {
            if (MASK[row] & (1 << (W - 1 - col))) c.drawPixel(x + col, y + row, color);
        }
    }
}
}

void MainScene::onEnter() {
    active = &GameEngine::ins().activeSpecies();
    targetX = monsterX;
    targetY = monsterY;
    velocityX = 0.0f;
    velocityY = 0.0f;
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
    updatePmdSpriteState(nowMs);
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

void MainScene::updateMonsterAi(uint32_t nowMs, float dtSeconds) {
    static constexpr float MIN_X = 42.0f;
    static constexpr float MAX_X = 168.0f;
    static constexpr float MIN_Y = 78.0f;
    static constexpr float MAX_Y = 100.0f;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (mon.fainted || mon.hpCur == 0 || (mon.statusBits & Game::STATUS_SLEEP)) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::IDLE;
        return;
    }

    if ((int32_t)(nowMs - nextAiDecisionMs) >= 0) {
        chooseAiGoal(nowMs);
    }

    if (aiMode == AiMode::IDLE) {
        velocityX = 0.0f;
        velocityY = 0.0f;
    } else {
        float dx = targetX - monsterX;
        float dy = targetY - monsterY;
        float dist = sqrtf(dx * dx + dy * dy);
        float speed = aiMode == AiMode::SEEK_FOOD ? 19.0f : 10.5f;
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
        } else {
            velocityX = dx / dist * speed;
            velocityY = dy / dist * speed * 0.75f;
            monsterX += velocityX * dtSeconds;
            monsterY += velocityY * dtSeconds;
            if (fabsf(dx) > 8.0f) facingRight = dx > 0.0f;
        }
    }

    monsterX = clampf(monsterX, MIN_X, MAX_X);
    monsterY = clampf(monsterY, MIN_Y, MAX_Y);
}

void MainScene::updatePmdSpriteState(uint32_t nowMs) {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config) return;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool sleeping = (mon.statusBits & Game::STATUS_SLEEP) != 0;
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
    static constexpr float MIN_X = 42.0f;
    static constexpr float MAX_X = 168.0f;
    static constexpr float MIN_Y = 78.0f;
    static constexpr float MAX_Y = 100.0f;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool nearFood = fabsf(monsterX - 164.0f) < 10.0f && fabsf(monsterY - 96.0f) < 6.0f;
    bool hungry = mon.satiety < 55 && GameEngine::ins().foodCount() > 0;
    if (hungry && !nearFood && random(0, 100) < 35) {
        aiMode = AiMode::SEEK_FOOD;
        targetX = 164.0f + random(-4, 5);
        targetY = 96.0f + random(-3, 4);
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
    for (uint8_t tries = 0; tries < 8; ++tries) {
        targetX = clampf(monsterX + (float)random(-24, 25), MIN_X, MAX_X);
        targetY = clampf(monsterY + (float)random(-9, 10), MIN_Y, MAX_Y);
        if (fabsf(targetX - monsterX) >= 8.0f || fabsf(targetY - monsterY) >= 4.0f) break;
    }
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
        bool fed = GameEngine::ins().consumeFood();
        toast = fed ? Ui::Menu::FEED_TOAST : Ui::Menu::NO_FOOD;
        toastUntil = Hal::ins().millis() + 1200;
        return true;
    }

    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    return false;
}

void MainScene::drawBackground() {
    auto& c = PixelRenderer::canvas();
    bool night = mainSceneIsNight();
    uint16_t wall = night ? PixelRenderer::rgb(58, 70, 92) : PixelRenderer::rgb(194, 219, 224);
    uint16_t upperWall = night ? PixelRenderer::rgb(45, 56, 78) : PixelRenderer::rgb(165, 202, 214);
    uint16_t frame = night ? PixelRenderer::rgb(37, 47, 65) : PixelRenderer::rgb(95, 130, 138);
    uint16_t window = night ? PixelRenderer::rgb(31, 43, 72) : PixelRenderer::rgb(238, 247, 230);
    PixelRenderer::clear(wall);
    c.fillRect(0, 0, Hal::DISPLAY_W, 42, upperWall);
    c.fillRect(18, 10, 50, 26, window);
    c.drawRect(18, 10, 50, 26, frame);
    c.drawLine(43, 10, 43, 35, frame);
    c.drawLine(18, 23, 67, 23, frame);
    c.fillRect(172, 21, 33, 16, night ? PixelRenderer::rgb(79, 87, 104) : PixelRenderer::rgb(140, 166, 173));
    c.drawRect(172, 21, 33, 16, frame);
}

void MainScene::drawFloor() {
    auto& c = PixelRenderer::canvas();
    bool night = mainSceneIsNight();
    uint16_t floor = night ? PixelRenderer::rgb(98, 82, 74) : PixelRenderer::rgb(226, 209, 174);
    uint16_t floorLine = night ? PixelRenderer::rgb(74, 64, 62) : PixelRenderer::rgb(206, 187, 151);
    uint16_t rug = night ? PixelRenderer::rgb(121, 101, 82) : PixelRenderer::rgb(240, 225, 188);
    uint16_t rugBorder = night ? PixelRenderer::rgb(91, 70, 60) : PixelRenderer::rgb(173, 140, 101);
    c.fillRect(0, 42, Hal::DISPLAY_W, Hal::DISPLAY_H - 42, floor);
    for (int y = 56; y < Hal::DISPLAY_H; y += 16) {
        c.drawLine(0, y, Hal::DISPLAY_W, y, floorLine);
    }
    c.fillRect(12, 58, 164, 58, rug);
    c.drawRect(12, 58, 164, 58, rugBorder);
    if (!night) {
        fillRectAlpha(23, 42, 54, 41, PixelRenderer::rgb(255, 250, 205), 42);
        fillRectAlpha(31, 70, 46, 22, PixelRenderer::rgb(255, 248, 190), 24);
    } else {
        fillRectAlpha(148, 52, 80, 68, PixelRenderer::rgb(255, 199, 104), 28);
        fillRectAlpha(172, 68, 43, 35, PixelRenderer::rgb(255, 214, 128), 45);
    }
    c.fillRect(188, 72, 32, 26, night ? PixelRenderer::rgb(128, 91, 72) : PixelRenderer::rgb(186, 141, 105));
    c.fillRect(193, 61, 22, 13, night ? PixelRenderer::rgb(179, 123, 75) : PixelRenderer::rgb(207, 168, 128));
    if (night) c.fillRect(196, 57, 8, 4, PixelRenderer::rgb(255, 214, 128));
}

void MainScene::drawFood() {
    if (GameEngine::ins().foodCount() == 0) return;
    auto& c = PixelRenderer::canvas();
    uint8_t foodIndex = GameEngine::ins().selectedFoodIndex();
    uint16_t foodColor = foodIndex == 1 ? PixelRenderer::rgb(255, 138, 112)
                                        : PixelRenderer::rgb(245, 180, 87);
    uint16_t garnishColor = foodIndex == 1 ? PixelRenderer::rgb(255, 216, 72)
                                           : PixelRenderer::rgb(92, 151, 80);
    c.fillEllipse(191, 111, 16, 6, PixelRenderer::rgb(122, 96, 76));
    c.fillEllipse(191, 108, 13, 5, foodColor);
    c.fillCircle(186, 106, 2, garnishColor);
    c.fillCircle(194, 107, 2, PixelRenderer::rgb(178, 79, 57));
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

    int rx = constrain((int)(frameW * (floating ? 0.25f : 0.38f)), floating ? 10 : 13, floating ? 22 : 36);
    int ry = constrain((int)(frameH * (floating ? 0.055f : 0.105f)), floating ? 3 : 4, floating ? 6 : 10);
    if (night) {
        rx = (rx * 11 + 5) / 10;
        ry = (ry * 11 + 5) / 10;
    }

    int shadowY = (int)(monsterY + constrain((int)(frameH * 0.34f), 14, 28));
    PixelRenderer::canvas().fillEllipse((int)monsterX, shadowY, rx, ry,
                                        night ? PixelRenderer::rgb(53, 44, 50) : PixelRenderer::rgb(159, 139, 117));
}

void MainScene::drawMonster() {
    auto& c = PixelRenderer::canvas();
    int x = (int)monsterX;
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int y = (int)(monsterY - pmdFloatYOffset(config, Hal::ins().millis()));
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
    int y = (int)(monsterY - pmdFloatYOffset(config, Hal::ins().millis())) - 31;
    c.fillCircle(x, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillCircle(x + 5, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillTriangle(x - 3, y + 2, x + 8, y + 2, x + 2, y + 9, PixelRenderer::rgb(255, 103, 135));
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

    int heartX = PANEL_X + 12;
    int heartY = PANEL_Y + 27;
    uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
    if (!mon.fainted && mon.hpCur > 0 && mon.hpMax > 0) {
        uint8_t heartTier = hpPct > 66 ? 3 : (hpPct > 33 ? 2 : 1);
        uint16_t heartColor = hpPct > 66 ? PixelRenderer::rgb(239, 85, 85) :
                              (hpPct > 33 ? PixelRenderer::rgb(255, 138, 72) :
                               PixelRenderer::rgb(204, 55, 72));
        drawHeartIcon(heartX, heartY, heartTier, heartColor);
    }

    uint16_t mood = moodColor(GameEngine::ins().moodValue());
    int moodX = PANEL_X + 29;
    int moodY = PANEL_Y + 27;
    c.fillCircle(moodX, moodY, 4, mood);
    c.drawCircle(moodX, moodY, 4, PixelRenderer::rgb(245, 246, 232));

    uint8_t hunger = GameEngine::ins().hungerValue();
    int barX = PANEL_X + 36;
    int barY = PANEL_Y + 25;
    int barW = 24;
    c.fillRect(barX, barY, barW, 5, PixelRenderer::rgb(68, 72, 78));
    int fillW = ((barW - 2) * hunger) / 100;
    if (fillW > 0) c.fillRect(barX + 1, barY + 1, fillW, 3, hungerColor(hunger));
    c.drawRect(barX, barY, barW, 5, PixelRenderer::rgb(245, 246, 232));
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
