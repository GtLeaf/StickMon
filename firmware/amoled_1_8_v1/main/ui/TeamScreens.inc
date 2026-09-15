// Team list, status, and moves rendering.
namespace {

constexpr int TEAM_CARD_TOP = UiMetrics::PAGE_HEADER_HEIGHT + 8;
constexpr int TEAM_CARD_HEIGHT = 140;
constexpr int TEAM_CARD_GAP = 12;
constexpr int TEAM_STATUS_CONTENT_SHIFT =
    UiMetrics::PAGE_HEADER_HEIGHT - UiMetrics::HOME_HEADER_HEIGHT;
constexpr int TEAM_POPUP_X = 208;
constexpr int TEAM_POPUP_WIDTH = 148;
constexpr int TEAM_POPUP_PADDING = 6;
constexpr int TEAM_POPUP_ROW_HEIGHT = 56;

int teamPopupItemCount(uint8_t slot) {
    return slot == 0 ? 2 : 4;
}

int teamPopupTop(uint8_t slot) {
    const int desired = TEAM_CARD_TOP +
        slot * (TEAM_CARD_HEIGHT + TEAM_CARD_GAP) + 76;
    const int height = 2 * TEAM_POPUP_PADDING +
        teamPopupItemCount(slot) * TEAM_POPUP_ROW_HEIGHT;
    return std::min(desired, AmoledUi::HEIGHT - height - 4);
}

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        canvas.fillCircle((centerX), (centerY), (38), rgb(42, 61, 68));
        text(canvas, centerX - 32, centerY - 16, Ui::Amoled::MONSTER,
             rgb(115, 226, 183));
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    float scale = std::min(2.0f, std::min(100.0f / std::max(1, width),
                                         108.0f / std::max(1, height)));
    int drawnWidth = static_cast<int>(std::lround(width * scale));
    int drawnHeight = static_cast<int>(std::lround(height * scale));
    if (!PokemonSprites::drawFrameScaled(
            frame, centerX - drawnWidth / 2,
            centerY - drawnHeight / 2, scale)) {
        canvas.fillCircle((centerX), (centerY), (38), rgb(42, 61, 68));
    }
}

void drawTeamConfirm(Canvas565& canvas) {
    canvas.fillRoundRect((20), (264), (328), (164), (12), rgb(17, 27, 34));
    canvas.drawRoundRect((20), (264), (328), (164), (12), rgb(82, 117, 117));
    text(canvas, 100, 290, Ui::Amoled::CHANGE_LEADER,
         rgb(226, 238, 233));
    text(canvas, 124, 320, Ui::Amoled::SET_FIRST, rgb(248, 210, 105));
    canvas.fillRoundRect((36), (348), (140), (72), (8), rgb(36, 54, 61));
    canvas.fillRoundRect((192), (348), (140), (72), (8), rgb(91, 49, 55));
    text(canvas, 78, 376, Ui::Amoled::YES, rgb(115, 226, 183));
    text(canvas, 226, 376, Ui::BACK, rgb(239, 143, 148));
}

constexpr uint8_t TEAM_STATUS_PAGE_COUNT = 5;
constexpr int TEAM_STATUS_INDICATOR_Y = 424;
constexpr int TEAM_STATUS_INDICATOR_SPACING = 28;

const char* teamStatusGender(const Game::MonsterRuntime& monster) {
    switch (static_cast<Game::Gender>(monster.gender)) {
    case Game::Gender::MALE: return Ui::Status::GENDER_MALE;
    case Game::Gender::FEMALE: return Ui::Status::GENDER_FEMALE;
    default: return Ui::Status::GENDER_NONE;
    }
}

const char* teamStatusOrigin(Game::Origin origin) {
    switch (origin) {
    case Game::Origin::STARTER: return Ui::Status::ORIGIN_STARTER;
    case Game::Origin::HATCHED: return Ui::Status::ORIGIN_HATCHED;
    case Game::Origin::BEFRIENDED: return Ui::Status::ORIGIN_BEFRIENDED;
    case Game::Origin::TRADED: return Ui::Status::ORIGIN_TRADED;
    case Game::Origin::GIFT: return Ui::Status::ORIGIN_GIFT;
    default: return Ui::Status::ORIGIN_UNKNOWN;
    }
}

const char* teamStatusProficiency(uint8_t value) {
    if (value >= 90) return Ui::Status::PROF_FULL;
    if (value >= 60) return Ui::Status::PROF_HIGH;
    if (value >= 25) return Ui::Status::PROF_MID;
    return Ui::Status::PROF_LOW;
}

