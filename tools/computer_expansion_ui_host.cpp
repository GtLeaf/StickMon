#include <algorithm>
#include <string>
#include <cassert>
#define private public
#include "ui/common/MarketShopUi.h"
#include "ui/round_touch/MarketShopUi.h"
#undef private
int main(){Game::GameState s{};s.coins=15000;s.gameMinutesTotal=0;Game::MarketShop::ensurePeriod(s);
    Ui::MarketShopUi micro;micro.page_=Ui::MarketShopUi::Page::SHELF;micro.scroll_=424;
    Ui::RoundMarketShopUi round;round.page_=Ui::RoundMarketShopUi::Page::SHELF;round.scroll_=484;
    assert(micro.hit(180,390,s)==Game::MarketShop::EXPANSION);
    assert(round.hit(180,260,s)==Game::MarketShop::EXPANSION);
    round.page_=Ui::RoundMarketShopUi::Page::CONFIRM;round.slot_=Game::MarketShop::EXPANSION;round.period_=0;
    assert(round.hit(180,340,s)==Ui::RoundMarketShopUi::PRIMARY);
    round.page_=Ui::RoundMarketShopUi::Page::RESULT;assert(round.hit(180,340,s)==Ui::RoundMarketShopUi::PRIMARY);
    micro.page_=Ui::MarketShopUi::Page::CONFIRM;micro.slot_=Game::MarketShop::EXPANSION;micro.period_=0;s.coins=0;assert(micro.hit(184,416,s)==-1);
    assert(Ui::RoundMarketStall::width("购买13000")==144);
}
