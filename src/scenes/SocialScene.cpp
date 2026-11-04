#include "scenes/SocialScene.h"

#include "core/GameEngine.h"
#include "core/TraceLog.h"
#include "hardware/EspNowLink.h"
#include "hardware/Hal.h"

void SocialScene::onEnter() {
    auto& service = GameEngine::ins().visitService();
    service.attach(&GameEngine::ins().gameState());
    if (!service.active() && !service.busy()) service.stop();
    EspNowLink::ins().begin();
    lastState = service.state();
    ui.enter(service.viewModel(), Hal::ins().millis());
}

void SocialScene::onExit() {
    auto& service = GameEngine::ins().visitService();
    if (!service.active() && service.state() != State::ENDING) {
        service.stop();
        EspNowLink::ins().end();
    }
}

SceneUpdateResult SocialScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    RenderDemand demand;
    auto& service = GameEngine::ins().visitService();
    const auto model = service.viewModel();
    if (model.state != lastState) {
        STICKMON_TRACEF("[VisitStick] state=%u->%u rooms=%u\n",
                        static_cast<unsigned>(lastState),
                        static_cast<unsigned>(model.state), model.roomCount);
        lastState = model.state;
        ui.stateChanged(model.state, nowMs);
        demand.redraw();
        if (model.state == State::ACTIVE) {
            GameEngine::ins().requestScene(SceneID::MAIN);
            return demand.result();
        }
    }
    ui.update(model, nowMs, demand);
    return demand.result();
}

void SocialScene::render() { ui.render(); }
bool SocialScene::onButton(const ButtonEvent& event) { return ui.onButton(event); }

Ui::SocialFrame SocialScene::socialFrame() const {
    Ui::SocialFrame frame;
    frame.model = GameEngine::ins().visitService().viewModel();
    frame.nowMs = Hal::ins().millis();
    return frame;
}

bool SocialScene::activateSocial(Ui::SocialAction action, uint8_t value) {
    auto& service = GameEngine::ins().visitService();
    switch (action) {
    case Ui::SocialAction::BACK:
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    case Ui::SocialAction::HOST: service.startHost(); break;
    case Ui::SocialAction::SEARCH: service.startSearch(); break;
    case Ui::SocialAction::STOP: service.stop(); break;
    case Ui::SocialAction::SELECT_ROOM: service.selectRoom(value); break;
    case Ui::SocialAction::DECIDE: service.acceptIncoming(value != 0); break;
    case Ui::SocialAction::END_VISIT: service.endVisit(); break;
    }
    return true;
}