void drawTeamStatusStat(Canvas565& canvas, int x, int y,
                        const char* label, uint16_t value,
                        uint16_t color = rgb(226, 238, 233)) {
    char line[24] = {};
    std::snprintf(line, sizeof(line), Ui::Status::STAT_ROW_FMT, label, value);
    text(canvas, x, y, line, color);
}

void drawTeamStatusPageIndicator(Canvas565& canvas, uint8_t page) {
    const int firstX = AmoledUi::WIDTH / 2 -
        (TEAM_STATUS_PAGE_COUNT - 1) * TEAM_STATUS_INDICATOR_SPACING / 2;
    for (uint8_t index = 0; index < TEAM_STATUS_PAGE_COUNT; ++index) {
        const bool active = index == page;
        canvas.fillCircle(firstX + index * TEAM_STATUS_INDICATOR_SPACING,
                          TEAM_STATUS_INDICATOR_Y, active ? 6 : 4,
                          active ? rgb(115, 226, 183) : rgb(82, 117, 117));
    }
}

void drawTeamStatusMove(Canvas565& canvas, const Game::MonsterRuntime& monster,
                        const Species& species, uint8_t slot, int y,
                        int baseX) {
    const MoveInfo* move = slot == 0
        ? findMove(moveIdForMonster(species, monster, false))
        : findMove(specialMoveIdForMonster(monster, slot - 1));
    const uint16_t labelColor = move ? rgb(115, 226, 183)
                                     : rgb(91, 104, 104);
    text(canvas, baseX + 16, y, slot == 0 ? Ui::Status::BASIC_MOVE
                                          : Ui::Status::SPECIAL_MOVE,
         labelColor);
    if (!move) {
        text(canvas, baseX + 176, y, Ui::Status::MOVE_NOT_LEARNED,
             rgb(91, 104, 104));
        return;
    }
    text(canvas, baseX + 160, y, move->name, rgb(226, 238, 233));
    char detail[48] = {};
    std::snprintf(detail, sizeof(detail), Ui::Status::POWER_FMT,
                  static_cast<unsigned>(move->power));
    text(canvas, baseX + 16, y + 38, detail, rgb(248, 210, 105));
    std::snprintf(detail, sizeof(detail), Ui::Status::PROFICIENCY_FMT,
                  teamStatusProficiency(monster.moveProficiency[slot]));
    text(canvas, baseX + 176, y + 38, detail, rgb(248, 210, 105));
}

const char* moveSlotLabel(uint8_t slot) {
    switch (slot) {
    case 0: return Ui::Amoled::BATTLE_BASIC;
    case 1: return Ui::Amoled::BATTLE_SPECIAL_1;
    case 2: return Ui::Amoled::BATTLE_SPECIAL_2;
    default: return Ui::Amoled::MOVES;
    }
}

const char* teamMovesProficiencyName(uint8_t value) {
    if (value >= 90) return Ui::Amoled::MOVE_PROF_MASTERED;
    if (value >= 60) return Ui::Amoled::MOVE_PROF_SKILLED;
    if (value >= 25) return Ui::Amoled::MOVE_PROF_FAMILIAR;
    return Ui::Amoled::MOVE_PROF_BEGINNER;
}

uint16_t teamMoveTypeColor(TypeId type) {
    switch (type) {
    case TypeId::FIRE: return rgb(240, 128, 48);
    case TypeId::WATER: return rgb(104, 144, 240);
    case TypeId::GRASS: return rgb(120, 200, 80);
    case TypeId::ELECTRIC: return rgb(248, 208, 48);
    case TypeId::ICE: return rgb(152, 216, 216);
    case TypeId::FIGHTING: return rgb(192, 48, 40);
    case TypeId::POISON: return rgb(160, 64, 160);
    case TypeId::GROUND: return rgb(224, 192, 104);
    case TypeId::FLYING: return rgb(168, 144, 240);
    case TypeId::PSYCHIC: return rgb(248, 88, 136);
    case TypeId::BUG: return rgb(168, 184, 32);
    case TypeId::ROCK: return rgb(184, 160, 56);
    case TypeId::GHOST: return rgb(112, 88, 152);
    case TypeId::DRAGON: return rgb(112, 56, 248);
    case TypeId::DARK: return rgb(112, 88, 72);
    case TypeId::STEEL: return rgb(184, 184, 208);
    case TypeId::FAIRY: return rgb(238, 153, 172);
    case TypeId::NORMAL:
    default: return rgb(168, 168, 120);
    }
}

