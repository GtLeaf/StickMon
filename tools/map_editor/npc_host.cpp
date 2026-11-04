#include "game/AuthoredNpcPopulation.h"
#include <iostream>
#include <iomanip>
int main() {
    using namespace AuthoredNpc;
    unsigned count, ticks;
    if (!(std::cin >> count >> ticks) || count > CAPACITY || ticks > 1200) return 2;
    Definition definitions[CAPACITY];
    for (unsigned i=0;i<count;++i) {
        unsigned spawn,direction,behavior,step,lo,hi,n; uint32_t seed;
        if (!(std::cin >> spawn >> direction >> behavior >> step >> lo >> hi >> seed >> n) ||
            spawn>=CELLS || direction>3 || behavior>2 || step<2 || step>20 || lo<1 || hi<lo || hi>600 || n>PATROL_CAPACITY) return 2;
        auto& d=definitions[i]; d.spawn=spawn; d.direction=direction; d.behavior=Behavior(behavior);
        d.stepTicks=step;d.waitMin=lo;d.waitMax=hi;d.seed=seed;d.patrolCount=n;
        for(unsigned j=0;j<n;++j) {unsigned cell;if(!(std::cin>>cell)||cell>=CELLS)return 2;d.patrol[j]=cell;}
        for(auto& cell:d.allowed) {unsigned value;if(!(std::cin>>value)||value>1)return 2;cell=value;}
        if(!d.allowed[d.spawn])return 2;
    }
    unsigned rulesCount; uint32_t seed;
    using AuthoredNpcPopulation::RULE_CAPACITY;
    AuthoredNpcPopulation::Rule rules[RULE_CAPACITY];
    if(!(std::cin>>rulesCount)||rulesCount>RULE_CAPACITY||count+2*rulesCount>CAPACITY)return 2;
    for(unsigned i=0;i<rulesCount;++i) {
        auto& r=rules[i];unsigned lo,hi,stayLo,stayHi,families,ports;
        if(!(std::cin>>lo>>hi>>stayLo>>stayHi>>r.seed>>families)||lo<10||hi<lo||hi>600||
           stayLo<10||stayHi<stayLo||stayHi>600||!families||families>AuthoredNpcPopulation::FAMILY_CAPACITY)return 2;
        r.intervalMin=lo;r.intervalMax=hi;r.stayMin=stayLo;r.stayMax=stayHi;r.familyCount=families;
        for(unsigned j=0;j<families;++j) {
            unsigned a,b;if(!(std::cin>>a>>b)||!a||a==b||a>65535||b>65535)return 2;
            r.families[j]={uint16_t(a),uint16_t(b)};
        }
        if(!(std::cin>>ports)||!ports||ports>AuthoredNpcPopulation::PORT_CAPACITY)return 2;
        r.portCount=ports;
        for(unsigned j=0;j<ports;++j) {
            unsigned cell,direction;if(!(std::cin>>cell>>direction)||cell>=CELLS||direction>3)return 2;
            r.ports[j]={Cell(cell),uint8_t(direction)};
        }
        for(auto& cell:r.allowed){unsigned v;if(!(std::cin>>v)||v>1)return 2;cell=v;}
        for(auto& cell:r.zone){unsigned v;if(!(std::cin>>v)||v>1)return 2;cell=v;}
    }
    if(!(std::cin>>seed))return 2;
    AuthoredNpcPopulation::Simulation sim;sim.reset(definitions,count,rules,rulesCount,seed);
    std::cout << std::fixed << std::setprecision(4) << '[';
    for(unsigned t=0;t<=ticks;++t) {
        if(t)std::cout<<',';std::cout<<'[';
        for(unsigned i=0;i<sim.count();++i) {if(i)std::cout<<',';const auto pose=sim.pose(i);const auto p=pose.actor;
            std::cout<<'['<<p.x<<','<<p.y<<','<<int(p.direction)<<','<<int(p.frame)<<','<<p.moving;
            if(rulesCount)std::cout<<','<<pose.visible<<','<<pose.speciesId;
            std::cout<<']';}
        std::cout<<']';sim.tick();
    }
    std::cout<<']';
}
