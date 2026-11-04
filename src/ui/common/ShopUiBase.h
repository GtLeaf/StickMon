#pragma once

#include "core/Scene.h"
#include "core/features/ShopFlow.h"

namespace Ui {

class ShopHost {
public:
    virtual ~ShopHost() = default;
    virtual const Game::GameState& shopState() const = 0;
    virtual Game::ShopService::BuyResult buyShopItem(Features::ShopFlow::Item item) = 0;
    virtual Game::ShopService::SellResult sellShopItem(Features::ShopFlow::Item item) = 0;
    virtual void leaveShop() = 0;
};

class ShopUiBase : public Scene, public Features::ShopFlow {
public:
    explicit ShopUiBase(ShopHost& host) : host(host) {}
protected:
    ShopHost& host;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    mutable uint32_t iconTraceMask = 0;
    char toastBuffer[24] = {};
    virtual void returnToCategories() = 0;
    void buyCurrent();
    void sellCurrent();
    void drawItemIcon(Item item, int centerX, int centerY, float scale,
                      uint16_t fallbackColor) const;
    static const char* nameFor(Item item);
    static const char* descFor(Item item);
};

void drawShopWrappedText(const char* value, int x, int y, int maxWidth,
                         uint16_t color, uint8_t maxLines);

}  // namespace Ui
