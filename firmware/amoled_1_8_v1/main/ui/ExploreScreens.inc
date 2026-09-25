// Explore area, route, and explore menu rendering.
int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount) {
    if (y < 0 || y >= EXPLORE_SELECTOR_TOP_HEIGHT) return -1;
    int count = std::min<int>(visibleAreaCount, Game::EXPLORE_AREA_COUNT);
    if (count <= 0) return -1;
    int index = selectedArea + static_cast<int>(std::lround(
        (x - EXPLORE_SELECTOR_CENTER_X) /
        static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
    if (index < 0 || index >= count) return -1;
    int expectedX = EXPLORE_SELECTOR_CENTER_X +
        (index - static_cast<int>(selectedArea)) *
            EXPLORE_SELECTOR_AREA_SPACING;
    return std::abs(x - expectedX) <= 68 ? index : -1;
}

bool exploreStartAt(int x, int y) {
    return EXPLORE_START_BUTTON_NATIVE.contains(x, y);
}

bool exploreSelectionBackAt(int x, int y) {
    return EXPLORE_BACK_BUTTON_NATIVE.contains(x, y);
}

void drawExploreNativeButtonLabel(Canvas565& canvas,
                                  const AmoledUi::Rect& button,
                                  const char* label, uint16_t color) {
    const int labelWidth = textWidth(label);
    PixelRenderer::text(
        canvas, button.x + (button.width - labelWidth) / 2, button.y + 8,
        label, color, 1);
}

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         PixelCache565& backgroundCache,
                         uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    const uint16_t muted = rgb(137, 155, 158);
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    drawExploreBackgroundLayer(canvas, backgroundCache, rowBegin, rowEnd);
    pageClip.reset();

    const int count = std::min<int>(model.visibleAreaCount,
                                    Game::EXPLORE_AREA_COUNT);
    const int centerX = EXPLORE_SELECTOR_CENTER_X;
    const int selectorBottom = EXPLORE_SELECTOR_TOP_HEIGHT;
    const int buttonTop = EXPLORE_SELECTOR_BUTTON_TOP;
    const bool selectedLocked = model.selectedArea > model.unlockedArea;

    if (rowBegin < selectorBottom) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, selectorBottom);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (bottom - top));
        if (count > 0) {
            for (int index = 0; index < count; ++index) {
                float offset = static_cast<float>(index) -
                               model.areaAnimCursor;
                if (std::fabs(offset) > 1.35f) continue;
                int x = centerX + static_cast<int>(std::lround(
                    offset * EXPLORE_SELECTOR_AREA_SPACING));
                bool active = std::fabs(offset) < 0.5f;
                uint16_t color = active ? ink : muted;
                text(canvas, x - textWidth(EXPLORE_AREA_NAMES[index]) / 2,
                     44, EXPLORE_AREA_NAMES[index], color);
                if (active) {
                    canvas.drawFastHLine(
                        (x - 44),
                        (112),
                        (88),
                        ink);
                }
            }
        }
        canvas.drawFastHLine((16),
                             (selectorBottom - 2),
                             (336),
                             rgb(77, 105, 106));
        pageClip.reset();
    }

    if (rowEnd > selectorBottom && rowBegin < buttonTop) {
        int top = std::max<int>(rowBegin, selectorBottom);
        int bottom = std::min<int>(rowEnd, buttonTop);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (bottom - top));
        const int previewCenterX = AmoledUi::WIDTH / 2;
    const char* title = selectedLocked
        ? Ui::Explore::AREA_LOCKED
        : model.previewPool.count > 0 && ExplorePool::poolHasRare(
              model.previewPool)
            ? Ui::Explore::MASS_OUTBREAK
            : Ui::Explore::HABITAT_MONSTERS;
    text(canvas, previewCenterX - textWidth(title) / 2, selectorBottom + 16,
         title, selectedLocked ? rgb(137, 155, 158) : ink);

    pageClip.setRect((0),
                       ((std::max(top, EXPLORE_PREVIEW_TOP))),
                       (AmoledUi::WIDTH),
                       ((std::max(0,
                           std::min(bottom, EXPLORE_PREVIEW_BOTTOM) -
                           std::max(top, EXPLORE_PREVIEW_TOP)))));
    if (selectedLocked) {
        const char* message = Ui::Explore::DEFEAT_PREVIOUS_BOSS;
        text(canvas, previewCenterX - textWidth(message) / 2,
             EXPLORE_PREVIEW_CENTER_Y - 16, message, muted);
    } else if (model.previewPool.count > 0) {
        uint8_t poolCount = model.previewPool.count;
        uint32_t elapsed = Platform::clock().millis() -
                           model.previewStartedAt;
        uint8_t current = static_cast<uint8_t>(
            (elapsed / EXPLORE_PREVIEW_CYCLE_MS + 1) % poolCount);
        uint32_t cycleElapsed = elapsed % EXPLORE_PREVIEW_CYCLE_MS;
        float progress = cycleElapsed <= EXPLORE_PREVIEW_HOLD_MS
            ? 0.0f
            : (cycleElapsed - EXPLORE_PREVIEW_HOLD_MS) /
                  static_cast<float>(EXPLORE_PREVIEW_MOVE_MS);
        progress = std::min(1.0f, progress);
        progress = progress * progress * (3.0f - 2.0f * progress);

        const int previewCenterNativeX =
            (previewCenterX);
        if (poolCount == 1) {
            drawExplorePreviewSlot(canvas, model, current, previewCenterNativeX);
        } else {
            // Keep both edge members until they have fully crossed the clip.
            // All slots share one translation, including at cycle rollover.
            const int translation = static_cast<int>(std::lround(
                progress * EXPLORE_PREVIEW_SLOT_SPACING));
            for (int slot = -1; slot <= 2; ++slot) {
                const int x = previewCenterNativeX +
                    slot * EXPLORE_PREVIEW_SLOT_SPACING - translation;
                if (x + EXPLORE_PREVIEW_NATIVE_MAX_WIDTH / 2 <= 0 ||
                    x - EXPLORE_PREVIEW_NATIVE_MAX_WIDTH / 2 >= AmoledUi::WIDTH) {
                    continue;
                }
                const uint8_t index = static_cast<uint8_t>(
                    (current + poolCount + slot) % poolCount);
                drawExplorePreviewSlot(canvas, model, index, x);
            }
        }
    }
    pageClip.reset();

    }

    if (rowEnd > buttonTop && rowBegin < AmoledUi::HEIGHT) {
        int top = std::max<int>(rowBegin, buttonTop);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (rowEnd - top));
        canvas.drawFastHLine((16),
                             EXPLORE_SELECTOR_DIVIDER_NATIVE_Y,
                             (336),
                             rgb(77, 105, 106));
        const uint16_t startFill = selectedLocked
            ? rgb(78, 91, 96) : rgb(42, 61, 68);
        canvas.fillRoundRect(EXPLORE_START_BUTTON_NATIVE.x,
                             EXPLORE_START_BUTTON_NATIVE.y,
                             EXPLORE_START_BUTTON_NATIVE.width,
                             EXPLORE_START_BUTTON_NATIVE.height, 8, startFill);
        canvas.drawRoundRect(EXPLORE_START_BUTTON_NATIVE.x,
                             EXPLORE_START_BUTTON_NATIVE.y,
                             EXPLORE_START_BUTTON_NATIVE.width,
                             EXPLORE_START_BUTTON_NATIVE.height, 8,
                             selectedLocked ? rgb(78, 91, 96)
                                            : rgb(115, 226, 183));
        canvas.fillRoundRect(EXPLORE_BACK_BUTTON_NATIVE.x,
                             EXPLORE_BACK_BUTTON_NATIVE.y,
                             EXPLORE_BACK_BUTTON_NATIVE.width,
                             EXPLORE_BACK_BUTTON_NATIVE.height, 8,
                             rgb(27, 43, 51));
        canvas.drawRoundRect(EXPLORE_BACK_BUTTON_NATIVE.x,
                             EXPLORE_BACK_BUTTON_NATIVE.y,
                             EXPLORE_BACK_BUTTON_NATIVE.width,
                             EXPLORE_BACK_BUTTON_NATIVE.height, 8,
                             rgb(67, 97, 101));
        drawExploreNativeButtonLabel(
            canvas, EXPLORE_START_BUTTON_NATIVE, Ui::Explore::DEPART,
            selectedLocked ? rgb(126, 145, 145) : rgb(226, 238, 233));
        drawExploreNativeButtonLabel(canvas, EXPLORE_BACK_BUTTON_NATIVE,
                                     Ui::BACK, rgb(226, 238, 233));
        pageClip.reset();
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

