// Extracted screen rendering and hit testing.
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
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
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
    int row = contentY / 86;
    return row >= 0 && row < storageCount ? row : -1;
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
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    const char* title = model.page == ComputerViewModel::Page::STATUS
        ? Ui::Amoled::STATUS_PAGE
        : model.page == ComputerViewModel::Page::AI_HOSTING
            ? Ui::Amoled::AI_HOSTING
        : model.page == ComputerViewModel::Page::CLAW_SETUP
            ? Ui::Amoled::BACKEND : Ui::COMPUTER;
#if STICKMON_HAS_CLAW
    if (model.page == ComputerViewModel::Page::CLAW_SETUP) {
        UiCommon::drawHeader(canvas, HEADER_HEIGHT, true);
        UiCommon::drawHeaderText(canvas, 72, title, rgb(115, 226, 183), HEADER_HEIGHT);
        drawClawTabs(canvas, model.clawLogView);
    } else {
        UiCommon::drawPageHeader(canvas, title);
    }
#else
    UiCommon::drawPageHeader(canvas, title);
#endif

    if (model.page == ComputerViewModel::Page::MENU) {
        static constexpr const char* ITEMS[] = {
            Ui::SOCIAL,
#if STICKMON_HAS_CLAW
            Ui::Amoled::AI_HOSTING, Ui::BACK,
#else
            Ui::BACK,
#endif
        };
        constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);
        for (int index = 0; index < ITEM_COUNT; ++index) {
            int y = MENU_CONTENT_TOP + index * COMPUTER_MENU_ROW_HEIGHT;
            bool selected = index == model.pressedItem;
            canvas.fillRoundRect((12), (y + 4), (344), (COMPUTER_MENU_CELL_HEIGHT), (8), selected ? rgb(42, 61, 68) : rgb(24, 34, 42));
            text(canvas, 40, y + 26, ITEMS[index],
                 index == ITEM_COUNT - 1 ? rgb(115, 226, 183)
                                         : rgb(226, 238, 233));
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
        for (uint8_t index = 0; index < count && index < Game::STORAGE_CAP;
             ++index) {
            int y = MENU_CONTENT_TOP + index * 86 -
                    static_cast<int>(std::lround(model.storageScroll));
            if (y + 86 <= MENU_CONTENT_TOP || y >= 448) continue;
            const Game::MonsterRuntime& monster = model.state->storage[index];
            const Species* species = findSpecies(monster.speciesId);
            bool selected = index == model.pressedItem;
            if (selected) canvas.fillRoundRect((12), (y + 4), (344), (78), (8), rgb(42, 61, 68));
            if (species) {
                const PokemonSprites::SpriteFrame* frame =
                    PokemonSprites::findSpeciesSprite(
                        monster.speciesId, PokemonSprites::SpriteKind::ICON_0);
                if (frame) PokemonSprites::drawFrameScaled(frame, 28, y + 12,
                                                             1.0f, false);
                text(canvas, 84, y + 14, species->name,
                     selected ? rgb(248, 210, 105) : rgb(226, 238, 233));
            }
            char status[24];
            std::snprintf(status, sizeof(status), Ui::Amoled::STATUS_LINE_FMT,
                          monster.level, monster.hpCur, monster.hpMax);
            text(canvas, 84, y + 50, status, rgb(126, 175, 175));
        }
        int maxScroll = std::max(
            0, static_cast<int>(count) * 86 - (AmoledUi::HEIGHT - MENU_CONTENT_TOP));
        if (model.storageScroll > 1.0f) {
            canvas.fillTriangle((328), (68), (340), (68), (334), (60), rgb(126, 175, 175));
        }
        if (model.storageScroll < maxScroll - 1.0f) {
            canvas.fillTriangle((328), (430), (340), (430), (334), (438), rgb(126, 175, 175));
        }
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
