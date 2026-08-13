#pragma once

#include <cstdint>

#include "core/AppSceneFlow.h"
#include "game/ExploreMapGenerator.h"
#include "game/GameState.h"
#include "game/ShopService.h"

class Canvas565;

namespace AmoledV1 {

inline constexpr int HOME_HEADER_HEIGHT = 24;
inline constexpr int HOME_ROOM_TOP = HOME_HEADER_HEIGHT;
inline constexpr int HOME_ROOM_WIDTH = 184;
inline constexpr int HOME_ROOM_HEIGHT = 148;
inline constexpr int HOME_STATUS_TOP = HOME_ROOM_TOP + HOME_ROOM_HEIGHT;

enum class HomeHitTarget {
    NONE,
    MENU,
    LOCK,
    PET,
    BOWL,
};

struct HomeViewModel {
    struct MonsterHud {
        uint8_t hp = 0;
        uint8_t hunger = 0;
    };

    uint16_t speciesId = 1;
    MonsterHud monsters[Game::TEAM_CAP];
    uint8_t monsterCount = 1;
    uint16_t gameMinutesOfDay = 8 * 60 + 30;
    int16_t cameraX = 0;
    int16_t cameraY = 0;
    int16_t petCenterX = 92;
    int16_t petGroundY = 151;
    int16_t bowlCenterX = 145;
    int16_t bowlCenterY = 143;
    uint8_t petFrame = 0;
    bool petWalking = false;
    bool petFacingRight = true;
    bool night = false;
    bool showHearts = false;
    bool bowlFilled = false;
    const char* toast = nullptr;
};

struct MenuViewModel {
    float scroll = 0.0f;
    int pressedItem = -1;
    const char* toast = nullptr;
};

struct ExploreViewModel {
    float scroll = 0.0f;
    uint8_t visibleAreaCount = 1;
    uint8_t unlockedArea = 0;
    uint8_t selectedArea = 0;
    uint8_t currentLevel = 1;
    int pressedArea = -1;
    const char* toast = nullptr;
};

struct ExploreRouteViewModel {
    const ExploreMapGenerator::Map* map = nullptr;
    uint16_t speciesId = 1;
    uint8_t area = 0;
    uint8_t pathIndex = 0;
    uint8_t routeIndex = 0;
    uint8_t routePointCount = 0;
    uint8_t walkDirection = 0;
    uint8_t petFrame = 0;
    uint16_t steps = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    int16_t cameraX = 0;
    int16_t cameraY = 0;
    bool walking = false;
    bool autoWalk = false;
    bool complete = false;
    bool exitConfirm = false;
};

struct ExploreMenuViewModel {
    int pressedItem = -1;
    const char* toast = nullptr;
};

struct ShopCategoryViewModel {
    const Game::GameState* state = nullptr;
    uint32_t coins = 0;
    Game::ShopService::Category category =
        Game::ShopService::Category::DAILY;
    int pressedItem = -1;
    const char* toast = nullptr;
};

struct TeamViewModel {
    const Game::GameState* state = nullptr;
    int pressedSlot = -1;
    bool confirmOpen = false;
    uint8_t pendingSlot = 0;
    const char* toast = nullptr;
};

struct RoomMenuViewModel {
    const Game::GameState* state = nullptr;
    int pressedItem = -1;
    const char* toast = nullptr;
};

enum class ShowerMode : uint8_t {
    MENU = 0,
    SOAP_SELECT,
    SOAPING,
    BRUSHING,
    RINSING,
    COMPLETE,
    EXIT_CONFIRM,
};

struct ShowerViewModel {
    const Game::GameState* state = nullptr;
    ShowerMode mode = ShowerMode::MENU;
    uint16_t speciesId = 1;
    uint8_t soapIndex = 0;
    uint8_t soapProgress = 0;
    uint8_t brushProgress = 0;
    uint8_t rinseProgress = 0;
    uint8_t completionHearts = 0;
    int16_t toolX = 24;
    int16_t toolY = 196;
    int pressedItem = -1;
    bool toolDragging = false;
    bool exitConfirmYes = false;
    const char* toast = nullptr;
};

enum class ItemListMode : uint8_t {
    BAG = 0,
    BUY,
    SELL,
};

struct ItemListViewModel {
    const Game::GameState* state = nullptr;
    ItemListMode mode = ItemListMode::BAG;
    Game::ShopService::Category category =
        Game::ShopService::Category::DAILY;
    float scroll = 0.0f;
    uint8_t itemCount = 0;
    uint32_t coins = 0;
    int pressedItem = -1;
    bool confirmOpen = false;
    Game::ItemId pendingItem = Game::ItemId::COUNT;
    const char* toast = nullptr;
};

inline constexpr int MAIN_MENU_ITEM_COUNT =
    AppSceneFlow::mainMenuItemCount(false);

void renderHomeScreen(Canvas565& canvas, const HomeViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = 224);
HomeHitTarget homeHitTargetAt(int x, int y, int petCenterX = 92,
                              int petGroundY = 151,
                              int bowlCenterX = 145,
                              int bowlCenterY = 143);

void renderMainMenu(Canvas565& canvas, const MenuViewModel& model,
                    uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool mainMenuBackAt(int x, int y);
int mainMenuItemAt(int x, int y, float scroll);
float mainMenuMaxScroll();

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool exploreBackAt(int x, int y);
bool exploreMenuAt(int x, int y);
int exploreAreaAt(int x, int y, float scroll, uint8_t visibleAreaCount);
float exploreMaxScroll(uint8_t visibleAreaCount);

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              uint16_t rowBegin = 0,
                              uint16_t rowEnd = 224);
bool exploreRouteBackAt(int x, int y);
bool exploreRouteMenuAt(int x, int y);
int exploreRouteExitChoiceAt(int x, int y);
bool exploreRouteMapAt(int x, int y);

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin = 0,
                             uint16_t rowEnd = 224);
bool exploreRouteMenuBackAt(int x, int y);
int exploreRouteMenuItemAt(int x, int y);

void renderShopCategoryScreen(Canvas565& canvas,
                              const ShopCategoryViewModel& model,
                              uint16_t rowBegin = 0,
                              uint16_t rowEnd = 224);
int shopCategoryItemAt(int x, int y);

void renderItemListScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin = 0,
                          uint16_t rowEnd = 224);
void renderShopItemScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin = 0,
                          uint16_t rowEnd = 224);
bool itemListBackAt(int x, int y);
int itemListItemAt(int x, int y, float scroll, uint8_t itemCount);
float itemListMaxScroll(uint8_t itemCount);
int shopItemAt(int x, int y, float scroll, uint8_t itemCount);
int shopSelectedItem(float scroll, uint8_t itemCount);
float shopItemScrollForIndex(uint8_t index);
float shopItemMaxScroll(uint8_t itemCount);
bool shopActionAt(int x, int y);
int itemConfirmChoiceAt(int x, int y);

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool teamBackAt(int x, int y);
int teamMemberAt(int x, int y, uint8_t teamCount);
int teamConfirmChoiceAt(int x, int y);

void renderRoomMenuScreen(Canvas565& canvas, const RoomMenuViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = 224);
int roomMenuItemAt(int x, int y);

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool showerBackAt(int x, int y);
int showerMenuItemAt(int x, int y);
int showerSoapItemAt(int x, int y);
int showerExitChoiceAt(int x, int y);
bool showerToolAt(int x, int y, int toolX, int toolY);

}  // namespace AmoledV1