bool exploreRouteBackAt(int x, int y) {
    (void)x;
    (void)y;
    return false;
}

bool exploreRouteBagAt(int x, int y) {
    return x >= EXPLORE_ROUTE_BAG_BUTTON_X - 8 &&
           x < EXPLORE_ROUTE_BAG_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8 &&
           y >= HOME_STATUS_TOP && y < AmoledUi::HEIGHT;
}

bool exploreRouteMenuAt(int x, int y) {
    return x >= EXPLORE_ROUTE_MENU_BUTTON_X - 8 &&
           x < EXPLORE_ROUTE_MENU_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8 &&
           y >= HOME_STATUS_TOP && y < AmoledUi::HEIGHT;
}

int exploreRouteExitChoiceAt(int x, int y) {
    if (y < 334 || y >= 408) return -1;
    if (x >= 36 && x < 176) return 0;
    if (x >= 192 && x < 332) return 1;
    return -1;
}

bool exploreRouteMapAt(int x, int y) {
    return x >= 0 && x < AmoledUi::WIDTH &&
           y >= EXPLORE_ROUTE_MAP_TOP && y < EXPLORE_ROUTE_MAP_BOTTOM;
}

void drawExploreRouteHud(Canvas565& canvas,
                         const ExploreRouteViewModel& model) {
    const uint16_t panelColor = rgb(8, 17, 22);
    const char* name = EXPLORE_AREA_NAMES[
        model.area < Game::EXPLORE_AREA_COUNT ? model.area : 0];
    const int namePanelWidth = std::min(
        AmoledUi::WIDTH - EXPLORE_ROUTE_HUD_X * 2,
        textWidth(name) + 14);
    fillRoundRectAlpha(canvas, EXPLORE_ROUTE_HUD_X, EXPLORE_ROUTE_HUD_Y,
                       namePanelWidth, EXPLORE_ROUTE_HUD_HEIGHT, 14,
                       panelColor, 168);
    canvas.drawRoundRect(
        (EXPLORE_ROUTE_HUD_X),
        (EXPLORE_ROUTE_HUD_Y),
        (namePanelWidth),
        (EXPLORE_ROUTE_HUD_HEIGHT),
        (14), rgb(87, 121, 119));
    text(canvas, EXPLORE_ROUTE_HUD_X + 14, EXPLORE_ROUTE_HUD_Y + 6,
         name, rgb(226, 238, 233));
}

