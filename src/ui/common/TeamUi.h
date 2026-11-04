#pragma once

#include "core/Scene.h"
#include "core/features/TeamFeature.h"
#include "ui/common/MenuPageHost.h"
#include "ui/UiProfile.h"
#if STICKMON_UI_COMPACT_TOUCH
#include "core/TouchScroll.h"
#endif

namespace Ui {

using TeamAction = Features::Team::Action;

class TeamHost : public virtual MenuPageHost {
public:
    virtual void openTeamStatus(uint8_t slot) = 0;
    virtual void openTeamMoves(uint8_t slot) = 0;
    virtual bool makeTeamFirst(uint8_t slot) = 0;
    virtual bool leaveTeam(uint8_t slot) = 0;
    virtual bool selectingTeamItem() const { return false; }
    virtual void useTeamItemOn(uint8_t) {}
    virtual uint16_t teamItemHp(uint8_t slot) const { return pageState().team[slot].hpCur; }
};

class TeamUi : public Scene {
public:
    explicit TeamUi(TeamHost& host) : host(host) {}
    uint8_t teamCursor = 0;
    uint8_t teamActionCursor = 0;
    bool teamActionOpen = false;
#if STICKMON_UI_COMPACT_TOUCH
    TouchScroll gridTouch;
#endif

    static constexpr bool directTouch() { return Ui::compactTouch; }
    void reset();
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override { renderWithToast(nullptr); }
    void renderWithToast(const char* value);
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return !directTouch(); }
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    uint8_t teamActionCount() const;
    TeamAction teamActionAt(uint8_t index) const;
    const char* teamActionLabel(uint8_t index) const;

private:
    TeamHost& host;
    void activateAction(TeamAction action);
    void renderToast(const char* value);
    void renderActionPopup();
};

}  // namespace Ui
