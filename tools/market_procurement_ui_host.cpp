#include "ui/common/MarketShopUi.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using UiShop = Ui::MarketShopUi;
using Event = Platform::IInputDevice::TouchEvent;
using Type = Platform::IInputDevice::TouchType;
static void settle(UiShop& ui) { for(int i=0;i<40;++i)ui.update(100); }
struct Driver {
    UiShop ui;
    Game::GameState state{};
    uint32_t now=1000;
    Driver() { state.coins=20000;Game::MarketShop::ensurePeriod(state);ui.open(state);settle(ui); }
    UiShop::Request tap(int x,int y) {
        ui.touch({Type::DOWN,int16_t(x),int16_t(y),now},state);
        now+=60;auto result=ui.touch({Type::UP,int16_t(x),int16_t(y),now},state);now+=140;return result;
    }
    void drag(int start,int finish) {
        ui.touch({Type::DOWN,184,int16_t(start),now},state);
        ui.touch({Type::MOVE,184,int16_t(finish),now+100},state);
        ui.touch({Type::UP,184,int16_t(finish),now+300},state);now+=500;
    }
};
static void adviceFlow() {
    Driver d;assert(d.ui.page()==UiShop::Page::MENU);
    const auto original=d.state;
    d.tap(184,354);assert(d.ui.page()==UiShop::Page::ADVICE);
    auto r=d.tap(184,244);
    assert(r.kind==UiShop::Request::ADVISE&&r.advice==Game::MarketSupply::Advice::EXPLORE);
    assert(r.period==d.state.marketSeason.periodDay&&d.ui.pending());
    assert(std::memcmp(&d.state,&original,sizeof(original))==0); // UI emits only; App owns transaction.
    assert(d.tap(184,300).kind==UiShop::Request::NONE);
    assert(d.tap(184,100).kind==UiShop::Request::NONE&&d.ui.page()==UiShop::Page::ADVICE);
    d.ui.adviceCommitted(false);assert(!d.ui.pending()&&d.ui.page()==UiShop::Page::ADVICE);
    r=d.tap(184,300);assert(r.kind==UiShop::Request::ADVISE&&r.advice==Game::MarketSupply::Advice::GROWTH);
    d.state.marketSupply.pendingAdvice=uint8_t(r.advice);d.ui.adviceCommitted(true);
    assert(d.ui.page()==UiShop::Page::ADVICE_DONE&&!d.ui.pending());
    d.tap(184,414);assert(d.ui.page()==UiShop::Page::MENU);
    d.tap(184,354);r=d.tap(184,390);assert(r.advice==Game::MarketSupply::Advice::RARE);
    d.ui.rejected(Game::MarketShop::Result::STALE);assert(!d.ui.pending()&&d.ui.page()==UiShop::Page::ADVICE);
    r=d.tap(184,390);assert(r.kind==UiShop::Request::ADVISE);d.ui.adviceCommitted(true);
    d.tap(184,100);assert(d.ui.page()==UiShop::Page::MENU);
    d.tap(184,354);d.tap(184,430);assert(d.ui.page()==UiShop::Page::ADVICE);
    d.tap(184,100);assert(d.ui.page()==UiShop::Page::MENU);
    d.tap(184,354);d.tap(184,100);assert(d.ui.page()==UiShop::Page::MENU);
    assert(std::memcmp(d.state.marketSeason.stock,original.marketSeason.stock,sizeof(original.marketSeason.stock))==0);
    d.tap(184,100);assert(d.ui.animating());settle(d.ui);assert(!d.ui.active());
}
// A visible row's lower portion must activate that row, while the space
// below the list must never retain the old Back button's action.
static void iconListTargetsAndOutsideReturn() {
    Driver d;
    d.tap(48,400);assert(d.ui.page()==UiShop::Page::ADVICE);
    assert(d.tap(184,430).kind==UiShop::Request::NONE);
    assert(d.ui.page()==UiShop::Page::ADVICE&&!d.ui.pending());
    auto r=d.tap(184,414);
    assert(r.kind==UiShop::Request::ADVISE&&r.advice==Game::MarketSupply::Advice::RARE);
    assert(d.ui.page()==UiShop::Page::ADVICE&&d.ui.pending());
    assert(d.tap(184,414).kind==UiShop::Request::NONE);
    d.ui.adviceCommitted(false);
    d.ui.touch({Type::DOWN,184,330,d.now},d.state);
    d.ui.touch({Type::MOVE,184,400,d.now+40},d.state);
    assert(d.ui.touch({Type::UP,184,400,d.now+80},d.state).kind==UiShop::Request::NONE);
    assert(!d.ui.pending()&&d.ui.page()==UiShop::Page::ADVICE);
    d.now+=200;d.tap(184,100);assert(d.ui.page()==UiShop::Page::MENU);
    d.tap(48,320);assert(d.ui.page()==UiShop::Page::SHELF);
}
static void shelfFlow() {
    Driver d;d.tap(184,280);assert(d.ui.page()==UiShop::Page::SHELF);
    d.drag(410,254);assert(d.ui.scrollOffset()==156);
    d.tap(184,312);assert(d.ui.page()==UiShop::Page::CONFIRM&&d.ui.selected()==Game::MarketShop::NUTRIENT);
    auto r=d.tap(272,414);assert(r.kind==UiShop::Request::BUY&&r.slot==Game::MarketShop::NUTRIENT);
    assert(r.advice==Game::MarketSupply::Advice::NONE);
    d.ui.committed(false);assert(d.ui.page()==UiShop::Page::CONFIRM&&!d.ui.pending());
    d.tap(96,414);assert(d.ui.page()==UiShop::Page::SHELF&&d.ui.scrollOffset()==156);
    d.tap(184,100);assert(d.ui.page()==UiShop::Page::MENU&&d.ui.scrollOffset()==156);
    assert(!d.ui.update(100)); // No shelf inertia survives home navigation.
    d.tap(184,280);assert(d.ui.page()==UiShop::Page::SHELF&&d.ui.scrollOffset()==156);
    d.tap(184,384);assert(d.ui.page()==UiShop::Page::CONFIRM&&d.ui.selected()==Game::MarketShop::RESET_BERRY);
    d.tap(184,100);assert(d.ui.page()==UiShop::Page::SHELF&&d.ui.scrollOffset()==156);
    d.tap(184,312);r=d.tap(272,414);assert(r.kind==UiShop::Request::BUY);
    d.ui.committed(true,Game::MarketShop::item(d.state,r.slot));assert(d.ui.page()==UiShop::Page::RESULT);
    d.tap(184,100);assert(d.ui.page()==UiShop::Page::SHELF&&d.ui.scrollOffset()==156);
}
static void capturedGestures() {
    Driver d;d.tap(184,280);
    d.ui.touch({Type::DOWN,184,400,d.now},d.state);
    d.ui.touch({Type::MOVE,184,350,d.now+30},d.state);
    d.ui.touch({Type::UP,184,330,d.now+50},d.state);
    assert(d.ui.update(33));
    d.tap(184,300);assert(d.ui.page()==UiShop::Page::SHELF&&!d.ui.update(33));
    d.ui.touch({Type::DOWN,184,300,d.now},d.state);
    d.ui.touch({Type::MOVE,184,100,d.now+50},d.state);
    d.ui.touch({Type::UP,184,100,d.now+100},d.state);
    assert(d.ui.page()==UiShop::Page::SHELF);
    d.ui.cancelTouch();assert(!d.ui.update(100));
    d.ui.touch({Type::DOWN,184,300,d.now+200},d.state);
    d.ui.touch({Type::CANCEL,184,100,d.now+240},d.state);
    d.ui.touch({Type::UP,184,100,d.now+260},d.state);
    assert(d.ui.page()==UiShop::Page::SHELF);
}
int main() {
    iconListTargetsAndOutsideReturn();adviceFlow();shelfFlow();capturedGestures();
    std::puts("market-procurement-ui: menu/advice request-once/save failure/retry/replacement/outside/back, concrete direct confirm, preserved scroll/inertia/cancel OK");
}