void drawExploreRouteBottomHud(Canvas565& canvas,
                               const ExploreRouteViewModel& model) {
    canvas.fillRect(
        (0),
        (HOME_STATUS_TOP),
        (AmoledUi::WIDTH),
        (AmoledUi::HEIGHT - HOME_STATUS_TOP),
        rgb(18, 27, 35));
    canvas.drawFastHLine(
        (0),
        (HOME_STATUS_TOP),
        (AmoledUi::WIDTH), rgb(71, 108, 108));
    drawExploreBagIcon(canvas);
    drawExploreMenuIcon(canvas);

    canvas.fillRoundRect(
        (HOME_MONSTER_PANEL_X),
        (352),
        (HOME_MONSTER_PANEL_W),
        (88), (8),
        rgb(24, 34, 42));
    canvas.drawRoundRect(
        (HOME_MONSTER_PANEL_X),
        (352),
        (HOME_MONSTER_PANEL_W),
        (88), (8),
        rgb(72, 83, 98));

    uint8_t count = model.state
        ? std::min<uint8_t>(model.state->teamCount, 2) : 0;
    for (uint8_t index = 0; index < count; ++index) {
        const Game::MonsterRuntime& monster = model.state->team[index];
        uint8_t percent = monster.hpMax == 0 ? 0
            : static_cast<uint8_t>(std::min<uint32_t>(
                100, static_cast<uint32_t>(monster.hpCur) * 100 /
                    monster.hpMax));
        const int rowY = 354 + index * 42;
        drawHomeHpBar(canvas, HOME_MONSTER_PANEL_X + 14, rowY + 14,
                      98, percent);
        GameAssets::Kind status = GameAssets::statusKind(monster.majorStatus);
        if (status != GameAssets::Kind::COUNT) {
            GameAssets::draw(status, HOME_MONSTER_PANEL_X + 126, rowY + 2, 2.0f);
        }
    }
}

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& mapCache,
                              uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    if (rowBegin < EXPLORE_ROUTE_MAP_BOTTOM &&
        rowEnd > EXPLORE_ROUTE_MAP_TOP) {
        int top = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
        int bottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        if (model.map) {
            drawExploreRouteMapLayer(canvas, model, mapCache, top, bottom);
            drawExploreRoutePickup(canvas, model);
            bool bossBehindPlayer = exploreRouteBossBehindPlayer(model);
            if (bossBehindPlayer) drawExploreRouteBoss(canvas, model);
            if (model.companionVisible &&
                model.companionWorldY <= model.worldY) {
                drawExploreRouteCompanion(canvas, model);
            }
            drawExploreRoutePet(canvas, model);
            if (model.companionVisible &&
                model.companionWorldY > model.worldY) {
                drawExploreRouteCompanion(canvas, model);
            }
            if (!bossBehindPlayer) drawExploreRouteBoss(canvas, model);
            drawExploreRouteMapForegroundLayer(canvas, model, top, bottom);
        } else {
            canvas.fillRect(
                (0),
                (EXPLORE_ROUTE_MAP_TOP),
                (AmoledUi::WIDTH),
                ((EXPLORE_ROUTE_MAP_BOTTOM -
                                       EXPLORE_ROUTE_MAP_TOP)),
                ExploreAreaCatalog::fieldColor(model.area));
        }
        pageClip.reset();
    }

    if (model.complete && rowBegin < 268 && rowEnd > 136) {
        int top = std::max<int>(rowBegin, 136);
        int bottom = std::min<int>(rowEnd, 268);
        pageClip.setRect(
            (0), (top),
            (AmoledUi::WIDTH),
            (bottom - top));
        canvas.fillRoundRect(
            (40), (136),
            (288), (132),
            (12), rgb(17, 27, 34));
        canvas.drawRoundRect(
            (40), (136),
            (288), (132),
            (12), rgb(82, 117, 117));
        text(canvas,
             (AmoledUi::WIDTH -
              textWidth(Ui::Explore::RESULT_END)) / 2,
             160, Ui::Explore::RESULT_END, rgb(248, 210, 105));
        text(canvas,
             (AmoledUi::WIDTH -
              textWidth(Ui::Explore::ANY_KEY_RETURN)) / 2,
             212, Ui::Explore::ANY_KEY_RETURN, rgb(226, 238, 233));
        pageClip.reset();
    }

    if (rowBegin < EXPLORE_ROUTE_HUD_Y + EXPLORE_ROUTE_HUD_HEIGHT &&
        rowEnd > EXPLORE_ROUTE_HUD_Y) {
        int top = std::max<int>(rowBegin, EXPLORE_ROUTE_HUD_Y);
        int bottom = std::min<int>(
            rowEnd, EXPLORE_ROUTE_HUD_Y + EXPLORE_ROUTE_HUD_HEIGHT);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        drawExploreRouteHud(canvas, model);
        pageClip.reset();
    }

    if (rowEnd > HOME_STATUS_TOP) {
        int top = std::max<int>(rowBegin, HOME_STATUS_TOP);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (rowEnd - top));
        drawExploreRouteBottomHud(canvas, model);
        pageClip.reset();
    }

    if (model.exitConfirm &&
        rowBegin < 428 && rowEnd > 290) {
        int top = std::max<int>(rowBegin, 290);
        int bottom = std::min<int>(rowEnd, 428);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        if (model.exitConfirm) {
            canvas.fillRoundRect((20), (290), (328), (138), (12), rgb(17, 27, 34));
            canvas.drawRoundRect((20), (290), (328), (138), (12), rgb(82, 117, 117));
            text(canvas, 98, 308, Ui::Amoled::LEAVE_ROUTE,
                 rgb(226, 238, 233));
            canvas.fillRoundRect((36), (334), (140), (74), (8), rgb(36, 54, 61));
            canvas.fillRoundRect((192), (334), (140), (74), (8), rgb(91, 49, 55));
            text(canvas, 72, 364, Ui::Amoled::STAY, rgb(115, 226, 183));
            text(canvas, 234, 364, Ui::Amoled::EXIT, rgb(239, 143, 148));
        }
        pageClip.reset();
    }

    if (model.toast && rowBegin < UiCommon::TOAST_BOTTOM &&
        rowEnd > UiCommon::TOAST_TOP) {
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
        drawToast(canvas, model.toast);
        pageClip.reset();
    }
    drawSceneFadeOverlay(canvas, model.fadeAlpha, rowBegin, rowEnd);
}

