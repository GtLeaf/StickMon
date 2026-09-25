// Extracted screen rendering and hit testing.
namespace {

constexpr int CONTACT_MENU_X = 188;
constexpr int CONTACT_MENU_WIDTH = 168;
constexpr int CONTACT_MENU_ROW_HEIGHT = 54;
constexpr int CONTACT_MENU_PADDING = 8;
constexpr int CONTACT_CONFIRM_Y = 164;
constexpr int CONTACT_CONFIRM_HEIGHT = 160;

int contactActionCount(const ComputerViewModel& model) {
    return 1 + (model.contactInTeam ? 0 : 1) +
           (model.contactCanDelete ? 1 : 0);
}

const char* contactActionLabel(const ComputerViewModel& model, int index) {
    if (index == 0) return Ui::Storage::ACTION_STATUS;
    int cursor = 1;
    if (!model.contactInTeam) {
        if (index == cursor) return Ui::Storage::ACTION_INVITE;
        ++cursor;
    }
    if (model.contactCanDelete && index == cursor) {
        return Ui::Storage::ACTION_DELETE;
    }
    return Ui::Storage::ACTION_STATUS;
}

const char* contactBondLabel(Game::Bond::Value value) {
    const uint8_t level = static_cast<uint8_t>(Game::Bond::levelFor(value));
    return Ui::Storage::BOND_LEVELS[level - 1];
}

uint16_t contactBondColor(Game::Bond::Value value) {
    switch (Game::Bond::levelFor(value)) {
    case Game::Bond::Level::AVERSE: return rgb(239, 143, 148);
    case Game::Bond::Level::DISTANT: return rgb(126, 145, 145);
    case Game::Bond::Level::ACQUAINTED: return rgb(135, 180, 198);
    case Game::Bond::Level::FAMILIAR: return rgb(115, 226, 183);
    case Game::Bond::Level::TRUSTED: return rgb(248, 210, 105);
    case Game::Bond::Level::CLOSE: return rgb(255, 126, 168);
    }
    return rgb(226, 238, 233);
}

const Game::MonsterRuntime* liveContactMonster(
    const ComputerViewModel& model, uint8_t slot) {
    return model.state ? ContactRoster::liveMonsterForContact(*model.state, slot)
                       : nullptr;
}

int contactMenuHeight(int actionCount) {
    return actionCount * CONTACT_MENU_ROW_HEIGHT + 2 * CONTACT_MENU_PADDING;
}

int contactMenuTop(int actionCount, uint8_t slot, float scroll) {
    const int rowCenter = MENU_CONTENT_TOP + slot * CONTACT_ROW_HEIGHT +
                          CONTACT_ROW_HEIGHT / 2 -
                          static_cast<int>(std::lround(scroll));
    return std::clamp(rowCenter - contactMenuHeight(actionCount) / 2,
                      MENU_CONTENT_TOP + 8,
                      AmoledUi::HEIGHT - contactMenuHeight(actionCount) - 8);
}

void drawContactAvatar(const PokemonSprites::SpriteFrame* frame,
                       int centerX, int centerY, int maxSize) {
    if (!frame) return;
    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    const float scale = std::min(4.0f,
        std::min(static_cast<float>(maxSize) / width,
                 static_cast<float>(maxSize) / height));
    const int drawnWidth = static_cast<int>(std::lround(width * scale));
    const int drawnHeight = static_cast<int>(std::lround(height * scale));
    PokemonSprites::drawFrameScaled(frame, centerX - drawnWidth / 2,
                                    centerY - drawnHeight / 2, scale, false);
}

}  // namespace

bool computerBackAt(int x, int y, ComputerViewModel::Page page) {
    if (page == ComputerViewModel::Page::CLAW_SETUP) {
        return x >= 0 && x < 60 && y >= 0 && y < HEADER_HEIGHT;
    }
    return UiCommon::pageHeaderBackAt(x, y);
}

int clawTabAt(int x, int y) {
    if (y < 0 || y >= HEADER_HEIGHT) return -1;
    if (x >= CLAW_TAB_CONNECT_LEFT &&
        x < CLAW_TAB_CONNECT_LEFT + CLAW_TAB_WIDTH) {
        return 0;
    }
    if (x >= CLAW_TAB_LOG_LEFT && x < CLAW_TAB_LOG_LEFT + CLAW_TAB_WIDTH) {
        return 1;
    }
    return -1;
}

