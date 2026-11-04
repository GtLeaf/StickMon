#pragma once
#include "AuthoredNpc.h"

// Scheduled visitors for authored maps. The editor and exported maps use this
// same controller; species selection never changes the saved map source.
namespace AuthoredNpcPopulation {
using namespace AuthoredNpc;
constexpr int RULE_CAPACITY = CAPACITY / 2, FAMILY_CAPACITY = 64, PORT_CAPACITY = 17;
// Existing walking artwork reaches up to 2.93 cells above its feet. Cross
// three cells outside the map before hiding, at the same speed as normal steps.
constexpr int CROSSING_CELLS = 3;
struct Family { uint16_t adult = 0, juvenile = 0; }; // juvenile=0: solo.
struct Port { Cell cell = 0; uint8_t direction = 0; }; // Outward direction.
struct Rule {
    bool allowed[CELLS]{}, zone[CELLS]{};
    Family families[FAMILY_CAPACITY]{};
    Port ports[PORT_CAPACITY]{};
    uint8_t familyCount = 0, portCount = 0;
    uint16_t intervalMin = 80, intervalMax = 150, stayMin = 100, stayMax = 200;
    uint32_t seed = 1;
};
enum class Phase : uint8_t { WAITING, ENTERING, VISITING, LEAVING };
struct Group {
    Phase phase = Phase::WAITING;
    uint32_t random = 1;
    uint16_t timer = 0;
    Family family{};
    Port port{};
    Cell target = 0, approach = 0, followTarget = 0;
    bool approached = false;
    // 0 = waiting outside, 1 = crossing in, 2 = inside, 3 = crossing out, 4 = gone.
    uint8_t stage[2]{}, crossing[2]{};
};
struct Pose { AuthoredNpc::Pose actor{}; bool visible = false; uint16_t speciesId = 0; };

class Simulation {
    Definition definitions_[CAPACITY]{};
    AuthoredNpc::Simulation actors_;
    const Rule* rules_ = nullptr;
    Group groups_[RULE_CAPACITY]{};
    uint8_t manual_ = 0, rulesCount_ = 0;
    int8_t transit_ = -1;
    static uint32_t next(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    static uint16_t between(uint32_t& s, uint16_t lo, uint16_t hi) { return lo + next(s) % (hi-lo+1); }
    int slot(int group, int member) const { return manual_ + group*2 + member; }
    static int distance(int a, int b) {
        const int dx = a%WIDTH-b%WIDTH, dy = a/WIDTH-b/WIDTH;
        return (dx<0?-dx:dx) + (dy<0?-dy:dy);
    }
    bool chooseDestination(int i) {
        const auto& r = rules_[i]; auto& g = groups_[i];
        const int first = next(g.random) % r.portCount;
        for (int pi=0;pi<r.portCount;++pi) {
            const auto port = r.ports[(first+pi)%r.portCount];
            if (!actors_.available(port.cell)) continue;
            int16_t previous[CELLS]; for (auto& v : previous) v = -1;
            Cell queue[CELLS]; int head=0,tail=0;
            previous[port.cell] = port.cell; queue[tail++] = port.cell;
            while (head<tail) {
                const int cell=queue[head++];
                const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
                for (int d=0;d<4;++d) {
                    const int x=cell%WIDTH+dx[d],y=cell/WIDTH+dy[d],n=y*WIDTH+x;
                    if(x<0||x>=WIDTH||y<0||y>=HEIGHT||previous[n]>=0||!r.allowed[n]||!actors_.available(n))continue;
                    previous[n]=cell;queue[tail++]=Cell(n);
                }
            }
            Cell candidates[CELLS],approaches[CELLS];int count=0;
            for(int c=0;c<CELLS;++c) {
                if(!r.zone[c]||previous[c]<0||c==port.cell)continue;
                if(!g.family.juvenile) {
                    candidates[count]=approaches[count]=Cell(c);++count;continue;
                }
                const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
                for(int d=0;d<4;++d) {
                    const int x=c%WIDTH+dx[d],y=c/WIDTH+dy[d],n=y*WIDTH+x;
                    if(x<0||x>=WIDTH||y<0||y>=HEIGHT||!r.zone[n]||previous[n]<0)continue;
                    int ancestor=n;
                    while(ancestor!=c&&previous[ancestor]!=ancestor)ancestor=previous[ancestor];
                    if(ancestor==c)continue;
                    candidates[count]=Cell(c);approaches[count++]=Cell(n);break;
                }
            }
            if(!count)continue;
            const int choice=next(g.random)%count;
            g.port=port;g.target=candidates[choice];g.approach=approaches[choice];
            // Reach the near stall cell first; reserve the far cell as the
            // final step so a two-cell zone cannot be approached sideways.
            for(int m=0;m<2;++m)definitions_[slot(i,m)].allowed[g.target]=!g.family.juvenile;
            g.approached=!g.family.juvenile;g.followTarget=port.cell;
            return true;
        }
        return false;
    }
    void crossIn(int i,int member) {
        auto& g=groups_[i];const int n=slot(i,member);
        if (g.stage[member]==0 && actors_.available(g.port.cell)) {
            actors_.activate(n,g.port.cell,(g.port.direction+2)%4);
            g.stage[member]=1;g.crossing[member]=0;
        }
    }
    void advanceCrossings(int i) {
        auto& g=groups_[i];
        for(int m=0;m<2;++m) {
            if(g.stage[m]!=1&&g.stage[m]!=3)continue;
            if(++g.crossing[m]>=definitions_[slot(i,m)].stepTicks*CROSSING_CELLS) {
                if(g.stage[m]==1)g.stage[m]=2;
                else { g.stage[m]=4;actors_.deactivate(slot(i,m)); }
            }
        }
    }
    void travel(int i,int lead,int target,bool exiting) {
        auto& g=groups_[i];const int follow=1-lead;
        const int a=slot(i,lead),b=slot(i,follow);
        const auto& leader=actors_.state(a);const auto& child=actors_.state(b);
        if(!g.family.juvenile) {
            if(g.stage[lead]==2&&!leader.moving) {
                if(exiting&&leader.to==target) {g.stage[lead]=3;g.crossing[lead]=0;}
                else if(leader.to!=target)actors_.walkTo(a,target);
            }
            return;
        }
        // A follower uses the cell vacated by its partner. Reversing the order
        // for departure lets a pair leave even a two-cell stall without swapping.
        if(g.stage[follow]==2&&!child.moving)
            actors_.walkTo(b,exiting&&g.stage[lead]>=3?target:g.followTarget);
        if(g.stage[lead]==2&&!leader.moving&&leader.to!=target &&
           (g.stage[follow]==0 || (g.stage[follow]==2&&distance(leader.to,child.to)<=1))) {
            const auto previous=leader.to;
            if(actors_.walkTo(a,target))g.followTarget=previous;
        }
        if(exiting) {
            if(g.stage[lead]==2&&!leader.moving&&leader.to==target) {
                g.stage[lead]=3;g.crossing[lead]=0;
            }
            if(g.stage[lead]>=3 && g.stage[follow]==2&&!child.moving) {
                if(child.to==target&&!child.moving) {g.stage[follow]=3;g.crossing[follow]=0;}
            }
        }
    }
public:
    void reset(const Definition* manual,uint8_t count,const Rule* rules,uint8_t ruleCount,uint32_t seed) {
        manual_=count>CAPACITY?CAPACITY:count;rules_=rules;transit_=-1;
        rulesCount_=ruleCount>RULE_CAPACITY?RULE_CAPACITY:ruleCount;
        if(manual_+rulesCount_*2>CAPACITY)rulesCount_=(CAPACITY-manual_)/2;
        for(int i=0;i<manual_;++i)definitions_[i]=manual[i];
        for(int i=0;i<rulesCount_;++i) {
            auto& g=groups_[i];g={};g.random=rules[i].seed^seed;if(!g.random)g.random=1;
            g.timer=between(g.random,rules[i].intervalMin,rules[i].intervalMax);
            for(int m=0;m<2;++m) {
                auto& d=definitions_[slot(i,m)];d={};
                for(int c=0;c<CELLS;++c)d.allowed[c]=rules[i].allowed[c];
                d.stepTicks=6;d.behavior=Behavior::IDLE;
            }
        }
        actors_.reset(definitions_,manual_+rulesCount_*2);
        for(int i=manual_;i<actors_.count();++i)actors_.deactivate(i);
    }
    uint8_t count() const { return actors_.count(); }
    const Group& group(int i) const { return groups_[i]; }
    void tick() {
        actors_.tick();
        for(int i=0;i<rulesCount_;++i) {
            auto& g=groups_[i];const auto& r=rules_[i];advanceCrossings(i);
            if(g.phase==Phase::WAITING) {
                if(g.timer){--g.timer;continue;}
                if(transit_>=0)continue;
                if(!r.familyCount||!r.portCount)continue;
                g.family=r.families[next(g.random)%r.familyCount];
                if(!g.family.adult||!chooseDestination(i))continue;
                g.stage[0]=g.stage[1]=g.crossing[0]=g.crossing[1]=0;
                if(!g.family.juvenile)g.stage[1]=4;
                g.phase=Phase::ENTERING;transit_=i;crossIn(i,0);
            } else if(g.phase==Phase::ENTERING) {
                if(g.stage[0]==2&&g.family.juvenile)crossIn(i,1);
                const auto& lead=actors_.state(slot(i,0));
                if(g.stage[0]==2&&!lead.moving&&lead.to==g.approach) {
                    g.approached=true;
                    for(int m=0;m<2;++m)definitions_[slot(i,m)].allowed[g.target]=true;
                }
                travel(i,0,g.approached?g.target:g.approach,false);
                const auto& a=actors_.state(slot(i,0));const auto& b=actors_.state(slot(i,1));
                if(g.stage[0]==2&&!a.moving&&a.to==g.target&&r.zone[a.to]&&
                   (!g.family.juvenile||(g.stage[1]==2&&!b.moving&&r.zone[b.to]&&distance(a.to,b.to)==1))) {
                    g.phase=Phase::VISITING;transit_=-1;g.timer=between(g.random,r.stayMin,r.stayMax);
                }
            } else if(g.phase==Phase::VISITING) {
                if(g.timer){--g.timer;continue;}
                if(transit_>=0)continue;
                g.phase=Phase::LEAVING;transit_=i;g.followTarget=actors_.state(slot(i,g.family.juvenile?1:0)).to;
            } else {
                travel(i,g.family.juvenile?1:0,g.port.cell,true);
                if(g.stage[0]==4&&g.stage[1]==4) {
                    g.phase=Phase::WAITING;transit_=-1;g.timer=between(g.random,r.intervalMin,r.intervalMax);
                }
            }
        }
    }
    Pose pose(int i) const {
        Pose p;p.actor=actors_.pose(i);p.visible=actors_.state(i).active;
        if(i<manual_)return p;
        const int group=(i-manual_)/2,member=(i-manual_)%2;const auto& g=groups_[group];
        p.speciesId=member?g.family.juvenile:g.family.adult;
        const int stage=g.stage[member];
        if(stage==1||stage==3) {
            const float t=float(g.crossing[member])/(definitions_[i].stepTicks*CROSSING_CELLS);
            const float offset=CROSSING_CELLS*(stage==1?1-t:t);
            const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
            p.actor.x+=dx[g.port.direction]*offset;p.actor.y+=dy[g.port.direction]*offset;
            p.actor.direction=stage==1?(g.port.direction+2)%4:g.port.direction;
            p.actor.moving=true;p.actor.frame=g.crossing[member]%4;
        }
        return p;
    }
};
} // namespace AuthoredNpcPopulation
