#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"

#include <cassert>
#include <cstdint>

namespace {

uint32_t nextIv = 0;

uint32_t orderedRange(uint32_t minimum, uint32_t maximum) {
    assert(minimum == 0);
    assert(maximum == Game::IV_MAX + 1);
    return nextIv++;
}

}  // namespace

int main() {
    GameRandom::setRangeProvider(orderedRange);
    Game::MonsterRuntime monster;
    monster.level = 42;
    monster.exp = 12345;
    monster.nature = 7;
    monster.hpCur = 11;
    Game::MonsterFactory::rollIndividualValues(monster);
    for (uint8_t index = 0; index < Game::STAT_COUNT; ++index) {
        assert(Game::ivAt(monster.ivPacked, index) == index);
    }
    assert(monster.level == 42);
    assert(monster.exp == 12345);
    assert(monster.nature == 7);
    assert(monster.hpCur == 11);

    GameRandom::setRangeProvider(nullptr);
    GameRandom::seed(0x12345678);
    Game::MonsterRuntime created = Game::MonsterFactory::create(1, 5);
    assert(created.ivPacked != 0);
    assert(created.hpMax == maxHpFor(starterSpecies(), created));
    assert(created.hpCur == created.hpMax);
    return 0;
}