int computerItemAt(int x, int y, ComputerViewModel::Page page,
                   float storageScroll, uint8_t storageCount,
                   bool clawEnabled) {
    if (x < 0 || x >= AmoledUi::WIDTH || y < MENU_CONTENT_TOP ||
        y >= AmoledUi::HEIGHT) return -1;
    if (page != ComputerViewModel::Page::STORAGE &&
        (x < 12 || x >= 356)) return -1;
    if (page == ComputerViewModel::Page::MENU) {
        int row = (y - MENU_CONTENT_TOP) / COMPUTER_MENU_ROW_HEIGHT;
#if STICKMON_HAS_CLAW
        return row < 3 ? row : -1;
#else
        return row < 2 ? row : -1;
#endif
    }
    if (page == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
        const int row = (y - MENU_CONTENT_TOP) / COMPUTER_MENU_ROW_HEIGHT;
        const int rowCount = clawEnabled ? 4 : 3;
        return row < rowCount ? row : -1;
#else
        return -1;
#endif
    }
    if (page != ComputerViewModel::Page::STORAGE || storageCount == 0) {
        return -1;
    }
    int contentY = y - MENU_CONTENT_TOP +
                   static_cast<int>(std::lround(storageScroll));
    int row = contentY / CONTACT_ROW_HEIGHT;
    return row >= 0 && row < storageCount ? row : -1;
}

int computerMaxStorageScroll(uint8_t storageCount) {
    return std::max(0, static_cast<int>(storageCount) * CONTACT_ROW_HEIGHT -
                       (AmoledUi::HEIGHT - UiMetrics::CONTENT_TOP));
}

int computerContactActionItemAt(int x, int y, uint8_t actionCount,
                                uint8_t slot, float scroll) {
    if (!computerContactMenuAt(x, y, actionCount, slot, scroll)) return -1;
    const int localY = y - contactMenuTop(actionCount, slot, scroll) -
                       CONTACT_MENU_PADDING;
    return localY >= 0 && localY < actionCount * CONTACT_MENU_ROW_HEIGHT
        ? localY / CONTACT_MENU_ROW_HEIGHT : -1;
}

bool computerContactMenuAt(int x, int y, uint8_t actionCount,
                           uint8_t slot, float scroll) {
    return actionCount > 0 && x >= CONTACT_MENU_X &&
           x < CONTACT_MENU_X + CONTACT_MENU_WIDTH &&
           y >= contactMenuTop(actionCount, slot, scroll) &&
           y < contactMenuTop(actionCount, slot, scroll) +
                   contactMenuHeight(actionCount);
}

int computerContactConfirmChoiceAt(int x, int y) {
    if (x < 40 || x >= AmoledUi::WIDTH - 40 ||
        y < CONTACT_CONFIRM_Y ||
        y >= CONTACT_CONFIRM_Y + CONTACT_CONFIRM_HEIGHT) return -1;
    if (y >= CONTACT_CONFIRM_Y + 88 && y < CONTACT_CONFIRM_Y + 144) {
        if (x < 176) return 0;
        if (x >= 192) return 1;
    }
    return -1;
}

void drawAiToggle(Canvas565& canvas, int centerX, int centerY, bool on,
                  bool pressed) {
    const uint16_t track = on ? rgb(62, 124, 105) : rgb(60, 72, 80);
    const uint16_t knob = on ? rgb(226, 255, 234) : rgb(168, 181, 184);
    canvas.fillRoundRect((centerX - 36), (centerY - 14), (72), (28), (14), track);
    if (pressed) {
        canvas.drawRoundRect((centerX - 36), (centerY - 14), (72), (28), (14), rgb(248, 210, 105));
    }
    canvas.fillCircle(((centerX + (on ? 20 : -20))), (centerY), (12), knob);
}

void renderComputerScreen(Canvas565& canvas, const ComputerViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT,
                    UiMetrics::PAGE_BACKGROUND);
    const char* title = model.page == ComputerViewModel::Page::STATUS
        ? Ui::Amoled::STATUS_PAGE
        : model.page == ComputerViewModel::Page::STORAGE
            ? Ui::Amoled::STORAGE_PAGE
        : model.page == ComputerViewModel::Page::AI_HOSTING
            ? Ui::Amoled::AI_HOSTING
        : model.page == ComputerViewModel::Page::CLAW_SETUP
            ? Ui::Amoled::BACKEND : Ui::COMPUTER;
