#include "game/MarketMapData.h"
#include "game/MarketSession.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static void validate(const Market::Session& s) {
    const auto& d=s.data();
    for(int i=0;i<s.count();++i) {
        const auto& a=s.actor(i);
        if(a.stage==2) {
            const auto& mask=i<d.residentCount?d.residents[i].navigation:
                i<d.residentCount+2?d.pedestrian:d.roads;
            assert(mask.has(a.from)&&mask.has(a.to));
            if(a.from%Market::WIDTH!=a.to%Market::WIDTH&&a.from/Market::WIDTH!=a.to/Market::WIDTH) {
                assert(mask.has(Market::node(a.from%Market::WIDTH,a.to/Market::WIDTH)));
                assert(mask.has(Market::node(a.to%Market::WIDTH,a.from/Market::WIDTH)));
            }
        }
        if(a.stage!=4)for(int j=0;j<i;++j) {
            const auto& b=s.actor(j);if(b.stage==4)continue;
            assert(a.from!=b.from&&a.from!=b.to&&a.to!=b.from&&a.to!=b.to);
            const auto swept=[&](const Market::Actor& diagonal,const Market::Actor& other) {
                if(diagonal.from%Market::WIDTH==diagonal.to%Market::WIDTH||
                   diagonal.from/Market::WIDTH==diagonal.to/Market::WIDTH)return;
                for(auto side:{Market::node(diagonal.from%Market::WIDTH,diagonal.to/Market::WIDTH),
                               Market::node(diagonal.to%Market::WIDTH,diagonal.from/Market::WIDTH)})
                    assert(side!=other.from&&side!=other.to);
            };
            swept(a,b);swept(b,a);
        }
    }
}

static Market::Data openGround() {
    Market::Data d;d.entry=Market::node(Market::WIDTH/2,Market::HEIGHT-1);
    for(int y=1;y<Market::HEIGHT-1;++y)for(int x=1;x<Market::WIDTH-1;++x) {
        d.roads.set(Market::node(x,y));d.pedestrian.set(Market::node(x,y));
    }
    d.roads.set(d.entry);d.pedestrian.set(d.entry);return d;
}