void drawTeamMoveTypeBadge(Canvas565& canvas, int x, int y, TypeId type) {
    constexpr int WIDTH = 76;
    constexpr int HEIGHT = 44;
    canvas.fillRoundRect(x + 2, y + 3, WIDTH, HEIGHT, 6, rgb(7, 10, 12));
    canvas.fillRoundRect(x, y, WIDTH, HEIGHT, 6, rgb(232, 236, 218));
    canvas.fillRoundRect(x + 2, y + 2, WIDTH - 4, HEIGHT - 4, 4,
                         teamMoveTypeColor(type));
    const char* label = typeName(type);
    const int labelX = x + (WIDTH - textWidth(label)) / 2;
    const int labelY = y + (HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
    text(canvas, labelX + 1, labelY + 1, label, rgb(38, 42, 38));
    text(canvas, labelX, labelY, label, rgb(248, 248, 240));
}

void drawTeamStatusPageContent(Canvas565& canvas,
                               const Game::MonsterRuntime& monster,
                               const Species& species, uint8_t page,
                               int baseX) {
    char line[64] = {};
    if (page == 0) {
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findSpeciesSprite(
                monster.speciesId, PokemonSprites::SpriteKind::STATUS);
        if (frame) {
            uint8_t width = FlashStorage::readByte(&frame->width);
            uint8_t height = FlashStorage::readByte(&frame->height);
            int x = baseX + 52 - width / 2;
            int y = 126 + TEAM_STATUS_CONTENT_SHIFT - height / 2;
            PokemonSprites::drawFrame(frame, x, y);
        }
        text(canvas, baseX + 112, 64 + TEAM_STATUS_CONTENT_SHIFT, species.name,
             rgb(226, 238, 233));
        std::snprintf(line, sizeof(line), "Lv%u %s", monster.level,
                      teamStatusGender(monster));
        text(canvas, baseX + 112, 104 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(226, 238, 233));
        std::snprintf(line, sizeof(line), "%s%s/%s", "属性:",
                      typeName(species.type1), typeName(species.type2));
        text(canvas, baseX + 112, 144 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(248, 210, 105));
        std::snprintf(line, sizeof(line), "%s%s", Ui::Status::ABILITY,
                      abilityName(abilityForSpecies(species)));
        text(canvas, baseX + 112, 184 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(115, 226, 183));
        std::snprintf(line, sizeof(line), "%s%s", "性格:",
                      natureName(monster.nature));
        text(canvas, baseX + 112, 224 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(226, 238, 233));
        int8_t liked = natureLikedFoodIndex(monster.nature);
        int8_t disliked = natureDislikedFoodIndex(monster.nature);
        if (liked >= 0 && disliked >= 0) {
            std::snprintf(line, sizeof(line), "喜%s 厌%s",
                          Ui::Status::FLAVOR_NAMES[liked],
                          Ui::Status::FLAVOR_NAMES[disliked]);
            text(canvas, baseX + 112, 264 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(126, 175, 175));
        }
        std::snprintf(line, sizeof(line), "%s%s", Ui::Status::SOURCE_INFO,
                      teamStatusOrigin(monster.origin));
        text(canvas, baseX + 16, 312 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(226, 238, 233));
        if (monster.origin == Game::Origin::BEFRIENDED &&
            monster.metArea < Ui::Explore::AREA_COUNT) {
            std::snprintf(line, sizeof(line), "地点:%s",
                          Ui::Explore::AREA_ITEMS[monster.metArea]);
            text(canvas, baseX + 16, 352 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(248, 210, 105));
        }
    } else if (page == 1) {
        std::snprintf(line, sizeof(line), Ui::Status::HP_FMT,
                      monster.hpCur, monster.hpMax);
        text(canvas, baseX + 16, 72 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(92, 222, 112));
        const int barX = baseX + 16;
        const int barY = 108 + TEAM_STATUS_CONTENT_SHIFT;
        const int barW = 336;
        const int barH = 14;
        canvas.fillRoundRect(barX, barY, barW, barH, 4, rgb(29, 34, 42));
        int hpWidth = monster.hpMax > 0
            ? (barW - 2) * std::min<uint16_t>(monster.hpCur, monster.hpMax) /
                  monster.hpMax
            : 0;
        if (hpWidth > 0) {
            canvas.fillRoundRect(barX + 1, barY + 1, hpWidth, barH - 2,
                                 3, rgb(92, 222, 112));
        }
        canvas.drawRoundRect(barX, barY, barW, barH, 4, rgb(67, 97, 101));
        drawTeamStatusStat(canvas, baseX + 16, 152 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_ATK,
                           statFor(species, monster, 1));
        drawTeamStatusStat(canvas, baseX + 192, 152 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPA,
                           statFor(species, monster, 3));
        drawTeamStatusStat(canvas, baseX + 16, 192 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_DEF,
                           statFor(species, monster, 2));
        drawTeamStatusStat(canvas, baseX + 192, 192 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPD,
                           statFor(species, monster, 4));
        drawTeamStatusStat(canvas, baseX + 16, 232 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPE,
                           statFor(species, monster, 5));
        std::snprintf(line, sizeof(line), Ui::Status::EXP_VALUE_FMT,
                      static_cast<unsigned long>(monster.exp));
        text(canvas, baseX + 16, 276 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(226, 238, 233));
        std::snprintf(line, sizeof(line), Ui::Status::EXP_NEXT_FMT,
                      static_cast<unsigned long>(
                          expToNextLevel(species.growthRate, monster.level,
                                         monster.exp)));
        text(canvas, baseX + 192, 276 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(226, 238, 233));
        uint32_t floor = minimumExpForLevel(species.growthRate, monster.level);
        uint32_t next = monster.level >= Game::LEVEL_MAX
            ? monster.exp
            : minimumExpForLevel(species.growthRate, monster.level + 1);
        uint8_t progress = next > floor
            ? static_cast<uint8_t>(std::min<uint32_t>(100,
                  (monster.exp > floor ? monster.exp - floor : 0) * 100 /
                  (next - floor))) : 100;
        canvas.fillRoundRect(baseX + 16, 320 + TEAM_STATUS_CONTENT_SHIFT, 336, 14, 4, rgb(29, 34, 42));
        int progressWidth = (334 * progress) / 100;
        if (progressWidth > 0) {
            canvas.fillRoundRect(baseX + 17, 321 + TEAM_STATUS_CONTENT_SHIFT, progressWidth, 12, 3,
                                 rgb(255, 216, 72));
        }
        canvas.drawRoundRect(baseX + 16, 320 + TEAM_STATUS_CONTENT_SHIFT, 336, 14, 4, rgb(67, 97, 101));
    } else if (page == 2) {
        drawTeamStatusMove(canvas, monster, species, 0, 72 + TEAM_STATUS_CONTENT_SHIFT, baseX);
        drawTeamStatusMove(canvas, monster, species, 1, 152 + TEAM_STATUS_CONTENT_SHIFT, baseX);
        drawTeamStatusMove(canvas, monster, species, 2, 252 + TEAM_STATUS_CONTENT_SHIFT, baseX);
    } else if (page == 3) {
        std::snprintf(line, sizeof(line), Ui::Status::TOTAL_FMT,
                      Game::evTotal(monster.ev), Game::EV_TOTAL_MAX);
        text(canvas, baseX + 16, 72 + TEAM_STATUS_CONTENT_SHIFT, line, rgb(248, 210, 105));
        drawTeamStatusStat(canvas, baseX + 16, 128 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_HP, monster.ev.hp);
        drawTeamStatusStat(canvas, baseX + 192, 128 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_ATK, monster.ev.atk);
        drawTeamStatusStat(canvas, baseX + 16, 176 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_DEF, monster.ev.def);
        drawTeamStatusStat(canvas, baseX + 192, 176 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPA, monster.ev.spa);
        drawTeamStatusStat(canvas, baseX + 16, 224 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPD, monster.ev.spd);
        drawTeamStatusStat(canvas, baseX + 192, 224 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPE, monster.ev.spe);
    } else {
        text(canvas, baseX + 16, 72 + TEAM_STATUS_CONTENT_SHIFT, "31为最高", rgb(126, 175, 175));
        drawTeamStatusStat(canvas, baseX + 16, 128 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_HP,
                           Game::ivAt(monster.ivPacked, 0), rgb(135, 214, 238));
        drawTeamStatusStat(canvas, baseX + 192, 128 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_ATK,
                           Game::ivAt(monster.ivPacked, 1), rgb(135, 214, 238));
        drawTeamStatusStat(canvas, baseX + 16, 176 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_DEF,
                           Game::ivAt(monster.ivPacked, 2), rgb(135, 214, 238));
        drawTeamStatusStat(canvas, baseX + 192, 176 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPA,
                           Game::ivAt(monster.ivPacked, 3), rgb(135, 214, 238));
        drawTeamStatusStat(canvas, baseX + 16, 224 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPD,
                           Game::ivAt(monster.ivPacked, 4), rgb(135, 214, 238));
        drawTeamStatusStat(canvas, baseX + 192, 224 + TEAM_STATUS_CONTENT_SHIFT, Ui::Status::STAT_SPE,
                           Game::ivAt(monster.ivPacked, 5), rgb(135, 214, 238));
    }
}

}  // namespace

