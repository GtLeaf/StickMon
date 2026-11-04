#pragma once

#include "ui/common/SocialUiHost.h"

namespace Ui {
class SocialUiBase {
public:
    explicit SocialUiBase(SocialHost& host) : host(host) {}
    virtual ~SocialUiBase() = default;
    SocialUiState state;
    virtual SocialLayout layout() const = 0;
    virtual void render() = 0;
    void enter(const Communication::VisitSessionService::ViewModel& model, uint32_t nowMs);
    void stateChanged(SocialState next, uint32_t nowMs);
    void update(const Communication::VisitSessionService::ViewModel& model,
                uint32_t nowMs, RenderDemand& demand);
    bool onButton(const ButtonEvent& event);
    virtual bool touchNavigation() const { return false; }
    virtual bool onTouch(const Platform::IInputDevice::TouchEvent&) { return false; }
    virtual void cancelTouch() {}

protected:
    SocialHost& host;
};
}  // namespace Ui
