// Extracted screen rendering and hit testing.
bool itemListBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

namespace {

int bagGridRows(uint8_t itemCount) {
    return (static_cast<int>(itemCount) + 2) / 3;
}

int bagExploreSectionTop(uint8_t dailyItemCount) {
    return SHOP_SECTION_HEADER_HEIGHT +
           bagGridRows(dailyItemCount) * ITEM_ROW_HEIGHT +
           SHOP_SECTION_GAP;
}

int bagGridContentHeight(uint8_t dailyItemCount, uint8_t exploreItemCount,
                         bool exploreOnly) {
    if (exploreOnly) {
        return SHOP_SECTION_HEADER_HEIGHT +
               bagGridRows(exploreItemCount) * ITEM_ROW_HEIGHT;
    }
    return bagExploreSectionTop(dailyItemCount) +
           SHOP_SECTION_HEADER_HEIGHT +
           bagGridRows(exploreItemCount) * ITEM_ROW_HEIGHT;
}

bool bagGridItemCenter(int index, float scroll, uint8_t dailyItemCount,
                       uint8_t exploreItemCount, bool exploreOnly,
                       int& centerX, int& centerY) {
    const int itemCount = exploreOnly
        ? static_cast<int>(exploreItemCount)
        : static_cast<int>(dailyItemCount) +
              static_cast<int>(exploreItemCount);
    if (index < 0 || index >= itemCount) return false;

    int sectionIndex = index;
    int sectionTop = SHOP_SECTION_HEADER_HEIGHT;
    if (!exploreOnly && index >= dailyItemCount) {
        sectionIndex -= dailyItemCount;
        sectionTop = bagExploreSectionTop(dailyItemCount) +
                     SHOP_SECTION_HEADER_HEIGHT;
    }
    const int column = sectionIndex % 3;
    const int row = sectionIndex / 3;
    const int cardX = 12 + column * 112;
    centerX = cardX + 52;
    centerY = MENU_CONTENT_TOP + sectionTop + row * ITEM_ROW_HEIGHT +
              ITEM_ROW_HEIGHT / 2 -
              static_cast<int>(std::lround(scroll));
    return true;
}

}  // namespace

float itemListMaxScroll(uint8_t dailyItemCount, uint8_t exploreItemCount,
                        uint8_t itemCount, bool exploreOnly) {
    (void)itemCount;
    int contentHeight = bagGridContentHeight(dailyItemCount,
                                              exploreItemCount,
                                              exploreOnly);
    return static_cast<float>(
        std::max(0, contentHeight - (AmoledUi::HEIGHT - MENU_CONTENT_TOP)));
}

namespace {

int shopGridRows(uint8_t itemCount) {
    return (static_cast<int>(itemCount) + 1) / 2;
}

int shopExploreSectionTop(uint8_t dailyItemCount) {
    return SHOP_SECTION_HEADER_HEIGHT +
           shopGridRows(dailyItemCount) * SHOP_GRID_ROW_HEIGHT +
           SHOP_SECTION_GAP;
}

int shopGridContentHeight(ShopViewModel::Mode mode,
                          uint8_t dailyItemCount,
                          uint8_t exploreItemCount,
                          uint8_t itemCount) {
    if (mode == ShopViewModel::Mode::SELL) {
        return SHOP_SECTION_HEADER_HEIGHT +
               shopGridRows(itemCount) * SHOP_GRID_ROW_HEIGHT +
               SHOP_SECTION_GAP;
    }
    return shopExploreSectionTop(dailyItemCount) +
           SHOP_SECTION_HEADER_HEIGHT +
           shopGridRows(exploreItemCount) * SHOP_GRID_ROW_HEIGHT +
           SHOP_SECTION_GAP;
}

bool shopGridItemCenter(int index, float scroll, ShopViewModel::Mode mode,
                        uint8_t dailyItemCount, uint8_t itemCount,
                        int& centerX, int& centerY) {
    (void)itemCount;
    if (index < 0) return false;
    int sectionIndex = index;
    int sectionTop = SHOP_SECTION_HEADER_HEIGHT;
    if (mode == ShopViewModel::Mode::BUY && index >= dailyItemCount) {
        sectionIndex -= dailyItemCount;
        sectionTop = shopExploreSectionTop(dailyItemCount) +
                     SHOP_SECTION_HEADER_HEIGHT;
    }
    int column = sectionIndex % 2;
    int row = sectionIndex / 2;
    centerX = SHOP_GRID_LEFT + column * SHOP_GRID_COLUMN_WIDTH +
              SHOP_GRID_COLUMN_WIDTH / 2;
    centerY = MENU_CONTENT_TOP + sectionTop +
              row * SHOP_GRID_ROW_HEIGHT + SHOP_GRID_ROW_HEIGHT / 2 -
              static_cast<int>(std::lround(scroll));
    return true;
}

}  // namespace