bool teamBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int teamMemberAt(int x, int y, uint8_t teamCount) {
    if (x < 12 || x >= 356 || y < TEAM_CARD_TOP) return -1;
    int pitch = TEAM_CARD_HEIGHT + TEAM_CARD_GAP;
    int index = (y - TEAM_CARD_TOP) / pitch;
    int rowY = TEAM_CARD_TOP + index * pitch;
    if (index < 0 || index >= teamCount || y >= rowY + TEAM_CARD_HEIGHT) {
        return -1;
    }
    return index;
}

bool teamMovesButtonAt(int x, int y, uint8_t teamSlot) {
    constexpr int pitch = TEAM_CARD_HEIGHT + TEAM_CARD_GAP;
    int rowY = TEAM_CARD_TOP + static_cast<int>(teamSlot) * pitch;
    return x >= 240 && x < 352 && y >= rowY + 78 && y < rowY + 134;
}

int teamActionPopupItemAt(int x, int y, uint8_t teamSlot) {
    if (teamSlot >= Game::TEAM_CAP || x < TEAM_POPUP_X ||
        x >= TEAM_POPUP_X + TEAM_POPUP_WIDTH) return -1;
    const int localY = y - teamPopupTop(teamSlot) - TEAM_POPUP_PADDING;
    if (localY < 0 ||
        localY >= teamPopupItemCount(teamSlot) * TEAM_POPUP_ROW_HEIGHT) return -1;
    return localY / TEAM_POPUP_ROW_HEIGHT;
}

