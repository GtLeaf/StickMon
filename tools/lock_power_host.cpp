#include "LockPowerSchedule.h"
#include "AmoledLockPower.h"
#include "SleepBreathClock.h"
#include <cassert>
#include <cstdint>

int main() {
    Amoled::LockPower power;
    assert(power.begin());
    assert(testConfig.min_freq_mhz == 80 && testConfig.max_freq_mhz == 240);
    assert(testConfig.light_sleep_enable);
    assert(testLocks[0].count == 1 && testLocks[1].count == 1);
    for (int i = 0; i < 10; ++i) power.setLockedRoom(true, true);
    assert(testLocks[0].count == 0 && testLocks[1].count == 0);
    power.setLockedRoom(true, false); // Touch not ready or finger held.
    assert(testLocks[0].count == 0 && testLocks[1].count == 1);
    power.setLockedRoom(true, true);
    for (int i = 0; i < 10; ++i) power.setLockedRoom(false, false);
    assert(testLocks[0].count == 1 && testLocks[1].count == 1);
    testFailCreate = true;
    Amoled::LockPower unavailable;
    assert(!unavailable.begin());
    unavailable.setLockedRoom(true, true); // Failed setup cannot release locks.
    assert(testLocks[0].count == 1 && testLocks[1].count == 1);

    using Amoled::LockPowerSchedule;
    for (bool sleeping : {false, true}) {
        LockPowerSchedule schedule;
        unsigned updates = 0, frames = 0, content = 0;
        uint32_t lastFrame = 0;
        // Input wakes the UI every 7ms. Neither clock may run at input rate.
        for (uint32_t now = 0; now < 30000; now += 7) {
            if (schedule.logicDue(now, true)) {
                schedule.updated(now);
                ++updates;
            }
            if (schedule.contentDue(now, sleeping)) {
                schedule.contentPresented(now);
                ++content;
            }
            if (schedule.frameDue(now, sleeping)) {
                if (frames) assert(now - lastFrame >= (sleeping ? 67U : 200U));
                schedule.presented(now);
                lastFrame = now;
                ++frames;
            }
        }
        assert(updates >= 280 && updates <= 300);
        assert(frames <= (sleeping ? 450U : 150U));
        assert(content <= (sleeping ? 60U : 150U));
        assert(schedule.logicDue(30001, false)); // Active scene resumes immediately.
    }
    LockPowerSchedule wrap;
    wrap.updated(UINT32_MAX - 50);
    wrap.presented(UINT32_MAX - 50);
    assert(!wrap.logicDue(48, true));
    assert(wrap.logicDue(49, true));
    assert(wrap.logicWait(48) == 1);
    assert(wrap.logicWait(49) == 0);
    assert(!wrap.frameDue(148, false));
    assert(wrap.frameDue(149, false));
    assert(!wrap.frameDue(15, true));
    assert(wrap.frameDue(16, true));
    wrap.contentPresented(UINT32_MAX - 50);
    assert(!wrap.contentDue(448, true));
    assert(wrap.contentDue(449, true));
    assert(wrap.wait(15, true) == 1);
    wrap.presented(16);
    assert(wrap.wait(16, true) > 0);

    // Opposite sprite orders, jitter, missed frames and millis wraparound.
    for (uint8_t expanded : {0, 1}) {
        AmoledV1::SleepBreathClock breath;
        const uint32_t start = UINT32_MAX - 100;
        breath.start(start, 800, 2, expanded);
        assert(breath.frame(start + 799) == 0);
        assert(breath.frame(start + 800) == 1);
        assert(breath.frame(start + 4800) == 0);
        const uint32_t peak = start + expanded * 800 + 400;
        assert(breath.expansion(peak) > 0.999f);
        assert(breath.expansion(peak + 800) < 0.001f);
        assert(breath.expansion(peak + 1600) > 0.999f);
    }
    AmoledV1::SleepBreathClock one;
    one.start(0, 800, 1, 0);
    assert(one.frame(2500) == 0);
    assert(one.expansion(400) > 0.999f && one.expansion(1200) < 0.001f);
    assert(LockPowerSchedule::intersects(100, 100, 120, 120, 130, 130, 40));
    assert(!LockPowerSchedule::intersects(0, 0, 368, 60, 180, 220, 74));
    assert(!LockPowerSchedule::intersects(0, 380, 368, 448, 180, 220, 74));
    assert(!LockPowerSchedule::intersects(0, 100, 106, 140, 180, 180, 74));
}
