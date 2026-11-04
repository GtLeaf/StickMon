#include "game/MarketMapData.h"
#include "game/MarketSession.h"
#include "game/ExploreDestinations.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

int main(int argc,char** argv) {
    const uint32_t seed=argc>1?uint32_t(std::strtoul(argv[1],nullptr,10)):38;
    const auto& d=MarketMapData::get();
    Game::GameState state{};
    assert(ExploreDestinations::visibleId(0,state)==ExploreDestinations::MARKET);
    assert(ExploreDestinations::visibleId(1,state)==0);
    assert(ExploreDestinations::unlocked(ExploreDestinations::MARKET,state));
    assert(Game::EXPLORE_AREA_COUNT==6);
    Market::Session sim,copy;
    assert(sim.reset(d,seed,25,133,0));assert(copy.reset(d,seed,25,133,0));
    int visits=0,departures=0;auto last=sim.visitPhase();
    uint32_t fingerprint=2166136261U;
    for(uint32_t t=1;t<=18000;++t) {
        const uint32_t now=t*100;
        sim.update(now);copy.update(now);
        if(t%400==0){sim.walkSelected();copy.walkSelected();sim.nextTarget();copy.nextTarget();}
        if(sim.visitPhase()!=last) {
            visits+=sim.visitPhase()==Market::VisitPhase::VISITING;
            departures+=sim.visitPhase()==Market::VisitPhase::WAITING;
            last=sim.visitPhase();
        }
        for(int i=0;i<sim.count();++i) {
            const auto& a=sim.actor(i);const auto p=sim.pose(i),q=copy.pose(i);
            assert(p.x==q.x&&p.y==q.y&&p.species==q.species&&p.visible==q.visible);
            if(a.stage==2) {
                const auto& mask=i<d.residentCount?d.residents[i].navigation:d.roads;
                assert(mask.has(a.from)&&mask.has(a.to));
            }
            if(a.stage!=4)for(int j=0;j<i;++j) {
                const auto& b=sim.actor(j);if(b.stage==4)continue;
                assert(a.from!=b.from&&a.from!=b.to&&a.to!=b.from&&a.to!=b.to);
            }
            fingerprint=(fingerprint^uint32_t(p.x*16))*16777619U;
            fingerprint=(fingerprint^uint32_t(p.y*16))*16777619U;
        }
        if(t==700) {
            const auto before=sim.pose(0);
            sim.pause(true);sim.update(now+10000);sim.pause(false);
            const auto after=sim.pose(0);assert(before.x==after.x&&before.y==after.y);
            // Restore the same timestamp anchor for deterministic comparison.
            sim.update(now,false);
        }
    }
    std::printf("seed=%u visits=%d departures=%d fingerprint=%08x phase=%u\n",seed,visits,departures,fingerprint,unsigned(last));
    assert(visits>=5&&departures>=5);
    sim.leave();
    for(uint32_t t=18001;t<19500&&!sim.finished();++t)sim.update(t*100);
    if(!sim.finished())for(int i=0;i<sim.count();++i){const auto& a=sim.actor(i);std::fprintf(stderr,"actor=%d stage=%d from=%u to=%u goal=%u blocked=%u\n",i,a.stage,a.from,a.to,a.goal,a.blocked);}
    assert(sim.finished());
}
