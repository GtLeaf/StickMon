#include "scenes/MainScene.h"
#include <algorithm>
#include <cstdio>
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
}

void MainScene::onEnter() {
    active = &GameEngine::ins().activeSpecies();
}

void MainScene::update(uint32_t nowMs, float dtSeconds) {
    active = &GameEngine::ins().activeSpecies();
    monsterX += velocity * dtSeconds;
    if (monsterX < 34.0f) {
        monsterX = 34.0f;
        velocity = -velocity;
    } else if (monsterX > 101.0f) {
        monsterX = 101.0f;
        velocity = -velocity;
    }
    bob += dtSeconds * 4.0f;

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

void MainScene::render() {
    RenderItem items[] = {
        {0, &MainScene::drawBackground},
        {10, &MainScene::drawFloor},
        {18, &MainScene::drawFood},
        {20, &MainScene::drawShadow},
        {30, &MainScene::drawMonster},
        {40, &MainScene::drawStateEffect},
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
    c.fillRect(0, 0, Hal::DISPLAY_W, 54, PixelRenderer::rgb(165, 202, 214));
    c.fillRect(12, 18, 39, 30, PixelRenderer::rgb(238, 247, 230));
    c.drawRect(12, 18, 39, 30, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(31, 18, 31, 47, PixelRenderer::rgb(95, 130, 138));
    c.drawLine(12, 33, 50, 33, PixelRenderer::rgb(95, 130, 138));
}

void MainScene::drawFloor() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 54, Hal::DISPLAY_W, Hal::DISPLAY_H - 54, PixelRenderer::rgb(226, 209, 174));
    for (int y = 70; y < Hal::DISPLAY_H; y += 18) {
        c.drawLine(0, y, Hal::DISPLAY_W, y, PixelRenderer::rgb(206, 187, 151));
    }
    c.fillRect(8, 72, 119, 112, PixelRenderer::rgb(240, 225, 188));
    c.drawRect(8, 72, 119, 112, PixelRenderer::rgb(173, 140, 101));
}

void MainScene::drawFood() {
    if (GameEngine::ins().foodCount() == 0) return;
    auto& c = PixelRenderer::canvas();
    c.fillEllipse(103, 158, 16, 7, PixelRenderer::rgb(122, 96, 76));
    c.fillEllipse(103, 155, 13, 5, PixelRenderer::rgb(245, 180, 87));
    c.fillCircle(98, 153, 2, PixelRenderer::rgb(92, 151, 80));
    c.fillCircle(106, 154, 2, PixelRenderer::rgb(178, 79, 57));
}

void MainScene::drawShadow() {
    PixelRenderer::canvas().fillEllipse((int)monsterX, 168, 25, 7, PixelRenderer::rgb(159, 139, 117));
}

void MainScene::drawMonster() {
    auto& c = PixelRenderer::canvas();
    int x = (int)monsterX;
    int y = (int)(monsterY + sinf(bob) * 2.0f);
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
    int y = (int)monsterY - 32;
    c.fillCircle(x, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillCircle(x + 5, y, 3, PixelRenderer::rgb(255, 103, 135));
    c.fillTriangle(x - 3, y + 2, x + 8, y + 2, x + 2, y + 9, PixelRenderer::rgb(255, 103, 135));
}

void MainScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 4;
    static constexpr int PANEL_Y = 4;
    static constexpr int PANEL_W = 96;
    static constexpr int PANEL_H = 48;
    static constexpr int HEART_SIZE = 9;
    static constexpr uint16_t HEART_MASK[HEART_SIZE] = {
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

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    fillRectAlpha(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(8, 10, 14), 150);
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(255, 255, 255));

    PixelRenderer::text(PANEL_X + 6, PANEL_Y + 5, active->name, PixelRenderer::rgb(245, 246, 232));

    int heartX = PANEL_X + 7;
    int heartY = PANEL_Y + 25;
    uint8_t hpPct = mon.hpMax > 0 ? (uint8_t)((uint32_t)mon.hpCur * 100 / mon.hpMax) : 0;
    int fillRows = (hpPct * HEART_SIZE + 50) / 100;
    if (fillRows < 1 && hpPct > 0) fillRows = 1;
    if (fillRows > HEART_SIZE) fillRows = HEART_SIZE;
    for (int row = 0; row < HEART_SIZE; ++row) {
        uint16_t mask = HEART_MASK[row];
        bool fill = row >= HEART_SIZE - fillRows;
        uint16_t color = fill ? PixelRenderer::rgb(239, 85, 85) : PixelRenderer::rgb(82, 87, 95);
        for (int col = 0; col < HEART_SIZE; ++col) {
            if (mask & (1 << (HEART_SIZE - 1 - col))) {
                c.drawPixel(heartX + col, heartY + row, color);
            }
        }
    }

    uint16_t mood = moodColor(GameEngine::ins().moodValue());
    int moodX = heartX + HEART_SIZE + 9;
    int moodY = heartY + 4;
    c.fillCircle(moodX, moodY, 4, mood);
    c.drawCircle(moodX, moodY, 4, PixelRenderer::rgb(245, 246, 232));

    uint8_t hunger = GameEngine::ins().hungerValue();
    int barX = PANEL_X + 7;
    int barY = PANEL_Y + 40;
    int barW = PANEL_W - 14;
    c.fillRect(barX, barY, barW, 5, PixelRenderer::rgb(68, 72, 78));
    int fillW = ((barW - 2) * hunger) / 100;
    if (fillW > 0) c.fillRect(barX + 1, barY + 1, fillW, 3, hungerColor(hunger));
    c.drawRect(barX, barY, barW, 5, PixelRenderer::rgb(245, 246, 232));
}

void MainScene::drawToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(16, 62, 103, 18, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(24, 67, toast, PixelRenderer::rgb(255, 255, 255));
}

void MainScene::sortAndDraw(RenderItem* items, uint8_t count) {
    std::sort(items, items + count, [](const RenderItem& a, const RenderItem& b) {
        return a.z < b.z;
    });
    for (uint8_t i = 0; i < count; ++i) {
        (this->*items[i].draw)();
    }
}
