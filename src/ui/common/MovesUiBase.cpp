#include "ui/common/MovesUiBase.h"

#include <algorithm>
#include <cstdio>
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"

namespace Ui {

void MovesUiBase::execute(const Features::Moves::Action& action) {
    using Selection = Features::Moves::Selection;
    if (action.kind == Selection::BACK) {
        if (movePageMode == MovePageMode::RECALL_SELECT) movePageMode = MovePageMode::MANAGE;
        host.pageBack();
    } else if (action.kind == Selection::BASIC_LOCKED) {
        host.pageNotice(Ui::Team::MOVE_BASIC_LOCKED);
    } else if (action.kind == Selection::REPLACE) {
        const auto* move = findMove(action.move);
        if (host.recallPageMove(moveMonsterIndex, action.move, action.slot)) {
            char text[48];
            snprintf(text, sizeof(text), Ui::Team::MOVE_RECALLED_FMT, move ? move->name : "");
            host.pageNotice(text);
            movePageMode = MovePageMode::MANAGE;
            host.pageBack();
        } else host.pageNotice(Ui::Team::MOVE_RECALL_FAILED);
    }
}

void MovesUiBase::forgetMove(uint8_t slot) {
    if (host.forgetPageMove(moveMonsterIndex, slot)) host.pageNotice(Ui::Team::MOVE_FORGOT);
}

namespace MovesDrawing {

int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) { width += *p == ' ' ? 5 : 8; ++p; }
        else if ((*p & 0xE0) == 0xC0) { width += 16; p += 2; }
        else if ((*p & 0xF0) == 0xE0) { width += 16; p += 3; }
        else { width += 8; ++p; }
    }
    return width;
}

uint16_t typeColor(TypeId type) {
    switch (type) {
    case TypeId::FIRE: return PixelRenderer::rgb(239, 91, 67);
    case TypeId::WATER: return PixelRenderer::rgb(67, 145, 224);
    case TypeId::GRASS: return PixelRenderer::rgb(92, 188, 92);
    case TypeId::ELECTRIC: return PixelRenderer::rgb(245, 204, 67);
    case TypeId::ICE: return PixelRenderer::rgb(115, 206, 218);
    case TypeId::FIGHTING: return PixelRenderer::rgb(190, 85, 64);
    case TypeId::POISON: return PixelRenderer::rgb(155, 95, 196);
    case TypeId::GROUND: return PixelRenderer::rgb(184, 142, 83);
    case TypeId::FLYING: return PixelRenderer::rgb(135, 166, 220);
    case TypeId::PSYCHIC: return PixelRenderer::rgb(226, 98, 168);
    case TypeId::BUG: return PixelRenderer::rgb(142, 185, 72);
    case TypeId::ROCK: return PixelRenderer::rgb(160, 136, 84);
    case TypeId::GHOST: return PixelRenderer::rgb(105, 86, 160);
    case TypeId::DRAGON: return PixelRenderer::rgb(88, 118, 210);
    case TypeId::DARK: return PixelRenderer::rgb(86, 76, 70);
    case TypeId::STEEL: return PixelRenderer::rgb(144, 158, 172);
    case TypeId::FAIRY: return PixelRenderer::rgb(232, 138, 202);
    default: return PixelRenderer::rgb(154, 158, 132);
    }
}

int drawTypeBracket(int x, int y, TypeId type) {
    PixelRenderer::text(x, y, "[", PixelRenderer::rgb(241, 242, 232), 1);
    const char* name = typeName(type);
    PixelRenderer::text(x + 8, y, name, typeColor(type), 1);
    int closeX = x + 8 + textPixelWidth(name);
    PixelRenderer::text(closeX, y, "]", PixelRenderer::rgb(241, 242, 232), 1);
    return closeX + 10;
}

static uint16_t proficiencyColor(uint8_t value) {
    if (value >= 90) return PixelRenderer::rgb(75, 209, 225);
    if (value >= 60) return PixelRenderer::rgb(92, 222, 112);
    if (value >= 25) return PixelRenderer::rgb(255, 216, 72);
    return PixelRenderer::rgb(239, 85, 85);
}

void drawProficiencyBar(int x, int y, int width, int height, uint8_t value) {
    auto& c = PixelRenderer::canvas();
    value = std::min<uint8_t>(value, 100);
    const uint16_t trackColor = PixelRenderer::rgb(82, 87, 95);
    const uint16_t borderColor = PixelRenderer::rgb(45, 48, 56);
    c.fillRoundRect(x, y, width, height, height / 2, trackColor);
    const int fillWidth = ((width - 2) * value) / 100;
    if (fillWidth > 0) {
        const int fillHeight = height - 2;
        c.fillRoundRect(x + 1, y + 1, fillWidth, fillHeight,
            std::min(fillHeight / 2, fillWidth / 2), proficiencyColor(value));
    }
    c.drawRoundRect(x, y, width, height, height / 2, borderColor);
}

}  // namespace MovesDrawing
}  // namespace Ui
