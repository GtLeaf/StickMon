#pragma once

#include "core/Scene.h"
#include "core/VisitSessionService.h"
#include "ui/SocialUi.h"

class SocialScene : public Scene, private Ui::SocialHost {
public:
    SocialScene() : ui(*this) {}
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return ui.touchNavigation(); }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override { return ui.onTouch(event); }
    void cancelTouch() override { Scene::cancelTouch(); ui.cancelTouch(); }

private:
    using State = Communication::VisitSessionService::State;
    Ui::SocialUi ui;
    State lastState = State::IDLE;
    Ui::SocialFrame socialFrame() const override;
    bool activateSocial(Ui::SocialAction action, uint8_t value = 0) override;
};
