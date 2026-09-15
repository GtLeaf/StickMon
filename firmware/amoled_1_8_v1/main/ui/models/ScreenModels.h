#pragma once

#include <cstddef>
#include <cstdint>

#include "TouchTest.h"
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
namespace AmoledV1 {

struct HomeViewModel {
    struct MonsterHud {
        uint8_t hp = 0;
        uint8_t hunger = 0;
        bool hpKnown = true;
    };

    uint16_t speciesId = 1;
    MonsterHud monsters[Game::TEAM_CAP];
    uint8_t monsterCount = 1;
    uint16_t gameMinutesOfDay = 8 * 60 + 30;
    int16_t cameraX = 0;
    int16_t cameraY = 0;
    int16_t petCenterX = 184;
    int16_t petGroundY = 302;
    int8_t petRenderOffsetY = 0;
    bool petVisible = true;
    int16_t bowlCenterX = 290;
    int16_t bowlCenterY = 286;
    uint8_t petFrame = 0;
    enum class PetVisualAction : uint8_t {
        IDLE,
        WALKING,
        STOPPING,
    };
    PokemonSprites::WalkDirection petDirection =
        PokemonSprites::WalkDirection::DOWN;
    PetVisualAction petAction = PetVisualAction::IDLE;
    bool companionVisible = false;
    uint16_t companionSpeciesId = 0;
    int16_t companionCenterX = 184;
    int16_t companionGroundY = 302;
    int8_t companionRenderOffsetY = 0;
    uint8_t companionFrame = 0;
    PokemonSprites::WalkDirection companionDirection =
        PokemonSprites::WalkDirection::DOWN;
    PetVisualAction companionAction = PetVisualAction::IDLE;
    bool companionLongMove = true;
    bool companionResting = false;
    bool petLongMove = true;
    bool petResting = false;
    bool night = false;
    uint8_t moodHearts = 0;
    uint8_t moodBurstHeart = 0xFF;
    uint16_t moodBurstAgeMs = 0;
    bool showHearts = false;
    bool bowlFilled = false;
    uint8_t fadeAlpha = 0;
    const char* toast = nullptr;
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool debugContactPrompt = false;
    bool debugContactActive = false;
    uint16_t debugContactSpeciesId = 0;
    uint8_t debugContactKind = 0;
    bool debugPairChaseActive = false;
    uint16_t debugPairSpeciesId = 0;
    int16_t debugPairCenterX = 184;
    int16_t debugPairGroundY = 302;
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
        TOUCH_TEST,
    };
    enum class Popup : uint8_t {
        NONE = 0,
        SWITCH_MONSTER,
        SET_TIME,
    };
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
    bool touchDisplayEnabled = false;
    const TouchTest::State* touchTest = nullptr;
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
    const Game::GameState* state = nullptr;
    uint16_t speciesId = 1;
    bool companionVisible = false;
    uint16_t companionSpeciesId = 0;
    float companionWorldX = 0.0f;
    float companionWorldY = 0.0f;
    uint8_t companionWalkDirection = 0;
    uint8_t companionFrame = 0;
    bool companionWalking = false;
    uint8_t area = 0;
    uint8_t pathIndex = 0;
    uint8_t routeIndex = 0;
    uint8_t routePointCount = 0;
    uint8_t walkDirection = 0;
    uint8_t petFrame = 0;
    uint8_t mapFrame = 0;
    uint32_t animationNowMs = 0;
    uint16_t steps = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    int16_t cameraX = 0;
    int16_t cameraY = 0;
    bool walking = false;
    bool autoWalk = false;
    bool sliding = false;
    bool complete = false;
    bool bossPending = false;
    uint8_t bossIndex = 0;
    uint16_t bossSpeciesId = 0;
    uint8_t fadeAlpha = 0;
    bool exitConfirm = false;
    uint8_t pickupIndex = 0;
    uint8_t pickupItem = 0;
    bool pickupAvailable = false;
    Prompt prompt = Prompt::NONE;
    const char* toast = nullptr;
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
    bool actionPopupOpen = false;
    uint8_t actionPopupSlot = 0;
    uint8_t pendingSlot = 0;
    const char* toast = nullptr;
};

struct TeamStatusViewModel {
    const Game::GameState* state = nullptr;
    uint8_t teamSlot = 0;
    uint8_t page = 0;
    // Horizontal slide offset of the current page in pixels. Non-zero while
    // dragging or snapping; the neighbor page is drawn beside it.
    int16_t slideOffsetX = 0;
};

struct TeamMovesViewModel {
    enum class Mode : uint8_t { MANAGE = 0, RECALL_SELECT, RECALL_REPLACE };
    const Game::GameState* state = nullptr;
    uint8_t teamSlot = 0;
    Mode mode = Mode::MANAGE;
    uint8_t selectedItem = 0xFF;
    int16_t scrollOffsetY = 0;
    uint8_t recallCount = 0;
    Game::MoveId recallIds[80] = {};
    uint8_t recallSelected = 0xFF;
    float detailProgress = 0.0f;
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
    enum class Page : uint8_t {
        MENU = 0, STATUS, STORAGE, AI_HOSTING, CLAW_SETUP
    };
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
    bool clawEnabled = false;
    bool wifiEnabled = false;
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
    bool showPlayerExperience = false;
    uint8_t playerExperience = 0;
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
    int16_t toolX = 48;
    int16_t toolY = 392;
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
    uint8_t dailyItemCount = 0;
    uint8_t exploreItemCount = 0;
    uint8_t itemCount = 0;
    bool exploreOnly = false;
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

}  // namespace AmoledV1
