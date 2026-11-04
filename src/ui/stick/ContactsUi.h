#pragma once

#include "ui/common/ContactsUiBase.h"

namespace Ui {
class ContactsUi : public ContactsUiBase {
public:
    explicit ContactsUi(ContactsHost& host) : ContactsUiBase(host) {}
    float storageScroll = 0.0f;
    void reset();
    void enter();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return true; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
protected:
    void resetListScroll() override { storageScroll = 0.0f; }
};
}  // namespace Ui
