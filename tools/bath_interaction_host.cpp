#include "game/BathInteraction.h"
#include "game/GameRandom.h"

#include <cassert>
#include <cmath>

using namespace Game::BathInteraction;

static Foam small(float x, float y, uint8_t stage = 0) {
    Foam foam;
    foam.x = foam.restX = x;
    foam.y = y;
    foam.stage = stage;
    foam.active = true;
    return foam;
}

int main() {
    Layout stick;
    Layout amoled{184, 212, 80, 288, 124, 282, 232, 296, 2};
    State a, b;
    GameRandom::seed(123);
    const int countA = spawn(a, stick, 1000);
    GameRandom::seed(123);
    const int countB = spawn(b, amoled, 1000);
    assert(countA == countB && countA >= 1 && countA <= 2);
    for (int i = 0; i < countA; ++i) {
        assert(b.foam[i].x == 184 + (a.foam[i].x - 99) * 2);
        assert(b.foam[i].y == 212 + (a.foam[i].y - 78) * 2);
        assert(a.foam[i].stage == b.foam[i].stage);
    }
    for (int i = 0; i < 30; ++i) spawn(a, stick, 1000 + i * 180);
    int active = 0;
    for (const auto& foam : a.foam) active += foam.active;
    assert(active == FOAM_CAP);

    // One isolated bubble cannot grow; different halves cannot pair.
    a = {};
    a.foam[0] = small(99, 60);
    assert(rub(a, stick, 99, 60, 1000).touched);
    assert(!rub(a, stick, 99, 60, 1200).merged);
    assert(a.foam[0].stage == 0);
    a.foam[1] = small(99, 100, 1);
    assert(partner(a, stick, a.foam[0]) == nullptr);
    a.foam[1].y = 70;
    assert(rub(a, stick, 99, 60, 1300).merged);
    assert(a.foam[0].stage == 2 && !a.foam[1].active);
    assert(a.foam[0].y < restY(stick, 2));
    // Large bubbles must land and receive four rubs before the next merge.
    a.foam[0].brushProgress = 4;
    a.foam[1] = small(100, restY(stick, 2), 2);
    assert(!merge(a, stick, a.foam[0], 2000));
    a.foam[0].y = restY(stick, 2) + a.foam[0].restYOffset;
    assert(!merge(a, stick, a.foam[0], 1500));
    assert(merge(a, stick, a.foam[0], 2000));
    assert(a.foam[0].stage == 3 && !a.foam[1].active);

    // Movement depends on elapsed time and geometry, not update count.
    a = {};
    a.foam[0] = small(99, 60, 2);
    a.foam[0].restX = 85;
    b = a;
    for (int i = 0; i < 50; ++i) update(a, stick, .02f);
    for (int i = 0; i < 25; ++i) update(b, stick, .04f);
    assert(std::fabs(a.foam[0].x - b.foam[0].x) < .001f);
    assert(std::fabs(a.foam[0].y - b.foam[0].y) < .001f);
    assert(std::fabs(a.foam[0].y - 84) < .001f);
    for (int i = 0; i < 100; ++i) update(a, stick, .02f);
    assert(std::fabs(a.foam[0].y - restY(stick, 2)) < .001f);

    // Only the lower layer counts; six is insufficient, seven triggers once.
    a = {};
    a.foam[0] = small(80, 110, 3);
    a.foam[1] = small(110, 110, 2);
    a.foam[2] = small(99, 60);
    assert(lowerWeight(a, stick) == 6);
    assert(!checkAtmosphere(a, stick));
    a.foam[2].y = 100;
    assert(checkAtmosphere(a, stick));
    assert(a.atmosphereTarget && !anyFoam(a));
    assert(!checkAtmosphere(a, stick));

    a = {};
    a.foam[0] = small(99, 110, 3);
    for (int i = 0; i < 3; ++i) { rinse(a); assert(anyFoam(a)); }
    rinse(a);
    assert(!anyFoam(a));
    return 0;
}
