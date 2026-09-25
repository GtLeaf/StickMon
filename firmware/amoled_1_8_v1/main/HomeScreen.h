#pragma once

#include "TouchTest.h"

#include <cstddef>
#include <cstdint>

#include "core/BuildConfig.h"
#include "AmoledGeometry.h"
#include "core/AppSceneFlow.h"
#include "core/VisitSessionService.h"
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "brain/ClawStatusLog.h"
#include "game/BattleSystem.h"
#include "game/ExplorePool.h"
#include "game/ExploreMapGenerator.h"
#include "game/GameState.h"
#include "game/ShopService.h"
#include "ui/models/ScreenModels.h"
#include "ui/UiMetrics.h"
#include "ui/RoomScreens.h"
#include "ui/SettingsScreens.h"

class Canvas565;

namespace PokemonSprites {
struct SpriteFrame;
}

namespace AmoledV1 {

class PixelCache565;

bool prepareBattleBackground(Canvas565& canvas, PixelCache565& backgroundCache,
                             GameAssets::Kind kind);

inline constexpr int HOME_HEADER_HEIGHT = UiMetrics::HOME_HEADER_HEIGHT;
inline constexpr int HOME_ROOM_TOP = HOME_HEADER_HEIGHT;
inline constexpr int MAIN_MENU_CONTENT_TOP = 0;
inline constexpr int HOME_ROOM_WIDTH = 368;
inline constexpr int HOME_ROOM_HEIGHT = UiMetrics::HOME_ROOM_HEIGHT;
inline constexpr int HOME_STATUS_TOP = UiMetrics::HOME_STATUS_TOP;
inline constexpr int EXPLORE_SELECTOR_TOP_HEIGHT = 104;
inline constexpr int EXPLORE_SELECTOR_CENTER_X = 184;
inline constexpr int EXPLORE_SELECTOR_AREA_SPACING = 144;
// Keep the centered action group close to the physical bottom of the AMOLED
// display while leaving a 12-pixel native bottom margin.
inline constexpr int EXPLORE_SELECTOR_BUTTON_TOP = 384;
inline constexpr int EXPLORE_PREVIEW_TOP = 168;
inline constexpr int EXPLORE_PREVIEW_BOTTOM = 328;
inline constexpr int SHOP_LEFT_PANEL_WIDTH = 112;
inline constexpr int TEAM_MOVES_HEADER_HEIGHT = UiMetrics::PAGE_HEADER_HEIGHT;
inline constexpr int CONTACT_ROW_HEIGHT = 90;
// ESP-Claw setup page: header tabs and the log window geometry (UI space).
inline constexpr int CLAW_TAB_CONNECT_LEFT = 204;
inline constexpr int CLAW_TAB_LOG_LEFT = 286;
inline constexpr int CLAW_TAB_WIDTH = 78;
inline constexpr int CLAW_LOG_TOP = 156;
inline constexpr int CLAW_LOG_HEIGHT = 280;
inline constexpr int CLAW_LOG_ROW_HEIGHT = 32;
inline constexpr int CLAW_LOG_VIEWPORT = 272;

enum class HomeHitTarget {
    NONE,
    MENU,
    LOCK,
    PET,
    BOWL,
};

inline constexpr int MAIN_MENU_ITEM_COUNT =
    AppSceneFlow::mainMenuItemCount(STICKMON_ENABLE_DEBUG_FEATURES != 0);

void renderHomeScreen(Canvas565& canvas, const HomeViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
HomeHitTarget homeHitTargetAt(int x, int y, int petCenterX = 184,
                              int petGroundY = 302,
                              int bowlCenterX = 290,
                              int bowlCenterY = 286);
int recallConfirmChoiceAt(int x, int y);

void renderMainMenu(Canvas565& canvas, const MenuViewModel& model,
                    uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool mainMenuBackAt(int x, int y);
int mainMenuItemAt(int x, int y, float scroll);
float mainMenuMaxScroll();

#if STICKMON_ENABLE_DEBUG_FEATURES
void renderDebugScreen(Canvas565& canvas, const DebugViewModel& model,
                       uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool debugBackAt(int x, int y);
int debugItemAt(int x, int y, DebugViewModel::Category category,
                float scroll);
float debugMaxScroll(DebugViewModel::Category category);
int debugPopupChoiceAt(int x, int y);
int debugPopupDigitAt(int x, int y, uint8_t digitCount);
int debugContactChoiceAt(int x, int y);
#endif

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         PixelCache565& backgroundCache,
                         uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount);
bool exploreStartAt(int x, int y);
bool exploreSelectionBackAt(int x, int y);

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& mapCache,
                              uint16_t rowBegin = 0,
                              uint16_t rowEnd = AmoledUi::HEIGHT);
bool exploreRouteBackAt(int x, int y);
bool exploreRouteBagAt(int x, int y);
bool exploreRouteMenuAt(int x, int y);
int exploreRouteExitChoiceAt(int x, int y);
bool exploreRouteMapAt(int x, int y);

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin = 0,
                             uint16_t rowEnd = AmoledUi::HEIGHT);
bool exploreRouteMenuBackAt(int x, int y);
int exploreRouteMenuItemAt(int x, int y);

void renderCommunicationScreen(Canvas565& canvas,
                               const CommunicationViewModel& model,
                               uint16_t rowBegin = 0,
                               uint16_t rowEnd = AmoledUi::HEIGHT);
bool communicationBackAt(int x, int y);
int communicationItemAt(int x, int y,
                        const CommunicationViewModel& model);

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

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamBackAt(int x, int y);
int teamMemberAt(int x, int y, uint8_t teamCount);
bool teamMovesButtonAt(int x, int y, uint8_t teamSlot);
int teamConfirmChoiceAt(int x, int y);
int teamActionPopupItemAt(int x, int y, uint8_t teamSlot);
void renderTeamStatusScreen(Canvas565& canvas,
                            const TeamStatusViewModel& model,
                            uint16_t rowBegin = 0,
                            uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamStatusBackAt(int x, int y);
void renderTeamMovesScreen(Canvas565& canvas,
                           const TeamMovesViewModel& model,
                           uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool teamMovesBackAt(int x, int y);
int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount);
int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount, int16_t scrollOffsetY,
                    uint8_t selectedItem, float detailProgress);

void renderComputerScreen(Canvas565& canvas, const ComputerViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool computerBackAt(int x, int y,
                    ComputerViewModel::Page page = ComputerViewModel::Page::MENU);
int computerItemAt(int x, int y, ComputerViewModel::Page page,
                   float storageScroll = 0.0f,
                   uint8_t storageCount = Game::STORAGE_CAP,
                   bool clawEnabled = false);
int computerMaxStorageScroll(uint8_t storageCount);
int computerContactActionItemAt(int x, int y, uint8_t actionCount,
                                uint8_t slot, float scroll);
bool computerContactMenuAt(int x, int y, uint8_t actionCount,
                           uint8_t slot, float scroll);
int computerContactConfirmChoiceAt(int x, int y);
// Header tabs on the CLAW_SETUP page: 0 = 连接 (QR), 1 = 日志 (status log).
int clawTabAt(int x, int y);

void renderProgressionScreen(Canvas565& canvas,
                             const ProgressionViewModel& model,
                             uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
int progressionItemAt(int x, int y,
                      ProgressionViewModel::Mode mode);
int progressionReplaceItemAt(int x, int y, int16_t scrollOffsetY,
                             uint8_t selectedItem, float detailProgress);
void renderBattleScreen(Canvas565& canvas, const BattleViewModel& model,
                        PixelCache565& backgroundCache,
                        uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool battleBackAt(int x, int y);
int battleItemAt(int x, int y, BattleViewModel::Phase phase);

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool showerBackAt(int x, int y);
int showerMenuItemAt(int x, int y);
int showerSoapItemAt(int x, int y);
int showerExitChoiceAt(int x, int y);
bool showerToolAt(int x, int y, int toolX, int toolY);

}  // namespace AmoledV1