#if STICKMON_HAS_CLAW
    if (model.page == ComputerViewModel::Page::CLAW_SETUP) {
        UiCommon::drawHeader(canvas, HEADER_HEIGHT, true);
        UiCommon::drawHeaderText(canvas, 72, title, rgb(115, 226, 183), HEADER_HEIGHT);
        drawClawTabs(canvas, model.clawLogView);
    } else if (model.page != ComputerViewModel::Page::STORAGE) {
        UiCommon::drawPageHeader(canvas, title);
    }
#else
    if (model.page != ComputerViewModel::Page::STORAGE) {
        UiCommon::drawPageHeader(canvas, title);
    }
#endif

    if (model.page == ComputerViewModel::Page::MENU) {
        static constexpr const char* ITEMS[] = {
            Ui::SOCIAL, Ui::Amoled::STORAGE_PAGE,
#if STICKMON_HAS_CLAW
            Ui::Amoled::AI_HOSTING,
#endif
        };
        constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);
        for (int index = 0; index < ITEM_COUNT; ++index) {
            int y = MENU_CONTENT_TOP + index * COMPUTER_MENU_ROW_HEIGHT;
            bool selected = index == model.pressedItem;
            canvas.fillRoundRect((12), (y + 4), (344), (COMPUTER_MENU_CELL_HEIGHT), (8), selected ? rgb(42, 61, 68) : rgb(24, 34, 42));
            text(canvas, 40, y + 26, ITEMS[index], rgb(226, 238, 233));
            if (index == 1 && model.state) {
                char count[12];
                std::snprintf(count, sizeof(count),
                              Ui::Computer::STORAGE_COUNT_FMT,
                              model.state->storageCount);
                text(canvas, 344 - textWidth(count), y + 26, count,
                     selected ? rgb(248, 210, 105) : rgb(126, 175, 175));
            }
        }
    } else if (model.page == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
        static constexpr int AI_ROW_COUNT = 4;
        int rowCount = model.clawEnabled ? AI_ROW_COUNT : 3;
        const char* labels[AI_ROW_COUNT] = {
            Ui::Amoled::WIFI, Ui::Amoled::ESP_CLAW,
            Ui::Amoled::BACKEND, Ui::BACK,
        };
        const bool values[AI_ROW_COUNT] = {
            model.wifiEnabled, model.clawEnabled, false, false,
        };
        for (int index = 0; index < rowCount; ++index) {
            int y = MENU_CONTENT_TOP + index * COMPUTER_MENU_ROW_HEIGHT;
            bool selected = index == model.pressedItem;
            canvas.fillRoundRect((12), (y + 4), (344), (COMPUTER_MENU_CELL_HEIGHT), (8), selected ? rgb(42, 61, 68)
                                          : rgb(24, 34, 42));
            text(canvas, 40, y + 26, labels[index],
                 index == rowCount - 1 ? rgb(115, 226, 183)
                                       : rgb(226, 238, 233));
            if (index < 2) {
                drawAiToggle(canvas, 284, y + 42, values[index], selected);
            }
        }
#endif
    } else if (model.page == ComputerViewModel::Page::STATUS) {
        uint8_t count = model.state
            ? Game::TeamRoster::memberCount(*model.state) : 0;
        if (count == 0) {
            text(canvas, 104, 204, Ui::Amoled::TEAM_EMPTY,
                 rgb(126, 145, 145));
        }
        for (uint8_t index = 0; index < count && index < 2; ++index) {
            const Game::MonsterRuntime& monster = model.state->team[index];
            const Species* species = findSpecies(monster.speciesId);
            int y = MENU_CONTENT_TOP + index * 156;
            canvas.fillRoundRect((12), (y + 6), (344), (140), (8), rgb(24, 34, 42));
            if (species) {
                const PokemonSprites::SpriteFrame* frame =
                    PokemonSprites::findSpeciesSprite(
                        monster.speciesId, PokemonSprites::SpriteKind::ICON_0);
                if (frame) PokemonSprites::drawFrameScaled(
                    frame, 28, y + 26, 1.24f, false);
                text(canvas, 100, y + 20, species->name, rgb(226, 238, 233));
            }
            char line[28];
            std::snprintf(line, sizeof(line), Ui::Amoled::STATUS_LINE_FMT,
                          monster.level, monster.hpCur, monster.hpMax);
            text(canvas, 100, y + 60, line, rgb(126, 175, 175));
            std::snprintf(line, sizeof(line), Ui::Amoled::HUNGER_FMT,
                          Game::HomeHud::hungerPercent(monster));
            text(canvas, 100, y + 100, line, rgb(248, 210, 105));
        }
    } else if (model.page == ComputerViewModel::Page::STORAGE) {
        uint8_t count = model.state ? model.state->storageCount : 0;
        if (count == 0) {
            text(canvas, 96, 204, Ui::Amoled::STORAGE_EMPTY,
                 rgb(126, 145, 145));
        }
        pageClip.setRect(0, MENU_CONTENT_TOP, AmoledUi::WIDTH,
                         AmoledUi::HEIGHT - MENU_CONTENT_TOP);
        for (uint8_t index = 0; index < count && index < Game::STORAGE_CAP;
             ++index) {
            int y = MENU_CONTENT_TOP + index * CONTACT_ROW_HEIGHT -
                    static_cast<int>(std::lround(model.storageScroll));
            if (y + CONTACT_ROW_HEIGHT <= MENU_CONTENT_TOP || y >= 448) continue;
            const Game::MonsterRuntime* monster = liveContactMonster(model, index);
            const Species* species = monster ? findSpecies(monster->speciesId) : nullptr;
            const bool selected = index == model.pressedItem ||
                ((model.contactActionOpen || model.contactConfirmOpen) &&
                 index == model.contactActionSlot);
            if (selected) {
                canvas.fillRect(0, y, AmoledUi::WIDTH, CONTACT_ROW_HEIGHT,
                                rgb(29, 43, 42));
            }
            int8_t teamSlot = ContactRoster::teamSlotForContact(
                *model.state, index);
            const bool inTeam = teamSlot >= 0;
            const bool visiting = index == model.contactVisitingSlot;
            const bool inviteLocked = monster &&
                Game::Bond::inviteLockedToday(
                    monster->petCountToday,
                    Game::Bond::invitationDay(model.state->gameMinutesTotal));
            if (species) {
                const PokemonSprites::SpriteFrame* frame =
                    PokemonSprites::findSpeciesSprite(
                        monster->speciesId, PokemonSprites::SpriteKind::ICON_0);
                drawContactAvatar(frame, 52, y + 45, 76);
                const char* status = visiting ? Ui::Storage::VISITING_SHORT
                    : inTeam ? Ui::Storage::IN_TEAM
                    : inviteLocked ? Ui::Storage::INVITE_LOCKED_SHORT : nullptr;
                const int statusX = status ? 340 - textWidth(status) : 340;
                pageClip.setRect(104, y + 10,
                                 std::max(0, statusX - 112), 32);
                text(canvas, 104, y + 10, species->name,
                     selected ? rgb(248, 210, 105) : rgb(226, 238, 233));
                pageClip.setRect(0, MENU_CONTENT_TOP, AmoledUi::WIDTH,
                                 AmoledUi::HEIGHT - MENU_CONTENT_TOP);
                text(canvas, 104, y + 48, contactBondLabel(monster->bond),
                     contactBondColor(monster->bond));
                if (status) {
                    text(canvas, statusX, y + 10, status,
                         rgb(126, 175, 175));
                }
                canvas.drawLine(350, y + 37, 357, y + 45,
                                rgb(126, 145, 145));
                canvas.drawLine(357, y + 45, 350, y + 53,
                                rgb(126, 145, 145));
            }
            if (index + 1 < count) canvas.drawFastHLine(
                92, y + CONTACT_ROW_HEIGHT - 1, 276, rgb(48, 62, 68));
        }
        pageClip.reset();
        if ((model.contactActionOpen || model.contactConfirmOpen) &&
            model.contactActionSlot < count) {
            PixelRenderer::fillRectAlpha(0, MENU_CONTENT_TOP,
                AmoledUi::WIDTH, AmoledUi::HEIGHT - MENU_CONTENT_TOP,
                rgb(0, 0, 0), 136);
        }
        if (model.contactActionOpen && !model.contactConfirmOpen &&
            model.contactActionSlot < count) {
            const int actionCount = contactActionCount(model);
            const int top = contactMenuTop(
                actionCount, model.contactActionSlot, model.storageScroll);
            canvas.fillRoundRect(CONTACT_MENU_X, top, CONTACT_MENU_WIDTH,
                                 contactMenuHeight(actionCount), 8,
                                 rgb(24, 34, 42));
            canvas.drawRoundRect(CONTACT_MENU_X, top, CONTACT_MENU_WIDTH,
                                 contactMenuHeight(actionCount), 8,
                                 rgb(67, 97, 101));
            for (int action = 0; action < actionCount; ++action) {
                const int rowTop = top + CONTACT_MENU_PADDING +
                                   action * CONTACT_MENU_ROW_HEIGHT;
                const bool pressed = action == model.contactActionPressedItem;
                if (pressed) {
                    canvas.fillRect(CONTACT_MENU_X + 4, rowTop,
                                    CONTACT_MENU_WIDTH - 8,
                                    CONTACT_MENU_ROW_HEIGHT,
                                    rgb(42, 61, 68));
                }
                const char* label = contactActionLabel(model, action);
                const bool isDelete = model.contactCanDelete &&
                    action == actionCount - 1;
                text(canvas, CONTACT_MENU_X + 28, rowTop + 11, label,
                     isDelete ? rgb(239, 143, 148)
                     : pressed ? rgb(248, 210, 105) : rgb(226, 238, 233));
                if (action + 1 < actionCount) canvas.drawFastHLine(
                    CONTACT_MENU_X + 16, rowTop + CONTACT_MENU_ROW_HEIGHT - 1,
                    CONTACT_MENU_WIDTH - 32, rgb(48, 62, 68));
            }
        }
        if (model.contactConfirmOpen && model.contactActionSlot < count) {
            const Game::MonsterRuntime* monster = liveContactMonster(
                model, model.contactActionSlot);
            const Species* species = monster
                ? findSpecies(monster->speciesId) : nullptr;
            const char* name = species ? species->name : Ui::Amoled::MONSTER;
            char prompt[64];
            std::snprintf(prompt, sizeof(prompt),
                          model.contactConfirmDelete
                              ? Ui::Storage::DELETE_CONFIRM_FMT
                              : Ui::Storage::INVITE_CONFIRM_FMT,
                          name);
            canvas.fillRoundRect(28, CONTACT_CONFIRM_Y,
                                 312, CONTACT_CONFIRM_HEIGHT,
                                 8, rgb(28, 31, 31));
            text(canvas, (AmoledUi::WIDTH - textWidth(prompt)) / 2,
                 CONTACT_CONFIRM_Y + 20, prompt, rgb(226, 238, 233));
            canvas.fillRoundRect(40, CONTACT_CONFIRM_Y + 88, 136, 56,
                                 8, rgb(42, 61, 68));
            canvas.fillRoundRect(192, CONTACT_CONFIRM_Y + 88, 136, 56,
                                 8, rgb(44, 46, 47));
            text(canvas, 76, CONTACT_CONFIRM_Y + 100, Ui::Storage::YES,
                 rgb(115, 226, 183));
            text(canvas, 228, CONTACT_CONFIRM_Y + 100, Ui::Storage::NO,
                 rgb(226, 238, 233));
        }
        pageClip.setRect(0, 0, AmoledUi::WIDTH,
                         UiMetrics::PAGE_HEADER_HEIGHT);
        UiCommon::drawPageHeader(canvas, title);
        char capacity[12];
        std::snprintf(capacity, sizeof(capacity),
                      Ui::Computer::STORAGE_COUNT_FMT, count);
        UiCommon::drawPageHeaderCenteredText(canvas, capacity,
                                              rgb(126, 175, 175));
        pageClip.reset();
#if STICKMON_HAS_CLAW
    } else if (!model.clawLogView) {
        const uint16_t labelColor = rgb(115, 226, 183);
        const uint16_t valueColor = rgb(226, 238, 233);
        const char* ip = model.clawIp && model.clawIp[0]
            ? model.clawIp : "192.168.4.1";
        const bool portalReady = model.clawSsid && model.clawSsid[0] &&
                                 model.clawPassword && model.clawPassword[0];
        const char* ssid = portalReady ? model.clawSsid : "NOT READY";
        const char* password = portalReady ? model.clawPassword : "NOT READY";
        const int contentBottom = portalReady
            ? drawClawQr(canvas, model.clawSsid, model.clawPassword)
            : drawClawQr(canvas, nullptr, nullptr);
        // textWidth() matches the shared font advances (8 per ASCII char,
        // 16 per CJK char), so the URL stays centered regardless of content.
        char address[32];
        std::snprintf(address, sizeof(address), "http://%s", ip);
        const int urlY = contentBottom + 12;
        const int addressX =
            std::max(0, (AmoledUi::WIDTH - textWidth(address)) / 2);
        PixelRenderer::text(canvas, (addressX), (urlY), address, valueColor);
        const int wifiY = urlY + 40;
        const int passY = wifiY + 40;
        PixelRenderer::text(canvas, (8), (wifiY), Ui::Amoled::CLAW_WIFI, labelColor);
        PixelRenderer::text(canvas, ((8 + textWidth(Ui::Amoled::CLAW_WIFI) + 16)), (wifiY), ssid, valueColor);
        PixelRenderer::text(canvas, (8), (passY), Ui::Amoled::CLAW_PASSWORD, labelColor);
        PixelRenderer::text(canvas, ((8 + textWidth(Ui::Amoled::CLAW_PASSWORD) + 16)), (passY), password, valueColor);
    } else {
        // Log view: keep each service status on its own row above the log.
        // The log window starts below all three rows so values cannot overlap.
        const bool staConnected = model.clawStaConnected;
        text(canvas, 8, 52,
             staConnected ? Ui::Amoled::CLAW_WIFI_ON : Ui::Amoled::CLAW_WIFI_OFF,
             staConnected ? rgb(115, 226, 183) : rgb(248, 210, 105));
        text(canvas, 8, 84, Ui::Amoled::CLAW_WECHAT_LABEL,
             rgb(115, 226, 183));
        text(canvas, 104, 84,
             clawWechatPhaseText(model.clawWechatPhase,
                                 model.clawWechatPersisted),
             clawWechatPhaseColor(model.clawWechatPhase,
                                  model.clawWechatPersisted));
        text(canvas, 8, 116, Ui::Amoled::ESP_CLAW,
             rgb(115, 226, 183));
        text(canvas, 168, 116,
             model.clawStarted ? Ui::Amoled::CLAW_AGENT_ON
                               : Ui::Amoled::CLAW_AGENT_OFF,
             model.clawStarted ? rgb(115, 226, 183) : rgb(248, 210, 105));
        canvas.drawRect((8), (CLAW_LOG_TOP), (352), (CLAW_LOG_HEIGHT), rgb(56, 87, 89));
        if (model.clawLogCount == 0 || !model.clawLog) {
            text(canvas, 120, CLAW_LOG_TOP + (CLAW_LOG_HEIGHT - 32) / 2,
                 Ui::Amoled::CLAW_LOG_EMPTY,
                 rgb(126, 145, 145));
        } else {
            pageClip.setRect((10), (CLAW_LOG_TOP + 2), (348), (CLAW_LOG_HEIGHT - 4));
            const int scroll =
                std::max(0, static_cast<int>(std::lround(model.clawLogScroll)));
            const size_t first =
                static_cast<size_t>(scroll / CLAW_LOG_ROW_HEIGHT);
            for (size_t i = first; i < model.clawLogCount; ++i) {
                const int y = CLAW_LOG_TOP + 8 +
                    static_cast<int>(i) * CLAW_LOG_ROW_HEIGHT - scroll;
                if (y >= CLAW_LOG_TOP + CLAW_LOG_HEIGHT) break;
                text(canvas, 20, y, model.clawLog[i].text,
                     clawLogLevelColor(model.clawLog[i].level));
            }
            pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
            const int maxScroll = std::max(
                0, static_cast<int>(model.clawLogCount) * CLAW_LOG_ROW_HEIGHT -
                       CLAW_LOG_VIEWPORT);
            if (model.clawLogScroll > 1.0f) {
                canvas.fillTriangle((328), (CLAW_LOG_TOP + 16), (340), (CLAW_LOG_TOP + 16), (334), (CLAW_LOG_TOP + 8), rgb(126, 175, 175));
            }
            if (model.clawLogScroll < maxScroll - 1.0f) {
                canvas.fillTriangle((328), (422), (340), (422), (334), (430), rgb(126, 175, 175));
            }
        }
#endif
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}