static void eightDirections() {
    auto d=openGround();
    for(int direction=0;direction<8;++direction) {
        Market::Session s;assert(s.reset(d,38,25,0,0));s.enableManual();
        auto& p=const_cast<Market::Actor&>(s.actor(0));
        p.stage=2;p.from=p.to=Market::node(15,15);p.goal=Market::NONE;
        s.setDirection(direction);
        for(uint32_t t=17;t<=1020;t+=17) {
            s.update(t);validate(s);
            const auto pose=s.playerPose();
            const float amount=13.0f*t/Market::walkDurationMs(direction);
            assert(std::abs(pose.x-(195+Market::DIRECTION_X[direction]*amount))<0.01f);
            assert(std::abs(pose.y-(195+Market::DIRECTION_Y[direction]*amount))<0.01f);
            assert(pose.direction==direction);
        }
        s.pause(true);const auto paused=s.playerPose();s.update(1100);
        assert(s.playerPose().x==paused.x&&s.playerPose().y==paused.y);
        s.pause(false);s.update(1200);s.update(1300);s.update(1400);
        const auto stopped=s.playerPose();s.update(1500);
        assert(stopped.x==s.playerPose().x&&stopped.y==s.playerPose().y);
    }
    const auto block=[&](int x,int y) {const auto n=Market::node(x,y);d.pedestrian.bytes[n/8]&=uint8_t(~(1U<<(n%8)));};
    // A diagonal cannot cut the corner. It slides down the open axis.
    block(11,10);
    Market::Session s;assert(s.reset(d,38,25,0,0));s.enableManual();
    auto& p=const_cast<Market::Actor&>(s.actor(0));
    p.stage=2;p.from=p.to=Market::node(10,10);p.goal=Market::NONE;
    s.setDirection(7);s.update(100);
    assert(p.to==Market::node(10,11)&&p.direction==0);
    block(10,11);assert(s.reset(d,38,25,0,0));s.enableManual();
    p.stage=2;p.from=p.to=Market::node(10,10);p.goal=Market::NONE;
    s.setDirection(7);s.update(100);assert(p.from==p.to&&p.to==Market::node(10,10));

    // Existing traffic blocks the diagonal; the player takes the clear axis.
    d=openGround();assert(s.reset(d,38,25,133,0));s.enableManual();
    p.stage=2;p.from=p.to=Market::node(10,10);p.goal=Market::NONE;
    auto& f=const_cast<Market::Actor&>(s.actor(1));
    f.stage=2;f.from=Market::node(11,10);f.to=Market::node(11,11);f.direction=0;f.motionMs=0;
    s.setDirection(7);s.update(100);validate(s);
    assert(p.to==Market::node(10,11)&&p.direction==0);

    // The companion follows diagonals and yields when the player reverses.
    assert(s.reset(d,38,25,133,0));s.enableManual();
    p.stage=2;p.from=p.to=Market::node(15,15);p.goal=Market::NONE;
    f.stage=2;f.from=f.to=Market::node(17,17);f.goal=Market::NONE;
    s.setDirection(5);bool followed=false;
    for(uint32_t t=20;t<=1400;t+=20){s.update(t);validate(s);followed|=f.from!=f.to&&f.direction>=4;}
    assert(followed);const auto before=s.playerPose();s.setDirection(7);
    for(uint32_t t=1420;t<=3400;t+=20){s.update(t);validate(s);}
    assert(s.playerPose().x>before.x&&s.playerPose().y>before.y);

    // Fast UI polling must not retry a trapped companion's route every frame.
    for(int direction=0;direction<8;++direction) {
        const auto n=Market::node(8+Market::DIRECTION_X[direction],8+Market::DIRECTION_Y[direction]);
        d.pedestrian.bytes[n/8]&=uint8_t(~(1U<<(n%8)));
    }
    assert(s.reset(d,38,25,133,0));s.enableManual();
    p.stage=2;p.from=p.to=Market::node(15,15);p.goal=Market::NONE;
    f.stage=2;f.from=f.to=Market::node(8,8);f.goal=Market::NONE;
    s.setDirection(3);
    for(uint32_t t=1;t<=1000;++t){s.update(t);validate(s);}
    assert(f.blocked>0&&f.blocked<=20);
}

static void walkOut(int outward,uint16_t companion) {
    Market::Data d;d.outward=uint8_t(outward);
    const int ex=outward==1?0:outward==3?Market::WIDTH-1:Market::WIDTH/2;
    const int ey=outward==2?0:outward==0?Market::HEIGHT-1:Market::HEIGHT/2;
    d.entry=Market::node(ex,ey);
    for(int y=1;y<Market::HEIGHT-1;++y)for(int x=1;x<Market::WIDTH-1;++x) {
        d.roads.set(Market::node(x,y));d.pedestrian.set(Market::node(x,y));
    }
    d.roads.set(d.entry);d.pedestrian.set(d.entry);
    Market::Session s;assert(s.reset(d,38,25,companion,0));s.enableManual();
    uint32_t t=0;
    const auto advance=[&](int n){while(n--){t+=100;s.update(t);validate(s);}};
    advance(90);assert(!s.exiting()&&!s.finished());
    const auto idle=s.playerPose();advance(20);
    assert(s.playerPose().x==idle.x&&s.playerPose().y==idle.y);
    assert(std::abs(idle.x-ex*13)+std::abs(idle.y-ey*13)==13);
    if(companion)assert(s.actor(1).from==d.entry&&s.actor(1).to==d.entry);
    // No explicit leave() call: the outward step starts a collision-safe
    // departure even when the companion is still occupying the doorway.
    s.setDirection(outward);advance(1);assert(s.exiting());
    s.setDirection((outward+2)%4);
    for(int i=0;i<1000&&!s.finished();++i)advance(1);
    assert(s.finished());
    // Re-entering creates a fresh session, not another immediate exit.
    assert(s.reset(d,39,25,companion,t));s.enableManual();advance(90);
    assert(!s.exiting()&&!s.finished());

    // Other boundary cells remain closed even if their mask is walkable.
    const int wall=(outward+1)%4;
    const int wx=wall==1?0:wall==3?Market::WIDTH-1:Market::WIDTH/2;
    const int wy=wall==2?0:wall==0?Market::HEIGHT-1:Market::HEIGHT/2;
    const int dx[4]={0,-1,0,1},dy[4]={1,0,-1,0};
    const auto inside=Market::node(wx-dx[wall],wy-dy[wall]);
    d.pedestrian.set(Market::node(wx,wy));
    auto& p=const_cast<Market::Actor&>(s.actor(0));
    p.stage=2;p.from=p.to=inside;p.goal=Market::NONE;
    s.setDirection(wall);advance(5);
    assert(!s.exiting()&&s.actor(0).from==inside&&s.actor(0).to==inside);
}

