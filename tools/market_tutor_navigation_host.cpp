// Public controller replay: scene taps replace Back, wide actions retain the
// payment handshake, and the recall viewport exposes four complete rows.
#include "ui/common/MarketTutorUi.h"
#include "game/MonsterFactory.h"
#include <cstdio>
#include <stdexcept>

using UI = Ui::MarketTutorUi;
using Page = UI::Page;
using Type = Platform::IInputDevice::TouchType;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

struct Replay {
    Game::GameState state{};
    UI ui;
    uint32_t clock = 1000;
    Replay() {
        state.teamCount = 1; state.coins = 3000; state.bag.heartScale = 2;
        state.team[0] = Game::MonsterFactory::create(133, 100);
        state.team[0].move2Id = 129; state.team[0].move3Id = 44;
        state.team[0].ivPacked = 12; state.team[0].metAt = 24;
        Game::MarketTutor::ensurePeriod(state);
        state.marketTutor.move = 247; state.marketTutor.drawnSpecies = 133;
        ui.open(state); settle(); tap(184, 380); settle();
        CHECK(ui.page() == Page::MENU);
    }
    void settle() { for (int i = 0; i < 6; ++i) ui.update(100); }
    UI::Request tap(int x, int y) {
        ui.touch({Type::DOWN, int16_t(x), int16_t(y), clock}, state);
        const auto request = ui.touch({Type::UP, int16_t(x), int16_t(y), clock + 60}, state);
        clock += 200; return request;
    }
    void teachConfirm() {
        tap(184, 244); tap(184, 328); tap(184, 294);
        CHECK(ui.page() == Page::CONFIRM);
    }
};

// A lingering footer hit would return to the parent from any of these pages.
static void footerIsInert() {
    Replay r;
    r.tap(184, 244); CHECK(r.ui.page() == Page::TUTOR);
    r.tap(184, 414); CHECK(r.ui.page() == Page::TUTOR);
    r.tap(184, 328); CHECK(r.ui.page() == Page::SLOT);
    r.tap(184, 414); CHECK(r.ui.page() == Page::SLOT);
    r.tap(184, 100); CHECK(r.ui.page() == Page::TUTOR);
    r.tap(184, 100); CHECK(r.ui.page() == Page::MENU);
    for (int serviceY : {380, 310}) {
        r.tap(184, serviceY);
        const auto page = r.ui.page();
        CHECK(page == Page::TRAIN_MEMBER || page == Page::RECALL_MEMBER);
        r.tap(184, 414); CHECK(r.ui.page() == page);
        r.tap(184, 207); CHECK(r.ui.page() == Page::MENU);
    }
}

// The former Back half and the middle gap now both issue the primary action.
static void wideActions() {
    for (int actionX : {24, 96, 184, 272, 343}) {
        Replay r; r.teachConfirm();
        auto request = r.tap(actionX, 414);
        CHECK(request.kind == UI::Request::TEACH && request.member == 0 && request.slot == 1);
        CHECK(r.ui.page() == Page::CONFIRM && r.ui.pending());
        CHECK(r.tap(actionX, 414).kind == UI::Request::NONE);
        r.ui.committed(false, r.state);
        CHECK(r.ui.page() == Page::CONFIRM && !r.ui.pending() && r.state.coins == 3000);
        CHECK(r.tap(actionX, 414).kind == UI::Request::TEACH);

        Replay training; training.tap(184, 380); training.tap(184, 328);
        CHECK(training.ui.page() == Page::TRAIN_MOVE);
        CHECK(training.tap(actionX, 414).kind == UI::Request::TRAIN);

        Replay recall; recall.tap(184, 310); recall.tap(184, 328); recall.tap(184, 244); recall.tap(184, 294);
        CHECK(recall.ui.page() == Page::CONFIRM);
        CHECK(recall.tap(actionX, 414).kind == UI::Request::RECALL);
    }
}

// The fourth visible row must select move index3, rather than navigate Back.
static void fourRecallRows() {
    Replay r; r.tap(184, 310); r.tap(184, 328);
    CHECK(r.ui.page() == Page::RECALL_LIST);
    Game::MoveId moves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
    const auto count = Game::MoveManagementService::collectRecallable(*findSpecies(133), r.state.team[0], moves,
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
    CHECK(count > 4);
    r.tap(184, 439); CHECK(r.ui.page() == Page::SLOT);
    r.tap(184, 294); CHECK(r.ui.page() == Page::CONFIRM);
    const auto request = r.tap(184, 414);
    CHECK(request.kind == UI::Request::RECALL && request.move == moves[3]);
}

static void outsideReturnsOneLevel() {
    Replay r; r.teachConfirm();
    for (auto expected : {Page::SLOT, Page::TUTOR, Page::MENU}) {
        r.tap(184, 208); CHECK(!r.ui.animating());
        r.tap(184, 207); CHECK(r.ui.page() == expected && !r.ui.animating());
    }
    r.tap(184, 207); CHECK(r.ui.animating()); r.settle(); CHECK(!r.ui.active());
    Replay train; train.tap(184, 380); train.tap(184, 328);
    train.tap(184, 100); CHECK(train.ui.page() == Page::TRAIN_MEMBER);
    train.tap(184, 100); CHECK(train.ui.page() == Page::MENU);
    Replay recall; recall.tap(184, 310); recall.tap(184, 328); recall.tap(184, 244); recall.tap(184, 294);
    for (auto expected : {Page::SLOT, Page::RECALL_LIST, Page::RECALL_MEMBER, Page::MENU}) {
        recall.tap(184, 100); CHECK(recall.ui.page() == expected && !recall.ui.animating());
    }
}

static void performanceAndLeave() {
    for (bool outside : {false, true}) {
        Replay r; r.teachConfirm(); CHECK(r.tap(184, 414).kind == UI::Request::TEACH);
        CHECK(Game::MarketTutor::teach(r.state, 0, 1) == Game::MarketTutor::Result::OK);
        r.ui.committed(true, r.state); CHECK(r.ui.page() == Page::TEACHING);
        r.ui.touch({Type::DOWN, 184, 100, r.clock}, r.state);
        r.ui.update(250); r.ui.update(150);
        r.ui.touch({Type::UP, 184, 100, r.clock + 500}, r.state); r.clock += 600;
        CHECK(r.ui.page() == Page::TEACHING);
        r.tap(184, 100); CHECK(r.ui.page() == Page::RESULT && !r.ui.animating());
        CHECK(r.state.coins == 2600 && r.state.team[0].move2Id == 247);
        r.tap(184, outside ? 100 : 414); CHECK(r.ui.animating());
        r.settle(); CHECK(!r.ui.active() && r.state.coins == 2600);
    }
}

int main() {
    int failures = 0;
    const auto run = [&](const char* name, void (*test)()) {
        try { test(); std::printf("PASS %s\n", name); }
        catch (const std::exception& e) { std::printf("FAIL %s: %s\n", name, e.what()); ++failures; }
    };
    run("footer area is inert on tutor, slot and member pages", footerIsInert);
    run("wide teach/train/recall action and save retry", wideActions);
    run("fourth recall row selects the fourth move", fourRecallRows);
    run("scene returns exactly one level and menu closes", outsideReturnsOneLevel);
    run("performance400ms guard, skip and both Leave targets", performanceAndLeave);
    return failures ? 1 : 0;
}
