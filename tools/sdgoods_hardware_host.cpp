// SPDX-License-Identifier: Apache-2.0
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include "AudioPolicy.h"
#include "BatteryPolicy.h"
#include "ChargingTrend.h"
#include "PowerActions.h"
#include "PowerButton.h"

namespace {
std::string calls;
struct Board {
    bool save=false, unlock=false, shutdown=false;
    bool takeSaveRequest() { bool v=save; save=false; return v; }
    bool takeUnlockRequest() { bool v=unlock; unlock=false; return v; }
    bool takeShutdownRequest() { bool v=shutdown; shutdown=false; return v; }
    void powerOff() { calls += 'P'; }
};
struct Session {
    bool ok=true;
    bool save() { calls += 'S'; return ok; }
};
struct Engine {
    void wakeFromIdle() { calls += 'W'; }
    void invalidateScene() { calls += 'I'; }
};
}
int main() {
    using A=Stickmon::PowerButton::Action;
    Stickmon::PowerButton button;
    button.begin(true, 0);
    assert(button.update(false, 3000)==A::NONE);
    assert(button.update(false, 3030)==A::NONE);
    assert(button.update(true, 3100)==A::NONE);
    assert(button.update(false, 3110)==A::NONE); // contact bounce
    assert(button.update(true, 3120)==A::NONE);
    assert(button.update(true, 3150)==A::PRESSED && button.down());
    assert(button.update(false, 3200)==A::NONE);
    assert(button.update(false, 3230)==A::TOGGLE_LOCK && !button.down());
    button.begin(false, 0xffffff00u);
    button.update(true, 0xffffff10u);
    assert(button.update(true, 0xffffff30u)==A::PRESSED);
    assert(button.update(true, 0x900u)==A::NONE && button.down());
    button.update(false, 0x910u);
    assert(button.update(false, 0x930u)==A::SHUTDOWN);
    Board board; Session session; Engine engine;
    board.save=true; Sdgoods::processPowerActions(board,session,engine);
    assert(calls=="S");
    Sdgoods::processPowerActions(board,session,engine); assert(calls=="S");
    board.unlock=true; Sdgoods::processPowerActions(board,session,engine); assert(calls=="SWI");
    session.ok=false; board.shutdown=true;
    Sdgoods::processPowerActions(board,session,engine); assert(calls=="SWIS");
    session.ok=true; board.shutdown=true;
    Sdgoods::processPowerActions(board,session,engine); assert(calls=="SWISSP");
    std::puts("PASS power press before simulation, bounce/wake-held/wraparound, unlock and save-failure shutdown guard");
    const int16_t mono[]{-32768,-1234,0,1234,32767}; int16_t stereo[10];
    for(unsigned v=0;v<=100;++v) {
        Sdgoods::AudioPolicy::stereo(mono,stereo,5,v);
        for(unsigned i=0;i<5;++i) {
            assert(stereo[2*i]==int32_t(mono[i])*int(v)/100);
            assert(stereo[2*i+1]==stereo[2*i]);
        }
    }
    assert(!Sdgoods::AudioPolicy::audible(0,true,false));
    assert(!Sdgoods::AudioPolicy::audible(50,false,false));
    assert(!Sdgoods::AudioPolicy::audible(50,true,true));
    assert(Sdgoods::AudioPolicy::audible(50,true,false));
    assert(Sdgoods::AudioPolicy::microphoneSample(std::numeric_limits<int32_t>::max())==32767);
    assert(Sdgoods::AudioPolicy::microphoneSample(std::numeric_limits<int32_t>::min())==-32768);
    std::puts("PASS muted zero PCM and PA gate, stereo output range and microphone clipping");
    assert(Sdgoods::batteryPercent(0)==-1 && Sdgoods::batteryPercent(2499)==-1);
    assert(Sdgoods::batteryPercent(3000)==0 && Sdgoods::batteryPercent(3640)==50);
    assert(Sdgoods::batteryPercent(4280)==100 && Sdgoods::batteryPercent(4500)==100);
    assert(Sdgoods::batteryPercent(3300)==23 && Sdgoods::batteryPercent(4200)==94);
    assert(Sdgoods::batteryPercent(3249)==19 && Sdgoods::batteryPercent(3250)==20);
    assert(Sdgoods::batteryPercent(3006)==0 && Sdgoods::batteryPercent(3007)==1);
    assert(Sdgoods::batteryPercent(4501)==-1);
    for(int mv=2501;mv<=4500;++mv)
        assert(Sdgoods::batteryPercent(mv)>=Sdgoods::batteryPercent(mv-1));
    std::puts("PASS battery voltage estimate boundaries and unknown readings");
    Sdgoods::ChargingTrend trend;
    uint32_t now = 0;
    const auto block = [&](int mv) {
        for (int i=0;i<5;++i) { trend.sample(now,mv); now+=2000; }
    };
    // Sustained rises enter after four complete blocks; one step does not.
    for (int i=0;i<4;++i) { block(3600+i*8); assert(trend.charging()==(i==3)); }
    block(3624); assert(trend.charging());
    for (int i=0;i<10;++i) block(3624);
    assert(trend.charging());
    block(3624); assert(!trend.charging()); // 120s since qualifying rise
    trend.reset();
    block(3600); block(3700); block(3700); block(3700);
    assert(!trend.charging());
    for (int i=0;i<40;++i) {
        block(3700+(i%2?3:-3)); assert(!trend.charging());
    }
    trend.reset();
    for (int i=0;i<4;++i) block(3700-i*8);
    assert(!trend.charging());
    trend.reset();
    for (int i=0;i<4;++i) block(3600+i*8);
    block(3620); assert(trend.charging());
    block(3616); assert(!trend.charging()); // sustained decline
    trend.reset();
    for (int i=0;i<4;++i) block(3600+i*8);
    block(3600); assert(!trend.charging()); // single large drop
    trend.reset();
    for (int i=0;i<4;++i) block(3600+i*8);
    trend.sample(now+6001,3632); assert(!trend.charging());
    now+=8001;
    for (int i=0;i<4;++i) block(3640+i*8);
    assert(trend.charging());
    trend.sample(now,4501); assert(!trend.charging());
    trend.reset(); now=0xffffc000u;
    for (int i=0;i<4;++i) block(3600+i*8);
    assert(trend.charging()); // millis rollover
    trend.reset();
    for (int i=0;i<40;++i) trend.sample(100,3600+i*10);
    assert(!trend.charging()); // repeated queries are not fresh samples
    trend.reset(); now=0;
    for (int i=0;i<4;++i) block(3600+i*7);
    assert(!trend.charging()); // below minimum total rise
    std::puts("PASS charging estimate: sustained rise, plateau timeout, step/noise/discharge rejection, drops, gaps, invalid ADC and rollover");
}
