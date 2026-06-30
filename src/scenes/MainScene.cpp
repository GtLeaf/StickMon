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
    nextAiDecisionMs = 0;
}

void MainScene::update(uint32_t nowMs, float dtSeconds) {
    active = &GameEngine::ins().activeSpecies();
    updateMonsterAi(nowMs, dtSeconds);
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
    if (mon.fainted || mon.hpCur == 0) {
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
        float speed = aiMode == AiMode::SEEK_FOOD ? 24.0f : 16.0f;
        if (mon.mood < 40 || mon.satiety < 20) speed *= 0.72f;
        float step = speed * dtSeconds;
        if (dist < 1.2f || step >= dist) {
            monsterX = targetX;
            monsterY = targetY;
            aiMode = AiMode::IDLE;
            velocityX = 0.0f;
            velocityY = 0.0f;
            nextAiDecisionMs = nowMs + random(500, 1101);
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

void MainScene::chooseAiGoal(uint32_t nowMs) {
    static constexpr float MIN_X = 42.0f;
    static constexpr float MAX_X = 168.0f;
    static constexpr float MIN_Y = 78.0f;
    static constexpr float MAX_Y = 100.0f;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool nearFood = fabsf(monsterX - 164.0f) < 10.0f && fabsf(monsterY - 96.0f) < 6.0f;
    bool hungry = mon.satiety < 55 && GameEngine::ins().foodCount() > 0;
    if (hungry && !nearFood && random(0, 100) < 45) {
        aiMode = AiMode::SEEK_FOOD;
        targetX = 164.0f + random(-4, 5);
        targetY = 96.0f + random(-3, 4);
        nextAiDecisionMs = nowMs + random(1800, 3201);
        return;
    }

    if (random(0, 100) < 22) {
        aiMode = AiMode::IDLE;
        targetX = monsterX;
        targetY = monsterY;
        nextAiDecisionMs = nowMs + random(600, 1501);
        return;
    }

    aiMode = AiMode::WANDER;
    for (uint8_t tries = 0; tries < 8; ++tries) {
        targetX = MIN_X + random(0, (int)(MAX_X - MIN_X + 1));
        targetY = MIN_Y + random(0, (int)(MAX_Y - MIN_Y + 1));
        if (fabsf(targetX - monsterX) >= 24.0f || fabsf(targetY - monsterY) >= 10.0f) break;
    }
    nextAiDecisionMs = nowMs + random(1800, 4201);
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
    PixelRenderer::clear(PixelRenderer::rgb(194, 219, 224));
    c.fillRect(0, 0, Hal::DISPLAY_W, 42, PixelRenderer::rgb(165, 202, 214));
    c.fillRect(18, 10, 50, 26, PixelRenderer::rgb(238, 247, 230));
    c.drawRect(18, 10, 50, 26, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(43, 10, 43, 35, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(18, 23, 67, 23, PixelRenderer::rgb(95, 130, 138));
    c.fillRect(172, 21, 33, 16, PixelRenderer::rgb(140, 166, 173));
    c.drawRect(172, 21, 33, 16, PixelRenderer::rgb(95, 130, 138));
}

void MainScene::drawFloor() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 42, Hal::DISPLAY_W, Hal::DISPLAY_H - 42, PixelRenderer::rgb(226, 209, 174));
    for (int y = 56; y < Hal::DISPLAY_H; y += 16) {
        c.drawLine(0, y, Hal::DISPLAY_W, y, PixelRenderer::rgb(206, 187, 151));
    }
    c.fillRect(12, 58, 164, 58, PixelRenderer::rgb(240, 225, 188));
    c.drawRect(12, 58, 164, 58, PixelRenderer::rgb(173, 140, 101));
    c.fillRect(188, 72, 32, 26, PixelRenderer::rgb(186, 141, 105));
    c.fillRect(193, 61, 22, 13, PixelRenderer::rgb(207, 168, 128));
}

void MainScene::drawFood() {
    if (GameEngine::ins().foodCount() == 0) return;
    auto& c = PixelRenderer::canvas();
    c.fillEllipse(191, 111, 16, 6, PixelRenderer::rgb(122, 96, 76));
    c.fillEllipse(191, 108, 13, 5, PixelRenderer::rgb(245, 180, 87));
    c.fillCircle(186, 106, 2, PixelRenderer::rgb(92, 151, 80));
    c.fillCircle(194, 107, 2, PixelRenderer::rgb(178, 79, 57));
}

void MainScene::drawShadow() {
    int shadowY = (int)(monsterY + 21.0f);
    PixelRenderer::canvas().fillEllipse((int)monsterX, shadowY, 24, 6, PixelRenderer::rgb(159, 139, 117));
}

void MainScene::drawMonster() {
    auto& c = PixelRenderer::canvas();
    int x = (int)monsterX;
    int y = (int)monsterY;
    PokemonSprites::SpriteKind kind = ((Hal::ins().millis() / 520) % 2 == 0)
        ? PokemonSprites::SpriteKind::ICON_0
        : PokemonSprites::SpriteKind::ICON_1;
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(active->id, kind);
    if (frame) {
        uint8_t w = pgm_read_byte(&frame->width);
        uint8_t h = pgm_read_byte(&frame->height);
        PixelRenderer::drawRgb565Rle(x - w / 2, y - h / 2, w, h, PokemonSprites::SPRITE_RLE,
                                     pgm_read_dword(&frame->offset), pgm_read_dword(&frame->length),
                                     facingRight);
        return;
    }
    c.fillRect(x - 19, y - 25, 38, 42, active->colorA);
    c.fillRect(x - 12, y - 17, 24, 25, active->colorB);
    c.fillCircle(x - 7, y - 8, 2, PixelRenderer::rgb(24, 30, 38));
    c.fillCircle(x + 7, y - 8, 2, PixelRenderer::rgb(24, 30, 38));
    c.drawLine(x - 5, y + 4, x + 5, y + 4, PixelRenderer::rgb(24, 30, 38));
}

void MainScene::drawStateEffect() {
    if (GameEngine::ins().moodValue() < 50) return;
    auto& c = PixelRenderer::canvas();
    int x = (int)monsterX + 18;
    int y = (int)monsterY - 31;
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