int teamConfirmChoiceAt(int x, int y) {
    return itemConfirmChoiceAt(x, y);
}

bool teamStatusBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

void renderTeamStatusScreen(Canvas565& canvas,
                            const TeamStatusViewModel& model,
                            uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd || !model.state ||
        model.teamSlot >= model.state->teamCount ||
        model.teamSlot >= Game::TEAM_CAP) return;

    const Game::MonsterRuntime& monster = model.state->team[model.teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return;
    const uint8_t page = model.page < TEAM_STATUS_PAGE_COUNT ? model.page : 0;

    pageClip.setRect(0, rowBegin, AmoledUi::WIDTH, rowEnd - rowBegin);
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT,
                    UiMetrics::PAGE_BACKGROUND);
    const char* title = page == 0 ? Ui::STATUS
        : page == 1 ? Ui::Status::CURRENT_STATS
        : page == 2 ? Ui::Status::MOVE_INFO
        : page == 3 ? Ui::Status::EFFORT_STATS
                    : Ui::Status::INDIVIDUAL_STATS;
    UiCommon::drawPageHeader(canvas, title);

    const int slideOffset = model.slideOffsetX;
    drawTeamStatusPageContent(canvas, monster, *species, page, slideOffset);
    if (slideOffset < 0 && page + 1 < TEAM_STATUS_PAGE_COUNT) {
        drawTeamStatusPageContent(canvas, monster, *species, page + 1,
                                  slideOffset + AmoledUi::WIDTH);
    } else if (slideOffset > 0 && page > 0) {
        drawTeamStatusPageContent(canvas, monster, *species, page - 1,
                                  slideOffset - AmoledUi::WIDTH);
    }

    drawTeamStatusPageIndicator(canvas, page);
    pageClip.reset();
}

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd || !model.state) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (bottom - rowBegin));
        canvas.fillRect((0), (0), (AmoledUi::WIDTH), (MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);
        UiCommon::drawPageHeader(canvas, Ui::TEAM);
        pageClip.reset();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    pageClip.setRect((0), (contentTop), (AmoledUi::WIDTH), (rowEnd - contentTop));
    canvas.fillRect((0), (MENU_CONTENT_TOP), (AmoledUi::WIDTH), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);

    uint8_t teamCount = std::min<uint8_t>(model.state->teamCount,
                                           Game::TEAM_CAP);
    for (uint8_t slot = 0; slot < teamCount; ++slot) {
        const Game::MonsterRuntime& monster = model.state->team[slot];
        int y = TEAM_CARD_TOP + slot * (TEAM_CARD_HEIGHT + TEAM_CARD_GAP);
        uint16_t background = model.pressedSlot == slot
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        uint16_t border = slot == 0 ? rgb(115, 226, 183)
                                    : rgb(67, 97, 101);
        canvas.fillRoundRect((12), (y), (344), (TEAM_CARD_HEIGHT), (10), background);
        canvas.drawRoundRect((12), (y), (344), (TEAM_CARD_HEIGHT), (10), border);
        drawTeamSprite(canvas, monster.speciesId, 70, y + 72);

        const Species* species = findSpecies(monster.speciesId);
        PixelRenderer::text((128), (y + 12), species ? species->name : Ui::Amoled::MONSTER, rgb(226, 238, 233), 1);
        char level[12];
        std::snprintf(level, sizeof(level), "LV%u", monster.level);
        text(canvas, 128, y + 56, level, rgb(126, 175, 175));
        char hp[20];
        std::snprintf(hp, sizeof(hp), "%u/%u",
                      monster.hpCur, monster.hpMax);
        text(canvas, 344 - textWidth(hp),
             y + 56, hp, rgb(226, 238, 233));
        drawHomeHpBar(canvas, 128, y + 84, 208,
                      Game::HomeHud::hpPercent(monster));

        if (monster.majorStatus != Game::MajorStatus::NONE) {
            const int iconX = 128 + textWidth(species ? species->name : Ui::Amoled::MONSTER) + 10;
            canvas.fillCircle(iconX + 8, y + 24, 8, rgb(239, 143, 148));
            canvas.drawCircle(iconX + 8, y + 24, 8, rgb(248, 210, 105));
            canvas.drawFastHLine(iconX + 4, y + 24, 8, rgb(12, 18, 25));
            canvas.drawFastVLine(iconX + 8, y + 20, 8, rgb(12, 18, 25));
        }
        if (slot != 0 && monster.origin != Game::Origin::VISITOR) {
            canvas.drawLine((328), (y + 18), (338), (y + 26), rgb(115, 226, 183));
            canvas.drawLine((338), (y + 26), (328), (y + 34), rgb(115, 226, 183));
        }
    }
    drawToast(canvas, model.toast);
    if (model.confirmOpen) drawTeamConfirm(canvas);
    if (model.actionPopupOpen) {
        const bool leader = model.actionPopupSlot == 0;
        constexpr int popupTextX = TEAM_POPUP_X + 16;
        constexpr int popupSeparatorX = TEAM_POPUP_X + 12;
        constexpr int popupSeparatorWidth = TEAM_POPUP_WIDTH - 24;
        const int itemCount = teamPopupItemCount(model.actionPopupSlot);
        const int height = 2 * TEAM_POPUP_PADDING + itemCount * TEAM_POPUP_ROW_HEIGHT;
        const int top = teamPopupTop(model.actionPopupSlot);
        const uint16_t popupTextColor = rgb(226, 238, 233);
        const uint16_t popupSeparatorColor = rgb(82, 117, 117);

        canvas.fillRoundRect(TEAM_POPUP_X, top, TEAM_POPUP_WIDTH, height, 8,
                             rgb(17, 27, 34));
        canvas.drawRoundRect(TEAM_POPUP_X, top, TEAM_POPUP_WIDTH, height, 8,
                             rgb(115, 226, 183));

        for (int item = 0; item < itemCount; ++item) {
            const int rowTop = top + TEAM_POPUP_PADDING + item * TEAM_POPUP_ROW_HEIGHT;
            const int itemTop = rowTop +
                (TEAM_POPUP_ROW_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
            const char* label = nullptr;
            if (!leader && item == 0) label = "首位";
            else if ((!leader && item == 1) || (leader && item == 0)) {
                label = Ui::STATUS;
            } else if ((!leader && item == 2) || (leader && item == 1)) {
                label = Ui::Amoled::MOVES;
            } else if (!leader && item == 3) {
                label = Ui::Team::ACTION_LEAVE;
            } else {
                label = Ui::Team::ACTION_BACK;
            }
            text(canvas, popupTextX, itemTop, label, popupTextColor);

            if (item + 1 < itemCount) {
                // Center the separator in the gap between the 32px labels.
                canvas.fillRect(popupSeparatorX,
                                rowTop + TEAM_POPUP_ROW_HEIGHT - 1,
                                popupSeparatorWidth, 2,
                                popupSeparatorColor);
            }
        }
    }
    pageClip.reset();
}

bool teamMovesBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount) {
    return teamMovesItemAt(x, y, mode, recallCount, 0, 0xFF, 0.0f);
}

