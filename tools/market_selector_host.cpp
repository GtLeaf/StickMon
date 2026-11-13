#include "ui/common/MarketShopUi.h"
#include <cassert>
#include <cstdio>
using Event = Platform::IInputDevice::TouchEvent;
using Type = Platform::IInputDevice::TouchType;
int main() {
    Game::GameState state{}; state.coins=20000;
    for(auto& clear:state.explorePoolRerollCounts)clear=1;
    Game::MarketShop::ensurePeriod(state);
    Ui::MarketShopUi ui; ui.open(state);
    for(int i=0;i<40;++i)ui.update(100);
    assert(ui.page()==Ui::MarketShopUi::Page::SHELF);
    uint32_t now=1000;
    auto tap=[&](int x,int y){Event e{Type::DOWN,int16_t(x),int16_t(y),now};ui.touch(e,state);e.type=Type::UP;e.timestampMs+=60;now+=200;return ui.touch(e,state);};
    Event drag{Type::DOWN,184,380,now};ui.touch(drag,state);drag.type=Type::MOVE;drag.y=224;ui.touch(drag,state);drag.type=Type::UP;ui.touch(drag,state);
    assert(ui.scrollOffset()==156&&ui.page()==Ui::MarketShopUi::Page::SHELF);
    tap(184,312);assert(ui.page()==Ui::MarketShopUi::Page::SELECT_ITEM);
    tap(272,262);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    const auto request=tap(272,414);
    assert(request.kind==Ui::MarketShopUi::Request::BUY&&request.slot==Game::MarketShop::NUTRIENT&&request.choice==Game::ItemId::PROTEIN);
    ui.committed(false);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    tap(96,414);assert(ui.page()==Ui::MarketShopUi::Page::SELECT_ITEM);
    tap(184,414);assert(ui.page()==Ui::MarketShopUi::Page::SHELF&&ui.scrollOffset()==156);
    std::puts("market-selector: scroll/capture, attribute choice, request identity, failed-save and back position OK");
}
