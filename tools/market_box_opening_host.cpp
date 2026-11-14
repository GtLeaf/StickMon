#include "ui/common/MarketShopUi.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using UI = Ui::MarketShopUi;
using Page = UI::Page;
using Type = Platform::IInputDevice::TouchType;
struct Driver {
    Game::GameState state{};
    UI ui;
    uint32_t time = 1000;
    Driver() {
        state.coins = 20000;
        Game::MarketShop::ensurePeriod(state);
        ui.open(state);advance(4000);tap(184,280);
    }
    void advance(int ms) { while(ms>0){const int step=std::min(ms,33);ui.update(step);ms-=step;} }
    UI::Request event(Type type,int x,int y,int dt=40) {
        time+=dt;return ui.touch({type,int16_t(x),int16_t(y),time},state);
    }
    UI::Request tap(int x,int y) { event(Type::DOWN,x,y);return event(Type::UP,x,y); }
    void selectBox() {
        event(Type::DOWN,184,410);event(Type::MOVE,184,58,100);event(Type::UP,184,58,200);
        assert(ui.scrollOffset()==352);tap(184,404);assert(ui.page()==Page::CONFIRM);
    }
    void clearSound() { SfxCue cue;while(ui.takeSfx(cue)){} }
    void silent() { SfxCue cue;assert(!ui.takeSfx(cue)); }
    void buyBox() {
        selectBox();auto r=tap(272,414);assert(r.kind==UI::Request::BUY&&r.slot==Game::MarketShop::BOX);
        Game::MarketShop::Reward reward;
        assert(Game::MarketShop::buy(state,r.slot,r.period,&reward)==Game::MarketShop::Result::OK);
        clearSound();ui.committed(true,reward.item,reward.amount);
    }
};
static void explicitOpeningAndOnceOnlySound() {
    Driver d;d.buyBox();
    // Regression: a paid box used to reveal instantly and play ITEM_GET here.
    assert(d.ui.page()!=Page::RESULT);d.silent();
    const auto arrivedFrom=d.ui.page();const auto bought=d.state;
    d.event(Type::UP,272,414);d.tap(184,316);d.tap(184,100);
    assert(d.ui.page()==arrivedFrom);d.advance(599);assert(d.ui.page()==arrivedFrom);
    d.advance(1);const auto waiting=d.ui.page();assert(waiting!=arrivedFrom&&waiting!=Page::RESULT);
    d.event(Type::UP,184,316);d.tap(184,100);d.advance(60000);
    assert(d.ui.page()==waiting);d.silent(); // waiting never auto-opens
    d.event(Type::DOWN,184,316);d.event(Type::MOVE,184,280);d.event(Type::MOVE,184,316);d.event(Type::UP,184,316);
    assert(d.ui.page()==waiting);
    d.event(Type::DOWN,184,316);d.ui.cancelTouch();d.event(Type::UP,184,316);
    assert(d.ui.page()==waiting);
    d.tap(184,400);const auto opening=d.ui.page();assert(opening!=waiting&&opening!=Page::RESULT);d.silent();
    d.tap(184,400);d.tap(184,100);d.advance(899);assert(d.ui.page()==opening);d.silent();
    assert(!d.ui.update(0));d.silent();assert(d.ui.page()==opening);
    d.advance(1);SfxCue cue;assert(d.ui.takeSfx(cue)&&cue==SfxCue::ITEM_GET);d.silent();
    d.event(Type::DOWN,184,414);d.advance(349);assert(d.ui.page()==opening);
    d.advance(1);assert(d.ui.page()==Page::RESULT);d.event(Type::UP,184,414);
    assert(d.ui.page()==Page::RESULT);d.advance(4000);d.silent();
    assert(!std::memcmp(&bought,&d.state,sizeof(bought)));
    d.tap(184,414);assert(d.ui.page()==Page::SHELF&&d.ui.scrollOffset()==352);
}
static void failedSaveAndClose() {
    Driver d;d.selectBox();const auto before=d.state;
    assert(d.tap(272,414).kind==UI::Request::BUY);d.clearSound();d.ui.committed(false);
    SfxCue cue;assert(d.ui.page()==Page::CONFIRM&&d.ui.takeSfx(cue)&&cue==SfxCue::UI_CANCEL);
    assert(!std::memcmp(&before,&d.state,sizeof(before)));
    assert(d.tap(272,414).kind==UI::Request::BUY);d.ui.committed(true,Game::ItemId::CANDY,1);
    assert(d.ui.page()!=Page::RESULT);d.advance(600);d.tap(184,316);d.advance(850);
    d.ui.close();assert(!d.ui.active()&&!d.ui.update(200));d.silent();
}
static void ordinaryPurchaseRemainsImmediate() {
    Driver d;d.tap(184,252);assert(d.ui.page()==Page::CONFIRM);
    assert(d.tap(272,414).kind==UI::Request::BUY);d.clearSound();d.ui.committed(true,Game::ItemId::POTION,1);
    SfxCue cue;assert(d.ui.page()==Page::RESULT&&d.ui.takeSfx(cue)&&cue==SfxCue::BUY_ITEM);d.silent();
}
int main() {
    explicitOpeningAndOnceOnlySound();failedSaveAndClose();ordinaryPurchaseRemainsImmediate();
    std::puts("market-box-opening: durable-only arrival, explicit tap, cancellation/drag guards, deferred once-only sound, guarded result and ordinary purchase OK");
}
