#include "ui/common/MarketStallMotion.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

int main() {
    using Ui::MarketStallMotion::tween;
    int previous = 328, largest = 0;
    for (unsigned ms = 40; ms <= 440; ms += 40) {
        const int current = tween(328, 168, ms, 420);
        assert(current <= previous && current >= 168);
        largest = std::max(largest, previous - current);
        previous = current;
    }
    std::printf("opening peak step at40ms: %dpx (limit24px)\n", largest);
    std::fflush(stdout);
    assert(largest <= 24);
    assert(previous == 168);
    for (const unsigned duration : {360U, 420U, 520U}) {
        int ascending = -240, descending = 448;
        for (unsigned ms = 0; ms <= duration + 40; ++ms) {
            const int up = tween(-240, 240, ms, duration);
            const int down = tween(448, 168, ms, duration);
            assert(up >= ascending && up <= 240);
            assert(down <= descending && down >= 168);
            ascending = up; descending = down;
        }
        assert(ascending == 240 && descending == 168);
    }
    assert(tween(0, 64, 0, 0) == 64);
    assert(Ui::MarketStallMotion::hudOffset(true, false, false, 0, 420) == -64);
    // Panel and badge share progress to within integer-coordinate rounding.
    for (unsigned ms = 0; ms <= 420; ms += 20) {
        const int panelTravel = 328 - tween(328, 168, ms, 420);
        const int hudTravel = 64 + Ui::MarketStallMotion::hudOffset(false, true, false, ms, 420);
        assert(std::abs(panelTravel * 64 - hudTravel * 160) < 224);
    }
    std::puts("market-stall-motion: bounded steps, monotonic endpoints and synchronized HUD OK");
}
