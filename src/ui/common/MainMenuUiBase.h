#pragma once

#include "core/AppSceneFlow.h"
#include "core/BuildConfig.h"
#include "core/Scene.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {

class MainMenuHost : public virtual MenuPageHost {
public:
    virtual bool activateMainMenu(uint8_t index) = 0;
    virtual void openMainMenuFood() = 0;
    virtual void leaveMainMenu() = 0;
};

class MainMenuUiBase : public Scene {
public:
    explicit MainMenuUiBase(MainMenuHost& host) : host(host) {}
    static constexpr uint8_t ITEM_COUNT =
        AppSceneFlow::mainMenuItemCount(STICKMON_ENABLE_DEBUG_FEATURES != 0);
    int8_t cursor = 0;
    static int8_t lastCursor;
    void rememberCursor() { lastCursor = cursor; }

protected:
    MainMenuHost& host;
    void resetState(bool fromHome);
    bool advanceCursor();
    bool handleButton(const ButtonEvent& event);
};

}  // namespace Ui
