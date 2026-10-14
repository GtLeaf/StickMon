#include "LockPetDoubleTap.h"

#include <cassert>
#include <cstdint>

using AmoledV1::LockPetDoubleTap;

int main() {
    LockPetDoubleTap gesture;
    const auto tap = [&](uint32_t start, bool hit = true, uint32_t hold = 120) {
        gesture.press(start, 180, 220, hit);
        return gesture.release(start + hold, 180, 220, hit);
    };
    assert(!tap(0));
    assert(tap(250));
    assert(!tap(1000));  // Completed pairs cannot spill into the next unlock.
    gesture.reset();
    assert(!tap(2000));
    assert(!tap(2501));  // Slow taps restart the pair.
    assert(tap(2800));
    assert(!tap(4000));
    assert(!tap(4200, false));
    assert(!tap(4400));  // An outside tap breaks the pair.
    assert(tap(4600));
    assert(!tap(5000));
    assert(!tap(5200, true, 400));  // A hold breaks the pair.
    assert(!tap(5700));
    gesture.press(5900, 180, 220, true);
    gesture.move(205, 220, true);
    assert(!gesture.release(6020, 180, 220, true));  // Returning after a drag fails.
    assert(!tap(6200));
    gesture.press(6400, 180, 220, true);
    gesture.move(180, 220, false);
    assert(!gesture.release(6520, 180, 220, true));
    assert(!tap(6700));
    gesture.reset();  // Lock transitions discard earlier taps.
    assert(!tap(6900));
    assert(tap(7150));
    assert(!gesture.release(7300, 180, 220, true));  // UP without DOWN.
    gesture.press(7500, 180, 220, true);
    assert(!gesture.release(7600, 180, 220, false));  // Released outside pet.
    gesture.reset();
    assert(!tap(UINT32_MAX - 200));
    assert(tap(49));  // Millisecond counter wraparound.
    gesture.reset();
    assert(!tap(8000));
    assert(tap(8500));  // The 500 ms boundary is inclusive.
}
