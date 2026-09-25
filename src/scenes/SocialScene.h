#pragma once

#include "core/Scene.h"
#include "core/VisitSessionService.h"

class SocialScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    using State = Communication::VisitSessionService::State;

    uint8_t cursor = 0;
    uint8_t roomCursor = 0;
    State lastState = State::IDLE;
    uint8_t lastRoomCount = 0;
    uint32_t stateStartedMs = 0;
    uint32_t lastVisualTick = UINT32_MAX;

    void renderMenu();
    void renderRoomList(const Communication::VisitSessionService::ViewModel& model);
    void renderStatus(const char* title, uint32_t nowMs,
                      const char* detail = nullptr);
};
