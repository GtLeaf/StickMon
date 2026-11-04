#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

inline constexpr int SHOP_LEFT_PANEL_WIDTH = 112;

void renderItemListScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin = 0,
                          uint16_t rowEnd = AmoledUi::HEIGHT);
void renderShopScreen(Canvas565& canvas, const ShopViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool itemListBackAt(int x, int y, bool detailOpen = false);
int itemListItemAt(int x, int y, float scroll, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount,
                   bool exploreOnly = false);
float itemListMaxScroll(uint8_t dailyItemCount, uint8_t exploreItemCount,
                        uint8_t itemCount, bool exploreOnly = false);
int shopMenuItemAt(int x, int y);
int shopGridItemAt(int x, int y, float scroll,
                   ShopViewModel::Mode mode, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount);
float shopGridMaxScroll(ShopViewModel::Mode mode, uint8_t dailyItemCount,
                        uint8_t exploreItemCount, uint8_t itemCount);
int itemConfirmChoiceAt(int x, int y);

}  // namespace AmoledV1
