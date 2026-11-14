#include "ui/common/MarketListScroll.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
using Scroll = Ui::MarketListScroll;
using Event = Platform::IInputDevice::TouchEvent;
using Type = Platform::IInputDevice::TouchType;
static constexpr Scroll::Bounds BOX{10,216,348,224};
struct Fixture {
    Scroll motion;
    TouchTap tap;
    int offset = 0, max = 800;
    bool send(Type type,int x,int y,uint32_t t) {
        Event e{type,int16_t(x),int16_t(y),t};
        bool consumed=motion.touch(e,BOX,max,offset,tap);
        if(!consumed)tap.update(e,1);
        return consumed;
    }
    void flick(uint32_t t=1000) {
        send(Type::DOWN,184,410,t);
        assert(send(Type::MOVE,184,370,t+30));
        assert(send(Type::MOVE,184,330,t+60));
        assert(send(Type::UP,184,310,t+75));
    }
};
int main() {
    // A flick must continue after release, converge, then stop requesting pixels.
    int stops[3]{};int i=0;
    for(uint32_t step:{16U,33U,66U}) {
        Fixture f;f.flick();assert(f.offset==100&&f.motion.moving());
        int last=f.offset;assert(f.motion.update(step,f.max,f.offset));assert(f.offset>last);
        unsigned elapsed=step;
        while(f.motion.moving()&&elapsed<800){f.motion.update(step,f.max,f.offset);elapsed+=step;}
        assert(!f.motion.moving()&&elapsed<=500&&f.offset>150&&f.offset<260);
        stops[i++]=f.offset;last=f.offset;
        assert(!f.motion.update(1000,f.max,f.offset)&&f.offset==last);
    }
    assert(std::abs(stops[0]-stops[1])<=4&&std::abs(stops[1]-stops[2])<=4);
    // Pressing a moving list brakes and consumes the entire gesture.
    Fixture brake;brake.flick();brake.motion.update(33,brake.max,brake.offset);
    const int position=brake.offset;
    assert(brake.send(Type::DOWN,184,300,1100));assert(!brake.motion.moving());
    assert(brake.send(Type::UP,184,300,1150));
    assert(!brake.motion.update(200,brake.max,brake.offset)&&brake.offset==position);
    // An out-and-back drag cannot turn into a row click or outside navigation.
    Fixture reverse;reverse.send(Type::DOWN,184,300,2000);
    assert(reverse.send(Type::MOVE,184,100,2050));
    assert(reverse.send(Type::MOVE,184,300,2100));
    assert(reverse.send(Type::UP,184,300,2120));
    // Long release hold stops a fling; cancellation also cancels pending taps.
    Fixture held;held.send(Type::DOWN,184,410,3000);held.send(Type::MOVE,184,300,3050);
    assert(held.send(Type::UP,184,300,3300));assert(!held.motion.moving());
    held.flick(4000);held.motion.cancel();assert(!held.motion.moving());
    int before=held.offset;assert(!held.motion.update(200,held.max,held.offset)&&held.offset==before);
    // Bounds stop movement; a non-scrollable list never flings.
    Fixture bound;bound.max=120;bound.flick();bound.motion.update(100,120,bound.offset);
    assert(bound.offset==120&&!bound.motion.moving());
    Fixture none;none.max=0;none.flick();assert(none.offset==0&&!none.motion.moving());
    // Coarse or repeated timestamps must not divide by zero or create speed.
    Fixture same;same.send(Type::DOWN,184,400,5000);same.send(Type::MOVE,184,300,5000);
    same.send(Type::UP,184,300,5000);assert(!same.motion.moving());
    Fixture wrap;wrap.flick(UINT32_MAX-40);assert(wrap.motion.moving());
    std::puts("market-scroll: cadence-independent fling/rest, brake, captured reversal, held release, cancel and bounds OK");
}
