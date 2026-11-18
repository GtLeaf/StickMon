// SPDX-License-Identifier: Apache-2.0
#include "StandbyTouch.h"
#include "Cst816TouchState.h"
#include "PowerInputState.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
struct Sensor {
    Sdgoods::StandbyTouch wake;
    uint8_t data[5]{};
    bool sample(unsigned count, unsigned x, unsigned y, uint32_t now) {
        data[0]=uint8_t(count);data[1]=uint8_t(x>>8);data[2]=uint8_t(x);
        data[3]=uint8_t(y>>8);data[4]=uint8_t(y);
        return wake.sample(data,now);
    }
    bool up(uint32_t now){return sample(0,0,0,now);}
    bool down(uint32_t now,unsigned x=180,unsigned y=180){return sample(1,x,y,now);}
    bool tap(uint32_t now,unsigned x=180,unsigned y=180,unsigned duration=80){
        assert(!down(now,x,y));return up(now+duration);
    }
};
}
int main() {
    Sensor sensor;
    // A pre-existing contact cannot count as the first tap after screen lock.
    assert(!sensor.down(0));assert(!sensor.up(80));
    assert(!sensor.tap(120,0,0));
    assert(sensor.tap(300,359,359)); // Both sensor edges; no proximity requirement.
    assert(!sensor.tap(500)); // Waking consumes the completed pair.
    assert(!sensor.tap(1000)); // Previous first tap expired.
    assert(sensor.tap(1200));
    std::puts("PASS release arming, arbitrary positions and one wake per pair");

    sensor.wake.reset();sensor.up(0);
    assert(!sensor.tap(100));
    assert(sensor.tap(530,180,180,250)); // First up=180; second down gap=350.
    sensor.wake.reset();sensor.up(1000);
    assert(!sensor.tap(1100));
    assert(!sensor.tap(1531)); // Gap 351 expires; this becomes a new first tap.
    assert(sensor.tap(1700));
    sensor.wake.reset();sensor.up(2000);
    assert(!sensor.tap(2100));assert(!sensor.tap(2300,180,180,251));
    assert(!sensor.tap(2600));assert(sensor.tap(2800));
    std::puts("PASS 350ms gap and 250ms hold boundaries");

    sensor.wake.reset();sensor.up(0);
    assert(!sensor.tap(100));assert(!sensor.down(300));
    assert(!sensor.down(320,205,180));assert(!sensor.down(340)); // Moving back cannot undo a drag.
    assert(!sensor.up(360));assert(!sensor.tap(400));assert(sensor.tap(600));
    sensor.wake.reset();sensor.up(0);
    assert(!sensor.tap(100));assert(!sensor.down(300));
    assert(!sensor.down(320,204,180));assert(sensor.up(350)); // Exactly 24px is a tap.
    std::puts("PASS slide rejection without imposing a distance between taps");

    for(unsigned invalid=0;invalid<4;++invalid) {
        sensor.wake.reset();sensor.up(0);assert(!sensor.tap(100));assert(!sensor.down(300));
        if(invalid==0)assert(!sensor.wake.sample(nullptr,320));
        else if(invalid==1)assert(!sensor.sample(2,180,180,320));
        else if(invalid==2)assert(!sensor.down(320,360,180));
        else assert(!sensor.down(320,180,4095));
        assert(!sensor.up(400));assert(!sensor.tap(500));assert(sensor.tap(700));
    }
    sensor.wake.reset();sensor.up(0);
    assert(!sensor.tap(100));sensor.wake.reset(); // Key press / state switch clears partial double tap.
    assert(!sensor.down(250));assert(!sensor.up(330));
    assert(!sensor.tap(400));assert(sensor.tap(600));
    std::puts("PASS failed reads, invalid contacts and resets never generate wake");

    sensor.wake.reset();sensor.up(0xfffffe00u);
    assert(!sensor.tap(0xffffff00u));assert(sensor.tap(0x20u));
    sensor.wake.reset();sensor.up(0);
    for(uint32_t now=20;now<=60000;now+=20)assert(!sensor.up(now));
    std::puts("PASS clock rollover and 60 seconds without spontaneous wake");

    // Use the real normal touch decoder to verify the post-wake release guard.
    Sdgoods::Cst816TouchState touch;
    using Event=Platform::IInputDevice::TouchEvent;
    using Type=Platform::IInputDevice::TouchType;
    Event event{};
    touch.sample(nullptr,0);
    sensor.down(20);touch.sample(sensor.data,20);assert(!touch.poll(event));
    sensor.up(40);touch.sample(sensor.data,40);assert(!touch.poll(event));
    sensor.down(60);touch.sample(sensor.data,60);
    assert(touch.poll(event)&&event.type==Type::DOWN);
    assert(!touch.poll(event));
    std::puts("PASS normal touch stays blocked until release after wake");

    Sdgoods::PowerInputState input;
    input.begin(false,0);
    uint8_t raw[5]{};
    const auto route=[&](unsigned count,unsigned x,unsigned y,uint32_t now) {
        raw[0]=uint8_t(count);raw[1]=uint8_t(x>>8);raw[2]=uint8_t(x);
        raw[3]=uint8_t(y>>8);raw[4]=uint8_t(y);
        return input.sampleTouch(raw,now);
    };
    const auto noPageEvents=[&](uint32_t now) {
        while(input.pollTouch(event,now))assert(event.type==Type::CANCEL);
    };
    route(0,0,0,0);noPageEvents(0);route(1,100,200,20);
    assert(input.pollTouch(event,20)&&event.type==Type::DOWN);
    input.pollKey(true,40);input.pollKey(true,70);
    assert(input.takeSaveRequest());noPageEvents(70);
    input.pollKey(false,100);input.pollKey(false,130);
    assert(input.lockRequestReady(130));
    route(1,100,200,150);noPageEvents(150); // Pending scene preparation blocks UI.
    input.completeLockRequest(160);
    assert(input.locked());
    route(1,100,200,180);route(0,0,0,200); // The old contact cannot be a tap.
    route(1,0,0,220);assert(!route(0,0,0,280));noPageEvents(280);
    route(1,359,359,380);assert(route(0,0,0,440));
    assert(!input.locked() && input.takeUnlockRequest() && !input.takeUnlockRequest());
    noPageEvents(440); // Both wake taps must be consumed by the real router.
    route(1,100,200,460);noPageEvents(460);
    route(0,0,0,480);route(1,100,200,500);
    assert(input.pollTouch(event,500)&&event.type==Type::DOWN);
    route(0,0,0,560);assert(input.pollTouch(event,560)&&event.type==Type::UP);
    std::puts("PASS production routing: pending lock blocks UI; standby consumes both wake taps; wake requires fresh contact");

    input.setLocked(true,600);route(0,0,0,620);
    route(1,100,200,640);route(0,0,0,700);
    input.pollKey(true,760);input.pollKey(true,790);
    input.pollKey(false,850);input.pollKey(false,880);
    assert(!input.locked()&&input.takeUnlockRequest()); // Power key wakes, not locks again.
    input.setLocked(true,900);route(0,0,0,920);
    route(1,100,200,940);assert(!route(0,0,0,1000));
    route(1,100,200,1100);assert(route(0,0,0,1160));
    input.takeUnlockRequest();
    std::puts("PASS production routing: key wake and lock transitions clear partial double taps");

    input.setLocked(true,1200);route(0,0,0,1220);
    route(1,100,200,1240);route(0,0,0,1300);
    input.pollKey(true,1320);input.pollKey(true,1350);
    route(0,0,0,1400);route(1,100,200,1420);route(0,0,0,1480);
    route(1,100,200,1580);assert(!route(0,0,0,1660));
    assert(input.locked()&&!input.takeUnlockRequest());
    input.pollKey(true,3320);
    assert(input.locked()&&input.shutdownPending()&&!input.takeShutdownRequest());
    route(0,0,0,3340);route(1,100,200,3360);route(0,0,0,3420);
    route(1,100,200,3500);assert(!route(0,0,0,3580));
    assert(!input.takeUnlockRequest());noPageEvents(3580);
    input.pollKey(false,3600);input.pollKey(false,3630);
    assert(input.takeShutdownRequest()&&!input.takeShutdownRequest());
    assert(input.locked()&&input.shutdownPending());
    std::puts("PASS production routing: held key and confirmed shutdown cannot be woken by touch; cut request only on release");

    Sdgoods::PowerInputState recovery;
    recovery.begin(false,0);
    recovery.pollKey(true,100,false);recovery.pollKey(true,130,false);
    recovery.pollKey(false,200,false);recovery.pollKey(false,230,false);
    assert(!recovery.lockRequestReady(230));
    recovery.pollKey(true,300,false);recovery.pollKey(true,330,false);
    recovery.pollKey(true,2300,false);
    assert(recovery.locked()&&recovery.shutdownPending()&&!recovery.takeShutdownRequest());
    recovery.pollKey(false,2400,false);recovery.pollKey(false,2430,false);
    assert(recovery.takeShutdownRequest());
    std::puts("PASS production routing: recovery shutdown requires no game or save session");
}
