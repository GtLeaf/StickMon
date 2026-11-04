#pragma once

#include "assets/PokemonSprites.h"
#include "core/BuildConfig.h"
#include "core/Scene.h"
#include "game/GameState.h"

namespace Ui {

enum class HomeAction : uint8_t { PET, FEED, MENU };
enum class HomeLayer : uint8_t {
    BACKGROUND, FLOOR, FOOD, MAIN_SHADOW, VISITOR_SHADOW,
    MAIN_ACTOR, VISITOR_ACTOR, HEARTS, LIGHT, WALK_BOUNDARY,
    HUD, TOAST, CONTACT, TUTORIAL, PROGRESSION
};

struct HomeActorVisual {
    const PokemonSprites::SpriteFrame* frame = nullptr;
    float x = 0, y = 0, groundOffsetY = 18;
    float lift = 0;
    float drop = 0, hop = 0;
    int8_t offsetX = 0, offsetY = 0;
    bool hidden = false;
    bool shadowHidden = false;
    bool floating = false;
    bool flip = false;
    bool shadowFlip = false;
    bool sleepZ = false;
};

struct HomeHudRow {
    uint8_t slot = 0;
    uint8_t hunger = 0;
    uint8_t hp = 0;
};

// Read-only presentation data. World coordinates retain their save semantics.
struct HomeFrame {
    uint32_t nowMs = 0;
    bool night = false;
    HomeActorVisual main, visitor;
    bool foodPresent = false;
    uint8_t foodIndex = 0, foodBites = 0;
    float foodX = 0, foodY = 0;
    uint8_t hearts = 0;
    bool heartPulse = false;
    float heartLift = 0;
    uint8_t lightSource = 0;
    float lightLift = 0;
    uint16_t gameMinutes = 0;
    uint8_t hudCount = 0;
    HomeHudRow hud[Game::TEAM_CAP] = {};
    bool touchBlocked = false;
    bool showMenu = false;
    bool contactVisible = false, contactChoice = false, contactYes = true;
    char contactText[64] = {};
    bool tutorialVisible = false, tutorialLongPress = false;
    uint8_t tutorialButton = 0;
    char tutorialText[48] = {};
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool walkBoundaryVisible = false;
    const float* routeX = nullptr;
    const float* routeY = nullptr;
    uint8_t routeIndex = 0, routeCount = 0;
    float targetX = 0, targetY = 0;
    char debugText[32] = {};
#endif
};

class HomeHost {
public:
    virtual ~HomeHost() = default;
    virtual HomeFrame homeFrame() const = 0;
    virtual bool homeOverlayActive() const = 0;
    virtual bool handleHomeOverlayButton(const ButtonEvent& event) = 0;
    virtual void cancelHomePair() = 0;
    virtual bool activateHome(HomeAction action) = 0;
    virtual void saveHome() = 0;
    // Shared progression presentation is also used by exploration/battle.
    virtual void renderHomeProgression() = 0;
};

}  // namespace Ui
