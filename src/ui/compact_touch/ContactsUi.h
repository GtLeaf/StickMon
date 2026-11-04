#pragma once

#include "ui/common/ContactsUiBase.h"
#include "core/TouchScroll.h"
#include "presentation/MenuGrid.h"

namespace Ui {

class ContactsUi : public ContactsUiBase {
public:
    explicit ContactsUi(ContactsHost& host) : ContactsUiBase(host) {}
    float storageScroll = 0;
    TouchScroll listTouch;
    bool revealCursor = true;

    static constexpr bool directTouch() { return true; }
    void reset();
    void enter();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override { Scene::cancelTouch(); listTouch.cancel(); }
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;

protected:
    void resetListScroll() override { storageScroll = 0; }
    bool backItemsVisible() const override { return false; }

private:
    MenuGrid::Layout listLayout() const;
    MenuGrid::Rect actionBounds() const;
    MenuGrid::Rect confirmBounds() const;
    MenuGrid::Rect confirmChoice(bool yes) const;
    void renderActions();
    void renderConfirm();
};

}  // namespace Ui