int shopMenuItemAt(int x, int y) {
    if (x < 0 || x >= SHOP_RAIL_DIVIDER_X) return -1;
    for (int index = 0; index < 2; ++index) {
        int top = 88 + index * 108;
        if (y >= top && y < top + 84) return index;
    }
    return -1;
}

int shopGridItemAt(int x, int y, float scroll,
                   ShopViewModel::Mode mode, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount) {
    (void)exploreItemCount;
    if (x < SHOP_GRID_LEFT || x >= 360 ||
        y < MENU_CONTENT_TOP || y >= 448) {
        return -1;
    }
    for (uint8_t index = 0; index < itemCount; ++index) {
        int centerX = 0;
        int centerY = 0;
        if (!shopGridItemCenter(index, scroll, mode, dailyItemCount,
                                itemCount, centerX, centerY)) {
            continue;
        }
        if (std::abs(x - centerX) <= SHOP_GRID_COLUMN_WIDTH / 2 - 4 &&
            std::abs(y - centerY) <= SHOP_GRID_ROW_HEIGHT / 2 - 4) {
            return index;
        }
    }
    return -1;
}

float shopGridMaxScroll(ShopViewModel::Mode mode, uint8_t dailyItemCount,
                        uint8_t exploreItemCount, uint8_t itemCount) {
    int contentHeight = shopGridContentHeight(
        mode, dailyItemCount, exploreItemCount, itemCount);
    return static_cast<float>(
        std::max(0, contentHeight - (AmoledUi::HEIGHT - MENU_CONTENT_TOP)));
}

int itemListItemAt(int x, int y, float scroll, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount,
                   bool exploreOnly) {
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    for (uint8_t index = 0; index < itemCount; ++index) {
        int centerX = 0;
        int centerY = 0;
        if (!bagGridItemCenter(index, scroll, dailyItemCount,
                               exploreItemCount, exploreOnly,
                               centerX, centerY)) {
            continue;
        }
        if (std::abs(x - centerX) <= 52 &&
            std::abs(y - centerY) <= ITEM_ROW_HEIGHT / 2 - 4) {
            return index;
        }
    }
    return -1;
}

int itemConfirmChoiceAt(int x, int y) {
    if (SHOP_DETAIL_ACTION_RECT.contains(x, y)) return 0;
    if (SHOP_DETAIL_BACK_RECT.contains(x, y)) return 1;
    return -1;
}