static void wideDoorway(int outward,uint16_t companion,int lane,int turn=0) {
    Market::Data d;d.outward=uint8_t(outward);
    const int ex=outward==1?0:outward==3?Market::WIDTH-1:Market::WIDTH/2;
    const int ey=outward==2?0:outward==0?Market::HEIGHT-1:Market::HEIGHT/2;
    const int dx[]={0,-1,0,1},dy[]={1,0,-1,0};
    const int tx=dy[outward],ty=dx[outward];
    d.entry=Market::node(ex,ey);
    for(int y=1;y<Market::HEIGHT-1;++y)for(int x=1;x<Market::WIDTH-1;++x) {
        d.roads.set(Market::node(x,y));d.pedestrian.set(Market::node(x,y));
    }
    for(int offset=-1;offset<=1;++offset) {
        const auto n=Market::node(ex+tx*offset,ey+ty*offset);
        d.roads.set(n);d.pedestrian.set(n);
    }
    const auto remoteDoor=Market::node(ex+tx*5,ey+ty*5);
    d.roads.set(remoteDoor);d.pedestrian.set(remoteDoor);
    Market::Session s;assert(s.reset(d,38,25,companion,0));s.enableManual();
    uint32_t time=0;
    const auto advance=[&](int ticks) {while(ticks--){time+=100;s.update(time);validate(s);}};
    advance(90);assert(!s.exiting());
    // A separate open patch on the same edge is not the authored doorway.
    auto remote=s;
    auto& remotePlayer=const_cast<Market::Actor&>(remote.actor(0));
    remotePlayer.from=remotePlayer.to=Market::node(ex-dx[outward]+tx*5,ey-dy[outward]+ty*5);
    remote.setDirection(outward);remote.update(time+100);assert(!remote.exiting());
    if(lane) {
        const int tangent=tx? (tx*lane<0?1:3) : (ty*lane<0?2:0);
        s.setDirection(tangent);advance(1);s.setDirection(-1);advance(3);
    }
    assert(s.actor(0).to==Market::node(ex-dx[outward]+tx*lane,ey-dy[outward]+ty*lane));
    s.setDirection(Market::turnDirection(outward,turn));advance(1);
    assert(s.exiting());
    for(int i=0;i<1200&&!s.finished();++i)advance(1);
    assert(s.finished());
    // Pressing outward while already on the entry also leaves; merely
    // entering and waiting above did not trigger a departure.
    assert(s.reset(d,38,25,0,time));s.enableManual();advance(90);
    auto& atEntry=const_cast<Market::Actor&>(s.actor(0));
    atEntry.from=atEntry.to=d.entry;atEntry.goal=Market::NONE;
    s.setDirection(Market::turnDirection(outward,turn));advance(1);assert(s.exiting());
    for(int i=0;i<1200&&!s.finished();++i)advance(1);
    assert(s.finished());
}

