#include "game/MarketShopService.h"
#include "game/MarketTransaction.h"
#include "game/FriendshipService.h"
#include "game/MonsterFactory.h"
#include "core/SaveCodec.h"
#include "game/ContactRoster.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#if __has_include("game/StorageCapacity.h")
#include "game/StorageCapacity.h"
#define HAS_EXPANSION 1
#endif
using namespace Game;
int main(){
    assert(STORAGE_CAP == 200 && "computer must physically support 200 companions");
#ifdef HAS_EXPANSION
    GameState s{};s.coins=99999;s.gameMinutesTotal=0;s.team[0]=MonsterFactory::create(25,5);
    assert(StorageCapacity::capacity(s)==20&&StorageCapacity::nextCapacity(s)==40);
    assert(MarketShop::ensurePeriod(s));const auto original=s.marketSeason;
    assert(MarketShop::rowCount(s)==9&&MarketShop::stock(s,MarketShop::EXPANSION)==1);
    const unsigned prices[]={1000,2500,4000,5500,7000,8500,10000,11500,13000};
    for(unsigned i=0;i<9;++i){
        s.gameMinutesTotal=(i*7)*1440U;MarketShop::ensurePeriod(s);const auto period=MarketShop::periodDay(s);
        assert(StorageCapacity::capacity(s)==20+i*20&&StorageCapacity::price(s)==prices[i]);
        assert(MarketShop::item(s,MarketShop::EXPANSION)==ItemId::COMPUTER_EXPANSION);
        assert(MarketShop::priceForSlot(s,MarketShop::EXPANSION)==prices[i]);
        auto before=s;int notices=0;
        auto failed=MarketTransaction::commit(s,[&](GameState& x){return MarketShop::buy(x,MarketShop::EXPANSION,period);},[]{return false;},[&](auto){++notices;});
        assert(failed.status==MarketTransaction::Status::SAVE_FAILED&&std::memcmp(&before,&s,sizeof(s))==0&&notices==0);
        auto poor=s;poor.coins=prices[i]-1;assert(MarketShop::buy(poor,MarketShop::EXPANSION,period)==MarketShop::Result::NO_COINS);
        MarketShop::Reward reward;assert(MarketShop::buy(s,MarketShop::EXPANSION,period,&reward)==MarketShop::Result::OK);
        assert(reward.item==ItemId::COMPUTER_EXPANSION&&s.coins==before.coins-prices[i]);
        assert(StorageCapacity::capacity(s)==40+i*20&&MarketShop::stock(s,MarketShop::EXPANSION)==0);
        assert(std::memcmp(&before.marketSeason,&s.marketSeason,sizeof(s.marketSeason))==0);
        assert(ItemInventory::count(s,ItemId::COMPUTER_EXPANSION)==0);
        const auto paid=s;assert(MarketShop::buy(s,MarketShop::EXPANSION,period)!=MarketShop::Result::OK&&std::memcmp(&paid,&s,sizeof(s))==0);
        assert(!MarketShop::ensurePeriod(s)&&MarketShop::stock(s,MarketShop::EXPANSION)==0);
    }
    assert(StorageCapacity::capacity(s)==200&&!StorageCapacity::canExpand(s)&&MarketShop::rowCount(s)==8);
    assert(ShopService::buy(s,ItemId::COMPUTER_EXPANSION)==ShopService::BuyResult::INVALID_ITEM);
    s=GameState{};s.storageCount=20;auto monster=MonsterFactory::create(25,5);
    assert(!FriendshipService::recordContact(s,monster,0,1,nullptr));s.storageUpgrades=1;assert(FriendshipService::recordContact(s,monster,0,1,nullptr)&&s.storageCount==21);
    s.storageUpgrades=9;s.storageCount=199;assert(FriendshipService::recordContact(s,monster,0,2,nullptr)&&s.storageCount==200);assert(!FriendshipService::recordContact(s,monster,0,3,nullptr));
    s.teamCount=2;s.team[1]=s.storage[199];s.team[1].hpCur=1;
    assert(ContactRoster::storeTeamContact(s,1)&&s.storageCount==200&&s.storage[199].hpCur==1);
    s.team[1].ivPacked=UINT32_MAX;assert(!ContactRoster::storeTeamContact(s,1));
    std::puts("computer-expansion: capacities, all prices, period quotas, no bag, rollback and acquisition limits OK");
#endif
}
