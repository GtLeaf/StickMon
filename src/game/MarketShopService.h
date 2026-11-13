#pragma once
#include <cstdint>
#include "game/ExploreItemProgression.h"
#include "game/ExplorePartyService.h"
#include "game/ItemInventory.h"
#include "game/MarketItemCatalog.h"
#include "game/MarketSchedule.h"
#include "game/ShopService.h"

namespace Game::MarketShop {
constexpr uint16_t STALL_SPECIES = 352;
enum Slot : uint8_t { DISCOUNT_A, DISCOUNT_B, CANDY, NUTRIENT, RESET_BERRY, SPECIAL, RARE, BOX };
constexpr uint8_t ROW_COUNT = 8;
constexpr uint16_t BOX_PRICE = 300;
inline constexpr uint8_t INITIAL_STOCK[] = {3,3,1,2,3,2,1,1};
struct Reward { ItemId item = ItemId::COUNT; uint8_t amount = 0; };
enum class Result : uint8_t { OK, STALE, BAD_SLOT, SOLD_OUT, LOCKED, NO_COINS, BAG_FULL, DAILY_LIMIT };
inline uint32_t periodDay(const GameState& s) { return MarketSchedule::periodDay(s); }
inline bool selectable(uint8_t slot) { return slot == NUTRIENT || slot == RESET_BERRY; }
inline bool choiceMatches(uint8_t slot, ItemId id) {
    return slot == NUTRIENT ? MarketItemCatalog::nutrient(id) : slot == RESET_BERRY && MarketItemCatalog::resetBerry(id);
}
inline ItemId item(const GameState& s, uint8_t slot, ItemId choice = ItemId::COUNT) {
    if (slot >= ROW_COUNT || slot == BOX) return ItemId::COUNT;
    if (selectable(slot) && choice != ItemId::COUNT) return choiceMatches(slot,choice) ? choice : ItemId::COUNT;
    return s.marketSeason.items[slot] < uint8_t(ItemId::COUNT) ? ItemId(s.marketSeason.items[slot]) : ItemId::COUNT;
}
inline bool unlocked(const GameState& s, ItemId id) {
    return id < ItemId::COUNT && ExploreItemProgression::isShopItemUnlocked(id,s);
}
inline uint16_t price(ItemId id) {
    if (const auto* entry = MarketItemCatalog::find(id)) return entry->price;
    switch (id) {
    case ItemId::HEART_SCALE: return 600;
    case ItemId::CANDY: return 1800;
    case ItemId::FIRE_STONE: case ItemId::WATER_STONE: case ItemId::THUNDER_STONE: return 900;
    default: {
        const uint16_t base = ShopService::buyPrice(id);
        const uint16_t value = uint16_t(base*9/10/10*10);
        return value ? value : base;
    }
    }
}
inline uint16_t priceForSlot(const GameState& s,uint8_t slot,ItemId choice=ItemId::COUNT) {
    return slot == BOX ? BOX_PRICE : price(item(s,slot,choice));
}
inline uint32_t mix(uint32_t x) { x^=x>>16; x*=0x7feb352dU; x^=x>>15; x*=0x846ca68bU; x^=x>>16; return x?x:1; }
template<unsigned N> inline ItemId pick(const GameState& s,const ItemId (&list)[N],uint32_t& seed,ItemId avoid=ItemId::COUNT,ItemId avoidB=ItemId::COUNT) {
    ItemId candidates[N]; unsigned count=0;
    for (auto id:list) if(unlocked(s,id)&&id!=avoid&&id!=avoidB)candidates[count++]=id;
    if(!count)return ItemId::COUNT;
    seed=mix(seed+0x9e3779b9U);return candidates[seed%count];
}
inline Reward boxRewardForRoll(const GameState& s,uint8_t roll) {
    Reward r{ItemId::HONEY,3};
    if(roll>=25&&roll<50)r={ItemId::SUPER_POTION,3};
    else if(roll>=50&&roll<65)r={ItemId::MAX_REPEL,2};
    else if(roll>=65&&roll<80)r={ItemId::REVIVE,1};
    else if(roll>=80&&roll<92)r={ItemId::HEART_SCALE,1};
    else if(roll>=92&&roll<98){
        static constexpr ItemId stones[]={ItemId::FIRE_STONE,ItemId::WATER_STONE,ItemId::THUNDER_STONE};
        uint32_t seed=mix(s.marketSeason.periodDay^roll^0x424f58U);const auto stone=pick(s,stones,seed);
        if(stone!=ItemId::COUNT)r={stone,1};
    } else if(roll>=98&&roll<100)r={ItemId::CANDY,1};
    return unlocked(s,r.item)?r:Reward{ItemId::HONEY,3};
}
inline Reward boxReward(const GameState& s) {return boxRewardForRoll(s,uint8_t(s.marketSeason.boxRoll-1));}
inline bool ensurePeriod(GameState& s) {
    if(!MarketSchedule::isOpen(s))return false;
    const uint32_t period=periodDay(s);
    if(s.marketSeason.periodDay==period)return false;
    const bool migrate=s.marketSeason.periodDay==MARKET_PERIOD_NONE&&s.marketShop.periodDay==period;
    const auto previous = s.marketSeason;
    s.marketSeason=GameState::Season{};auto& shelf=s.marketSeason;shelf.periodDay=period;
    uint32_t seed=mix(period^0x4b45434cU);
    static constexpr ItemId discounts[]={ItemId::POTION,ItemId::SUPER_POTION,ItemId::ANTIDOTE,ItemId::PARALYZE_HEAL,
        ItemId::AWAKENING,ItemId::BURN_HEAL,ItemId::ICE_HEAL,ItemId::MAX_POTION,ItemId::FULL_RESTORE,
        ItemId::FULL_HEAL,ItemId::REVIVE,ItemId::MAX_REPEL,ItemId::HONEY};
    static constexpr ItemId specials[]={ItemId::MAX_REVIVE,ItemId::LAVA_COOKIE,ItemId::ORAN_BERRY,ItemId::SITRUS_BERRY,ItemId::LUM_BERRY};
    static constexpr ItemId rares[]={ItemId::HEART_SCALE,ItemId::FIRE_STONE,ItemId::WATER_STONE,ItemId::THUNDER_STONE};
    auto first=pick(s,discounts,seed,ItemId(previous.items[DISCOUNT_A]));
    if(first==ItemId::COUNT)first=pick(s,discounts,seed);
    const auto avoidPair=uint8_t(first)==previous.items[DISCOUNT_B]?ItemId(previous.items[DISCOUNT_A]):ItemId::COUNT;
    auto second=pick(s,discounts,seed,first,avoidPair);
    if(second==ItemId::COUNT)second=pick(s,discounts,seed,first);
    shelf.items[DISCOUNT_A]=uint8_t(first);shelf.items[DISCOUNT_B]=uint8_t(second);
    shelf.items[CANDY]=uint8_t(ItemId::CANDY);shelf.items[NUTRIENT]=uint8_t(ItemId::HP_UP);
    shelf.items[RESET_BERRY]=uint8_t(ItemId::POMEG_BERRY);
    auto special=pick(s,specials,seed,ItemId(previous.items[SPECIAL]));
    if(special==ItemId::COUNT)special=pick(s,specials,seed);
    auto rare=pick(s,rares,seed,ItemId(previous.items[RARE]));
    if(rare==ItemId::COUNT)rare=pick(s,rares,seed);
    shelf.items[SPECIAL]=uint8_t(special);shelf.items[RARE]=uint8_t(rare);
    for(auto& id:shelf.items)if(id>=uint8_t(ItemId::COUNT))id=255;
    for(uint8_t i=0;i<ROW_COUNT;++i)shelf.stock[i]=(i==BOX||unlocked(s,item(s,i)))?INITIAL_STOCK[i]:0;
    const auto& leader=s.team[ExplorePartyService::leaderSlot(s)];
    shelf.boxRoll=uint8_t(1+mix(period^leader.ivPacked^leader.metAt^0x4d595354U)%100);
    if(migrate){
        for(uint8_t i=0;i<ROW_COUNT-1;++i)for(uint8_t old=0;old<MARKET_SHELF_SLOTS;++old)
            if(shelf.stock[i]&&s.marketShop.items[old]==shelf.items[i]&&!s.marketShop.stock[old])--shelf.stock[i];
        if(s.marketShop.boxBought)shelf.stock[BOX]=0;
        if(s.marketShop.boxRoll)shelf.boxRoll=s.marketShop.boxRoll;
        if(s.candyPurchasesToday)shelf.stock[CANDY]=0;
    }
    return true;
}
inline bool soldOut(const GameState& s){for(auto stock:s.marketSeason.stock)if(stock)return false;return true;}
inline Result check(const GameState& s,uint8_t slot,uint32_t period,ItemId choice=ItemId::COUNT){
    if(!MarketSchedule::isOpen(s)||s.marketSeason.periodDay!=period||period!=periodDay(s))return Result::STALE;
    if(slot>=ROW_COUNT)return Result::BAD_SLOT;
    if(selectable(slot)&&choice!=ItemId::COUNT&&!choiceMatches(slot,choice))return Result::BAD_SLOT;
    Reward r{item(s,slot,choice),1};
    if(slot==BOX){if(!s.marketSeason.boxRoll||s.marketSeason.boxRoll>100)return Result::STALE;r=boxReward(s);}
    else if(r.item==ItemId::COUNT)return Result::LOCKED;
    if(!unlocked(s,r.item))return Result::LOCKED;
    if(!s.marketSeason.stock[slot])return Result::SOLD_OUT;
    if(unsigned(ItemInventory::count(s,r.item))+r.amount>ITEM_STACK_CAP)return Result::BAG_FULL;
    return s.coins<priceForSlot(s,slot,choice)?Result::NO_COINS:Result::OK;
}
inline Result buy(GameState& s,uint8_t slot,uint32_t period,Reward* granted=nullptr,ItemId choice=ItemId::COUNT){
    const auto result=check(s,slot,period,choice);if(result!=Result::OK)return result;
    const Reward r=slot==BOX?boxReward(s):Reward{item(s,slot,choice),1};
    if(!ItemInventory::add(s,r.item,r.amount))return Result::BAG_FULL;
    s.coins-=priceForSlot(s,slot,choice);--s.marketSeason.stock[slot];
    if(granted)*granted=r;
    return Result::OK;
}
}
