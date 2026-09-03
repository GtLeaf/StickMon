#pragma once

#include <cstddef>
#include <cstdint>

#include "core/BuildConfig.h"
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

class Canvas565;

namespace PokemonSprites {
struct SpriteFrame;
}

namespace AmoledV1 {

inline constexpr int HOME_HEADER_HEIGHT = 24;
inline constexpr int HOME_ROOM_TOP = HOME_HEADER_HEIGHT;
inline constexpr int MAIN_MENU_CONTENT_TOP = 0;
inline constexpr int HOME_ROOM_WIDTH = 184;
inline constexpr int HOME_ROOM_HEIGHT = 148;
inline constexpr int HOME_STATUS_TOP = HOME_ROOM_TOP + HOME_ROOM_HEIGHT;
inline constexpr int SETTINGS_SLIDER_LEFT = 74;
inline constexpr int SETTINGS_SLIDER_RIGHT = 166;
inline constexpr int SETTINGS_SLIDER_OFFSET_Y = 24;
inline constexpr int EXPLORE_SELECTOR_LEFT_WIDTH = 64;
inline constexpr int EXPLORE_SELECTOR_CENTER_Y = 124;
inline constexpr int EXPLORE_SELECTOR_AREA_SPACING = 32;
inline constexpr int SHOP_LEFT_PANEL_WIDTH = 56;
// ESP-Claw setup page: header tabs and the log window geometry (UI space).
inline constexpr int CLAW_TAB_CONNECT_LEFT = 102;
inline constexpr int CLAW_TAB_LOG_LEFT = 143;
inline constexpr int CLAW_TAB_WIDTH = 39;
inline constexpr int CLAW_LOG_TOP = 78;
inline constexpr int CLAW_LOG_HEIGHT = 140;
inline constexpr int CLAW_LOG_ROW_HEIGHT = 16;
inline constexpr int CLAW_LOG_VIEWPORT = 136;

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
    enum class PetVisualAction : uint8_t {
        IDLE,
        WALKING,
        STOPPING,
    };
    PokemonSprites::WalkDirection petDirection =
        PokemonSprites::WalkDirection::DOWN;
    PetVisualAction petAction = PetVisualAction::IDLE;
    bool petLongMove = true;
    bool petResting = false;
    bool night = false;
    uint8_t moodHearts = 0;
    uint8_t moodBurstHeart = 0xFF;
    uint16_t moodBurstAgeMs = 0;
    bool showHearts = false;
    bool bowlFilled = false;
    const char* toast = nullptr;
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool debugContactPrompt = false;
    bool debugContactActive = false;
    uint16_t debugContactSpeciesId = 0;
    uint8_t debugContactKind = 0;
    bool debugPairChaseActive = false;
    uint16_t debugPairSpeciesId = 0;
    int16_t debugPairCenterX = 92;
    int16_t debugPairGroundY = 151;
    uint8_t debugPairFrame = 0;
    PokemonSprites::WalkDirection debugPairDirection =
        PokemonSprites::WalkDirection::DOWN;
    uint8_t debugLightSource = 0;
    bool debugBoundaryVisible = false;
#endif
};

struct MenuViewModel {
    float scroll = 0.0f;
    int pressedItem = -1;
    const char* toast = nullptr;
};

#if STICKMON_ENABLE_DEBUG_FEATURES
struct DebugViewModel {
    enum class Category : uint8_t {
        ROOT = 0,
        MONSTER,
        RESOURCE,
        ENV,
        MOTION,
        BATTLE,
        CONTACT_EVENT,
    };
    enum class Popup : uint8_t { NONE = 0, SWITCH_MONSTER, SET_TIME };
    Category category = Category::ROOT;
    uint8_t cursor = 0;
    float scroll = 0.0f;
    int pressedItem = -1;
    Popup popup = Popup::NONE;
    uint8_t focus = 0;
    uint8_t digits[4] = {};
    const Game::GameState* state = nullptr;
    const char* toast = nullptr;
    const char* currentTime = nullptr;
    const char* lightSource = nullptr;
    bool tiltEnabled = false;
    bool boundaryVisible = false;
    bool battleBoundsVisible = false;
};
#endif

struct ExploreViewModel {
    uint8_t visibleAreaCount = 1;
    uint8_t unlockedArea = 0;
    uint8_t selectedArea = 0;
    uint8_t currentLevel = 1;
    float areaAnimCursor = 0.0f;
    uint32_t previewStartedAt = 0;
    ExplorePool::Pool previewPool{};
    const PokemonSprites::SpriteFrame* previewFrames[ExplorePool::POOL_CAP] = {};
    bool previewHidden[ExplorePool::POOL_CAP] = {};
    int pressedArea = -1;
    const char* toast = nullptr;
};

struct ExploreRouteViewModel {
    enum class Prompt : uint8_t { NONE = 0, BLOCKED, PUZZLE };
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
    bool sliding = false;
    bool complete = false;
    bool exitConfirm = false;
    uint8_t pickupIndex = 0;
    uint8_t pickupItem = 0;
    bool pickupAvailable = false;
    Prompt prompt = Prompt::NONE;
};

