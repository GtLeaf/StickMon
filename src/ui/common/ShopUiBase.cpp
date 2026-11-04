#include "ui/common/ShopUiBase.h"
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include <cmath>
#include <cstdio>

namespace Ui {

static uint8_t utf8CharBytes(uint8_t lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8GlyphWidth(uint8_t lead) {
    if (lead >= 0x80) return 16;
    return lead == ' ' ? 5 : 8;
}

void drawShopWrappedText(const char* value, int x, int y, int maxWidth,
                     uint16_t color, uint8_t maxLines) {
    if (!value || !*value || maxWidth <= 0 || maxLines == 0) return;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    char line[64] = {};
    uint8_t lineBytes = 0;
    int lineWidth = 0;
    uint8_t lineIndex = 0;

    auto flushLine = [&]() {
        if (lineBytes == 0 || lineIndex >= maxLines) return;
        line[lineBytes] = '\0';
        PixelRenderer::text(x, y + lineIndex * 16, line, color, 1);
        ++lineIndex;
        lineBytes = 0;
        lineWidth = 0;
    };

    while (*p && lineIndex < maxLines) {
        if (*p == '\n') {
            ++p;
            flushLine();
            continue;
        }
        uint8_t bytes = utf8CharBytes(*p);
        int glyphWidth = utf8GlyphWidth(*p);
        if (lineBytes > 0 && lineWidth + glyphWidth > maxWidth) {
            flushLine();
            if (lineIndex >= maxLines) break;
        }
        if (lineBytes + bytes >= sizeof(line)) {
            flushLine();
            if (lineIndex >= maxLines) break;
        }
        for (uint8_t i = 0; i < bytes && p[i]; ++i) line[lineBytes++] = (char)p[i];
        lineWidth += glyphWidth;
        p += bytes;
    }
    flushLine();
}

static uint32_t canvasFingerprint(int centerX, int centerY, int halfSize) {
    auto& canvas = PixelRenderer::canvas();
    uint32_t hash = 2166136261UL;
    for (int y = centerY - halfSize; y <= centerY + halfSize; ++y) {
        for (int x = centerX - halfSize; x <= centerX + halfSize; ++x) {
            uint16_t pixel = (x >= 0 && y >= 0 &&
                              x < canvas.width() && y < canvas.height())
                ? static_cast<uint16_t>(canvas.readPixel(x, y)) : 0;
            hash = (hash ^ pixel) * 16777619UL;
        }
    }
    return hash;
}

void ShopUiBase::buyCurrent() {
    const Item item = currentItem();
    if (item == BACK) { returnToCategories(); return; }
    switch (host.buyShopItem(item)) {
    case Game::ShopService::BuyResult::BOUGHT:
        std::snprintf(toastBuffer, sizeof(toastBuffer), Ui::Shop::BOUGHT_FMT, nameFor(item));
        toast = toastBuffer;
        break;
    case Game::ShopService::BuyResult::DAILY_LIMIT:
        toast = Ui::Shop::CANDY_DAILY_LIMIT;
        break;
    case Game::ShopService::BuyResult::NOT_ENOUGH_COINS:
        toast = Ui::Shop::NOT_ENOUGH_COINS;
        break;
    default:
        toast = Ui::Shop::BAG_FULL;
        break;
    }
    toastUntil = Hal::ins().millis() + 1200;
}

void ShopUiBase::sellCurrent() {
    const Item item = sellItemAtIndex(cursor);
    if (item == BACK) { returnToCategories(); return; }
    if (host.sellShopItem(item) == Game::ShopService::SellResult::SOLD) {
        std::snprintf(toastBuffer, sizeof(toastBuffer), Ui::Shop::SOLD_FMT, sellPriceFor(item));
        toast = toastBuffer;
        toastUntil = Hal::ins().millis() + 1000;
    }
    const uint8_t count = sellItemCount();
    if (count > 0 && cursor >= count) cursor = count - 1;
}

void ShopUiBase::drawItemIcon(Item item, int centerX, int centerY, float scale,
                             uint16_t fallbackColor) const {
    auto& c = PixelRenderer::canvas();
    if (item == BACK) {
        int halfW = (int)roundf(6.0f * scale);
        int halfH = (int)roundf(5.0f * scale);
        c.drawFastHLine(centerX - halfW, centerY, halfW * 2, fallbackColor);
        c.drawLine(centerX - halfW, centerY, centerX - 1, centerY - halfH, fallbackColor);
        c.drawLine(centerX - halfW, centerY, centerX - 1, centerY + halfH, fallbackColor);
        return;
    }
    Game::ItemId gameItem = gameItemIdFor(item);
    GameAssets::Kind kind = GameAssets::itemKind(gameItem);
    uint8_t itemIndex = static_cast<uint8_t>(item);
    uint32_t traceBit = itemIndex < 32 ? (1UL << itemIndex) : 0;
    bool shouldTrace = traceBit != 0 && (iconTraceMask & traceBit) == 0;
    uint32_t before = shouldTrace ? canvasFingerprint(centerX, centerY, 22) : 0;
    bool drawn = GameAssets::drawCentered(kind, centerX, centerY, scale);
    if (shouldTrace) {
        iconTraceMask |= traceBit;
        uint32_t after = canvasFingerprint(centerX, centerY, 22);
        Platform::logf(
            "[ShopIcon] item=%u gameItem=%u kind=%u drawn=%u changed=%u before=%08lx after=%08lx pos=%d,%d scale=%u\n",
            itemIndex, static_cast<unsigned>(gameItem),
            static_cast<unsigned>(kind), drawn ? 1U : 0U,
            before != after ? 1U : 0U,
            static_cast<unsigned long>(before), static_cast<unsigned long>(after),
            centerX, centerY, static_cast<unsigned>(scale * 100.0f + 0.5f));
    }
    if (drawn) return;

    // A stale or incomplete resource pack must not turn a valid product into
    // an unknown-item marker. Keep a small deterministic glyph so the item
    // remains identifiable and purchasable until the pack is updated.
    int half = (int)roundf(7.0f * scale);
    c.drawRect(centerX - half, centerY - half, half * 2 + 1, half * 2 + 1,
               fallbackColor);
    switch (item) {
    case FOOD:
    case TASTY_FOOD:
    case SWEET_FOOD:
    case SPICY_FOOD:
    case SOUR_FOOD:
    case BITTER_FOOD:
    case DRY_FOOD:
        c.fillRect(centerX - half + 3, centerY - half + 3,
                   half * 2 - 5, half * 2 - 5, fallbackColor);
        c.drawFastHLine(centerX - half + 4, centerY,
                        half * 2 - 7, PixelRenderer::rgb(7, 9, 14));
        break;
    case SOAP_0:
    case SOAP_1:
    case SOAP_2:
        c.drawFastHLine(centerX - half + 3, centerY,
                        half * 2 - 5, fallbackColor);
        c.drawFastVLine(centerX, centerY - half + 3,
                        half * 2 - 5, fallbackColor);
        break;
    default:
        c.fillRect(centerX - 3, centerY - 3, 7, 7, fallbackColor);
        break;
    }
}

const char* ShopUiBase::nameFor(Item item) {
    static_assert(sizeof(Ui::Shop::NAMES) / sizeof(Ui::Shop::NAMES[0]) == COUNT,
                  "Shop labels must match the item selection");
    uint8_t idx = (uint8_t)item;
    if (idx >= COUNT) return Ui::BACK;
    return Ui::Shop::NAMES[idx];
}

const char* ShopUiBase::descFor(Item item) {
    static_assert(sizeof(Ui::Shop::DESCS) / sizeof(Ui::Shop::DESCS[0]) == COUNT,
                  "Shop descriptions must match the item selection");
    uint8_t idx = (uint8_t)item;
    if (idx >= COUNT) return "";
    return Ui::Shop::DESCS[idx];
}

}  // namespace Ui
