#pragma once

#include <cmath>
#include <algorithm>
#include "game/MarketData.h"
#include "game/MarketDirection.h"

// Portable 100ms town simulation. All actors reserve the same half-cell mesh;
// no exploration counters, rewards, clocks or save structures are referenced.
namespace Market {
enum class VisitPhase : uint8_t { WAITING, ENTERING, VISITING, LEAVING };
struct Actor {
    uint16_t from=NONE, to=NONE, goal=NONE, previousGoal=NONE, species=0;
    uint16_t wait=0, blocked=0;
    uint16_t motionMs=0;
    uint8_t direction=0, progress=0, stepTicks=3, crossing=0;
    // Crossing: 1 entering, 2 inside, 3 leaving, 4 hidden.
    uint8_t stage=4, patrol=0, halfStep=0;
};
struct Pose { float x=0,y=0,lift=0; bool speaking=false; uint16_t species=0; uint8_t direction=0; bool visible=false,moving=false; };

class Session {
    const Data* data_=nullptr;
    Actor actors_[CAPACITY]{};
    struct Group {
        VisitPhase phase=VisitPhase::WAITING;
        uint16_t trail=NONE, goals[2]={NONE,NONE}, budget=0, dwell=0, retry=0, visited=0;
        uint8_t stall=255, remaining=0, family=255, speaker=0, hopper=255;
        bool browsing=false, shortVisit=false, showAttempted=false;
        uint32_t chat=0;
    };
    Group groups_[GROUP_CAPACITY]{};
    uint32_t gazeRandom_=1, showRandom_=1, stallChat_[STALL_CAPACITY]{};
    uint16_t gazeTimer_[STALL_CAPACITY]{}, responseTimer_[STALL_CAPACITY]{};
    int8_t response_[STALL_CAPACITY]{};
    uint16_t spawnTimer_=0;
    uint8_t lastFamily_=255;
    uint32_t random_=1, lastMs_=0, elapsed_=0;
    uint16_t remainder_=0, timer_=0, budget_=0, dwell_=0;
    uint16_t trail_=NONE;
    uint8_t target_=0;
    bool initialized_=false, paused_=false, exitRequested_=false, finished_=false;
    bool manual_=false, manualNavigationPending_=false;
    int8_t inputDirection_=-1;
    struct Conversation {
        int8_t actors[4]={-1,-1,-1,-1};
        uint16_t motion[4]{},duration[4]{},elapsed=0;
        int8_t speaker=-1;
        bool active=false,ready=false;
    } conversation_;
    VisitPhase phase_=VisitPhase::WAITING;
    uint32_t next() { random_^=random_<<13; random_^=random_>>17; random_^=random_<<5; return random_; }
    uint16_t between(uint16_t a,uint16_t b) { return uint16_t(a+next()%(b-a+1)); }
    int player() const { return data_->residentCount; }
    int partner() const { return player()+1; }
    int guest(int member,int group=0) const { return player()+2+group*2+member; }
    bool lively() const { return data_->maxGroups>1||data_->stallCount; }
    static uint32_t draw(uint32_t& state) { state^=state<<13;state^=state>>17;state^=state<<5;return state; }
    uint16_t gaze(uint16_t a,uint16_t b) { return uint16_t(a+draw(gazeRandom_)%(b-a+1)); }
    uint16_t show(uint16_t a,uint16_t b) { return uint16_t(a+draw(showRandom_)%(b-a+1)); }
    int groupOf(int actor) const { return actor<guest(0)?-1:(actor-guest(0))/2; }
    bool conversationEligible(int i) const {
        if(i<0||i>=count()||i==player()||i==partner()||actors_[i].stage!=2)return false;
        const int group=groupOf(i);
        if(group<0)return true;
        if(visitPhase(group)!=VisitPhase::VISITING)return false;
        const int other=guest((i-guest(0))%2?0:1,group);
        return !actors_[other].species||actors_[other].stage==2;
    }
    void advanceConversation(uint32_t delta) {
        if(conversation_.ready)return;
        conversation_.elapsed=uint16_t(std::min<uint32_t>(10000,conversation_.elapsed+delta));
        bool ready=true;
        for(int k=0;k<4;++k)if(conversation_.actors[k]>=0) {
            auto& a=actors_[conversation_.actors[k]];
            if(a.from==a.to)continue;
            if(conversation_.motion[k]+conversation_.elapsed>=conversation_.duration[k]) {
                a.from=a.to;a.motionMs=0;a.progress=0;
            } else ready=false;
        }
        conversation_.ready=ready;
    }
    bool claimed(int n,int self) const {
        const int owner=groupOf(self);
        for(int g=0;g<data_->maxGroups;++g)if(g!=owner&&groups_[g].stall!=255)
            for(auto point:groups_[g].goals)if(point==n)return true;
        return false;
    }
    uint8_t facing(int from,int to) const {
        const int dx=to%WIDTH-from%WIDTH,dy=to/WIDTH-from/WIDTH;
        return std::abs(dx)>std::abs(dy)?(dx<0?1:3):(dy<0?2:0);
    }
    static int distance(int a,int b) { return std::abs(a%WIDTH-b%WIDTH)+std::abs(a/WIDTH-b/WIDTH); }
    const Mask& allowed(int actor) const {
        if(manual_&&(actor==player()||actor==partner()))return data_->pedestrian;
        return actor < data_->residentCount ? data_->residents[actor].navigation : data_->roads;
    }
    bool manualActor(int actor) const { return manual_&&(actor==player()||actor==partner()); }
    bool reserved(int n,int self) const {
        for(int i=0;i<count();++i) {
            if(i==self || actors_[i].stage==4) continue;
            // Start and destination remain reserved for the whole edge move.
            if(actors_[i].from==n || actors_[i].to==n) return true;
            const auto& a=actors_[i];
            // Reserve both sides of a diagonal too. Cardinal traffic cannot
            // enter its swept square, nor can two diagonals cross mid-edge.
            if(a.from!=a.to&&a.from%WIDTH!=a.to%WIDTH&&a.from/WIDTH!=a.to/WIDTH&&
               (n==node(a.from%WIDTH,a.to/WIDTH)||n==node(a.to%WIDTH,a.from/WIDTH)))return true;
        }
        return false;
    }
    bool openStep(int self,int from,int to,bool dynamic=true) const {
        if(to<0||to>=NODES||!allowed(self).has(to)||(dynamic&&reserved(to,self)))return false;
        if(from%WIDTH!=to%WIDTH&&from/WIDTH!=to/WIDTH) {
            const int sides[]={node(from%WIDTH,to/WIDTH),node(to%WIDTH,from/WIDTH)};
            for(int n:sides)if(!allowed(self).has(n)||(dynamic&&reserved(n,self)))return false;
        }
        return true;
    }
    int route(int self,int goal,bool dynamic=true) const {
        if(goal<0||goal>=NODES||!allowed(self).has(goal)) return -1;
        const int start=actors_[self].stage==4?data_->entry:actors_[self].to;
        if(start>=NODES)return -1;
        int16_t previous[NODES]; for(auto& p:previous)p=-1;
        uint16_t queue[NODES];int head=0,tail=0;
        queue[tail++]=uint16_t(start); previous[start]=int16_t(start);
        while(head<tail) {
            const int n=queue[head++];
            if(n==goal) {
                int step=n;while(step!=start&&previous[step]!=start)step=previous[step];
                return step;
            }
            for(int d=0;d<(manualActor(self)?8:4);++d) {
                const int x=n%WIDTH+DIRECTION_X[d],y=n/WIDTH+DIRECTION_Y[d],v=y*WIDTH+x;
                if(x<0||x>=WIDTH||y<0||y>=HEIGHT||previous[v]>=0||!openStep(self,n,v,dynamic)||
                   (dynamic&&groupOf(self)>=0&&claimed(v,self)))continue;
                previous[v]=int16_t(n);queue[tail++]=uint16_t(v);
            }
        }
        return -1;
    }
    bool move(int i,int goal) {
        auto& a=actors_[i];
        if(a.stage!=2||a.from!=a.to||a.to==goal)return false;
        const int n=route(i,goal);
        if(n<0||n==a.to){++a.blocked;return false;}
        if(i<data_->residentCount) {
            const auto ticks=data_->residents[i].activity.stepTicks;
            a.stepTicks=uint8_t((ticks+(a.halfStep++&1U))/2);
        }
        a.from=a.to;a.to=uint16_t(n);a.progress=0;a.motionMs=0;a.blocked=0;
        a.direction=uint8_t(directionForDelta(n%WIDTH-a.from%WIDTH,n/WIDTH-a.from/WIDTH));
        if(manualActor(i))a.stepTicks=2;
        return true;
    }
    bool exitNode(int n) const {
        // The authored road can have a doorway wider than its center node.
        // Only its contiguous boundary segment opens, never other edges.
        const bool vertical=data_->outward==0||data_->outward==2;
        if(vertical ? n/WIDTH!=data_->entry/WIDTH : n%WIDTH!=data_->entry%WIDTH)return false;
        const int step=vertical?1:WIDTH;
        for(int p=std::min(n,int(data_->entry));p<=std::max(n,int(data_->entry));p+=step)
            if(!data_->roads.has(p)||!allowed(player()).has(p))return false;
        return true;
    }
    bool manualStep(int direction) {
        auto& a=actors_[player()];
        const bool outward=DIRECTION_X[direction]*DIRECTION_X[data_->outward]+
            DIRECTION_Y[direction]*DIRECTION_Y[data_->outward]>0;
        if(outward&&exitNode(a.to)){leave();return true;}
        const int x=a.to%WIDTH+DIRECTION_X[direction],y=a.to/WIDTH+DIRECTION_Y[direction];
        if(x<0||x>=WIDTH||y<0||y>=HEIGHT)return false;
        const auto n=node(x,y);
        // Only the authored doorway opens the map boundary. Request the
        // shared exit flow before reservations so a companion standing in
        // that doorway can leave first without blocking the player.
        if(outward&&exitNode(n)&&openStep(player(),a.to,n,false)){leave();return true;}
        if(x<1||x>=WIDTH-1||y<1||y>=HEIGHT-1)return false;
        if(!openStep(player(),a.to,n))return false;
        a.from=a.to;a.to=n;a.progress=0;a.motionMs=0;a.stepTicks=2;a.direction=uint8_t(direction);
        return true;
    }
    void manualStep() {
        auto& a=actors_[player()];
        if(inputDirection_<0||a.stage!=2||a.from!=a.to||exitRequested_)return;
        a.direction=uint8_t(inputDirection_);
        if(manualStep(inputDirection_)||inputDirection_<4)return;
        // Slide along the available axis when a wall or actor blocks a diagonal.
        if(!manualStep(DIRECTION_X[inputDirection_]<0?1:3))manualStep(DIRECTION_Y[inputDirection_]<0?2:0);
    }
    void yieldPartner() {
        const auto& p=actors_[player()];auto& f=actors_[partner()];
        if(inputDirection_<0||p.stage!=2||p.from!=p.to||f.stage!=2||f.from!=f.to)return;
        const int x=p.to%WIDTH+DIRECTION_X[inputDirection_],y=p.to/WIDTH+DIRECTION_Y[inputDirection_];
        if(f.to!=node(x,y)&&(inputDirection_<4||
           (f.to!=node(x,p.to/WIDTH)&&f.to!=node(p.to%WIDTH,y))))return;
        // A reversing player must not be trapped by their own companion.
        // Try forward, then either side; shared reservations still apply.
        const int turns[]={0,2,-2,1,-1,3,-3};
        for(int turn:turns) {
            const int d=turnDirection(inputDirection_,turn);
            const int x=f.to%WIDTH+DIRECTION_X[d],y=f.to/WIDTH+DIRECTION_Y[d];
            if(x<1||x>=WIDTH-1||y<1||y>=HEIGHT-1)continue;
            const int n=node(x,y);
            if(openStep(partner(),f.to,n)&&move(partner(),n))return;
        }
    }
    void crossing(int i,bool leaving) {
        auto& a=actors_[i];a.stage=leaving?3:1;a.crossing=0;
        a.direction=leaving?data_->outward:(data_->outward+2)%4;
    }
    bool activate(int i,uint16_t species) {
        if(reserved(data_->entry,i))return false;
        auto& a=actors_[i];a={};a.from=a.to=data_->entry;a.species=species;a.stepTicks=3;
        crossing(i,false);return true;
    }
    uint16_t pickGoal(int self) {
        uint16_t candidates[NODES];int count=0;
        if(self<data_->residentCount) {
            const auto& d=data_->residents[self].activity;
            if(d.behavior==AuthoredNpc::Behavior::IDLE)return NONE;
            if(d.behavior==AuthoredNpc::Behavior::PATROL&&d.patrolCount)
                return cellNode(d.patrol[actors_[self].patrol%d.patrolCount]);
            for(int y=1;y<HEIGHT;y+=2)for(int x=1;x<WIDTH;x+=2) {
                const auto n=node(x,y);
                if(allowed(self).has(n)&&n!=actors_[self].to&&!reserved(n,self)&&route(self,n,false)>=0)candidates[count++]=n;
            }
        } else {
            for(int s=0;s<data_->stopCount;++s) {
                const auto n=data_->stops[s];
                if(lively()&&groupOf(self)>=0) {
                    const int exits=int(data_->roads.has(n-WIDTH))+int(data_->roads.has(n-1))+
                        int(data_->roads.has(n+1))+int(data_->roads.has(n+WIDTH));
                    if(exits<3||claimed(n,self))continue;
                }
                if(n!=actors_[self].to&&n!=actors_[self].previousGoal&&!reserved(n,self))candidates[count++]=n;
            }
        }
        return count?candidates[next()%count]:NONE;
    }
    void follow(int follower,int leader,uint16_t trail) {
        auto& a=actors_[follower];const auto& b=actors_[leader];
        if(a.stage!=2||a.from!=a.to)return;
        if(b.stage>=3){move(follower,data_->entry);return;}
        if(distance(a.to,b.to)<=2)return;
        // Follow the vacated route, then pick a nearby free place while stopped.
        if(trail!=NONE&&trail!=b.to&&move(follower,trail))return;
        if(distance(a.to,b.to)>2) {
            int best=-1,score=NODES;
            for(int d=0;d<(manualActor(follower)?8:4);++d) {
                const int x=b.to%WIDTH+DIRECTION_X[d],y=b.to/WIDTH+DIRECTION_Y[d];
                if(x<0||x>=WIDTH||y<0||y>=HEIGHT)continue;
                const int n=node(x,y),cost=distance(a.to,n);
                if(cost<score&&route(follower,n)>=0){best=n;score=cost;}
            }
            if(best>=0)move(follower,best);
        }
    }
    void receivers() {
        for(int i=0;i<data_->residentCount;++i) {
            if(data_->residents[i].reception>3)continue;
            auto& a=actors_[i];int target=-1;
            for(int g=0;g<data_->maxGroups;++g) {
                const auto& v=groups_[g];
                if(v.stall==255||data_->stalls[v.stall].resident!=i||!v.browsing)continue;
                target=guest(v.speaker,g);break;
            }
            if(target<0&&!exitRequested_&&actors_[player()].stage==2) {
                const int n=actors_[player()].to,side=data_->residents[i].reception;
                const int dx=n%WIDTH-a.to%WIDTH,dy=n/WIDTH-a.to/WIDTH;
                const int front=side==0?dy:side==1?-dx:side==2?-dy:dx;
                if(front>0&&dx*dx+dy*dy<=9) {
                    bool clear=true;const int steps=std::max(std::abs(dx),std::abs(dy));
                    for(int k=0;k<=steps;++k) {
                        const int x=a.to%WIDTH+int(std::lround(float(dx)*k/steps));
                        const int y=a.to/WIDTH+int(std::lround(float(dy)*k/steps));
                        if(!data_->pedestrian.has(node(x,y)))clear=false;
                    }
                    if(clear)target=player();
                }
            }
            // A browsing group takes priority; player response stays stable
            // for two seconds even when another nearby actor crosses it.
            if(target<guest(0)&&response_[i]>=guest(0)){response_[i]=-1;responseTimer_[i]=0;}
            if(target>=guest(0)||responseTimer_[i]==0||response_[i]<0) {
                response_[i]=int8_t(target);responseTimer_[i]=target<0?0:gaze(20,50);
            }
            if(responseTimer_[i])--responseTimer_[i];
            if(response_[i]>=0&&(actors_[response_[i]].stage!=2||exitRequested_)) {
                response_[i]=-1;responseTimer_[i]=0;
            }
            if(a.from==a.to&&response_[i]>=0)a.direction=facing(a.to,actors_[response_[i]].to);
        }
    }
    void observe(int i) {
        const auto& r=data_->residents[i];if(r.reception>3)return;
        if(gazeTimer_[i]){--gazeTimer_[i];return;}
        auto& a=actors_[i];
        const bool front=gaze(0,9)<7;
        a.direction=r.reception;
        if(!front) {
            uint8_t options[4]{};int size=0;
            for(int d=0;d<4;++d)if(d!=r.reception&&(r.lookMask&(1U<<d)))options[size++]=uint8_t(d);
            if(size)a.direction=options[gaze(0,uint16_t(size-1))];
        }
        gazeTimer_[i]=front?gaze(30,60):gaze(15,25);
    }
    void residents() {
        for(int i=0;i<data_->residentCount;++i) {
            auto& a=actors_[i];if(a.from!=a.to)continue;
            if(response_[i]>=0&&data_->residents[i].reception<4)continue;
            if(a.wait){--a.wait;observe(i);continue;}
            if(data_->residents[i].activity.behavior==AuthoredNpc::Behavior::IDLE)observe(i);
            const auto& d=data_->residents[i].activity;
            if(a.goal==a.to) {
                a.previousGoal=a.goal;a.goal=NONE;a.wait=between(d.waitMin,d.waitMax);++a.patrol;gazeTimer_[i]=0;observe(i);continue;
            }
            if(a.goal==NONE)a.goal=pickGoal(i);
            if(a.goal!=NONE&&!move(i,a.goal)&&a.blocked>30){a.goal=NONE;a.blocked=0;}
        }
    }
    void legacyVisitors() {
        if(!data_->familyCount)return;
        auto& a=actors_[guest(0)];auto& b=actors_[guest(1)];
        if(phase_==VisitPhase::WAITING) {
            if(timer_){--timer_;return;}
            // Keep a blocked selection pending without advancing the RNG.
            const auto previousRandom=random_;
            const auto f=data_->families[next()%data_->familyCount];
            int free=0;for(int i=0;i<data_->stopCount;++i)free+=!reserved(data_->stops[i],-1);
            if(free<(f.juvenile?2:1)||reserved(data_->entry,-1)){random_=previousRandom;return;}
            if(!activate(guest(0),f.adult))return;
            b={};b.stage=4;b.species=f.juvenile;
            a.goal=pickGoal(guest(0));trail_=data_->entry;phase_=VisitPhase::ENTERING;
        }
        if(phase_==VisitPhase::ENTERING||phase_==VisitPhase::VISITING) {
            if(a.stage==2&&b.stage==4&&b.species)activate(guest(1),b.species);
            follow(guest(1),guest(0),trail_);
            if(a.stage==2&&a.from==a.to) {
                if(a.goal==NONE)a.goal=pickGoal(guest(0));
                if(a.to==a.goal) {
                    if(phase_==VisitPhase::ENTERING) {
                        phase_=VisitPhase::VISITING;budget_=between(data_->stayMin,data_->stayMax);dwell_=between(30,80);
                    } else if(dwell_)--dwell_;
                    else {a.previousGoal=a.goal;a.goal=pickGoal(guest(0));dwell_=between(30,80);}
                } else if(b.stage==4||b.stage==1||distance(a.to,b.to)<=6) {
                    const auto old=a.to;
                    if(move(guest(0),a.goal))trail_=old;
                    else if(a.blocked>30){a.goal=pickGoal(guest(0));a.blocked=0;}
                }
            }
            if(phase_==VisitPhase::VISITING&&budget_&&!--budget_)phase_=VisitPhase::LEAVING;
        }
        if(phase_==VisitPhase::LEAVING) {
            // Reverse leadership: the trailing juvenile can leave a short
            // stall without attempting to swap through its companion.
            auto& lead=b;auto& child=a;
            if(lead.stage==4&&child.stage<3) {
                // Entry may have been delayed by the player. Do not spawn a
                // new child just to complete an expired visit.
                if(move(guest(0),data_->entry))trail_=child.from;
                if(child.to==data_->entry&&child.from==child.to)crossing(guest(0),true);
            } else {
                if(lead.stage==2&&lead.from==lead.to) {
                    if(lead.to==data_->entry)crossing(guest(1),true);
                    else {const auto old=lead.to;if(move(guest(1),data_->entry))trail_=old;}
                }
                follow(guest(0),guest(1),trail_);
                if(lead.stage>=3&&child.stage==2&&child.to==data_->entry&&child.from==child.to)crossing(guest(0),true);
            }
            if(a.stage==4&&b.stage==4){phase_=VisitPhase::WAITING;timer_=between(data_->intervalMin,data_->intervalMax);}
        }
    }
    void cancelShow(Group& g) { g.chat=0;g.hopper=255; }
    void depart(Group& g) {
        cancelShow(g);g.stall=255;g.goals[0]=g.goals[1]=NONE;
        g.browsing=false;g.phase=VisitPhase::LEAVING;
    }
    bool advanceGuest(int i,int goal) {
        auto& a=actors_[i];
        if(a.stage!=2||a.from!=a.to||goal==NONE||a.to==goal)return false;
        if(move(i,goal))return true;
        // Let a blocked group step aside without changing its logical goal.
        // Every detour is a normal reserved edge, never a teleport.
        if(a.blocked<10)return false;
        const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
        const int start=between(0,3);
        for(int j=0;j<4;++j) {
            const int d=(start+j)%4,x=a.to%WIDTH+dx[d],y=a.to/WIDTH+dy[d];
            if(x<1||x>=WIDTH-1||y<1||y>=HEIGHT-1)continue;
            const int n=node(x,y);
            if(!claimed(n,i)&&!reserved(n,i)&&allowed(i).has(n)&&move(i,n))return true;
        }
        return false;
    }
    bool selectStall(int index) {
        auto& g=groups_[index];g.retry=10;
        if(!data_->stallCount)return false;
        const int start=between(0,uint16_t(data_->stallCount-1));
        for(int j=0;j<data_->stallCount;++j) {
            const int s=(start+j)%data_->stallCount;const auto& stall=data_->stalls[s];
            if(g.visited&(1U<<s))continue;
            bool busy=false;for(int k=0;k<data_->maxGroups;++k)if(k!=index&&groups_[k].stall==s)busy=true;
            if(busy)continue;
            for(int a=0;a<stall.pointCount;++a) {
                const int first=stall.points[a];
                if(reserved(first,guest(0,index))||claimed(first,guest(0,index))||route(guest(0,index),first,false)<0)continue;
                int second=NONE;
                if(actors_[guest(1,index)].species) {
                    for(int b=0;b<stall.pointCount;++b) {
                        const int n=stall.points[b];
                        if(n!=first&&distance(first,n)==2&&!reserved(n,guest(1,index))&&!claimed(n,guest(1,index))&&
                           route(guest(1,index),n,false)>=0){second=n;break;}
                    }
                    if(second==NONE)continue;
                }
                g.stall=uint8_t(s);g.goals[0]=uint16_t(first);g.goals[1]=uint16_t(second);
                g.visited|=uint16_t(1U<<s);g.retry=0;g.browsing=false;g.showAttempted=false;return true;
            }
        }
        return false;
    }
    void startShow(int index) {
        auto& g=groups_[index];
        if(g.showAttempted||!data_->performances||g.stall==255||g.shortVisit)return;
        const int vendor=data_->stalls[g.stall].resident;
        if(actors_[vendor].from!=actors_[vendor].to)return;
        g.showAttempted=true;
        if(show(0,9)>=6)return;
        if(stallChat_[g.stall]&&elapsed_-stallChat_[g.stall]<10000)return;
        int active=0;for(int k=0;k<data_->maxGroups;++k)active+=groups_[k].chat!=0;
        if(active>=2)return;
        g.chat=elapsed_;stallChat_[g.stall]=elapsed_;
        g.speaker=actors_[guest(1,index)].species&&show(0,4)==0?1:0;
        if(show(0,9)<4)g.hopper=uint8_t(show(0,1)?vendor:guest(g.speaker,index));
    }
    void spawnGroup() {
        if(spawnTimer_){--spawnTimer_;return;}
        if(exitRequested_||!data_->familyCount||reserved(data_->entry,-1))return;
        int slot=-1,population=0;bool solo=false,pair=false;
        for(int g=0;g<data_->maxGroups;++g) {
            const auto& v=groups_[g];
            if(v.phase==VisitPhase::WAITING){if(slot<0)slot=g;continue;}
            if(v.phase==VisitPhase::ENTERING&&
               (actors_[guest(0,g)].stage!=2||(actors_[guest(1,g)].species&&actors_[guest(1,g)].stage!=2)))return;
            const bool two=actors_[guest(1,g)].species!=0;population+=two?2:1;pair|=two;solo|=!two;
        }
        if(slot<0||population>=data_->maxVisitors)return;
        uint8_t candidates[64]{};int count=0;
        bool wantSolo=!solo?true:!pair?false:between(0,99)<data_->soloPercent;
        for(int pass=0;pass<3&&!count;++pass)for(int f=0;f<data_->familyCount;++f) {
            const auto& family=data_->families[f];
            if(population+(family.juvenile?2:1)>data_->maxVisitors)continue;
            if(pass<2&&wantSolo!=(family.juvenile==0))continue;
            bool duplicate=false;for(int g=0;g<data_->maxGroups;++g)
                if(groups_[g].phase!=VisitPhase::WAITING&&groups_[g].family==f)duplicate=true;
            if(pass==0&&(duplicate||lastFamily_==f))continue;
            candidates[count++]=uint8_t(f);
        }
        if(!count)return;
        const auto family=candidates[between(0,uint16_t(count-1))];const auto f=data_->families[family];
        if(!activate(guest(0,slot),f.adult))return;
        auto& b=actors_[guest(1,slot)];b={};b.species=f.juvenile;
        auto& g=groups_[slot];g={};g.phase=VisitPhase::ENTERING;g.trail=data_->entry;g.family=family;
        g.budget=between(data_->stayMin,data_->stayMax);g.shortVisit=between(0,4)==0;
        g.remaining=g.shortVisit?1:uint8_t(between(2,3));
        if(g.shortVisit)g.budget=uint16_t(g.budget/2);
        selectStall(slot);lastFamily_=family;spawnTimer_=between(data_->intervalMin,data_->intervalMax);
    }
    void livelyVisitors() {
        spawnGroup();
        // Rotate decision order so the same slot does not win every crossing.
        const int first=int(elapsed_/100%data_->maxGroups);
        for(int turn=0;turn<data_->maxGroups;++turn) {
            const int index=(turn+first)%data_->maxGroups;auto& g=groups_[index];
            if(g.phase==VisitPhase::WAITING)continue;
            auto& a=actors_[guest(0,index)];auto& b=actors_[guest(1,index)];
            if(g.phase!=VisitPhase::LEAVING&&(exitRequested_||!g.budget||!--g.budget))depart(g);
            if(g.chat&&elapsed_-g.chat>=3500)cancelShow(g);
            if(g.phase==VisitPhase::ENTERING||g.phase==VisitPhase::VISITING) {
                if(a.stage==2&&b.stage==4&&b.species)activate(guest(1,index),b.species);
                if(g.retry)--g.retry;
                if(g.stall==255&&!g.retry)selectStall(index);
                if(g.stall==255) {
                    // If all stalls are busy, watch briefly at an ordinary
                    // road stop and retry; the visit budget still advances.
                    if(a.to==a.goal&&a.from==a.to) {
                        if(!g.dwell)g.dwell=between(10,30);
                        if(g.dwell>1){--g.dwell;if(g.dwell%15==0)a.direction=uint8_t(gaze(0,3));}
                        else {g.dwell=0;a.previousGoal=a.goal;a.goal=pickGoal(guest(0,index));}
                        if(!data_->stallCount)g.phase=VisitPhase::VISITING;
                    } else {
                        if(a.goal==NONE)a.goal=pickGoal(guest(0,index));
                        const auto old=a.to;if(advanceGuest(guest(0,index),a.goal))g.trail=old;
                    }
                    follow(guest(1,index),guest(0,index),g.trail);continue;
                }
                const bool ready=a.stage==2&&a.from==a.to&&a.to==g.goals[0]&&
                    (!b.species||(b.stage==2&&b.from==b.to&&b.to==g.goals[1]));
                if(ready) {
                    if(!g.browsing) {
                        g.browsing=true;g.phase=VisitPhase::VISITING;
                        g.dwell=g.shortVisit?between(10,30):between(0,3)?between(40,80):between(80,120);
                        startShow(index);
                    }
                    const auto& vendor=actors_[data_->stalls[g.stall].resident];
                    a.direction=facing(a.to,vendor.to);
                    if(b.species) {
                        if(g.chat||g.dwell%20==0)b.direction=facing(b.to,g.chat&&g.speaker==0?actors_[guest(0,index)].to:vendor.to);
                    }
                    // The vendor may have been mid-edge on arrival. Retry
                    // once it is stationary, within the existing dwell only.
                    if(!g.chat&&g.dwell>35&&g.dwell%10==0&&actors_[data_->stalls[g.stall].resident].from==vendor.to)
                        startShow(index);
                    if(g.dwell&&!--g.dwell) {
                        cancelShow(g);g.stall=255;g.goals[0]=g.goals[1]=NONE;g.browsing=false;
                        if(!--g.remaining)depart(g);else selectStall(index);
                    }
                } else {
                    const auto old=a.to;if(advanceGuest(guest(0,index),g.goals[0]))g.trail=old;
                    if(b.species)advanceGuest(guest(1,index),g.goals[1]);
                    if(a.blocked>30||b.blocked>30) {
                        cancelShow(g);g.stall=255;g.goals[0]=g.goals[1]=NONE;g.browsing=false;selectStall(index);
                    }
                }
            }
            if(g.phase==VisitPhase::LEAVING) {
                const int lead=b.stage==4?guest(0,index):guest(1,index);
                const int child=lead==guest(0,index)?guest(1,index):guest(0,index);
                auto& l=actors_[lead];auto& c=actors_[child];
                if(l.stage==2&&l.from==l.to) {
                    if(l.to==data_->entry)crossing(lead,true);
                    else {const auto old=l.to;if(advanceGuest(lead,data_->entry))g.trail=old;}
                }
                if(c.stage==2) {
                    if(c.to==data_->entry&&c.from==c.to)crossing(child,true);
                    else advanceGuest(child,data_->entry);
                }
                if(a.stage==4&&b.stage==4){g={};a={};b={};}
            }
        }
    }
    void party() {
        auto& p=actors_[player()];auto& f=actors_[partner()];
        if(p.stage==2&&f.species&&f.stage==4&&!exitRequested_)activate(partner(),f.species);
        if(manual_&&exitRequested_) {
            // Either member may be closer to the exit after free walking.
            // Route both home instead of forcing the companion to trail a
            // player whose only escape can be occupied by that companion.
            if(f.stage==2&&f.from==f.to) {
                if(f.to==data_->entry)crossing(partner(),true);
                else move(partner(),data_->entry);
            }
        } else {
            if(manual_)yieldPartner();
            follow(partner(),player(),p.from);
        }
        if(manual_&&!exitRequested_)manualStep();
        if(p.stage==2&&p.from==p.to&&p.goal!=NONE) {
            if(p.to==p.goal){p.goal=NONE;if(!exitRequested_)nextTarget();}
            else move(player(),p.goal);
        }
        if(exitRequested_&&p.stage==2&&p.to==data_->entry&&p.from==p.to)crossing(player(),true);
        if(exitRequested_&&p.stage>=3&&f.stage==2&&f.to==data_->entry&&f.from==f.to)crossing(partner(),true);
        if(exitRequested_&&p.stage==4&&(f.stage==4||!f.species))finished_=true;
    }
    void tick() {
        elapsed_+=100;
        for(int i=0;i<count();++i) {
            auto& a=actors_[i];
            if(a.stage==1||a.stage==3) {
                if(++a.crossing>=a.stepTicks*6) {a.stage=a.stage==1?2:4;a.crossing=0;}
            } else if(!manualActor(i)&&a.stage==2&&a.from!=a.to&&++a.progress>=a.stepTicks) {
                a.from=a.to;a.progress=0;
            }
        }
        receivers();residents();party();
        if(lively())livelyVisitors();else legacyVisitors();
    }
public:
    bool reset(const Data& d,uint32_t seed,uint16_t playerSpecies,uint16_t companionSpecies,uint32_t now) {
        if(d.entry>=NODES||d.outward>3||(d.version!=1&&d.version!=2)||d.residentCount>12||d.stopCount>STOP_CAPACITY||d.familyCount>64||!d.roads.has(d.entry))return false;
        if(!d.intervalMin||d.intervalMax<d.intervalMin||!d.stayMin||d.stayMax<d.stayMin)return false;
        if(!d.maxGroups||d.maxGroups>GROUP_CAPACITY||d.residentCount+2+2*d.maxGroups>CAPACITY||
           !d.maxVisitors||d.maxVisitors>2*d.maxGroups||d.soloPercent>100||d.stallCount>STALL_CAPACITY||
           d.firstMax<d.firstMin)return false;
        for(int i=0;i<d.stallCount;++i) {
            const auto& s=d.stalls[i];
            if(s.resident>=d.residentCount||!s.pointCount||s.pointCount>CUSTOMER_CAPACITY)return false;
            for(int j=0;j<s.pointCount;++j)if(!d.roads.has(s.points[j])||distance(s.points[j],d.entry)<=4)return false;
        }
        for(int i=0;i<d.familyCount;++i) {
            const auto f=d.families[i];
            if(!f.adult||f.adult==f.juvenile||d.stopCount<(f.juvenile?2:1))return false;
        }
        for(int i=0;i<d.stopCount;++i)if(!d.roads.has(d.stops[i]))return false;
        for(int i=0;i<d.residentCount;++i) {
            const auto& r=d.residents[i];const auto& a=r.activity;
            if(a.spawn>=CELLS||!r.navigation.has(cellNode(a.spawn))||a.direction>3||a.stepTicks<2||
               !a.waitMin||a.waitMax<a.waitMin||a.patrolCount>32||(r.reception!=255&&r.reception>3)||r.lookMask>15)return false;
        }
        data_=&d;random_=seed?seed:1;lastMs_=now;elapsed_=0;remainder_=0;initialized_=true;
        paused_=exitRequested_=finished_=false;target_=0;phase_=VisitPhase::WAITING;conversation_={};
        manual_=false;manualNavigationPending_=false;inputDirection_=-1;budget_=dwell_=0;trail_=NONE;
        for(auto& a:actors_)a={};
        for(auto& g:groups_)g={};
        for(auto& value:stallChat_)value=0;
        for(int i=0;i<STALL_CAPACITY;++i){response_[i]=-1;responseTimer_[i]=gazeTimer_[i]=0;}
        gazeRandom_=(random_^0x91e10da5U)|1U;showRandom_=(random_^0xa341316cU)|1U;lastFamily_=255;
        for(int i=0;i<d.residentCount;++i) {
            auto& a=actors_[i];const auto& r=d.residents[i];a.stage=2;
            a.from=a.to=cellNode(r.activity.spawn);a.species=r.species;a.direction=r.activity.direction;
            a.stepTicks=r.activity.stepTicks/2?r.activity.stepTicks/2:1;
            a.wait=between(r.activity.waitMin,r.activity.waitMax);
        }
        actors_[partner()].species=companionSpecies;
        if(playerSpecies&&!activate(player(),playerSpecies))return false;
        // Move clear of the entrance before allowing an intentional departure.
        actors_[player()].goal=d.stopCount?d.stops[0]:NONE;
        timer_=between(d.intervalMin,d.intervalMax);
        spawnTimer_=d.firstMin?between(d.firstMin,d.firstMax):timer_;return true;
    }
    int count() const { return data_?data_->residentCount+2+2*data_->maxGroups:0; }
    const Data& data() const { return *data_; }
    const Actor& actor(int i) const { return actors_[i]; }
    VisitPhase visitPhase(int group=0) const { return lively()?groups_[group].phase:phase_; }
    int visitingStall(int group) const { return groups_[group].stall==255?-1:groups_[group].stall; }
    uint16_t customerPoint(int group,int member) const { return groups_[group].goals[member]; }
    bool finished() const { return finished_; }
    bool paused() const { return paused_; }
    bool exiting() const { return exitRequested_; }
    bool manual() const { return manual_; }
    void enableManual() {
        manual_=true;inputDirection_=-1;
        // Finish the doorway crossing, then wait for the user's direction.
        const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
        const int inward=(data_->outward+2)%4;
        actors_[player()].goal=node(data_->entry%WIDTH+dx[inward],data_->entry/WIDTH+dy[inward]);
    }
    void setDirection(int direction) {
        const int8_t nextDirection=!paused_&&!exitRequested_&&!conversing()&&direction>=0&&direction<8?int8_t(direction):-1;
        if(nextDirection!=inputDirection_)manualNavigationPending_=true;
        inputDirection_=nextDirection;
        if(inputDirection_>=0)actors_[player()].goal=NONE;
    }
    int nearbyNpc(int previous=-1,bool forConversation=false) const {
        const auto p=playerPose();
        const auto eligible=[&](int i,float radius) {
            if(i<0||i>=count()||i==player()||i==partner()||actors_[i].stage!=2)return false;
            if(forConversation&&!conversationEligible(i))return false;
            const auto q=pose(i);const float dx=q.x-p.x,dy=q.y-p.y;
            if(dx*dx+dy*dy>radius*radius)return false;
            // Every half-cell between the two feet must be open ground.
            const int steps=1+int(std::ceil(std::sqrt(dx*dx+dy*dy)/4));
            for(int s=0;s<=steps;++s) {
                const int x=int(std::lround((p.x+dx*s/steps)/13));
                const int y=int(std::lround((p.y+dy*s/steps)/13));
                if(x<0||x>=WIDTH||y<0||y>=HEIGHT||!data_->pedestrian.has(node(x,y)))return false;
            }
            return true;
        };
        if(actors_[player()].stage!=2||exitRequested_)return -1;
        if(eligible(previous,44))return previous;
        int best=-1;float distanceSquared=32*32;
        for(int i=0;i<count();++i)if(eligible(i,32)) {
            const auto q=pose(i);const float d=(q.x-p.x)*(q.x-p.x)+(q.y-p.y)*(q.y-p.y);
            if(d<=distanceSquared){best=i;distanceSquared=d;}
        }
        return best;
    }
    uint32_t timeMs() const { return elapsed_+remainder_; }
    bool conversing() const {return conversation_.active;}
    bool conversationReady() const {return conversation_.active&&conversation_.ready;}
    int conversationActor(int role) const {return role>=0&&role<4?conversation_.actors[role]:-1;}
    bool beginConversation(int target) {
        if(!manual_||paused_||conversing()||!conversationEligible(target)||nearbyNpc(target,true)!=target)return false;
        conversation_={};conversation_.active=true;
        conversation_.actors[0]=int8_t(player());conversation_.actors[1]=int8_t(target);
        if(actors_[partner()].species&&actors_[partner()].stage==2)conversation_.actors[3]=int8_t(partner());
        const int group=groupOf(target);
        if(group>=0) {
            const int other=guest((target-guest(0))%2?0:1,group);
            if(actors_[other].species)conversation_.actors[2]=int8_t(other);
        }
        for(int k=0;k<4;++k)if(conversation_.actors[k]>=0) {
            const int i=conversation_.actors[k];const auto& a=actors_[i];
            conversation_.duration[k]=manualActor(i)?walkDurationMs(a.direction):a.stepTicks*100;
            conversation_.motion[k]=manualActor(i)?a.motionMs:a.progress*100+remainder_;
        }
        inputDirection_=-1;manualNavigationPending_=false;
        for(auto& g:groups_)cancelShow(g);
        advanceConversation(0);return true;
    }
    void conversationSpeaker(int actor) {conversation_.speaker=int8_t(actor);}
    void endConversation() {conversation_={};inputDirection_=-1;manualNavigationPending_=false;}
    void pause(bool value) { paused_=value;if(value)inputDirection_=-1; }
    void stopPlayer() { actors_[player()].goal=NONE; }
    void nextTarget() { if(data_->stopCount)target_=(target_+1)%data_->stopCount; }
    uint16_t selectedTarget() const { return data_->stopCount?data_->stops[target_]:data_->entry; }
    bool walkTo(uint16_t goal) {
        if(conversing()||exitRequested_||!data_->roads.has(goal)||route(player(),goal,false)<0)return false;
        actors_[player()].goal=goal;return true;
    }
    void walkSelected() { walkTo(selectedTarget()); }
    void leave() {
        endConversation();
        inputDirection_=-1;exitRequested_=true;paused_=false;actors_[player()].goal=data_->entry;
        if(lively())for(auto& g:groups_)if(g.phase!=VisitPhase::WAITING)depart(g);
        for(int i=0;i<STALL_CAPACITY;++i){response_[i]=-1;responseTimer_[i]=0;}
    }
    uint16_t nearest(float worldX,float worldY) const {
        int best=NONE;float dist=26.0f*26.0f;
        for(int n=0;n<NODES;++n)if(data_->roads.has(n)) {
            float dx=n%WIDTH*13.0f-worldX,dy=n/WIDTH*13.0f-worldY,d=dx*dx+dy*dy;
            if(d<dist){dist=d;best=n;}
        }
        return uint16_t(best);
    }
    bool update(uint32_t now,bool active=true) {
        if(!initialized_)return false;
        const uint32_t delta=now-lastMs_;lastMs_=now;
        if(!active||paused_||finished_) {if(!active)inputDirection_=-1;return false;}
        uint32_t remaining=delta>250?0:delta;
        if(conversing()){advanceConversation(remaining);return !conversation_.ready;}
        if(manual_&&manualNavigationPending_){manualNavigationPending_=false;party();}
        while(remaining&&!finished_) {
            // Stop at arrivals as well as simulation ticks. The manual pair
            // keeps millisecond precision without changing NPC timing, and a
            // diagonal takes 283ms instead of gaining sqrt(2) speed.
            uint32_t step=std::min<uint32_t>(remaining,100-remainder_);
            bool arrived=false;
            if(manual_)for(int i=player();i<=partner();++i) {
                const auto& a=actors_[i];
                if(a.stage==2&&a.from!=a.to)step=std::min<uint32_t>(step,walkDurationMs(a.direction)-a.motionMs);
            }
            if(manual_)for(int i=player();i<=partner();++i) {
                auto& a=actors_[i];
                if(a.stage!=2||a.from==a.to)continue;
                a.motionMs=uint16_t(a.motionMs+step);
                if(a.motionMs>=walkDurationMs(a.direction)){a.from=a.to;a.motionMs=0;arrived=true;}
            }
            remaining-=step;remainder_=uint16_t(remainder_+step);
            if(remainder_==100){remainder_=0;tick();}
            else if(arrived)party();
        }
        return true;
    }
    Pose pose(int i) const {
        Pose p;const auto& a=actors_[i];p.visible=a.stage!=4;p.species=a.species;p.direction=a.direction;
        if(!p.visible)return p;
        const float fraction=remainder_/100.0f;
        float t=a.from==a.to?1.0f:manualActor(i)?float(a.motionMs)/walkDurationMs(a.direction):(a.progress+fraction)/a.stepTicks;
        int participant=-1;
        if(conversing())for(int k=0;k<4;++k)if(conversation_.actors[k]==i)participant=k;
        if(participant>=0&&a.from!=a.to)t=std::min(1.0f,float(conversation_.motion[participant]+conversation_.elapsed)/conversation_.duration[participant]);
        p.x=(a.from%WIDTH+(int(a.to%WIDTH)-int(a.from%WIDTH))*t)*13.0f;
        p.y=(a.from/WIDTH+(int(a.to/WIDTH)-int(a.from/WIDTH))*t)*13.0f;
        p.moving=a.from!=a.to;
        if(a.stage==1||a.stage==3) {
            const float t=(a.crossing+fraction)/(a.stepTicks*6);
            const float offset=78.0f*(a.stage==1?1-t:t);
            const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
            p.x+=dx[data_->outward]*offset;p.y+=dy[data_->outward]*offset;p.moving=true;
        }
        if(conversing()&&participant<0)p.moving=false;
        if(lively()&&a.stage==2&&!p.moving)for(int g=0;g<data_->maxGroups;++g) {
            const auto& v=groups_[g];if(!v.chat||v.stall==255)continue;
            const auto t=timeMs()-v.chat;
            const int speaker=guest(v.speaker,g),vendor=data_->stalls[v.stall].resident;
            p.speaking|=(i==speaker&&t>=600&&t<1800)||(i==vendor&&t>=2100&&t<3300);
            const uint32_t hopAt=v.hopper==vendor?2100:600;
            if(i==v.hopper&&t>=hopAt&&t<hopAt+300)
                p.lift=4.0f*std::sin(float(t-hopAt)/300.0f*3.14159265f);
        }
        if(conversationReady()&&participant>=0) {
            const auto& target=actors_[conversation_.actors[participant==0||participant==3?1:0]];
            p.direction=uint8_t(directionForDelta(int(target.to%WIDTH)-int(a.to%WIDTH),int(target.to/WIDTH)-int(a.to/WIDTH)));
            p.speaking=conversation_.speaker==i;p.lift=0;
        }
        return p;
    }
    Pose playerPose() const { return pose(player()); }
};
} // namespace Market
