#include "game/EncounterHistory.h"

#include <cassert>

int main() {
    Game::EncounterHistory history;
    assert(!history.contains(25));
    assert(history.add(25));
    assert(history.contains(25));
    assert(!history.add(25));
    assert(!history.add(0));

    history.count = 5;
    history.speciesIds[0] = 25;
    history.speciesIds[1] = 0;
    history.speciesIds[2] = 133;
    history.speciesIds[3] = 25;
    history.speciesIds[4] = 151;
    assert(history.sanitize());
    assert(history.count == 3);
    assert(history.speciesIds[0] == 25);
    assert(history.speciesIds[1] == 133);
    assert(history.speciesIds[2] == 151);
    assert(history.speciesIds[3] == 0);

    history.clear();
    for (uint16_t i = 1; i <= Game::ENCOUNTERED_SPECIES_CAP; ++i) {
        assert(history.add(i));
    }
    assert(history.count == Game::ENCOUNTERED_SPECIES_CAP);
    assert(!history.add(Game::ENCOUNTERED_SPECIES_CAP + 1));
    return 0;
}
