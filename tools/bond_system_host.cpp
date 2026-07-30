#include <cstddef>
#include <cstdio>

#include "game/BondSystem.h"
#include "game/GameState.h"

namespace {

int fail(int code, const char* message) {
    std::printf("[bond_system_host] FAIL %d: %s\n", code, message);
    return code;
}

} // namespace

int main() {
    using Game::Bond::Level;

    if (Game::Bond::levelFor(-100) != Level::AVERSE ||
        Game::Bond::levelFor(-51) != Level::AVERSE ||
        Game::Bond::levelFor(-50) != Level::DISTANT ||
        Game::Bond::levelFor(-1) != Level::DISTANT ||
        Game::Bond::levelFor(0) != Level::ACQUAINTED ||
        Game::Bond::levelFor(24) != Level::ACQUAINTED ||
        Game::Bond::levelFor(25) != Level::FAMILIAR ||
        Game::Bond::levelFor(49) != Level::FAMILIAR ||
        Game::Bond::levelFor(50) != Level::TRUSTED ||
        Game::Bond::levelFor(74) != Level::TRUSTED ||
        Game::Bond::levelFor(75) != Level::CLOSE ||
        Game::Bond::levelFor(100) != Level::CLOSE) {
        return fail(1, "signed bond thresholds");
    }

    if (Game::Bond::adventureGain(0) != 0 ||
        Game::Bond::adventureGain(1) != 1 ||
        Game::Bond::adventureGain(14) != 1 ||
        Game::Bond::adventureGain(15) != 2 ||
        Game::Bond::adventureGain(39) != 2 ||
        Game::Bond::adventureGain(40) != 3 ||
        Game::Bond::adventureGain(79) != 3 ||
        Game::Bond::adventureGain(80) != 4) {
        return fail(2, "step-scaled adventure gain");
    }

    if (Game::Bond::increase(98, 4) != 100 ||
        Game::Bond::decrease(6, Game::Bond::FAINT_LOSS) != -4 ||
        Game::Bond::decrease(-95, Game::Bond::FAINT_LOSS) != -100 ||
        Game::Bond::decrease(50, Game::Bond::FAINT_LOSS) != 40) {
        return fail(3, "bond saturation");
    }

    if (Game::Bond::inviteChance(75, false) != 100 ||
        Game::Bond::inviteChance(50, false) != 85 ||
        Game::Bond::inviteChance(25, false) != 60 ||
        Game::Bond::inviteChance(0, false) != 35 ||
        Game::Bond::inviteChance(-1, false) != 15 ||
        Game::Bond::inviteChance(-51, false) != 5 ||
        Game::Bond::inviteChance(-100, true) != 100) {
        return fail(4, "bond-scaled invitation chance");
    }

    uint32_t beforeSeven = 6U * 60U + 59U;
    uint32_t afterSeven = 7U * 60U;
    uint32_t nextDay = afterSeven + 24U * 60U;
    uint8_t marker = Game::Bond::inviteLockMarker(
        Game::Bond::invitationDay(afterSeven));
    if (Game::Bond::invitationDay(beforeSeven) != 0 ||
        Game::Bond::invitationDay(afterSeven) != 1 ||
        Game::Bond::invitationDay(nextDay) != 2 ||
        !Game::Bond::inviteLockedToday(marker, 1) ||
        Game::Bond::inviteLockedToday(marker, 2)) {
        return fail(5, "07:00 invitation lock boundary");
    }

    Game::MonsterRuntime monster;
    if (monster.bond != Game::Bond::MAX_VALUE ||
        sizeof(Game::MonsterRuntime) != 64 ||
        offsetof(Game::MonsterRuntime, bond) != 43) {
        return fail(6, "v1 save layout and defaults");
    }

    std::printf("[bond_system_host] all tests passed\n");
    return 0;
}