int main() {
    eightDirections();
    for(int side=0;side<4;++side)for(uint16_t companion:{uint16_t(0),uint16_t(133)})
        for(int lane=-1;lane<=1;++lane)wideDoorway(side,companion,lane);
    for(int side=0;side<4;++side)for(uint16_t companion:{uint16_t(0),uint16_t(133)})
        for(int turn:{-1,1})wideDoorway(side,companion,0,turn);
    for(int side=0;side<4;++side)for(uint16_t companion:{uint16_t(0),uint16_t(133)})walkOut(side,companion);
    // A narrow road surrounded by walkable grass, with a wall at x=12.
    Market::Data d;d.entry=Market::node(8,24);d.outward=0;
    for(int y=1;y<25;++y) {
        d.roads.set(Market::node(8,y));
        for(int x=1;x<12;++x)d.pedestrian.set(Market::node(x,y));
    }
    Market::Session s;assert(s.reset(d,38,25,133,0));s.enableManual();
    uint32_t t=0;
    const auto advance=[&](int n){while(n--){t+=100;s.update(t);validate(s);}};
    advance(70);
    const auto idle=s.playerPose();advance(30);
    assert(s.playerPose().x==idle.x&&s.playerPose().y==idle.y);
    s.setDirection(3);advance(30);s.setDirection(-1);advance(5);
    assert(s.playerPose().x==11*13); // Grass reached; wall cannot be crossed.
    assert(!d.roads.has(s.actor(0).to));
    const auto stopped=s.playerPose();advance(30);
    assert(s.playerPose().x==stopped.x&&s.playerPose().y==stopped.y);
    s.setDirection(1);advance(60);assert(s.playerPose().x==13); // Companion yields on reversal.
    s.setDirection(2);advance(15);s.update(t+100,false);t+=100;
    advance(5);const auto cancelled=s.playerPose();advance(20);
    assert(s.playerPose().x==cancelled.x&&s.playerPose().y==cancelled.y);
    s.leave();for(int i=0;i<1000&&!s.finished();++i)advance(1);assert(s.finished());

    // Chat is based on feet distance, excludes the player's companion, and
    // requires an unobstructed line through the collision mesh.
    d.residentCount=1;d.residents[0].species=184;
    auto& r=d.residents[0];r.activity.spawn=5*Market::CELL_WIDTH+5;r.activity.direction=0;
    r.activity.behavior=AuthoredNpc::Behavior::IDLE;r.activity.stepTicks=4;
    r.activity.waitMin=r.activity.waitMax=10;r.navigation.set(Market::cellNode(r.activity.spawn));
    assert(s.reset(d,38,25,133,0));s.enableManual();
    auto& p=const_cast<Market::Actor&>(s.actor(1));p.stage=2;p.from=p.to=Market::node(9,11);p.goal=Market::NONE;
    assert(s.nearbyNpc()==0);
    p.from=p.to=Market::node(8,11);assert(s.nearbyNpc()==-1&&s.nearbyNpc(0)==0);
    p.from=p.to=Market::node(7,11);assert(s.nearbyNpc(0)==-1);
    p.from=p.to=Market::node(9,11);d.pedestrian.bytes[Market::node(10,11)/8]&=uint8_t(~(1U<<(Market::node(10,11)%8)));
    assert(s.nearbyNpc()==-1);

    // Run real authored geometry with residents, guests and repeated turns.
    const auto& actual=MarketMapData::get();
    for(unsigned seed:{1U,2U,3U,17U,38U,99U,1024U}) {
        assert(s.reset(actual,seed,25,133,0));s.enableManual();unsigned random=seed;
        for(t=100;t<=600000;t+=100) {
            if(t%1300==0){random=random*1664525U+1013904223U;s.setDirection(int((random>>24)%9)-1);}
            s.update(t);validate(s);
        }
        s.leave();for(int i=0;i<2000&&!s.finished();++i){t+=100;s.update(t);validate(s);}
        if(!s.finished())for(int i=0;i<s.count();++i) {
            const auto& a=s.actor(i);
            std::fprintf(stderr,"manual exit stuck seed=%u actor=%d stage=%u from=%u to=%u goal=%u blocked=%u\n",seed,i,a.stage,a.from,a.to,a.goal,a.blocked);
        }
        assert(s.finished());
    }
    std::puts("market-walk: eight-way equal-speed corners swept-collisions companion-reverse doorway exits pause stop chat OK");
}
