#include "ui/common/StatusUiContent.h"
#include <algorithm>
#include <cstdio>
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"

namespace Ui { namespace StatusContent {

const Species& speciesFor(const Game::MonsterRuntime& mon) {
    const auto* species = findSpecies(mon.speciesId);
    return species ? *species : starterSpecies();
}

const char* originName(Game::Origin origin) {
    switch (origin) {
    case Game::Origin::STARTER: return Ui::Status::ORIGIN_STARTER;
    case Game::Origin::HATCHED: return Ui::Status::ORIGIN_HATCHED;
    case Game::Origin::BEFRIENDED: return Ui::Status::ORIGIN_BEFRIENDED;
    case Game::Origin::TRADED: return Ui::Status::ORIGIN_TRADED;
    case Game::Origin::GIFT: return Ui::Status::ORIGIN_GIFT;
    default: return Ui::Status::ORIGIN_UNKNOWN;
    }
}

void drawInfoRow(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::text(x, y, value, color, 1);
}

void drawStatRow(int x, int y, const char* label, uint16_t value, uint16_t color) {
    char buf[24];
    snprintf(buf, sizeof(buf), Ui::Status::STAT_ROW_FMT, label, value);
    PixelRenderer::text(x, y, buf, color, 1);
}

int textPixelWidth(const char* value) {
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

int wrapText(const char* value, int maxWidth, int x, int y, uint16_t color, bool draw) {
    if (!value || !*value || maxWidth <= 0) return 0;

    char line[96];
    int lineBytes = 0;
    int lineWidth = 0;
    int lines = 0;
    auto flushLine = [&]() {
        if (lineBytes == 0) return;
        line[lineBytes] = '\0';
        if (draw) PixelRenderer::text(x, y + lines * 16, line, color, 1);
        ++lines;
        lineBytes = 0;
        lineWidth = 0;
    };

    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p == '\n') {
            flushLine();
            ++p;
            continue;
        }

        int bytes = 1;
        int glyphWidth = *p == ' ' ? 5 : 8;
        if ((*p & 0xE0) == 0xC0) {
            bytes = 2;
            glyphWidth = 16;
        } else if ((*p & 0xF0) == 0xE0) {
            bytes = 3;
            glyphWidth = 16;
        } else if ((*p & 0xF8) == 0xF0) {
            bytes = 4;
            glyphWidth = 16;
        }

        if (lineBytes > 0 &&
            (lineWidth + glyphWidth > maxWidth || lineBytes + bytes >= (int)sizeof(line))) {
            flushLine();
        }
        for (int index = 0; index < bytes && p[index]; ++index) {
            line[lineBytes++] = static_cast<char>(p[index]);
        }
        lineWidth += glyphWidth;
        p += bytes;
    }
    flushLine();
    return lines;
}

int wrappedTextLineCount(const char* value, int maxWidth) {
    return wrapText(value, maxWidth, 0, 0, 0, false);
}

int drawWrappedText(int x, int y, const char* value, int maxWidth, uint16_t color) {
    return wrapText(value, maxWidth, x, y, color, true);
}

int statusPageContentHeight(uint8_t page) {
    switch (page) {
    case 0: return 129;
    case 1: return 128;
    case 2: return 192;
    case 3: return 123;
    default: return 123;
    }
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
    case TypeId::NORMAL:
    default: return PixelRenderer::rgb(154, 158, 132);
    }
}

const char* genderName(const Species& species, const Game::MonsterRuntime& mon) {
    if (species.id == 151) return Ui::Status::GENDER_NONE;
    if (species.id == 380) return Ui::Status::GENDER_FEMALE;
    if (species.id == 381) return Ui::Status::GENDER_MALE;
    return ((mon.ivPacked ^ mon.speciesId) & 1) ? Ui::Status::GENDER_FEMALE : Ui::Status::GENDER_MALE;
}

const char* proficiencyName(uint8_t value) {
    if (value >= 90) return Ui::Status::PROF_FULL;
    if (value >= 60) return Ui::Status::PROF_HIGH;
    if (value >= 25) return Ui::Status::PROF_MID;
    return Ui::Status::PROF_LOW;
}

const char* metAreaName(uint8_t metArea) {
    if (metArea < Ui::Explore::AREA_COUNT) return Ui::Explore::AREA_ITEMS[metArea];
    return Ui::Status::ORIGIN_UNKNOWN;
}

int drawTypeBracket(int x, int y, TypeId type) {
    PixelRenderer::text(x, y, "[", PixelRenderer::rgb(241, 242, 232), 1);
    const char* name = typeName(type);
    PixelRenderer::text(x + 8, y, name, typeColor(type), 1);
    int closeX = x + 8 + textPixelWidth(name);
    PixelRenderer::text(closeX, y, "]", PixelRenderer::rgb(241, 242, 232), 1);
    return closeX + 10;
}

