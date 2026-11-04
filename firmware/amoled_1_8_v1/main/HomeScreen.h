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
#include "ui/HatchScreens.h"
#include "ui/ItemShopScreens.h"
#include "ui/TeamScreens.h"
#include "ui/ComputerScreens.h"
#include "ui/ExploreScreens.h"
#include "ui/ProgressionScreens.h"
#include "ui/ShowerScreens.h"

class Canvas565;

namespace PokemonSprites {
struct SpriteFrame;
}

namespace AmoledV1 {

class PixelCache565;

AmoledUi::Rect homePetVisibleBounds(const HomeViewModel& model);

void drawHatchHeart(Canvas565& canvas, int x, int y);

bool prepareBattleBackground(Canvas565& canvas, PixelCache565& backgroundCache,
                             GameAssets::Kind kind);

inline constexpr int HOME_HEADER_HEIGHT = UiMetrics::HOME_HEADER_HEIGHT;
inline constexpr int HOME_ROOM_TOP = HOME_HEADER_HEIGHT;
inline constexpr int MAIN_MENU_CONTENT_TOP = 0;
inline constexpr int HOME_ROOM_WIDTH = 368;
inline constexpr int HOME_ROOM_HEIGHT = UiMetrics::HOME_ROOM_HEIGHT;
inline constexpr int HOME_STATUS_TOP = UiMetrics::HOME_STATUS_TOP;

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
void renderHomeScreenRegion(Canvas565& canvas, const HomeViewModel& model,
                            uint16_t rowBegin, uint16_t rowEnd,
                            uint16_t columnBegin, uint16_t columnEnd);
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
int debugFurnitureStyleAt(int x, int y);
bool debugFurnitureCancelAt(int x, int y);
int debugPopupChoiceAt(int x, int y);
int debugPopupDigitAt(int x, int y, uint8_t digitCount);
int debugContactChoiceAt(int x, int y);
#endif

void renderCommunicationScreen(Canvas565& canvas,
                               const CommunicationViewModel& model,
                               uint16_t rowBegin = 0,
                               uint16_t rowEnd = AmoledUi::HEIGHT);
bool communicationBackAt(int x, int y);
int communicationItemAt(int x, int y,
                        const CommunicationViewModel& model);

void renderBattleScreen(Canvas565& canvas, const BattleViewModel& model,
                        PixelCache565& backgroundCache,
                        uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
bool battleBackAt(int x, int y);
int battleItemAt(int x, int y, BattleViewModel::Phase phase);

}  // namespace AmoledV1
