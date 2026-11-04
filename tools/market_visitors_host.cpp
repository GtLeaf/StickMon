#include "game/MarketMapData.h"
#include "game/MarketSession.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static void reservations(const Market::Session& sim) {
    for(int i=0;i<sim.count();++i) {
        const auto& a=sim.actor(i);
        if(a.stage==4)continue;
        assert(a.species!=0);
        for(int j=0;j<i;++j) {
            const auto& b=sim.actor(j);
            if(b.stage==4)continue;
            assert(a.from!=b.from&&a.from!=b.to&&a.to!=b.from&&a.to!=b.to);
        }
    }
}

int main() {
    for(auto sid:{113,115,225,352})for(unsigned seed:{1,17,38,99})for(bool companion:{false,true}) {
        auto d=MarketMapData::get();d.maxGroups=1;d.maxVisitors=2;d.stallCount=0;
        d.families[0]={uint16_t(sid),0};d.familyCount=1;
        d.intervalMin=d.intervalMax=10;d.stayMin=d.stayMax=20;
        Market::Session sim;
        assert(sim.reset(d,seed,25,companion?133:0,0));
        unsigned now=0;int visits=0,departures=0;auto last=sim.visitPhase();
        for(int t=0;t<2400;++t) {
            sim.update(now+=100);reservations(sim);
            const auto a=sim.pose(d.residentCount+2),b=sim.pose(d.residentCount+3);
            assert(!b.visible&&b.species==0);
            if(a.visible)assert(a.species==sid);
            if(sim.visitPhase()!=last) {
                visits+=sim.visitPhase()==Market::VisitPhase::VISITING;
                departures+=sim.visitPhase()==Market::VisitPhase::WAITING;
                last=sim.visitPhase();
            }
            if(t==400) {
                const auto before=sim.pose(d.residentCount+2);
                const auto elapsed=sim.timeMs();
                sim.pause(true);sim.update(now+=10000);
                const auto after=sim.pose(d.residentCount+2);
                assert(sim.timeMs()==elapsed&&before.x==after.x&&before.y==after.y&&before.visible==after.visible);
                sim.pause(false);
            }
        }
        assert(visits>=3&&departures>=3);
        sim.leave();
        for(int t=0;t<1500&&!sim.finished();++t){sim.update(now+=100);reservations(sim);}
        assert(sim.finished());
        assert(sim.reset(d,seed,25,companion?133:0,now));
        assert(sim.visitPhase()==Market::VisitPhase::WAITING);
        assert(!sim.pose(d.residentCount+2).visible&&!sim.pose(d.residentCount+3).visible);
    }
    // One free destination is enough for a solo; pairs still need two.
    auto single=MarketMapData::get();single.maxGroups=1;single.maxVisitors=2;single.stallCount=0;single.familyCount=1;single.families[0]={225,0};single.stopCount=1;
    single.intervalMin=single.intervalMax=10;single.stayMin=single.stayMax=20;
    Market::Session sim;assert(sim.reset(single,38,0,0,0));bool arrived=false,departed=false;
    for(unsigned t=1;t<1800;++t) {
        sim.update(t*100);reservations(sim);
        if(sim.visitPhase()==Market::VisitPhase::VISITING)arrived=true;
        if(arrived&&sim.visitPhase()==Market::VisitPhase::WAITING)departed=true;
    }
    assert(arrived&&departed);
    single.families[0]={25,172};assert(!sim.reset(single,38,0,0,0));
    single.families[0]={0,0};assert(!sim.reset(single,38,0,0,0));
    // Reuse the two slots across alternating solo/pair visits in one session.
    auto mixed=MarketMapData::get();mixed.maxGroups=1;mixed.maxVisitors=2;mixed.stallCount=0;mixed.familyCount=1;mixed.families[0]={225,0};
    mixed.intervalMin=mixed.intervalMax=10;mixed.stayMin=mixed.stayMax=20;
    assert(sim.reset(mixed,17,25,133,0));int completed=0;auto last=sim.visitPhase();
    for(unsigned t=1;t<=6000;++t) {
        sim.update(t*100);reservations(sim);
        const auto& a=sim.actor(mixed.residentCount+2);
        const auto& b=sim.actor(mixed.residentCount+3);
        if(a.stage!=4&&!mixed.families[0].juvenile)assert(b.stage==4&&b.species==0);
        if(last==Market::VisitPhase::LEAVING&&sim.visitPhase()==Market::VisitPhase::WAITING) {
            ++completed;
            mixed.families[0]=completed%2?Market::Family{25,172}:Market::Family{225,0};
        }
        last=sim.visitPhase();
    }
    assert(completed>=6);
    std::puts("Solo visitors: entry/stay/exit, pause, companions, reentry and reservations passed");
}
