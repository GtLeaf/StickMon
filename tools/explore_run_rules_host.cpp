#include <cstdint>

#include "game/ExploreRunRules.h"

int main() {
    if (ExploreRunRules::allowsRegionalBoss(4, 6) ||
        !ExploreRunRules::allowsRegionalBoss(5, 6) ||
        !ExploreRunRules::allowsRegionalBoss(6, 6)) {
        return 1;
    }
    if (ExploreRunRules::allowsRegionalBoss(3, 5) ||
        !ExploreRunRules::allowsRegionalBoss(4, 5) ||
        !ExploreRunRules::allowsRegionalBoss(5, 5)) {
        return 2;
    }
    if (!ExploreRunRules::allowsRegionalBoss(3, 4) ||
        !ExploreRunRules::allowsRegionalBoss(4, 4)) {
        return 3;
    }
    if (!ExploreRunRules::shouldPlaceFinalReward(3, 4, false) ||
        ExploreRunRules::shouldPlaceFinalReward(2, 4, false) ||
        ExploreRunRules::shouldPlaceFinalReward(3, 4, true)) {
        return 4;
    }

    for (uint16_t step = 1; step <= 30; ++step) {
        bool expected = step == 10 || step == 20 || step == 30;
        if (ExploreRunRules::isRecoveryStep(step) != expected) return 5;
    }
    if (ExploreRunRules::recoveryAmount(0) != 0 ||
        ExploreRunRules::recoveryAmount(1) != 1 ||
        ExploreRunRules::recoveryAmount(10) != 1 ||
        ExploreRunRules::recoveryAmount(11) != 2 ||
        ExploreRunRules::recoveryAmount(65535) != 6554) {
        return 6;
    }
    if (ExploreRunRules::leaderSlotForHealth(true, true) != 0 ||
        ExploreRunRules::leaderSlotForHealth(false, true) != 1 ||
        ExploreRunRules::leaderSlotForHealth(false, false) != 0 ||
        !ExploreRunRules::showsHealthyFollower(true, true) ||
        ExploreRunRules::showsHealthyFollower(false, true) ||
        ExploreRunRules::showsHealthyFollower(true, false)) {
        return 7;
    }
    return 0;
}
