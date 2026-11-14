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
    assert(ui.page()==Ui::MarketShopUi::Page::MENU);
    uint32_t now=1000;
    auto tap=[&](int x,int y){Event e{Type::DOWN,int16_t(x),int16_t(y),now};ui.touch(e,state);e.type=Type::UP;e.timestampMs+=60;now+=200;return ui.touch(e,state);};
    tap(184,280);assert(ui.page()==Ui::MarketShopUi::Page::SHELF);
    // Empty stock is a disabled shelf row, including when a press starts on it.
    // The same surface must remain a drag origin for reaching later stock.
    state.marketSeason.stock[0]=0;
    assert(tap(184,252).kind==Ui::MarketShopUi::Request::NONE);
    assert(ui.page()==Ui::MarketShopUi::Page::SHELF);
    Event emptyDrag{Type::DOWN,184,252,now};ui.touch(emptyDrag,state);
    emptyDrag.type=Type::MOVE;emptyDrag.y=220;emptyDrag.timestampMs+=100;ui.touch(emptyDrag,state);
    emptyDrag.type=Type::UP;emptyDrag.timestampMs+=200;ui.touch(emptyDrag,state);
    now+=400;
    assert(ui.scrollOffset()==32&&ui.page()==Ui::MarketShopUi::Page::SHELF);
    ui.open(state);for(int i=0;i<40;++i)ui.update(100);tap(184,280);
    // Available stock remains inspectable with no money or no inventory room;
    // purchase rejection belongs to the confirm page, not the shelf hit test.
    state.coins=0;
    tap(184,324);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    assert(tap(272,414).kind==Ui::MarketShopUi::Request::NONE&&!ui.pending());
    tap(96,414);assert(ui.page()==Ui::MarketShopUi::Page::SHELF);
    state.coins=20000;
    const auto secondItem=Game::MarketShop::item(state,Game::MarketShop::DISCOUNT_B);
    while(Game::ItemInventory::add(state,secondItem)){}
    tap(184,324);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    assert(tap(272,414).kind==Ui::MarketShopUi::Request::NONE&&!ui.pending());
    tap(96,414);assert(ui.page()==Ui::MarketShopUi::Page::SHELF);
    Event drag{Type::DOWN,184,380,now};ui.touch(drag,state);drag.type=Type::MOVE;drag.y=224;ui.touch(drag,state);drag.type=Type::UP;ui.touch(drag,state);
    assert(ui.scrollOffset()==156&&ui.page()==Ui::MarketShopUi::Page::SHELF);
    tap(184,312);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    const auto request=tap(272,414);
    assert(request.kind==Ui::MarketShopUi::Request::BUY&&request.slot==Game::MarketShop::NUTRIENT&&request.advice==Game::MarketSupply::Advice::NONE);
    ui.committed(false);assert(ui.page()==Ui::MarketShopUi::Page::CONFIRM);
    tap(96,414);assert(ui.page()==Ui::MarketShopUi::Page::SHELF&&ui.scrollOffset()==156);
    std::puts("market-selector: sold-out disable/drag, scroll/capture, concrete direct confirm, request identity, failed-save and back position OK");
}
