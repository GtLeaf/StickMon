#include <cassert>

#include "game/ExperienceService.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"

int main() {
    GameRandom::seed(0x12345678U);
    Game::MonsterRuntime monster = Game::MonsterFactory::create(1, 5);
    const Species* species = findSpecies(monster.speciesId);
    assert(species != nullptr);
    assert(monster.level == 5);
    assert(monster.exp == minimumExpForLevel(species->growthRate, 5));
    assert(monster.hpCur == monster.hpMax && monster.hpMax > 0);

    const uint16_t oldHpMax = monster.hpMax;
    const uint32_t nextLevelExp = minimumExpForLevel(
        species->growthRate, 6);
    Game::ExperienceService::Result levelUp =
        Game::ExperienceService::add(monster, *species,
                                     nextLevelExp - monster.exp);
    assert(levelUp.awarded > 0);
    assert(levelUp.oldLevel == 5);
    assert(levelUp.newLevel >= 6);
    assert(levelUp.leveledUp);
    assert(levelUp.oldHpMax == oldHpMax);
    assert(levelUp.newHpMax == monster.hpMax);
    assert(monster.hpCur == monster.hpMax);

    Game::ExperienceService::Result noChange =
        Game::ExperienceService::add(monster, *species, 0);
    assert(noChange.awarded == 0);
    assert(noChange.oldLevel == noChange.newLevel);
    assert(!noChange.leveledUp);

    Game::MonsterRuntime fallback = Game::MonsterFactory::create(0xFFFF, 0);
    assert(fallback.speciesId == starterSpecies().id);
    assert(fallback.level == 1);
    assert(fallback.hpCur == fallback.hpMax);
    return 0;
}
