#include "ui/common/MarketTutorUi.h"
#include "ui/common/MarketShopUi.h"
#include "ui/common/MarketCareUi.h"
#include "game/MonsterFactory.h"
#include <cassert>
#include <cstdio>
using Event=Platform::IInputDevice::TouchEvent;
using Type=Platform::IInputDevice::TouchType;
static Game::GameState fixture() {
    Game::GameState s{};s.coins=20000;s.teamCount=1;s.bag.heartScale=2;
    s.team[0]=Game::MonsterFactory::create(133,100);
    s.team[0].move2Id=129;s.team[0].move3Id=44;
    s.storageCount=12;
    for(unsigned i=0;i<12;++i){s.storage[i]=Game::MonsterFactory::create(25,20);s.storage[i].ivPacked=100+i;s.storage[i].metAt=200+i;s.storage[i].bond=40;}
    Game::MarketTutor::ensurePeriod(s);Game::MarketShop::ensurePeriod(s);return s;
}
template<class UI> static void settle(UI& ui){for(int i=0;i<40;++i)ui.update(100);}
template<class UI> static void tap(UI& ui,const Game::GameState& state,int x,int y,uint32_t t){ui.touch({Type::DOWN,int16_t(x),int16_t(y),t},state);ui.touch({Type::UP,int16_t(x),int16_t(y),t+50},state);}
template<class UI> static void testList(UI& ui, const Game::GameState& state,int startY){
    const auto page=ui.page();
    ui.touch({Type::DOWN,184,int16_t(startY),2000},state);
    ui.touch({Type::MOVE,184,int16_t(startY-35),2030},state);
    ui.touch({Type::MOVE,184,int16_t(startY-70),2060},state);
    const auto release=ui.touch({Type::UP,184,int16_t(startY-85),2075},state);
    assert(release.kind==decltype(release.kind)(0)&&ui.page()==page);
    assert(ui.update(33)); // RED: current stall implementations stop on release.
    // A tap during inertia brakes, never selects a row.
    tap(ui,state,184,300,2100);assert(ui.page()==page);
    assert(!ui.update(100));
    // Drag outside and back to origin stays captured, including at a limit.
    ui.touch({Type::DOWN,184,300,2300},state);
    ui.touch({Type::MOVE,184,100,2350},state);
    ui.touch({Type::MOVE,184,300,2400},state);
    ui.touch({Type::UP,184,300,2400},state);
    assert(ui.page()==page);
    ui.cancelTouch();assert(!ui.update(100));
    // A cancelled gesture never reaches navigation on its stale release.
    ui.touch({Type::DOWN,184,300,2600},state);
    ui.touch({Type::MOVE,184,240,2640},state);
    ui.touch({Type::CANCEL,184,100,2650},state);
    ui.touch({Type::UP,184,100,2680},state);assert(ui.page()==page);
    for(int i=0;i<20;++i)ui.update(33);
    assert(!ui.update(100));
}
int main(){
    const auto state=fixture();
    Ui::MarketShopUi shop;shop.open(state);settle(shop);assert(shop.page()==Ui::MarketShopUi::Page::MENU);
    tap(shop,state,184,280,1000);assert(shop.page()==Ui::MarketShopUi::Page::SHELF);
    testList(shop,state,410);
    Ui::MarketTutorUi tutor;tutor.open(state);settle(tutor);tap(tutor,state,184,380,1000);settle(tutor);
    tap(tutor,state,184,310,1200);tap(tutor,state,184,328,1400);
    assert(tutor.page()==Ui::MarketTutorUi::Page::RECALL_LIST);testList(tutor,state,410);
    Ui::MarketCareUi care;care.open(state);settle(care);assert(care.page()==Ui::MarketCareUi::Page::MENU);
    tap(care,state,184,382,1200);assert(care.page()==Ui::MarketCareUi::Page::LIST);testList(care,state,370);
    std::puts("market-stall-scroll: all three real controllers fling, brake, capture reversed/outside drags and stop after cancel/rest OK");
}
