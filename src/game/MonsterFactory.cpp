#include "game/MonsterFactory.h"

#include "game/GameRandom.h"
#include "game/Species.h"

namespace Game {
namespace MonsterFactory {

MonsterRuntime create(uint16_t speciesId, uint8_t level) {
    const Species* species = findSpecies(speciesId);
    if (!species) species = &starterSpecies();
    if (level < 1) level = 1;
    if (level > LEVEL_MAX) level = LEVEL_MAX;

    MonsterRuntime monster;
    monster.speciesId = species->id;
    monster.level = level;
    monster.exp = minimumExpForLevel(species->growthRate, level);
    resetMovesForLevel(monster, *species);
    for (uint8_t index = 0; index < STAT_COUNT; ++index) {
        setIv(monster.ivPacked, index,
              static_cast<uint8_t>(GameRandom::range(0, IV_MAX + 1)));
    }
    monster.nature = static_cast<uint8_t>(
        GameRandom::range(0, NATURE_COUNT));
    monster.gender = static_cast<uint8_t>(
        GameRandom::range(static_cast<int32_t>(Gender::MALE),
                          static_cast<int32_t>(Gender::FEMALE) + 1));
    monster.hpMax = maxHpFor(*species, monster);
    monster.hpCur = monster.hpMax;
    return monster;
}

}  // namespace MonsterFactory
}  // namespace Game
