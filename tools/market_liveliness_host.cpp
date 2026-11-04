#include "game/MarketMapData.h"
#include "game/MarketSession.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static void invariant(const Market::Session& sim) {
    int guests=0,bubbles=0;uint16_t stalls=0;
    const auto& d=sim.data();
    for(int g=0;g<d.maxGroups;++g) {
        const int s=sim.visitingStall(g);
        if(s<0)continue;
        assert(!(stalls&(1U<<s)));stalls|=uint16_t(1U<<s);
        for(int other=0;other<g;++other)for(int a=0;a<2;++a)for(int b=0;b<2;++b)
            if(sim.customerPoint(g,a)!=Market::NONE)assert(sim.customerPoint(g,a)!=sim.customerPoint(other,b));
    }
    for(int i=0;i<sim.count();++i) {
        const auto& a=sim.actor(i);const auto p=sim.pose(i);
        bubbles+=p.speaking;
        assert(p.lift>=0&&p.lift<=4.01f);
        if(p.lift||p.speaking)assert(a.stage==2&&a.from==a.to);
        if(a.stage==4){assert(!p.speaking&&p.lift==0);continue;}
        assert(a.species);
        guests+=i>=d.residentCount+2;
        const auto& mask=i<d.residentCount?d.residents[i].navigation:sim.manual()?d.pedestrian:d.roads;
        assert(mask.has(a.from)&&mask.has(a.to));
        for(int j=0;j<i;++j) {
            const auto& b=sim.actor(j);if(b.stage==4)continue;
            assert(a.from!=b.from&&a.from!=b.to&&a.to!=b.from&&a.to!=b.to);
        }
    }
    assert(guests<=d.maxVisitors&&bubbles<=2);
}
int main() {
    const auto& d=MarketMapData::get();
    bool mixed=false,concurrent=false,three=false,hop=false,visitorBubble=false,vendorBubble=false,companionSpeaks=false;
    bool airPaused=false;
    bool front[12]{},looking[12]{};unsigned completed[3]{};
    for(unsigned seed:{1U,17U,38U,99U,1024U}) {
        Market::Session sim,again;assert(sim.reset(d,seed,25,133,0));assert(again.reset(d,seed,25,133,0));
        Market::VisitPhase previous[3]{};unsigned now=0;
        for(int t=0;t<9000;++t) {
            sim.update(now+=100);again.update(now);invariant(sim);
            int solo=0,pair=0,browsing=0,active=0;
            for(int g=0;g<3;++g) {
                const auto phase=sim.visitPhase(g);
                if(previous[g]==Market::VisitPhase::LEAVING&&phase==Market::VisitPhase::WAITING)++completed[g];
                previous[g]=phase;
                if(phase==Market::VisitPhase::WAITING)continue;
                ++active;
                const auto& b=sim.actor(d.residentCount+3+2*g);
                if(b.species)++pair;else ++solo;
                browsing+=phase==Market::VisitPhase::VISITING&&sim.visitingStall(g)>=0;
            }
            mixed|=solo&&pair;concurrent|=browsing>=2;three|=active==3;
            bool airborne=false;
            for(int i=0;i<sim.count();++i) {
                const auto p=sim.pose(i),q=again.pose(i);
                airborne|=p.lift>0;
                assert(p.x==q.x&&p.y==q.y&&p.direction==q.direction&&p.visible==q.visible&&p.species==q.species&&p.speaking==q.speaking&&p.lift==q.lift);
                hop|=p.lift>0;
                if(i<d.residentCount&&!p.moving){front[i]|=p.direction==d.residents[i].reception;looking[i]|=p.direction!=d.residents[i].reception;vendorBubble|=p.speaking;}
                if(i>=d.residentCount+2){visitorBubble|=p.speaking;companionSpeaks|=p.speaking&&((i-d.residentCount-2)%2==1);}
            }
            if(t==700||t==1400||(!airPaused&&airborne)) {
                airPaused|=airborne;
                const auto before=sim;const auto clock=sim.timeMs();
                sim.pause(true);sim.update(now+5000);assert(clock==sim.timeMs());
                for(int i=0;i<sim.count();++i){auto p=sim.pose(i),q=before.pose(i);assert(p.x==q.x&&p.y==q.y&&p.lift==q.lift&&p.speaking==q.speaking);}
                sim.pause(false);sim.update(now,false);
            }
        }
        sim.leave();
        for(int i=0;i<sim.count();++i)assert(!sim.pose(i).speaking&&sim.pose(i).lift==0);
        for(int t=0;t<1800&&!sim.finished();++t){sim.update(now+=100);invariant(sim);}
        assert(sim.finished());
        assert(sim.reset(d,seed,25,133,now));
        for(int g=0;g<3;++g)assert(sim.visitPhase(g)==Market::VisitPhase::WAITING&&sim.visitingStall(g)<0);
    }
    assert(mixed&&concurrent&&three&&hop&&visitorBubble&&vendorBubble&&companionSpeaks);
    for(int i=0;i<d.residentCount;++i)assert(front[i]&&looking[i]);
    for(auto n:completed)assert(n>=5);
    assert(airPaused);
    // A pair-only authored pool can use all six slots without collisions.
    auto pairs=d;pairs.familyCount=4;
    Market::Session crowded;assert(crowded.reset(pairs,38,25,133,0));bool six=false;
    for(unsigned t=1;t<=2400;++t) {
        crowded.update(t*100);invariant(crowded);int visible=0;
        for(int i=pairs.residentCount+2;i<crowded.count();++i)visible+=crowded.pose(i).visible;
        six|=visible==6;
    }
    assert(six);
    // Turning off cosmetic performances must not change paths or arrivals.
    auto quiet=d;quiet.performances=false;
    Market::Session normal,silent;assert(normal.reset(d,38,25,133,0));assert(silent.reset(quiet,38,25,133,0));
    for(unsigned t=1;t<=1800;++t) {
        normal.update(t*100);silent.update(t*100);
        for(int i=0;i<normal.count();++i) {
            const auto a=normal.pose(i),b=silent.pose(i);
            assert(a.x==b.x&&a.y==b.y&&a.visible==b.visible&&a.species==b.species);
            assert(!b.speaking&&b.lift==0);
        }
    }
    // Capacity and malformed exported navigation are rejected before indexing.
    auto bad=d;bad.maxGroups=4;Market::Session sim;assert(!sim.reset(bad,1,25,0,0));
    bad=d;bad.residentCount=12;assert(!sim.reset(bad,1,25,0,0));
    bad=d;bad.stalls[0].points[0]=Market::NONE;assert(!sim.reset(bad,1,25,0,0));
    std::printf("Lively market: mixed/three groups, two stalls, gaze, bubbles, hops, replay, pause, exit; completions=%u/%u/%u\n",completed[0],completed[1],completed[2]);
}
