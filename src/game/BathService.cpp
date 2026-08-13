#include "game/BathService.h"

#include <algorithm>

#include "game/ItemInventory.h"

namespace Game {
namespace BathService {

uint16_t careDailyCapForLevel(uint8_t level) {
    if (level <= 10) return 60;
    if (level <= 20) return 35;
    return 15;
}

uint8_t fullBathExperienceForLevel(uint8_t level) {
    uint16_t amount = 9 + (static_cast<uint16_t>(level) + 4) / 5;
    return static_cast<uint8_t>(std::min<uint16_t>(amount, 15));
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
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) return result;

    MonsterRuntime& monster = state.team[teamSlot];
    uint8_t fullBathExp = fullBathExperienceForLevel(monster.level);
    uint8_t soapExp = fullBathExp <= 13 ? 2 : 3;
    uint8_t brushExp = fullBathExp <= 11 ? 3 : 4;
    uint8_t requestedExp = 0;
    uint8_t requestedMood = 0;
    switch (stage) {
    case Stage::SOAP:
        requestedExp = soapExp;
        break;
    case Stage::BRUSH:
        requestedExp = brushExp;
        requestedMood = 2;
        break;
    case Stage::RINSE:
        requestedExp = fullBathExp - soapExp - brushExp;
        requestedMood = 8;
        break;
    }

    uint16_t cap = careDailyCapForLevel(monster.level);
    uint16_t available = state.careExpToday < cap
        ? cap - state.careExpToday : 0;
    result.experience = static_cast<uint8_t>(
        std::min<uint16_t>(requestedExp, available));
    state.careExpToday += result.experience;

    uint8_t moodBefore = monster.mood;
    monster.mood = static_cast<uint8_t>(std::min<uint16_t>(
        100, static_cast<uint16_t>(monster.mood) + requestedMood));
    result.moodGain = monster.mood - moodBefore;
    return result;
}

}  // namespace BathService
}  // namespace Game