struct ExploreMenuViewModel {
    uint8_t cursor = 0;
    int pressedItem = -1;
    const char* toast = nullptr;
};

using CommunicationViewModel = Communication::VisitSessionService::ViewModel;

struct TeamViewModel {
    const Game::GameState* state = nullptr;
    int pressedSlot = -1;
    bool confirmOpen = false;
    uint8_t pendingSlot = 0;
    const char* toast = nullptr;
};

struct TeamMovesViewModel {
    enum class Mode : uint8_t { MANAGE = 0, RECALL_SELECT, RECALL_REPLACE };
    const Game::GameState* state = nullptr;
    uint8_t teamSlot = 0;
    Mode mode = Mode::MANAGE;
    uint8_t selectedItem = 0xFF;
    uint8_t recallCount = 0;
    Game::MoveId recallIds[80] = {};
    uint8_t recallSelected = 0xFF;
    uint8_t forgetSlot = 0;
    bool forgetConfirmOpen = false;
    const char* toast = nullptr;
};

struct RoomMenuViewModel {
    const Game::GameState* state = nullptr;
    int pressedItem = -1;
    const char* toast = nullptr;
};

struct RoomFoodViewModel {
    const Game::GameState* state = nullptr;
    uint8_t selectedFood = 0;
    int pressedItem = -1;
    const char* toast = nullptr;
};

struct ComputerViewModel {
    enum class Page : uint8_t { MENU = 0, STATUS, STORAGE, CLAW_SETUP };
    const Game::GameState* state = nullptr;
    Page page = Page::MENU;
    float storageScroll = 0.0f;
    uint8_t selectedItem = 0;
    int pressedItem = -1;
    const char* clawSsid = nullptr;
    const char* clawPassword = nullptr;
    const char* clawIp = nullptr;
    // CLAW_SETUP log view (second header tab). clawLog points at a snapshot
    // buffer owned by the caller and stays valid only during the render call.
    bool clawLogView = false;
    float clawLogScroll = 0.0f;
    bool clawLogPinned = true;
    const Stickmon::ClawStatusLog::Entry* clawLog = nullptr;
    size_t clawLogCount = 0;
    bool clawStaConnected = false;
    char clawStaIp[16] = {};
    bool clawPhoneJoined = false;
    bool clawStarted = false;
    char clawWechatPhase[16] = {};
    bool clawWechatPersisted = false;
    const char* toast = nullptr;
};

struct SettingsViewModel {
    const Game::GameState* state = nullptr;
    uint8_t brightness = 128;
    uint8_t volume = 50;
    uint8_t pressedItem = 0xFF;
    const char* toast = nullptr;
};

struct ProgressionViewModel {
    enum class Mode : uint8_t { LEVEL_UP = 0, EVOLUTION, MOVE_LEARN, MOVE_REPLACE };
    const Game::GameState* state = nullptr;
    Mode mode = Mode::LEVEL_UP;
    uint8_t teamSlot = 0;
    uint8_t level = 1;
    uint16_t fromSpeciesId = 0;
    uint16_t toSpeciesId = 0;
    Game::MoveId moveId = 0;
    Game::MoveId oldMove2 = 0;
    Game::MoveId oldMove3 = 0;
    uint8_t pressedItem = 0xFF;
    const char* toast = nullptr;
};

struct BattleViewModel {
    enum class Phase : uint8_t {
        ACTION = 0,
        BAG_SELECT,
        SWITCH_SELECT,
        VICTORY,
        DEFEAT,
        FRIENDSHIP,
    };
    enum class FriendshipPrompt : uint8_t {
        OFFER = 0,
        TEAM,
        ACQUIRED,
        FULL,
    };
    const Game::GameState* state = nullptr;
    GameAssets::Kind battleBackground = GameAssets::Kind::BATTLE_BG_GRASS;
    uint16_t playerSpeciesId = 1;
    uint16_t wildSpeciesId = 1;
    uint8_t playerLevel = 1;
    uint8_t wildLevel = 1;
    uint8_t playerHp = 0;
    uint8_t wildHp = 0;
    uint8_t activeSlot = 0;
    uint8_t teamCount = 0;
    uint8_t battleBagCount = 0;
    Game::ItemId battleBagItems[8] = {};
    bool animationActive = false;
    bool animationAttackerWild = false;
    bool animationHit = false;
    uint16_t animationDamage = 0;
    uint8_t animationFrame = 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool debugDrawBounds = false;
#endif
    Game::MajorStatus playerStatus = Game::MajorStatus::NONE;
    Game::MajorStatus wildStatus = Game::MajorStatus::NONE;
    BattleSystem::BattleActorState playerBattleState;
    BattleSystem::BattleActorState wildBattleState;
    Phase phase = Phase::ACTION;
    FriendshipPrompt friendshipPrompt = FriendshipPrompt::OFFER;
    uint8_t pressedItem = 0xFF;
    const char* logLines[2] = {};
    uint8_t logCount = 0;
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

struct ShopViewModel {
    enum class Mode : uint8_t { BUY = 0, SELL };
    const Game::GameState* state = nullptr;
    Mode mode = Mode::BUY;
    float scroll = 0.0f;
    uint8_t dailyItemCount = 0;
    uint8_t exploreItemCount = 0;
    uint8_t itemCount = 0;
    uint32_t coins = 0;
    int pressedMenuItem = -1;
    int pressedItem = -1;
    int pressedDetailAction = -1;
    int detailItemIndex = -1;
    Game::ItemId detailItem = Game::ItemId::COUNT;
    float detailProgress = 0.0f;
    const char* toast = nullptr;
};

inline constexpr int MAIN_MENU_ITEM_COUNT =
    AppSceneFlow::mainMenuItemCount(STICKMON_ENABLE_DEBUG_FEATURES != 0);

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

#if STICKMON_ENABLE_DEBUG_FEATURES
void renderDebugScreen(Canvas565& canvas, const DebugViewModel& model,
                       uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool debugBackAt(int x, int y);
int debugItemAt(int x, int y, DebugViewModel::Category category,
                float scroll);
float debugMaxScroll(DebugViewModel::Category category);
int debugPopupChoiceAt(int x, int y);
int debugPopupDigitAt(int x, int y, uint8_t digitCount);
int debugContactChoiceAt(int x, int y);
#endif

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool exploreBackAt(int x, int y);
bool exploreMenuAt(int x, int y);
int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount);

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              uint16_t rowBegin = 0,
                              uint16_t rowEnd = 224);