int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount, int16_t scrollOffsetY,
                    uint8_t selectedItem, float detailProgress) {
    constexpr int LIST_TOP = TEAM_MOVES_HEADER_HEIGHT;
    constexpr int ROW_HEIGHT = 60;
    constexpr int ROW_STEP = 66;
    constexpr int DETAIL_HEIGHT = AmoledUi::HEIGHT / 2;
    if (x < 12 || x >= 356 || y < LIST_TOP || y >= AmoledUi::HEIGHT) {
        return -1;
    }
    const int rowCount = mode == TeamMovesViewModel::Mode::MANAGE
        ? Game::MOVE_SLOT_COUNT : mode == TeamMovesViewModel::Mode::RECALL_SELECT
            ? static_cast<int>(recallCount) + 1 : 3;
    const float progress = std::clamp(detailProgress, 0.0f, 1.0f);
    const int push = static_cast<int>(std::lround(DETAIL_HEIGHT * progress));
    for (int index = 0; index < rowCount; ++index) {
        int rowY = LIST_TOP + index * ROW_STEP - scrollOffsetY;
        if (selectedItem < rowCount && index > selectedItem) rowY += push;
        if (y >= rowY && y < rowY + ROW_HEIGHT) return index;
    }
    return -1;
}

void renderTeamMovesScreen(Canvas565& canvas,
                           const TeamMovesViewModel& model,
                           uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd || !model.state ||
        model.teamSlot >= model.state->teamCount) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT),
                    rgb(0, 0, 0));
    const Game::MonsterRuntime& monster = model.state->team[model.teamSlot];
    const Species* species = findSpecies(monster.speciesId);

    constexpr int LIST_TOP = TEAM_MOVES_HEADER_HEIGHT;
    constexpr int ROW_HEIGHT = 60;
    constexpr int ROW_STEP = 66;
    constexpr int DETAIL_HEIGHT = AmoledUi::HEIGHT / 2;

    const MoveInfo* selectedMove = nullptr;
    if (model.mode == TeamMovesViewModel::Mode::MANAGE &&
        model.selectedItem < Game::MOVE_SLOT_COUNT) {
        selectedMove = species
            ? Game::MoveManagementService::learnedMove(
                  *species, monster, model.selectedItem) : nullptr;
    } else if ((model.mode == TeamMovesViewModel::Mode::RECALL_SELECT ||
                model.mode == TeamMovesViewModel::Mode::RECALL_REPLACE) &&
               model.recallSelected < model.recallCount) {
        selectedMove = findMove(model.recallIds[model.recallSelected]);
    }
    const float detailProgress = std::clamp(model.detailProgress, 0.0f, 1.0f);
    const bool detailVisible = selectedMove != nullptr && detailProgress > 0.0f;
    uint8_t rowCount = model.mode == TeamMovesViewModel::Mode::MANAGE
        ? Game::MOVE_SLOT_COUNT : model.mode == TeamMovesViewModel::Mode::RECALL_SELECT
            ? static_cast<uint8_t>(model.recallCount + 1) : 3;
    const int detailTop = model.selectedItem < rowCount
        ? LIST_TOP + (model.selectedItem + 1) * ROW_STEP -
              model.scrollOffsetY
        : LIST_TOP;
    const int detailHeight = static_cast<int>(
        std::lround(DETAIL_HEIGHT * detailProgress));
    const int listBottom = AmoledUi::HEIGHT;
    if (detailVisible && detailHeight > 0) {
        const int panelTop = std::max(LIST_TOP, detailTop);
        const int panelBottom = std::min(AmoledUi::HEIGHT,
                                          detailTop + detailHeight);
        if (panelBottom > panelTop) {
            canvas.fillRect(0, panelTop, AmoledUi::WIDTH, panelBottom - panelTop,
                            rgb(17, 27, 34));
            if (detailTop >= LIST_TOP) {
                canvas.fillRect(0, detailTop, AmoledUi::WIDTH, 2,
                                rgb(56, 87, 89));
            }
        }
    }

    pageClip.setRect(0, LIST_TOP, AmoledUi::WIDTH,
                     listBottom - LIST_TOP);
    for (uint8_t index = 0; index < rowCount; ++index) {
        int y = LIST_TOP + index * ROW_STEP - model.scrollOffsetY;
        if (model.selectedItem < rowCount && index > model.selectedItem) {
            y += static_cast<int>(std::lround(DETAIL_HEIGHT * detailProgress));
        }
        if (y + ROW_HEIGHT <= LIST_TOP || y >= listBottom) continue;
        bool selected = index == model.selectedItem;
        canvas.fillRoundRect((12), (y), (344), ROW_HEIGHT, (8), selected ? rgb(42, 61, 68)
                                      : rgb(24, 34, 42));
        const int textY = y +
            (ROW_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
        if (model.mode == TeamMovesViewModel::Mode::MANAGE) {
            const MoveInfo* move = species
                ? Game::MoveManagementService::learnedMove(
                      *species, monster, index) : nullptr;
            if (move) {
                drawTeamMoveTypeBadge(canvas, 20, y + 8, move->type);
                text(canvas, 108, textY, move->name, rgb(226, 238, 233));
                const char* proficiency = teamMovesProficiencyName(
                    monster.moveProficiency[index]);
                text(canvas, 344 - textWidth(proficiency), textY,
                     proficiency, rgb(115, 226, 183));
            } else {
                text(canvas, 108, textY, moveSlotLabel(index),
                     rgb(91, 104, 104));
            }
        } else if (model.mode == TeamMovesViewModel::Mode::RECALL_SELECT) {
            const char* label = Ui::BACK;
            if (index < model.recallCount) {
                const MoveInfo* move = findMove(model.recallIds[index]);
                label = move ? move->name : Ui::Amoled::MOVES;
            }
            text(canvas, 28, textY, label, rgb(115, 226, 183));
        } else if (index < 2) {
            const char* label = index == 0 ? Ui::Amoled::REPLACE_SPECIAL_1
                                           : Ui::Amoled::REPLACE_SPECIAL_2;
            text(canvas, 28, textY, label, rgb(115, 226, 183));
        } else {
            text(canvas, 28, textY, Ui::BACK, rgb(115, 226, 183));
        }
    }
    pageClip.reset();

    if (detailVisible) {
        const int detailBase = detailTop;
        const uint8_t fade = static_cast<uint8_t>(std::lround(
            255.0f * detailProgress));
        auto fadeColor = [fade](uint16_t color) {
            const uint16_t r = static_cast<uint16_t>((color >> 11) & 0x1F);
            const uint16_t g = static_cast<uint16_t>((color >> 5) & 0x3F);
            const uint16_t b = static_cast<uint16_t>(color & 0x1F);
            return static_cast<uint16_t>(((r * fade / 255) << 11) |
                                         ((g * fade / 255) << 5) |
                                         (b * fade / 255));
        };
        const int visibleDetailTop = std::max(LIST_TOP, detailBase);
        const int visibleDetailBottom = std::min(
            AmoledUi::HEIGHT, detailBase + detailHeight);
        UiCommon::PageClip detailClip(
            canvas, static_cast<uint16_t>(std::clamp(visibleDetailTop, 0,
                                                     AmoledUi::HEIGHT)),
            static_cast<uint16_t>(std::clamp(visibleDetailBottom, 0,
                                             AmoledUi::HEIGHT)));
        detailClip.setRect(0, detailBase, AmoledUi::WIDTH,
                           std::max(0, std::min(AmoledUi::HEIGHT,
                                                detailBase + detailHeight) - detailBase));
        char detail[64] = {};
        std::snprintf(detail, sizeof(detail), "威力:%u  命中:%u",
                      selectedMove->power, selectedMove->accuracy);
        text(canvas, 24, detailBase + 20, detail,
             fadeColor(rgb(226, 238, 233)));
        const char* description = selectedMove->description;
        if (description && description[0]) {
            char line[80] = {};
            size_t lineLength = 0;
            int lineWidth = 0;
            int lineIndex = 0;
            const uint8_t* cursor = reinterpret_cast<const uint8_t*>(description);
            while (*cursor && lineIndex < 5) {
                size_t bytes = 1;
                while (cursor[bytes] && (cursor[bytes] & 0xC0) == 0x80) ++bytes;
                const int glyphWidth = *cursor < 0x80 ? 16 : 32;
                if (lineLength > 0 && lineWidth + glyphWidth > 320) {
                    line[lineLength] = '\0';
                    text(canvas, 24, detailBase + 64 + lineIndex * 28,
                         line, fadeColor(rgb(178, 194, 190)));
                    ++lineIndex;
                    lineLength = 0;
                    lineWidth = 0;
                    if (lineIndex >= 5) break;
                }
                if (lineLength + bytes >= sizeof(line)) break;
                std::memcpy(line + lineLength, cursor, bytes);
                lineLength += bytes;
                lineWidth += glyphWidth;
                cursor += bytes;
            }
            if (lineLength > 0 && lineIndex < 5) {
                line[lineLength] = '\0';
                text(canvas, 24, detailBase + 64 + lineIndex * 28,
                     line, fadeColor(rgb(178, 194, 190)));
            }
        }
        detailClip.reset();
    }
    pageClip.setRect(0, rowBegin, AmoledUi::WIDTH, rowEnd - rowBegin);

    if (model.scrollOffsetY > 0) {
        constexpr int FADE_HEIGHT = 28;
        const int strength = std::min<int>(255, model.scrollOffsetY * 18);
        for (int y = 0; y < FADE_HEIGHT; y += 4) {
            const uint8_t alpha = static_cast<uint8_t>(
                strength * (FADE_HEIGHT - y) / FADE_HEIGHT);
            PixelRenderer::fillRectAlpha(0, LIST_TOP + y,
                                         AmoledUi::WIDTH, 4,
                                         rgb(0, 0, 0), alpha);
        }
    }
    UiCommon::drawPageHeader(canvas, Ui::Amoled::MOVES);
    drawToast(canvas, model.toast);
    pageClip.reset();
}
