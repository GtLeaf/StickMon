#include "game/MarketTransaction.h"
#include "ui/common/MarketCoinMotion.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using Change=Game::MarketTransaction::CoinChange;
using Status=Game::MarketTransaction::Status;
enum class Result { OK, REFUSED };
static void near(float a,float b){assert(std::abs(a-b)<0.001f);}
static void durableBoundary() {
    Game::GameState state{};state.coins=1000;const auto before=state;
    std::vector<Change> events;int saves=0;
    auto notify=[&](Change change){events.push_back(change);};
    auto pay=[](Game::GameState& s){s.coins-=300;++s.bag.honey;return Result::OK;};
    auto tx=Game::MarketTransaction::commit(state,pay,[&](){++saves;return false;},notify);
    assert(tx.status==Status::SAVE_FAILED&&saves==1&&events.empty());
    assert(!std::memcmp(&state,&before,sizeof(state)));
    tx=Game::MarketTransaction::commit(state,[](Game::GameState& s){s.coins=1;return Result::REFUSED;},
        [&](){++saves;return true;},notify);
    assert(tx.status==Status::REJECTED&&saves==1&&events.empty());
    assert(!std::memcmp(&state,&before,sizeof(state)));
    tx=Game::MarketTransaction::commit(state,pay,[&](){++saves;return true;},notify);
    assert(tx.status==Status::COMMITTED&&state.coins==700&&state.bag.honey==before.bag.honey+1);
    assert(events.size()==1&&events[0].before==1000&&events[0].after==700&&events[0].delta()==-300);
    tx=Game::MarketTransaction::commit(state,[](Game::GameState& s){++s.bag.heartScale;return Result::OK;},[](){return true;},notify);
    assert(tx.status==Status::COMMITTED&&events.size()==1);
    tx=Game::MarketTransaction::commit(state,[](Game::GameState& s){s.coins+=500;return Result::OK;},[](){return true;},notify);
    assert(tx.status==Status::COMMITTED&&events.size()==2&&events[1].delta()==500&&state.coins==1200);
    assert((Change{0,UINT32_MAX}.delta()==4294967295LL));
    assert((Change{UINT32_MAX,0}.delta()==-4294967295LL));
}
static void rollAndRetarget() {
    Ui::MarketCoinMotion m;m.reset(27400);auto f=m.frame();
    near(f.wheels[2],4);assert(!f.rolling&&!f.tags[0].visible&&!f.tags[1].visible);
    assert(m.committed({27400,27100}));f=m.frame();near(f.wheels[2],4);
    assert(f.rolling&&f.tags[0].delta==-300);
    m.update(225);f=m.frame();near(f.wheels[2],1.375f);near(f.wheels[3],7);
    assert(!m.update(0));near(m.frame().wheels[2],f.wheels[2]);
    assert(m.committed({27100,27000}));near(m.frame().wheels[2],f.wheels[2]);
    assert(m.frame().tags[0].delta==-300&&m.frame().tags[1].delta==-100);
    m.update(450);f=m.frame();near(f.wheels[2],0);assert(!f.rolling&&f.balance==27000);
    assert(m.committed({27000,27500}));near(m.frame().wheels[2],0);
    m.update(225);near(m.frame().wheels[2],4.375f);
    m.update(225);near(m.frame().wheels[2],5);assert(!m.frame().rolling);
    m.update(450);assert(!m.frame().tags[0].visible&&!m.frame().tags[1].visible);
    assert(!m.update(1000));
    assert(!m.committed({27500,27500}));assert(!m.frame().rolling);
    // Synchronization/entry does not invent a gain and drops stale tags.
    m.reset(42);f=m.frame();assert(f.balance==42&&!f.rolling&&!f.tags[0].visible&&!f.tags[1].visible);
    near(f.wheels[0],2);near(f.widthDigits,2);
}
static void carryBoundsAndMixedSigns() {
    // A small opposite-sign transaction must not reverse an unchanged in-flight digit.
    Ui::MarketCoinMotion continuing;continuing.reset(27400);continuing.committed({27400,27100});continuing.update(225);
    continuing.committed({27100,27101});near(continuing.frame().wheels[2],1.375f);
    continuing.update(225);near(continuing.frame().wheels[2],1.046875f);
    Ui::MarketCoinMotion m;m.reset(999);m.committed({999,1000});m.update(100);
    const auto mid=m.frame();assert(mid.widthDigits>=3&&mid.widthDigits<=4);
    m.committed({1000,990});const auto retained=m.frame();
    for(int i=0;i<10;++i)near(mid.wheels[i],retained.wheels[i]);
    m.update(570);assert(!m.frame().rolling);near(m.frame().widthDigits,3);
    m.reset(UINT32_MAX);m.committed({UINT32_MAX,0});m.update(450);
    auto f=m.frame();for(auto wheel:f.wheels)near(std::fmod(wheel+100,10),0);
    assert(f.balance==0&&!f.rolling&&f.tags[0].delta==-4294967295LL);
    m.update(120);near(m.frame().widthDigits,1);
    m.reset(0);m.committed({0,UINT32_MAX});m.update(450);
    assert(m.frame().balance==UINT32_MAX&&!m.frame().rolling);near(m.frame().widthDigits,10);
    // Flooded presentations remain bounded; amounts are per transaction, never display gaps.
    for(uint32_t n=0;n<1000;++n){m.committed({UINT32_MAX-n,UINT32_MAX-n-1});m.update(1);}
    f=m.frame();int visible=0;for(const auto& tag:f.tags){visible+=tag.visible;if(tag.visible)assert(tag.delta==-1);}
    assert(visible==2);m.update(1000);assert(!m.frame().tags[0].visible&&!m.frame().tags[1].visible);
}
int main(){durableBoundary();rollAndRetarget();carryBoundsAndMixedSigns();
    std::puts("market-coins: atomic rollback/durable events, signed deltas, smooth per-digit retarget,450/900ms, carry/bounds, bounded tags and reset OK");}