bool exploreRouteMenuBackAt(int x, int y) {
    return x >= 0 && x < 60 && y >= 0 && y < HEADER_HEIGHT;
}

int exploreRouteMenuItemAt(int x, int y) {
    const int panelX = AmoledUi::WIDTH - EXPLORE_MENU_PANEL_WIDTH;
    if (x < panelX || x >= AmoledUi::WIDTH ||
        y < EXPLORE_MENU_PANEL_TOP ||
        y >= EXPLORE_MENU_PANEL_TOP + EXPLORE_MENU_PANEL_HEIGHT) {
        return -1;
    }
    int index = (y - EXPLORE_MENU_PANEL_TOP) /
                EXPLORE_MENU_PANEL_ROW_HEIGHT;
    return index < AppSceneFlow::exploreMenuItemCount() ? index : -1;
}

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    const int panelX = AmoledUi::WIDTH - EXPLORE_MENU_PANEL_WIDTH;
    pageClip.setRect((panelX), (rowBegin), (EXPLORE_MENU_PANEL_WIDTH), (rowEnd - rowBegin));
    canvas.fillRoundRect(panelX, EXPLORE_MENU_PANEL_TOP,
                         EXPLORE_MENU_PANEL_WIDTH, EXPLORE_MENU_PANEL_HEIGHT,
                         8, rgb(20, 25, 32));
    canvas.drawRoundRect(panelX, EXPLORE_MENU_PANEL_TOP,
                         EXPLORE_MENU_PANEL_WIDTH, EXPLORE_MENU_PANEL_HEIGHT,
                         8, rgb(190, 200, 205));

    for (uint8_t index = 0; index < AppSceneFlow::exploreMenuItemCount();
         ++index) {
        int y = EXPLORE_MENU_PANEL_TOP +
                index * EXPLORE_MENU_PANEL_ROW_HEIGHT;
        if (y + EXPLORE_MENU_PANEL_ROW_HEIGHT <= rowBegin || y >= rowEnd) {
            continue;
        }
        bool selected = index == model.cursor;
        bool pressed = index == model.pressedItem;
        if (pressed) {
            canvas.fillRoundRect(panelX + 4, y + 4,
                                 EXPLORE_MENU_PANEL_WIDTH - 8,
                                 EXPLORE_MENU_PANEL_ROW_HEIGHT - 8,
                                 4, rgb(33, 43, 48));
        }
        if (selected) {
            canvas.fillRect(panelX + 10,
                            y + (EXPLORE_MENU_PANEL_ROW_HEIGHT - 36) / 2,
                            6, 36, rgb(255, 216, 72));
        }
        uint16_t color = selected ? rgb(255, 216, 72)
                                  : rgb(235, 239, 232);
        text(canvas, panelX + 26, y + EXPLORE_MENU_TEXT_INSET,
             Ui::Explore::SIDE_MENU_ITEMS[index], color);
    }
    pageClip.reset();
    if (model.toast && rowBegin < UiCommon::TOAST_BOTTOM &&
        rowEnd > UiCommon::TOAST_TOP) {
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
        drawToast(canvas, model.toast);
        pageClip.reset();
    }
}

namespace {
