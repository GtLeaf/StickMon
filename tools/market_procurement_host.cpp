#include <cassert>
#include <cstdio>
#include "game/MarketSupply.h"
#include "game/MarketShopService.h"
#include "core/SaveCodec.h"
#include <cstring>
#include <type_traits>
#include <initializer_list>
using namespace Game;
static void plans() {
    constexpr uint8_t want[][8] = {
        {3,3,1,2,3,2,1,1}, {4,4,1,1,2,2,1,1}, {2,2,1,3,4,2,1,1},
        {2,3,2,2,3,2,1,1}, {3,2,1,2,3,2,1,2}, {2,2,2,2,3,2,1,2}};
    for(uint8_t p=0;p<6;++p) {
        assert(MarketSupply::validPlan(p)); unsigned total=0;
        for(unsigned i=0;i<8;++i){assert(MarketSupply::capacities(p)[i]==want[p][i]);total+=MarketSupply::capacities(p)[i];}
        assert(total==16 && MarketSupply::initialBoxStock(p)==want[p][7]);
    }
    assert(!MarketSupply::validPlan(6) && !MarketSupply::validPlan(255));
    assert(!MarketSupply::validAdvice(0) && !MarketSupply::validAdvice(4));
    for(uint8_t advice=1;advice<=3;++advice)assert(MarketSupply::validAdvice(advice));
}
static GameState fresh(uint32_t day=0) {
    GameState s{};s.coins=100000;s.gameMinutesTotal=day*1440;
    return s;
}
static void concreteDraws() {
    unsigned nutrient[6]={},berry[6]={};
    unsigned openings=0;
    for(uint32_t day=0;day<2000;++day) {
        if(!MarketSchedule::isOpenDay(day))continue;
        auto s=fresh(day);assert(MarketShop::ensurePeriod(s));++openings;
        const auto n=MarketShop::item(s,MarketShop::NUTRIENT),b=MarketShop::item(s,MarketShop::RESET_BERRY);
        assert(MarketItemCatalog::nutrient(n) && MarketItemCatalog::resetBerry(b));
        ++nutrient[uint8_t(n)-uint8_t(ItemId::HP_UP)];
        ++berry[uint8_t(b)-uint8_t(ItemId::POMEG_BERRY)];
        const auto shelf=s.marketSeason;
        s.team[0].ivPacked^=day+123;s.team[0].metAt+=17;
        assert(!MarketShop::ensurePeriod(s));
        assert(!std::memcmp(&shelf,&s.marketSeason,sizeof(shelf)));
        MarketShop::Reward r{};
        assert(MarketShop::buy(s,MarketShop::NUTRIENT,day,&r)==MarketShop::Result::OK && r.item==n);
        assert(MarketShop::buy(s,MarketShop::NUTRIENT,day,&r)==MarketShop::Result::OK && r.item==n);
        assert(MarketShop::buy(s,MarketShop::RESET_BERRY,day,&r)==MarketShop::Result::OK && r.item==b);
    }
    assert(openings>100);
    for(unsigned i=0;i<6;++i){assert(nutrient[i]>openings/10 && nutrient[i]<openings/4);assert(berry[i]>openings/10 && berry[i]<openings/4);}
}
static void adviceAndSupply() {
    using MarketSupply::Advice;
    static_assert(std::is_same_v<decltype(&MarketShop::item),ItemId(*)(const GameState&,uint8_t)>);
    static_assert(std::is_same_v<decltype(&MarketShop::check),MarketShop::Result(*)(const GameState&,uint8_t,uint32_t)>);
    static_assert(std::is_same_v<decltype(&MarketShop::buy),MarketShop::Result(*)(GameState&,uint8_t,uint32_t,MarketShop::Reward*)>);
    auto s=fresh();assert(MarketShop::ensurePeriod(s));
    s.marketSeason.stock[0]=1;s.marketSeason.stock[7]=0;
    const auto before=s;
    for(auto advice:{Advice::NONE,Advice(4),Advice(255)}) {
        assert(MarketShop::advise(s,advice,0)!=MarketShop::Result::OK);
        assert(!std::memcmp(&s,&before,sizeof(s)));
    }
    assert(MarketShop::advise(s,Advice::EXPLORE,3)==MarketShop::Result::STALE);
    assert(MarketShop::advise(s,Advice::EXPLORE,0)==MarketShop::Result::OK);
    assert(s.marketSupply.pendingAdvice==1);
    assert(MarketShop::advise(s,Advice::GROWTH,0)==MarketShop::Result::OK);
    assert(s.marketSupply.pendingAdvice==2 && !std::memcmp(&s.marketSeason,&before.marketSeason,sizeof(s.marketSeason)));
    assert(!MarketShop::ensurePeriod(s) && s.marketSupply.pendingAdvice==2);
    s.gameMinutesTotal=1440;const auto closed=s;
    assert(!MarketShop::ensurePeriod(s));
    assert(MarketShop::advise(s,Advice::RARE,0)==MarketShop::Result::STALE && !std::memcmp(&closed,&s,sizeof(s)));
    s.gameMinutesTotal=7*1440; // skipped opening on day 3 consumes once at actual opening
    assert(MarketShop::ensurePeriod(s));assert(s.marketSupply.stockPlan==2 && s.marketSupply.pendingAdvice==0);
    const uint8_t growth[]={2,2,1,3,4,2,1,1};assert(!std::memcmp(growth,s.marketSeason.stock,8));
    assert(!MarketShop::ensurePeriod(s) && s.marketSupply.stockPlan==2);
    s.gameMinutesTotal=10*1440;assert(MarketShop::ensurePeriod(s));
    const uint8_t normal[]={3,3,1,2,3,2,1,1};assert(!std::memcmp(normal,s.marketSeason.stock,8));
    assert(s.marketSupply.stockPlan==0 && s.marketSupply.pendingAdvice==0 && s.marketSupply.boxRoll2==0);
    unsigned seen[6]={};unsigned rareTotal=0;
    for(uint32_t day=0;day<4000;++day) {
        if(!MarketSchedule::isOpenDay(day))continue;
        auto baseline=fresh(day);assert(MarketShop::ensurePeriod(baseline));
        for(uint8_t advice=1;advice<=3;++advice){
            auto planned=fresh(day);planned.marketSupply.pendingAdvice=advice;assert(MarketShop::ensurePeriod(planned));
            assert(planned.marketSupply.pendingAdvice==0);
            assert(!std::memcmp(baseline.marketSeason.items,planned.marketSeason.items,8));
            assert(planned.marketSeason.boxRoll==baseline.marketSeason.boxRoll);
            for(uint8_t slot=0;slot<8;++slot)assert(MarketShop::priceForSlot(planned,slot)==MarketShop::priceForSlot(baseline,slot));
            const auto plan=planned.marketSupply.stockPlan;assert(advice==3 || plan==advice);
            assert(advice!=3 || (plan>=3 && plan<=5));++seen[plan];if(advice==3)++rareTotal;
            assert(!std::memcmp(planned.marketSeason.stock,MarketSupply::capacities(plan),8));
            assert(MarketSupply::initialBoxStock(plan)==2 ? (planned.marketSupply.boxRoll2>=1 && planned.marketSupply.boxRoll2<=100) : planned.marketSupply.boxRoll2==0);
        }
    }
    for(unsigned p=1;p<6;++p)assert(seen[p]);
    for(unsigned p=3;p<6;++p)assert(seen[p]>rareTotal/4 && seen[p]<rareTotal*5/12);
}
static void twoBoxes() {
    bool distinct=false,repeated=false;
    for(uint32_t day=0;day<4000;++day) {
        if(!MarketSchedule::isOpenDay(day))continue;
        auto s=fresh(day);s.marketSupply.pendingAdvice=3;assert(MarketShop::ensurePeriod(s));
        if(s.marketSeason.stock[MarketShop::BOX]!=2)continue;
        const uint8_t first=s.marketSeason.boxRoll,second=s.marketSupply.boxRoll2;
        assert(first>=1 && first<=100 && second>=1 && second<=100);
        distinct|=first!=second;repeated|=first==second;
        const auto r1=MarketShop::boxRewardForRoll(s,uint8_t(first-1));
        const auto r2=MarketShop::boxRewardForRoll(s,uint8_t(second-1));
        auto poor=s;poor.coins=299;const auto frozen=poor;
        assert(MarketShop::buy(poor,MarketShop::BOX,day)==MarketShop::Result::NO_COINS && !std::memcmp(&frozen,&poor,sizeof(poor)));
        MarketShop::Reward r;
        assert(MarketShop::buy(s,MarketShop::BOX,day,&r)==MarketShop::Result::OK && r.item==r1.item && r.amount==r1.amount);
        assert(s.marketSeason.stock[MarketShop::BOX]==1 && s.marketSeason.boxRoll==first && s.marketSupply.boxRoll2==second);
        s.team[0].ivPacked^=1234;assert(!MarketShop::ensurePeriod(s));
        auto full=s;const auto count=ItemInventory::count(full,r2.item);
        assert(ItemInventory::add(full,r2.item,ITEM_STACK_CAP-count));const auto fullBefore=full;
        assert(MarketShop::buy(full,MarketShop::BOX,day)==MarketShop::Result::BAG_FULL && !std::memcmp(&fullBefore,&full,sizeof(full)));
        const auto beforeSave=s;
        auto failed=s;assert(MarketShop::buy(failed,MarketShop::BOX,day,&r)==MarketShop::Result::OK);failed=beforeSave;
        assert(MarketShop::buy(failed,MarketShop::BOX,day,&r)==MarketShop::Result::OK && r.item==r2.item && r.amount==r2.amount);
        assert(MarketShop::buy(s,MarketShop::BOX,day,&r)==MarketShop::Result::OK && r.item==r2.item && r.amount==r2.amount);
        assert(s.marketSeason.stock[MarketShop::BOX]==0);
        const auto empty=s;assert(MarketShop::buy(s,MarketShop::BOX,day)==MarketShop::Result::SOLD_OUT && !std::memcmp(&empty,&s,sizeof(s)));
    }
    assert(distinct && repeated);
    auto s=fresh();s.marketSupply.stockPlan=5;MarketShop::ensurePeriod(s);
    s.marketSupply.stockPlan=5;s.marketSeason.stock[MarketShop::CANDY]=2;s.marketSeason.stock[MarketShop::BOX]=2;
    s.marketSeason.boxRoll=100;s.marketSupply.boxRoll2=100;
    const auto candy=s.bag.candy;
    for(unsigned i=0;i<2;++i){assert(MarketShop::buy(s,MarketShop::CANDY,0)==MarketShop::Result::OK);assert(MarketShop::buy(s,MarketShop::BOX,0)==MarketShop::Result::OK);}
    assert(s.bag.candy==candy+4 && s.candyPurchasesToday==0);
}
static GameState restart(const GameState& s) {
    static uint8_t bytes[SaveCodec::MAX_ENCODED_BYTES];size_t length=0;
    assert(SaveCodec::encode(s,{},19,bytes,sizeof(bytes),length));
    SaveCodec::Snapshot decoded;assert(SaveCodec::decode(bytes,length,decoded));
    assert(!std::memcmp(&s.marketSeason,&decoded.state.marketSeason,sizeof(s.marketSeason)));
    assert(!std::memcmp(&s.marketSupply,&decoded.state.marketSupply,sizeof(s.marketSupply)));
    return decoded.state;
}
static void restartFreezesAdviceAndDraws() {
    auto s=fresh();assert(MarketShop::ensurePeriod(s));
    assert(MarketShop::buy(s,MarketShop::NUTRIENT,0)==MarketShop::Result::OK);
    assert(MarketShop::advise(s,MarketSupply::Advice::GROWTH,0)==MarketShop::Result::OK);
    auto loaded=restart(s);assert(!MarketShop::ensurePeriod(loaded));
    loaded.gameMinutesTotal=3*1440;assert(MarketShop::ensurePeriod(loaded));
    assert(loaded.marketSupply.stockPlan==2 && loaded.marketSupply.pendingAdvice==0);
    loaded=restart(loaded);assert(!MarketShop::ensurePeriod(loaded));
    loaded.gameMinutesTotal=7*1440;assert(MarketShop::ensurePeriod(loaded));assert(loaded.marketSupply.stockPlan==0);
    for(uint32_t day=0;day<100;++day) {
        if(!MarketSchedule::isOpenDay(day))continue;
        s=fresh(day);s.marketSupply.pendingAdvice=3;assert(MarketShop::ensurePeriod(s));
        if(s.marketSeason.stock[MarketShop::BOX]!=2)continue;
        loaded=restart(s);assert(!MarketShop::ensurePeriod(loaded));
        const auto first=MarketShop::boxRewardForRoll(loaded,uint8_t(loaded.marketSeason.boxRoll-1));
        const auto second=MarketShop::boxRewardForRoll(loaded,uint8_t(loaded.marketSupply.boxRoll2-1));
        MarketShop::Reward r{};
        assert(MarketShop::buy(loaded,MarketShop::BOX,day,&r)==MarketShop::Result::OK && r.item==first.item && r.amount==first.amount);
        loaded=restart(loaded);loaded.team[0].ivPacked^=123;assert(!MarketShop::ensurePeriod(loaded));
        assert(MarketShop::buy(loaded,MarketShop::BOX,day,&r)==MarketShop::Result::OK && r.item==second.item && r.amount==second.amount);
        loaded=restart(loaded);assert(!MarketShop::ensurePeriod(loaded));
        assert(MarketShop::buy(loaded,MarketShop::BOX,day)==MarketShop::Result::SOLD_OUT);
        return;
    }
    assert(!"No two-box plan covered");
}
int main(){plans();concreteDraws();adviceAndSupply();twoBoxes();restartFreezesAdviceAndDraws();
    std::puts("market-procurement: 16-unit plans, next-opening advice, concrete draws, independent boxes, refusals and restart OK");}