namespace {

Game::ItemId itemForRow(const ItemListViewModel& model, uint8_t index) {
    if (!model.state) return Game::ItemId::COUNT;
    switch (model.mode) {
    case ItemListMode::BAG:
        if (model.exploreOnly) {
            return Game::ItemInventory::homeBagExploreItemAt(
                *model.state, index);
        }
        return Game::ItemInventory::homeBagItemAt(*model.state, index);
    case ItemListMode::BUY:
        return Game::ShopService::buyItemAt(
            model.category, *model.state, index);
    case ItemListMode::SELL:
        return Game::ShopService::sellItemAt(*model.state, index);
    }
    return Game::ItemId::COUNT;
}

void drawItemConfirm(Canvas565& canvas, const ItemListViewModel& model) {
    if (!model.confirmOpen || model.pendingItem == Game::ItemId::COUNT) return;
    if (model.mode != ItemListMode::BAG) return;
    GameAssets::drawCentered(GameAssets::itemKind(model.pendingItem), 80, 144, 1.5f);
    const char* name = Game::ShopService::shortName(model.pendingItem);
    text(canvas, 168, 84, name, rgb(226, 238, 233));
    char owned[16]; std::snprintf(owned, sizeof(owned), "X%u",
        Game::ItemInventory::count(*model.state, model.pendingItem));
    text(canvas, 168, 136, owned, rgb(239, 196, 154));
    text(canvas, 24, 248, Game::ShopService::shortDescription(model.pendingItem),
         rgb(126, 175, 175));
    const bool usable = Game::ItemInventory::usableFromHomeBag(
        model.pendingItem) &&
        (!model.exploreOnly ||
         Game::ItemInventory::usableInBattle(model.pendingItem));
    if (model.mode == ItemListMode::BAG && !usable) {
        canvas.fillRoundRect(SHOP_DETAIL_BACK_RECT.x,
                             SHOP_DETAIL_BACK_RECT.y,
                             SHOP_DETAIL_BACK_RECT.width,
                             SHOP_DETAIL_BACK_RECT.height, 8, rgb(91, 49, 55));
        PixelRenderer::text(canvas, SHOP_DETAIL_BACK_RECT.x + 42,
                            SHOP_DETAIL_BACK_RECT.y + 18, Ui::BACK,
                            rgb(239, 143, 148));
        return;
    }
    canvas.fillRoundRect(SHOP_DETAIL_ACTION_RECT.x,
                         SHOP_DETAIL_ACTION_RECT.y,
                         SHOP_DETAIL_ACTION_RECT.width,
                         SHOP_DETAIL_ACTION_RECT.height, 8, rgb(36, 54, 61));
        canvas.fillRoundRect(SHOP_DETAIL_BACK_RECT.x,
                         SHOP_DETAIL_BACK_RECT.y,
                         SHOP_DETAIL_BACK_RECT.width,
                         SHOP_DETAIL_BACK_RECT.height, 8, rgb(91, 49, 55));
    PixelRenderer::text(
        canvas,
        SHOP_DETAIL_ACTION_RECT.x +
            (SHOP_DETAIL_ACTION_RECT.width -
             textWidth(Ui::Amoled::YES)) / 2,
        SHOP_DETAIL_ACTION_RECT.y +
            (SHOP_DETAIL_ACTION_RECT.height - FontResource::LARGE_GLYPH_H) / 2,
        Ui::Amoled::USE,
        rgb(115, 226, 183));
    PixelRenderer::text(
        canvas,
        SHOP_DETAIL_BACK_RECT.x +
            (SHOP_DETAIL_BACK_RECT.width -
             textWidth(Ui::BACK)) / 2,
        SHOP_DETAIL_BACK_RECT.y +
            (SHOP_DETAIL_BACK_RECT.height - FontResource::LARGE_GLYPH_H) / 2,
        Ui::BACK, rgb(239, 143, 148));
}

}  // namespace

void renderItemListScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (bottom - rowBegin));
        canvas.fillRect((0), (0), (AmoledUi::WIDTH), (MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);
        if (model.mode != ItemListMode::BAG) {
            char coins[16];
            std::snprintf(coins, sizeof(coins), "C%lu",
                          static_cast<unsigned long>(model.coins));
            UiCommon::drawPageHeader(canvas, Ui::SHOP, coins);
        } else {
            UiCommon::drawPageHeader(canvas, Ui::BAG);
        }
        pageClip.reset();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    pageClip.setRect((0), (contentTop), (AmoledUi::WIDTH), (rowEnd - contentTop));
    canvas.fillRect((0), (MENU_CONTENT_TOP), (AmoledUi::WIDTH), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);

    // Details replace the inventory grid; clear the content area first so the
    // underlying cards cannot show through.
    if (model.confirmOpen && model.pendingItem != Game::ItemId::COUNT) {
        canvas.fillRect((0), (MENU_CONTENT_TOP), (AmoledUi::WIDTH),
                        (AmoledUi::HEIGHT - MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);
    }

    if (model.itemCount == 0) {
        text(canvas, 100, 210,
             model.mode == ItemListMode::SELL ? Ui::Amoled::NOTHING_TO_SELL
                                               : Ui::Amoled::NOTHING,
             rgb(126, 145, 145));
    }
    if (!model.confirmOpen && model.mode == ItemListMode::BAG) {
        const int scrollPixels = static_cast<int>(std::lround(model.scroll));
        auto drawSectionHeader = [&](int localY, const char* label) {
            int y = MENU_CONTENT_TOP + localY - scrollPixels;
            if (y + SHOP_SECTION_HEADER_HEIGHT <= MENU_CONTENT_TOP ||
                y >= 448) {
                return;
            }
            canvas.fillRect((12), (y), (344),
                            (SHOP_SECTION_HEADER_HEIGHT), rgb(20, 31, 38));
            text(canvas, 20, y + 6, label, rgb(126, 175, 175));
            canvas.drawFastHLine((20),
                                 (y + SHOP_SECTION_HEADER_HEIGHT - 2),
                                 (328), rgb(56, 87, 89));
        };
        if (model.exploreOnly) {
            drawSectionHeader(0, Ui::Shop::CATEGORY_EXPLORE);
        } else {
            drawSectionHeader(0, Ui::Shop::CATEGORY_DAILY);
            drawSectionHeader(bagExploreSectionTop(model.dailyItemCount),
                              Ui::Shop::CATEGORY_EXPLORE);
        }
    }

    if (!model.confirmOpen) for (uint8_t index = 0; index < model.itemCount; ++index) {
        int gridRow = 0;
        int gridCol = 0;
        int itemCenterX = 0;
        int computedCenterY = 0;
        int y = 0;
        if (model.mode == ItemListMode::BAG) {
            if (!bagGridItemCenter(index, model.scroll, model.dailyItemCount,
                                   model.exploreItemCount, model.exploreOnly,
                                   itemCenterX, computedCenterY)) {
                continue;
            }
            y = computedCenterY - ITEM_ROW_HEIGHT / 2;
            gridCol = (itemCenterX - 64) / 112;
        } else {
            gridRow = index;
            y = MENU_CONTENT_TOP + gridRow * ITEM_ROW_HEIGHT -
                static_cast<int>(std::lround(model.scroll));
        }
        if (y + ITEM_ROW_HEIGHT <= MENU_CONTENT_TOP || y >= 448) continue;
        Game::ItemId item = itemForRow(model, index);
        if (item == Game::ItemId::COUNT) continue;
        int cardX = model.mode == ItemListMode::BAG ? 12 + gridCol * 112 : 12;
        int cardW = model.mode == ItemListMode::BAG ? 104 : 344;
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect(cardX, y + 4, cardW,
                             ITEM_ROW_HEIGHT - 8, 8, background);
        const float itemIconScale = model.mode == ItemListMode::BAG
            ? BAG_ITEM_ICON_SCALE : 1.15f;
        const int itemCenterY = model.mode == ItemListMode::BAG
            ? y + ITEM_ROW_HEIGHT / 2 : y + 38;
        if (!GameAssets::drawCentered(GameAssets::itemKind(item),
                                      cardX + cardW / 2, itemCenterY,
                                      itemIconScale)) {
                        text(canvas, cardX + cardW / 2 - 6, y + 32, "?", rgb(248, 210, 105));
        }
        char owned[8];
        std::snprintf(owned, sizeof(owned), "X%u",
                      Game::ItemInventory::count(*model.state, item));
        if (model.mode != ItemListMode::BAG) {
            text(canvas, cardX + 8, y + 70, owned, rgb(248, 210, 105));
        }
        const char* name = Game::ShopService::shortName(item);
        if (model.mode != ItemListMode::BAG) {
            text(canvas, 96, y + 18, name, rgb(226, 238, 233));
        }
        if (model.mode != ItemListMode::BAG) {
            text(canvas, 96, y + 54,
                 Game::ShopService::shortDescription(item),
                 rgb(126, 175, 175));
        }
        if (model.mode != ItemListMode::BAG) {
            uint16_t price = model.mode == ItemListMode::SELL
                ? Game::ShopService::sellPrice(item)
                : Game::ShopService::buyPrice(item);
            char priceText[10];
            std::snprintf(priceText, sizeof(priceText), "C%u", price);
            int priceX = 344 - textWidth(priceText);
            canvas.fillRect((priceX - 4), (y + 10), ((textWidth(priceText) + 8)), (26), background);
            text(canvas, priceX, y + 16, priceText, rgb(248, 210, 105));
        }
    }
    drawToast(canvas, model.toast);
    drawItemConfirm(canvas, model);
    pageClip.reset();
}

namespace {

Game::ItemId shopItemForIndex(const ShopViewModel& model, uint8_t index) {
    if (!model.state || index >= model.itemCount) return Game::ItemId::COUNT;
    if (model.mode == ShopViewModel::Mode::SELL) {
        return Game::ShopService::sellItemAt(*model.state, index);
    }
    if (index < model.dailyItemCount) {
        return Game::ShopService::buyItemAt(
            Game::ShopService::Category::DAILY, *model.state, index);
    }
    return Game::ShopService::buyItemAt(
        Game::ShopService::Category::EXPLORE, *model.state,
        static_cast<uint8_t>(index - model.dailyItemCount));
}

uint16_t shopFadeColor(uint16_t foreground, uint8_t alpha,
                       uint16_t background = 0) {
    uint8_t inverse = static_cast<uint8_t>(255 - alpha);
    uint8_t red = static_cast<uint8_t>(
        ((((foreground >> 11) & 0x1F) * alpha) +
         (((background >> 11) & 0x1F) * inverse)) / 255);
    uint8_t green = static_cast<uint8_t>(
        ((((foreground >> 5) & 0x3F) * alpha) +
         (((background >> 5) & 0x3F) * inverse)) / 255);
    uint8_t blue = static_cast<uint8_t>(
        (((foreground & 0x1F) * alpha) +
         ((background & 0x1F) * inverse)) / 255);
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

}  // namespace

void renderShopScreen(Canvas565& canvas, const ShopViewModel& model,
                      uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (bottom - rowBegin));
        canvas.fillRect((0), (0), (AmoledUi::WIDTH), (MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);
        char coins[16];
        std::snprintf(coins, sizeof(coins), "C%lu",
                      static_cast<unsigned long>(model.coins));
        UiCommon::drawPageHeader(canvas, Ui::SHOP);
        UiCommon::drawPageHeaderCenteredText(canvas, coins,
                                             rgb(248, 210, 105));
        pageClip.reset();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    pageClip.setRect((0), (contentTop), (AmoledUi::WIDTH), (rowEnd - contentTop));
    canvas.fillRect((0), (MENU_CONTENT_TOP), (AmoledUi::WIDTH), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);

    const bool detailOpen = model.detailItem != Game::ItemId::COUNT;
    const bool showRail = !detailOpen;
    if (showRail) {
        canvas.fillRect((0), (MENU_CONTENT_TOP), (SHOP_RAIL_DIVIDER_X), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), rgb(17, 24, 31));
        canvas.drawFastVLine((SHOP_RAIL_DIVIDER_X), (MENU_CONTENT_TOP), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), rgb(67, 74, 84));

        static constexpr const char* MENU_LABELS[] = {
            Ui::Amoled::BUY, Ui::Amoled::SELL,
        };
        int selectedMenu = model.mode == ShopViewModel::Mode::BUY ? 0 : 1;
        for (int index = 0; index < 2; ++index) {
            int x = 10;
            int y = 88 + index * 108;
            bool selected = index == selectedMenu;
            if (selected) {
                canvas.fillRect((x), (y + 40), (6), (16), rgb(248, 210, 105));
            }
            const char* label = MENU_LABELS[index];
            text(canvas, x + (92 - textWidth(label)) / 2, y + 32, label,
                 selected ? rgb(248, 210, 105) : rgb(194, 210, 207));
        }
    }

    if (!detailOpen) {
        int scrollPixels = static_cast<int>(std::lround(model.scroll));
        auto drawSectionHeader = [&](int localY, const char* label) {
        int y = MENU_CONTENT_TOP + localY - scrollPixels;
        if (y + SHOP_SECTION_HEADER_HEIGHT <= MENU_CONTENT_TOP || y >= 448) {
            return;
        }
        canvas.fillRect((SHOP_GRID_LEFT), (y), (232), (SHOP_SECTION_HEADER_HEIGHT), rgb(20, 31, 38));
        text(canvas, SHOP_GRID_LEFT + 8, y + 6, label,
             rgb(126, 175, 175));
        canvas.drawFastHLine((SHOP_GRID_LEFT + 8), (y + SHOP_SECTION_HEADER_HEIGHT - 2), (216), rgb(56, 87, 89));
        };
        if (model.mode == ShopViewModel::Mode::BUY) {
            drawSectionHeader(0, Ui::Shop::CATEGORY_DAILY);
            drawSectionHeader(shopExploreSectionTop(model.dailyItemCount),
                              Ui::Shop::CATEGORY_EXPLORE);
        } else {
            drawSectionHeader(0, Ui::BAG);
        }

        for (uint8_t index = 0; index < model.itemCount; ++index) {
        int centerX = 0;
        int centerY = 0;
        if (!shopGridItemCenter(index, model.scroll, model.mode,
                                model.dailyItemCount, model.itemCount,
                                centerX, centerY) ||
            centerY + SHOP_GRID_ROW_HEIGHT / 2 < MENU_CONTENT_TOP ||
            centerY - SHOP_GRID_ROW_HEIGHT / 2 >= 448) {
            continue;
        }
        bool pressed = index == model.pressedItem;
        canvas.fillRoundRect((centerX - 54), (centerY - 46), (108), (92), (8), pressed ? rgb(42, 61, 68) : rgb(24, 34, 42));
        Game::ItemId item = shopItemForIndex(model, index);
        if (!drawShopItemCentered(GameAssets::itemKind(item), centerX, centerY,
                                  SHOP_GRID_ICON_SCALE, 255)) {
            text(canvas, centerX - 6, centerY - 14, "?",
                 rgb(248, 210, 105));
        }
        }

        if (model.itemCount == 0) {
            const char* empty = model.mode == ShopViewModel::Mode::SELL
                ? Ui::Amoled::NOTHING_TO_SELL : Ui::Amoled::NOTHING;
            text(canvas, 134, 216, empty, rgb(126, 145, 145));
        }
    }

    if (detailOpen) {
        int originX = 80;
        int originY = 144;
        int iconX = originX;
        int iconY = originY;
        float iconScale = SHOP_DETAIL_ICON_END_SCALE;
        if (!drawShopItemCentered(GameAssets::itemKind(model.detailItem),
                                  iconX, iconY, iconScale, 255)) {
            text(canvas, iconX - 6, iconY - 10, "?", rgb(248, 210, 105));
        }

        constexpr uint8_t detailAlpha = 255;
        const uint16_t detailBackground = UiMetrics::PAGE_BACKGROUND;
        const char* name = Game::ShopService::shortName(model.detailItem);
        text(canvas, 168, 84, name,
             shopFadeColor(rgb(226, 238, 233), detailAlpha,
                           detailBackground));
        char owned[16];
        std::snprintf(owned, sizeof(owned), Ui::Shop::OWNED_FMT,
                      model.state ? Game::ItemInventory::count(
                                        *model.state, model.detailItem) : 0);
        text(canvas, 168, 136, owned,
             shopFadeColor(rgb(239, 196, 154), detailAlpha,
                           detailBackground));
        char price[20];
        std::snprintf(price, sizeof(price),
                      model.mode == ShopViewModel::Mode::SELL
                          ? Ui::Shop::SELL_PRICE_FMT : Ui::Shop::PRICE_FMT,
                      model.mode == ShopViewModel::Mode::SELL
                          ? Game::ShopService::sellPrice(model.detailItem)
                          : Game::ShopService::buyPrice(model.detailItem));
        text(canvas, 168, 188, price,
             shopFadeColor(rgb(248, 210, 105), detailAlpha,
                           detailBackground));
        text(canvas, 24, 248,
             Game::ShopService::shortDescription(model.detailItem),
             shopFadeColor(rgb(126, 175, 175), detailAlpha,
                           detailBackground));

        uint16_t actionFill = shopFadeColor(
            model.pressedDetailAction == 0 ? rgb(48, 74, 68)
                                           : rgb(36, 54, 61),
            detailAlpha, detailBackground);
        uint16_t backFill = shopFadeColor(
            model.pressedDetailAction == 1 ? rgb(91, 49, 55)
                                           : rgb(46, 37, 44),
            detailAlpha, detailBackground);
        canvas.fillRoundRect(SHOP_DETAIL_ACTION_RECT.x,
                             SHOP_DETAIL_ACTION_RECT.y,
                             SHOP_DETAIL_ACTION_RECT.width,
                             SHOP_DETAIL_ACTION_RECT.height, 8, actionFill);
        canvas.fillRoundRect(SHOP_DETAIL_BACK_RECT.x,
                             SHOP_DETAIL_BACK_RECT.y,
                             SHOP_DETAIL_BACK_RECT.width,
                             SHOP_DETAIL_BACK_RECT.height, 8, backFill);
        const char* action = model.mode == ShopViewModel::Mode::SELL
            ? Ui::Amoled::SELL : Ui::Amoled::BUY;
        PixelRenderer::text(
            canvas,
            SHOP_DETAIL_ACTION_RECT.x +
                (SHOP_DETAIL_ACTION_RECT.width -
                 textWidth(action)) / 2,
            SHOP_DETAIL_ACTION_RECT.y +
                (SHOP_DETAIL_ACTION_RECT.height - FontResource::LARGE_GLYPH_H) / 2,
            action,
            shopFadeColor(rgb(115, 226, 183), detailAlpha,
                          detailBackground));
        PixelRenderer::text(
            canvas,
            SHOP_DETAIL_BACK_RECT.x +
                (SHOP_DETAIL_BACK_RECT.width -
                 textWidth(Ui::BACK)) / 2,
            SHOP_DETAIL_BACK_RECT.y +
                (SHOP_DETAIL_BACK_RECT.height - FontResource::LARGE_GLYPH_H) / 2,
            Ui::BACK,
            shopFadeColor(rgb(239, 143, 148), detailAlpha,
                          detailBackground));
    }

    drawToast(canvas, model.toast);
    pageClip.reset();
}