void drawStatusMonsterIcon(const Species& species, int x, int y) {
    static constexpr int PANEL_W = 72;
    static constexpr int PANEL_H = 78;
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, PokemonSprites::SpriteKind::STATUS);
    if (frame) {
        uint8_t w = FlashStorage::readByte(&frame->width);
        uint8_t h = FlashStorage::readByte(&frame->height);
        int drawX = x + (PANEL_W - w) / 2;
        int drawY = y + (PANEL_H - h) / 2;
        if (PokemonSprites::drawFrame(frame, drawX, drawY)) return;
    }
}

void paintDetails(uint8_t statusPage, const Species& mon,
                  const Game::MonsterRuntime& activeMon,
                  const MenuGrid::Rect& body, int scrollY) {
    auto& c = PixelRenderer::canvas();
    char buf[64];
    auto sx = [body](int x) { return body.x + x; };
    auto sy = [body, scrollY](int y) { return body.y + y - scrollY; };
    if (statusPage == 1) {
        PixelRenderer::text(sx(12), sy(8), Ui::Status::CURRENT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::HP_FMT, activeMon.hpCur, activeMon.hpMax);
        PixelRenderer::text(sx(14), sy(30), buf, PixelRenderer::rgb(92, 222, 112), 1);
        drawStatRow(sx(128), sy(30), Ui::Status::STAT_SPA, statFor(mon, activeMon, 3), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(14), sy(50), Ui::Status::STAT_ATK, statFor(mon, activeMon, 1), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(128), sy(50), Ui::Status::STAT_SPD, statFor(mon, activeMon, 4), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(14), sy(70), Ui::Status::STAT_DEF, statFor(mon, activeMon, 2), PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(128), sy(70), Ui::Status::STAT_SPE, statFor(mon, activeMon, 5), PixelRenderer::rgb(241, 242, 232));

        snprintf(buf, sizeof(buf), Ui::Status::EXP_VALUE_FMT, (unsigned long)activeMon.exp);
        PixelRenderer::text(sx(14), sy(92), buf, PixelRenderer::rgb(241, 242, 232), 1);
        uint32_t expNext = expToNextLevel(mon.growthRate, activeMon.level, activeMon.exp);
        snprintf(buf, sizeof(buf), Ui::Status::EXP_NEXT_FMT, (unsigned long)expNext);
        PixelRenderer::text(sx(128), sy(92), buf, PixelRenderer::rgb(241, 242, 232), 1);

        uint32_t levelExp = minimumExpForLevel(mon.growthRate, activeMon.level);
        uint32_t nextExp = activeMon.level >= Game::LEVEL_MAX
            ? activeMon.exp
            : minimumExpForLevel(mon.growthRate, activeMon.level + 1);
        uint8_t expPct = nextExp > levelExp
            ? (uint8_t)MathUtil::min<uint32_t>(100, (activeMon.exp - levelExp) * 100UL / (nextExp - levelExp))
            : 100;
        uint32_t maxExp = minimumExpForLevel(mon.growthRate, Game::LEVEL_MAX);
        uint8_t totalPct = maxExp > 0
            ? (uint8_t)MathUtil::min<uint32_t>(100, activeMon.exp * 100UL / maxExp)
            : 100;
        const int BAR_X = sx(14);
        static constexpr int BAR_Y = 118;
        static constexpr int BAR_W = 186;
        static constexpr int BAR_H = 9;
        int barY = sy(BAR_Y);
        c.fillRoundRect(BAR_X, barY, BAR_W, BAR_H, 2, PixelRenderer::rgb(29, 34, 42));
        int totalW = ((BAR_W - 2) * totalPct) / 100;
        if (totalW > 0) {
            c.fillRoundRect(BAR_X + 1, barY + 1, totalW, BAR_H - 2, 1,
                            PixelRenderer::rgb(112, 118, 126));
        }
        int levelW = ((BAR_W - 2) * expPct) / 100;
        if (levelW > 0) {
            c.fillRoundRect(BAR_X + 1, barY + 1, levelW, BAR_H - 2, 1,
                            PixelRenderer::rgb(255, 216, 72));
        }
        c.drawRoundRect(BAR_X, barY, BAR_W, BAR_H, 2, PixelRenderer::rgb(45, 48, 56));
    } else if (statusPage == 2) {
        const MoveInfo* basicMove = findMove(moveIdForMonster(mon, activeMon, false));
        PixelRenderer::text(sx(12), sy(8), Ui::Status::MOVE_INFO, PixelRenderer::rgb(67, 213, 224), 1);

        TypeId basicType = basicMove ? basicMove->type : TypeId::NORMAL;
        PixelRenderer::text(sx(14), sy(31), Ui::Status::BASIC_MOVE, PixelRenderer::rgb(67, 213, 224), 1);
        int basicNameX = drawTypeBracket(sx(18), sy(51), basicType);
        PixelRenderer::text(basicNameX + 4, sy(51), basicMove ? basicMove->name : Ui::Status::MOVE_UNKNOWN,
                            PixelRenderer::rgb(241, 242, 232), 1);

        const int MOVE_DESC_X = sx(18);
        const int MOVE_DESC_W = body.width - 36;
        auto drawSpecialMove = [&](uint8_t slot, int y) {
            const MoveInfo* specialMove = findMove(specialMoveIdForMonster(activeMon, slot));
            char label[24];
            snprintf(label, sizeof(label), "%s%u", Ui::Status::SPECIAL_MOVE, (unsigned)(slot + 1));
            PixelRenderer::text(sx(14), sy(y), label,
                                specialMove ? PixelRenderer::rgb(67, 213, 224) : PixelRenderer::rgb(156, 164, 176),
                                1);
            if (!specialMove) {
                PixelRenderer::text(sx(104), sy(y), Ui::Status::MOVE_NOT_LEARNED,
                                    PixelRenderer::rgb(156, 164, 176), 1);
                return y + 38;
            }
            snprintf(buf, sizeof(buf), Ui::Status::PROFICIENCY_FMT,
                     proficiencyName(activeMon.moveProficiency[slot + 1]));
            PixelRenderer::text(sx(128), sy(y), buf, PixelRenderer::rgb(255, 216, 72), 1);
            int specialNameX = drawTypeBracket(sx(18), sy(y + 20), specialMove->type);
            PixelRenderer::text(specialNameX + 4, sy(y + 20), specialMove->name,
                                PixelRenderer::rgb(241, 242, 232), 1);
            snprintf(buf, sizeof(buf), Ui::Status::POWER_FMT, specialMove->power);
            PixelRenderer::text(sx(24), sy(y + 44), buf, PixelRenderer::rgb(255, 216, 72), 1);
            int lines = drawWrappedText(MOVE_DESC_X, sy(y + 64), specialMove->description,
                                        MOVE_DESC_W, PixelRenderer::rgb(135, 214, 238));
            return y + 74 + std::max(1, lines) * 16;
        };
        int moveY = drawSpecialMove(0, 78);
        drawSpecialMove(1, moveY);
    } else if (statusPage == 3) {
        PixelRenderer::text(sx(12), sy(8), Ui::Status::EFFORT_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        snprintf(buf, sizeof(buf), Ui::Status::TOTAL_FMT, Game::evTotal(activeMon.ev), Game::EV_TOTAL_MAX);
        drawInfoRow(sx(126), sy(8), buf, PixelRenderer::rgb(255, 216, 72));
        drawStatRow(sx(18), sy(34), Ui::Status::STAT_HP, activeMon.ev.hp, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(18), sy(52), Ui::Status::STAT_ATK, activeMon.ev.atk, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(18), sy(70), Ui::Status::STAT_DEF, activeMon.ev.def, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(18), sy(88), Ui::Status::STAT_SPA, activeMon.ev.spa, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(18), sy(106), Ui::Status::STAT_SPD, activeMon.ev.spd, PixelRenderer::rgb(241, 242, 232));
        drawStatRow(sx(126), sy(34), Ui::Status::STAT_SPE, activeMon.ev.spe, PixelRenderer::rgb(241, 242, 232));
    } else {
        PixelRenderer::text(sx(12), sy(8), Ui::Status::INDIVIDUAL_STATS, PixelRenderer::rgb(67, 213, 224), 1);
        drawStatRow(sx(18), sy(34), Ui::Status::STAT_HP, Game::ivAt(activeMon.ivPacked, 0), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(sx(18), sy(52), Ui::Status::STAT_ATK, Game::ivAt(activeMon.ivPacked, 1), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(sx(18), sy(70), Ui::Status::STAT_DEF, Game::ivAt(activeMon.ivPacked, 2), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(sx(18), sy(88), Ui::Status::STAT_SPA, Game::ivAt(activeMon.ivPacked, 3), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(sx(18), sy(106), Ui::Status::STAT_SPD, Game::ivAt(activeMon.ivPacked, 4), PixelRenderer::rgb(135, 214, 238));
        drawStatRow(sx(126), sy(34), Ui::Status::STAT_SPE, Game::ivAt(activeMon.ivPacked, 5), PixelRenderer::rgb(135, 214, 238));
    }

}

} }  // namespace Ui::StatusContent
