#include <cassert>

#include "game/HomeChase.h"

int main() {
    Home::GroundFootprintProfile venusaur =
        Home::groundFootprintForSpecies(3, 64, 64);
    assert(venusaur.radiusX == 11.5f);
    assert(venusaur.radiusY == 5.0f);

    Home::GroundFootprintProfile generic =
        Home::groundFootprintForSpecies(999, 64, 64);
    assert(generic.radiusX == 12.0f);
    assert(generic.radiusY == 5.0f);

    Home::GroundFootprintProfile chase =
        Home::compactChaseFootprint(venusaur);
    assert(chase.radiusX > 6.89f && chase.radiusX < 6.91f);
    assert(chase.radiusY == 3.0f);

    Home::GroundFootprintProfile tiny =
        Home::compactChaseFootprint({2.0f, 1.0f});
    assert(tiny.radiusX == 3.0f);
    assert(tiny.radiusY == 2.0f);
    return 0;
}
