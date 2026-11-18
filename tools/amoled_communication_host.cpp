#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#define private public
#include "AmoledApp.h"
#undef private
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "presentation/VisitMenu.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace AmoledV1;
int main() {
    DesktopPlatform desktop("data");
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    assert(FontResource::ins().begin());
    std::vector<uint16_t> pixels(368 * 448);
    PixelRenderer::bind({pixels.data(), 368, 448, false});
    auto &c = PixelRenderer::canvas();
    c.setNativeText(true);
    using State = Communication::VisitSessionService::State;
    CommunicationViewModel model{};
    model.state = State::ROOM_LIST;
    model.roomCount = 4;
    for (int i = 0; i < 4; ++i) {
        model.rooms[i].roomId = 2;
        model.rooms[i].mac[5] = uint8_t(i + 1);
    }
    assert(communicationItemAt(100, 175, model) == -1); // gap between cards
    assert(communicationItemAt(100, 398, model) == Ui::VisitMenu::CANCEL);
    for (int state = 0; state < 12; ++state) {
        model.state = State(state);
        model.error = state == 10 ? "TEAM NOT SOLO" : nullptr;
        renderCommunicationScreen(c, model, 0, 448);
        auto full = pixels;
        std::fill(pixels.begin(), pixels.end(), 0xf81f);
        for (int y = 0; y < 448; y += 19)
            renderCommunicationScreen(c, model, y, std::min(448, y + 19));
        assert(full == pixels);
    }
    AmoledApp app;
    app.begin(desktop.millis());
    app.cancelTouch();
    app.gameState.oobeDone = true;
    app.gameState.teamCount = 2;
    app.gameState.team[0] = Game::MonsterFactory::create(1, 20);
    app.gameState.team[1] = Game::MonsterFactory::create(4, 20);
    app.visitSession.attach(&app.gameState);
    app.visitSession.startHost();
    assert(app.visitSession.state() == State::FAILED);
    app.sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
    auto tap = [&](int x, int y) {
        app.render(c);
        app.handleTouch({TouchEventType::DOWN, int16_t(x), int16_t(y), 0, 0, desktop.millis()});
        desktop.advanceMs(40);
        app.handleTouch({TouchEventType::UP, int16_t(x), int16_t(y), 0, 0, desktop.millis()});
    };
    tap(180, 370);
    assert(app.sceneFlow.current() == AppSceneFlow::Scene::TEAM);
    assert(app.visitSession.state() == State::IDLE);
    assert(!EspNowLink::ins().isEnabled());
    app.sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
    app.visitSession.startSearch();
    tap(180, 416);
    assert(app.visitSession.state() == State::IDLE);
    // A press must not turn into ACCEPT when a request arrives before release.
    app.visitSession.state_ = State::IDLE;
    app.sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
    app.render(c);
    app.handleTouch({TouchEventType::DOWN, 180, 270, 0, 0, desktop.millis()});
    app.visitSession.state_ = State::WAITING_HOST_DECISION;
    app.visitSession.incomingRequest_ = true;
    desktop.advanceMs(40);
    app.handleTouch({TouchEventType::UP, 180, 270, 0, 0, desktop.millis()});
    assert(app.visitSession.state() == State::WAITING_HOST_DECISION);
    // A room replaced at the same list index cannot be selected on release.
    app.visitSession.stop();
    app.visitSession.state_ = State::ROOM_LIST;
    auto &link = EspNowLink::ins();
    link.roomCountValue = 1;
    link.rooms[0].roomId = 2;
    link.rooms[0].mac[5] = 1;
    app.render(c);
    app.handleTouch({TouchEventType::DOWN, 180, 135, 0, 0, desktop.millis()});
    link.rooms[0].mac[5] = 2;
    desktop.advanceMs(40);
    app.handleTouch({TouchEventType::UP, 180, 135, 0, 0, desktop.millis()});
    assert(app.visitSession.state() == State::ROOM_LIST);
    // A changed requester, including the same MAC with a different request sequence, is not
    // accepted.
    app.visitSession.state_ = State::WAITING_HOST_DECISION;
    app.visitSession.incomingRequest_ = true;
    app.visitSession.pendingJoinSeq_ = 20;
    app.render(c);
    app.handleTouch({TouchEventType::DOWN, 180, 270, 0, 0, desktop.millis()});
    app.visitSession.pendingJoinSeq_ = 21;
    desktop.advanceMs(40);
    app.handleTouch({TouchEventType::UP, 180, 270, 0, 0, desktop.millis()});
    assert(app.visitSession.state() == State::WAITING_HOST_DECISION);
    app.visitSession.stop();
    link.end();
    std::puts("PASS AMOLED: real touch cancellation, changed state/room suppression, adjust-team "
              "navigation and 12-state dirty-band rendering");
}
