#include "game/ExperienceService.h"

#include <algorithm>

#include "game/Species.h"

namespace Game {
namespace ExperienceService {

Result add(MonsterRuntime& monster, const Species& species, uint32_t amount) {
    Result result;
    result.oldLevel = monster.level;
    result.newLevel = monster.level;
    result.oldHpMax = monster.hpMax;
    result.newHpMax = monster.hpMax;

    if (amount == 0) return result;

    const uint32_t maxExp = minimumExpForLevel(species.growthRate, LEVEL_MAX);
    const uint32_t oldExp = monster.exp;
    const uint64_t totalExp = static_cast<uint64_t>(monster.exp) + amount;
    monster.exp = totalExp > maxExp ? maxExp : static_cast<uint32_t>(totalExp);
    result.awarded = monster.exp > oldExp ? monster.exp - oldExp : 0;

    const uint8_t calculatedLevel = levelForExp(species.growthRate, monster.exp);
    monster.level = std::max<uint8_t>(monster.level, calculatedLevel);
    result.newLevel = monster.level;
    result.leveledUp = result.newLevel > result.oldLevel;

    monster.hpMax = maxHpFor(species, monster);
    result.newHpMax = monster.hpMax;
    if (monster.hpMax > result.oldHpMax) {
        monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
            monster.hpMax,
            static_cast<uint32_t>(monster.hpCur) +
                monster.hpMax - result.oldHpMax));
    }
    return result;
}

}  // namespace ExperienceService
}  // namespace Game
