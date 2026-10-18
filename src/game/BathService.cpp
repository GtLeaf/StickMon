#include "game/BathService.h"

#include <algorithm>

#include "game/ItemInventory.h"
#include "game/CareTicker.h"

namespace Game {
namespace BathService {

uint8_t hpRecoveryPercentForScore(uint8_t score) {
    switch (score) {
    case 1: return 45;
    case 2: return 70;
    case 3: return 100;
    default: return 0;
    }
}

uint16_t applyCompletionRecovery(GameState& state, uint8_t score,
                                 uint8_t teamSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) return 0;

    MonsterRuntime& monster = state.team[teamSlot];
    if (monster.fainted || monster.hpCur == 0 || monster.hpCur >= monster.hpMax) {
        return 0;
    }

    uint8_t percent = hpRecoveryPercentForScore(score);
    if (percent == 0) return 0;

    uint16_t amount = static_cast<uint16_t>(
        (static_cast<uint32_t>(monster.hpMax) * percent + 99) / 100);
    if (amount < 1) amount = 1;
    uint16_t nextHp = static_cast<uint16_t>(std::min<uint32_t>(
        monster.hpMax, static_cast<uint32_t>(monster.hpCur) + amount));
    uint16_t recovered = nextHp - monster.hpCur;
    monster.hpCur = nextHp;
    return recovered;
}

int8_t nextOwnedSoap(const GameState& state, int8_t from,
                     int8_t direction) {
    if (direction == 0) direction = 1;
    for (uint8_t step = 1; step <= SOAP_VARIANT_COUNT; ++step) {
        int index = from + direction * step;
        while (index < 0) index += SOAP_VARIANT_COUNT;
        index %= SOAP_VARIANT_COUNT;
        if (state.bag.soap[index] > 0) return static_cast<int8_t>(index);
    }
    return -1;
}

bool consumeSoap(GameState& state, uint8_t soapIndex) {
    ItemId item = itemIdForSoapIndex(soapIndex);
    return item != ItemId::COUNT && ItemInventory::remove(state, item);
}

RewardResult applyStageReward(GameState& state, Stage stage,
                              uint8_t teamSlot) {
    RewardResult result;
    resetDailyCareCounters(state);
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) return result;

    MonsterRuntime& monster = state.team[teamSlot];
    if (monster.origin == Origin::VISITOR || monster.fainted || monster.hpCur == 0)
        return result;
    uint8_t requestedMood = 0;
    switch (stage) {
    case Stage::SOAP:
        break;
    case Stage::BRUSH:
        requestedMood = 2;
        break;
    case Stage::RINSE:
        requestedMood = 8;
        break;
    }

    result.experience = CareExperience::grant(state, teamSlot,
        CareExperience::Source::BATH, static_cast<uint8_t>(stage));

    uint8_t moodBefore = monster.mood;
    monster.mood = static_cast<uint8_t>(std::min<uint16_t>(
        100, static_cast<uint16_t>(monster.mood) + requestedMood));
    result.moodGain = monster.mood - moodBefore;
    return result;
}

}  // namespace BathService
}  // namespace Game
