#include "game/BattleSystem.h"
#include <Arduino.h>

namespace {

static constexpr uint8_t N = 100;
static constexpr uint8_t H = 50;
static constexpr uint8_t D = 200;
static constexpr uint8_t Z = 0;

static constexpr uint8_t TYPE_EFFECT[18][18] = {
//   Nor Fir Wat Gra Ele Ice Fig Poi Gro Fly Psy Bug Roc Gho Dra Dar Ste Fai
    {N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  H,  Z,  N,  N,  H,  N}, // Normal
    {N,  H,  H,  D,  N,  D,  N,  N,  N,  N,  N,  D,  H,  N,  H,  N,  D,  N}, // Fire
    {N,  D,  H,  H,  N,  N,  N,  N,  D,  N,  N,  N,  D,  N,  H,  N,  N,  N}, // Water
    {N,  H,  D,  H,  N,  N,  N,  H,  D,  H,  N,  H,  D,  N,  H,  N,  H,  N}, // Grass
    {N,  N,  D,  H,  H,  N,  N,  N,  Z,  D,  N,  N,  N,  N,  H,  N,  N,  N}, // Electric
    {N,  H,  H,  D,  N,  H,  N,  N,  D,  D,  N,  N,  N,  N,  D,  N,  H,  N}, // Ice
    {D,  N,  N,  N,  N,  D,  N,  H,  N,  H,  H,  H,  D,  Z,  N,  D,  D,  H}, // Fighting
    {N,  N,  N,  D,  N,  N,  N,  H,  H,  N,  N,  N,  H,  H,  N,  N,  Z,  D}, // Poison
    {N,  D,  N,  H,  D,  N,  N,  D,  N,  Z,  N,  H,  D,  N,  N,  N,  D,  N}, // Ground
    {N,  N,  N,  D,  H,  N,  D,  N,  N,  N,  N,  D,  H,  N,  N,  N,  H,  N}, // Flying
    {N,  N,  N,  N,  N,  N,  D,  D,  N,  N,  H,  N,  N,  N,  N,  Z,  H,  N}, // Psychic
    {N,  H,  N,  D,  N,  N,  H,  H,  N,  H,  D,  N,  N,  H,  N,  D,  H,  H}, // Bug
    {N,  D,  N,  N,  N,  D,  H,  N,  H,  D,  N,  D,  N,  N,  N,  N,  H,  N}, // Rock
    {Z,  N,  N,  N,  N,  N,  N,  N,  N,  N,  D,  N,  N,  D,  N,  H,  N,  N}, // Ghost
    {N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  N,  D,  N,  H,  Z}, // Dragon
    {N,  N,  N,  N,  N,  N,  H,  N,  N,  N,  D,  N,  N,  D,  N,  H,  N,  H}, // Dark
    {N,  H,  H,  N,  H,  D,  N,  N,  N,  N,  N,  N,  D,  N,  N,  N,  H,  D}, // Steel
    {N,  H,  N,  N,  N,  N,  D,  H,  N,  N,  N,  N,  N,  N,  D,  D,  H,  N}, // Fairy
};

uint8_t typeIndex(TypeId type) {
    uint8_t idx = static_cast<uint8_t>(type);
    return idx < 18 ? idx : 0xFF;
}

uint8_t singleEffect(TypeId attack, TypeId defend) {
    if (defend == TypeId::NONE) return 100;
    uint8_t attackIdx = typeIndex(attack);
    uint8_t defendIdx = typeIndex(defend);
    if (attackIdx == 0xFF || defendIdx == 0xFF) return 100;
    return TYPE_EFFECT[attackIdx][defendIdx];
}

}

namespace BattleSystem {

uint8_t typeEffectiveness(TypeId attack, TypeId defend1, TypeId defend2) {
    uint16_t value = singleEffect(attack, defend1);
    value = (value * singleEffect(attack, defend2)) / 100;
    if (value > 255) value = 255;
    return (uint8_t)value;
}

uint8_t specialTriggerChance(const Game::MonsterRuntime& attacker) {
    uint16_t chance = 5;
    chance += min<uint8_t>(attacker.proficiency, 100) * 20 / 100;
    chance += attacker.affection * 25 / 255;
    if (attacker.hpMax > 0 && attacker.hpCur * 100UL < attacker.hpMax * 30UL) chance += 15;
    return chance > 85 ? 85 : (uint8_t)chance;
}

bool rollSpecialMove(const Game::MonsterRuntime& attacker) {
    return random(0, 100) < specialTriggerChance(attacker);
}

DamageResult calcBasicDamage(const Game::MonsterRuntime& attacker,
                             const Species& attackerSpecies,
                             const Game::MonsterRuntime& defender,
                             const Species& defenderSpecies,
                             bool specialMove) {
    DamageResult result;
    result.special = specialMove;
    if ((attacker.statusBits & Game::STATUS_SLEEP) || (attacker.statusBits & Game::STATUS_FREEZE) ||
        ((attacker.statusBits & Game::STATUS_PARALYSIS) && random(0, 100) < 25) ||
        ((attacker.statusBits & Game::STATUS_CONFUSION) && random(0, 100) < 33)) {
        result.statusBlocked = true;
        return result;
    }

    uint8_t moveId = specialMove ? attackerSpecies.specialMoveId : basicMoveIdForSpecies(attackerSpecies);
    const MoveInfo* move = findMove(moveId);
    uint8_t power = move ? move->power : (specialMove ? 55 : 35);
    TypeId attackType = move ? move->type : attackerSpecies.type1;
    uint16_t atk = statFor(attackerSpecies, attacker, specialMove ? 3 : 1);
    if (!specialMove && (attacker.statusBits & Game::STATUS_BURN)) atk /= 2;
    if (atk == 0) atk = 1;
    uint16_t def = statFor(defenderSpecies, defender, specialMove ? 4 : 2);
    if (def == 0) def = 1;

    uint16_t base = (((2 * attacker.level / 5 + 2) * power * atk / def) / 50) + 2;
    uint8_t stab = (attackType == attackerSpecies.type1 || attackType == attackerSpecies.type2) ? 150 : 100;
    uint8_t eff = typeEffectiveness(attackType, defenderSpecies.type1, defenderSpecies.type2);
    uint8_t randomFactor = random(85, 101);
    bool crit = random(0, 16) == 0;
    uint32_t damage = base;
    damage = damage * stab / 100;
    damage = damage * eff / 100;
    damage = damage * randomFactor / 100;
    if (crit) damage = damage * 3 / 2;
    if (damage == 0 && eff > 0) damage = 1;

    result.damage = (uint16_t)min<uint32_t>(damage, 999);
    result.effectiveness = eff;
    result.critical = crit;
    return result;
}

} // namespace BattleSystem