bool exploreRouteBackAt(int x, int y);
bool exploreRouteMenuAt(int x, int y);
int exploreRouteExitChoiceAt(int x, int y);
int exploreRoutePromptChoiceAt(int x, int y);
bool exploreRouteMapAt(int x, int y);

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin = 0,
                             uint16_t rowEnd = 224);
bool exploreRouteMenuBackAt(int x, int y);
int exploreRouteMenuItemAt(int x, int y);

void renderCommunicationScreen(Canvas565& canvas,
                               const CommunicationViewModel& model,
                               uint16_t rowBegin = 0,
                               uint16_t rowEnd = 224);
bool communicationBackAt(int x, int y);
int communicationItemAt(int x, int y,
                        const CommunicationViewModel& model);

void renderItemListScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin = 0,
                          uint16_t rowEnd = 224);
void renderShopScreen(Canvas565& canvas, const ShopViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool itemListBackAt(int x, int y);
int itemListItemAt(int x, int y, float scroll, uint8_t itemCount);
float itemListMaxScroll(uint8_t itemCount);
int shopMenuItemAt(int x, int y);
int shopGridItemAt(int x, int y, float scroll,
                   ShopViewModel::Mode mode, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount);
float shopGridMaxScroll(ShopViewModel::Mode mode, uint8_t dailyItemCount,
                        uint8_t exploreItemCount, uint8_t itemCount);
int itemConfirmChoiceAt(int x, int y);

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool teamBackAt(int x, int y);
int teamMemberAt(int x, int y, uint8_t teamCount);
bool teamMovesButtonAt(int x, int y, uint8_t teamSlot);
int teamConfirmChoiceAt(int x, int y);
void renderTeamMovesScreen(Canvas565& canvas,
                           const TeamMovesViewModel& model,
                           uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool teamMovesBackAt(int x, int y);
int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount);

void renderRoomMenuScreen(Canvas565& canvas, const RoomMenuViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = 224);
int roomMenuItemAt(int x, int y);
void renderRoomFoodScreen(Canvas565& canvas, const RoomFoodViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool roomFoodBackAt(int x, int y);
int roomFoodItemAt(int x, int y);

void renderComputerScreen(Canvas565& canvas, const ComputerViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool computerBackAt(int x, int y);
int computerItemAt(int x, int y, ComputerViewModel::Page page,
                   float storageScroll = 0.0f,
                   uint8_t storageCount = Game::STORAGE_CAP);
// Header tabs on the CLAW_SETUP page: 0 = 连接 (QR), 1 = 日志 (status log).
int clawTabAt(int x, int y);

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool settingsBackAt(int x, int y);
int settingsItemAt(int x, int y);

void renderProgressionScreen(Canvas565& canvas,
                             const ProgressionViewModel& model,
                             uint16_t rowBegin = 0, uint16_t rowEnd = 224);
int progressionItemAt(int x, int y,
                      ProgressionViewModel::Mode mode);
void renderBattleScreen(Canvas565& canvas, const BattleViewModel& model,
                        uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool battleBackAt(int x, int y);
int battleItemAt(int x, int y, BattleViewModel::Phase phase);

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin = 0, uint16_t rowEnd = 224);
bool showerBackAt(int x, int y);
int showerMenuItemAt(int x, int y);
int showerSoapItemAt(int x, int y);
int showerExitChoiceAt(int x, int y);
bool showerToolAt(int x, int y, int toolX, int toolY);

}  // namespace AmoledV1
