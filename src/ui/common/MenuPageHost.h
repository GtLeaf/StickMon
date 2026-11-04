#pragma once

#include "game/GameState.h"

namespace Ui {

// Application navigation and notifications shared by the delegated menu pages.
class MenuPageHost {
public:
    virtual ~MenuPageHost() = default;
    virtual const Game::GameState& pageState() const = 0;
    virtual bool pageExploreContext() const = 0;
    virtual void pageBack() = 0;
    virtual void pageHome() = 0;
    // The host must copy text before returning; callers may use temporary text.
    virtual void pageNotice(const char* text, uint32_t durationMs = 1100) = 0;
    virtual void pageBackTutorial() = 0;
    virtual void pageNavigationTutorial() = 0;
};

}  // namespace Ui
