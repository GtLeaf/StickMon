#include "game/ItemInventory.h"

#include <algorithm>

namespace Game {
namespace ItemInventory {
namespace {

uint8_t* stockPointer(GameState& state, ItemId item) {
    int8_t foodIndex = foodIndexForItemId(item);
    if (foodIndex >= 0) return &state.room.food[foodIndex];
    int8_t soapIndex = soapIndexForItemId(item);
    if (soapIndex >= 0) return &state.bag.soap[soapIndex];
    switch (item) {
    case ItemId::POTION: return &state.bag.potion;
    case ItemId::SUPER_POTION: return &state.bag.superPotion;
    case ItemId::ANTIDOTE: return &state.bag.antidote;
    case ItemId::CANDY: return &state.bag.candy;
    case ItemId::PARALYZE_HEAL: return &state.bag.paralyzeHeal;
    case ItemId::AWAKENING: return &state.bag.awakening;
    case ItemId::BURN_HEAL: return &state.bag.burnHeal;
    case ItemId::ICE_HEAL: return &state.bag.iceHeal;
    case ItemId::MAX_POTION: return &state.bag.maxPotion;
    case ItemId::FULL_RESTORE: return &state.bag.fullRestore;
    case ItemId::FULL_HEAL: return &state.bag.fullHeal;
    case ItemId::FIRE_STONE: return &state.bag.fireStone;
    case ItemId::WATER_STONE: return &state.bag.waterStone;
    case ItemId::THUNDER_STONE: return &state.bag.thunderStone;
    case ItemId::REVIVE: return &state.bag.revive;
    case ItemId::MAX_REPEL: return &state.bag.maxRepel;
    case ItemId::HONEY: return &state.bag.honey;
    case ItemId::NUGGET: return &state.bag.nugget;
    case ItemId::BIG_PEARL: return &state.bag.bigPearl;
    case ItemId::STAR_PIECE: return &state.bag.starPiece;
    case ItemId::HEART_SCALE: return &state.bag.heartScale;
    default: return nullptr;
    }
}

uint8_t totalFood(const GameState& state) {
    uint16_t total = 0;
    for (uint8_t index = 0; index < ROOM_FOOD_COUNT; ++index) {
        total += state.room.food[index];
    }
    return static_cast<uint8_t>(std::min<uint16_t>(total, 255));
}

void selectFirstFood(GameState& state) {
    for (uint8_t index = 0; index < ROOM_FOOD_COUNT; ++index) {
        if (state.room.food[index] > 0) {
            state.room.selectedFood = index;
            return;
        }
    }
    state.room.selectedFood = 0;
}

bool cureStatus(MonsterRuntime& monster, MajorStatus first,
                MajorStatus second = MajorStatus::NONE) {
    if (monster.majorStatus != first &&
        (second == MajorStatus::NONE || monster.majorStatus != second)) {
        return false;
    }
    monster.majorStatus = MajorStatus::NONE;
    monster.majorStatusTurns = 0;
    return true;
}

constexpr ItemId HOME_BAG_ITEMS[] = {
    ItemId::POTION,
    ItemId::SUPER_POTION,
    ItemId::ANTIDOTE,
    ItemId::CANDY,
    ItemId::PARALYZE_HEAL,
    ItemId::AWAKENING,
    ItemId::BURN_HEAL,
    ItemId::ICE_HEAL,
    ItemId::MAX_POTION,
    ItemId::FULL_RESTORE,
    ItemId::FULL_HEAL,
    ItemId::FIRE_STONE,
    ItemId::WATER_STONE,
    ItemId::THUNDER_STONE,
    ItemId::REVIVE,
    ItemId::MAX_REPEL,
    ItemId::HONEY,
    ItemId::HEART_SCALE,
};

}  // namespace

uint8_t count(const GameState& state, ItemId item) {
    const uint8_t* value = stockPointer(const_cast<GameState&>(state), item);
    return value ? *value : 0;
}

bool add(GameState& state, ItemId item, uint8_t amount) {
    if (amount == 0) return true;
    uint8_t* value = stockPointer(state, item);
    if (!value || *value >= ITEM_STACK_CAP ||
        amount > ITEM_STACK_CAP - *value) {
        return false;
    }
    bool hadFood = totalFood(state) > 0;
    *value = static_cast<uint8_t>(*value + amount);
    int8_t foodIndex = foodIndexForItemId(item);
    if (!hadFood && foodIndex >= 0) {
        state.room.selectedFood = static_cast<uint8_t>(foodIndex);
    }
    return true;
}

bool remove(GameState& state, ItemId item, uint8_t amount) {
    if (amount == 0) return true;
    uint8_t* value = stockPointer(state, item);
    if (!value || *value < amount) return false;
    *value = static_cast<uint8_t>(*value - amount);
    if (foodIndexForItemId(item) >= 0 &&
        state.room.selectedFood < ROOM_FOOD_COUNT &&
        state.room.food[state.room.selectedFood] == 0) {
        selectFirstFood(state);
    }
    return true;
}

bool usableFromHomeBag(ItemId item) {
    switch (item) {
    case ItemId::POTION:
    case ItemId::SUPER_POTION:
    case ItemId::ANTIDOTE:
    case ItemId::PARALYZE_HEAL:
    case ItemId::AWAKENING:
    case ItemId::BURN_HEAL:
    case ItemId::ICE_HEAL:
    case ItemId::MAX_POTION:
    case ItemId::FULL_RESTORE:
    case ItemId::FULL_HEAL:
    case ItemId::REVIVE:
        return true;
    default:
        return false;
    }
}

uint8_t preferredTarget(const GameState& state, ItemId item) {
    if (item == ItemId::REVIVE) {
        for (uint8_t slot = 0;
             slot < state.teamCount && slot < TEAM_CAP; ++slot) {
            if (state.team[slot].fainted || state.team[slot].hpCur == 0) {
                return slot;
            }
        }
    }
    return 0;
}

UseResult useOnTeam(GameState& state, ItemId item, uint8_t teamSlot) {
    if (count(state, item) == 0) return UseResult::NO_STOCK;
    if (teamSlot >= state.teamCount || teamSlot >= TEAM_CAP) {
        return UseResult::INVALID_TARGET;
    }
    MonsterRuntime& monster = state.team[teamSlot];
    switch (item) {
    case ItemId::POTION:
    case ItemId::SUPER_POTION:
    case ItemId::MAX_POTION:
        if (monster.fainted || monster.hpCur == 0) return UseResult::FAINTED;
        if (monster.hpCur >= monster.hpMax) return UseResult::HP_FULL;
        remove(state, item);
        if (item == ItemId::MAX_POTION) {
            monster.hpCur = monster.hpMax;
        } else {
            uint16_t healing = item == ItemId::POTION ? 20 : 50;
            monster.hpCur = std::min<uint16_t>(
                monster.hpMax, static_cast<uint16_t>(monster.hpCur + healing));
        }
        return UseResult::USED;

    case ItemId::FULL_RESTORE:
        if (monster.fainted || monster.hpCur == 0) return UseResult::FAINTED;
        if (monster.hpCur >= monster.hpMax &&
            monster.majorStatus == MajorStatus::NONE) {
            return UseResult::HP_FULL;
        }
        remove(state, item);
        monster.hpCur = monster.hpMax;
        monster.majorStatus = MajorStatus::NONE;
        monster.majorStatusTurns = 0;
        return UseResult::USED;

    case ItemId::ANTIDOTE:
        if (!cureStatus(monster, MajorStatus::POISON, MajorStatus::TOXIC)) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        return UseResult::USED;
    case ItemId::PARALYZE_HEAL:
        if (!cureStatus(monster, MajorStatus::PARALYSIS)) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        return UseResult::USED;
    case ItemId::AWAKENING:
        if (!cureStatus(monster, MajorStatus::SLEEP)) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        return UseResult::USED;
    case ItemId::BURN_HEAL:
        if (!cureStatus(monster, MajorStatus::BURN)) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        return UseResult::USED;
    case ItemId::ICE_HEAL:
        if (!cureStatus(monster, MajorStatus::FREEZE)) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        return UseResult::USED;
    case ItemId::FULL_HEAL:
        if (monster.majorStatus == MajorStatus::NONE) {
            return UseResult::STATUS_NORMAL;
        }
        remove(state, item);
        monster.majorStatus = MajorStatus::NONE;
        monster.majorStatusTurns = 0;
        return UseResult::USED;

    case ItemId::REVIVE:
        if (!monster.fainted && monster.hpCur > 0) {
            return UseResult::NO_FAINTED_TARGET;
        }
        remove(state, item);
        monster.fainted = false;
        monster.hpCur = std::max<uint16_t>(1, monster.hpMax / 2);
        return UseResult::USED;
    default:
        return UseResult::NOT_USABLE;
    }
}

uint8_t homeBagItemCount(const GameState& state) {
    uint8_t visible = 0;
    for (ItemId item : HOME_BAG_ITEMS) {
        if (count(state, item) > 0) ++visible;
    }
    return visible;
}

ItemId homeBagItemAt(const GameState& state, uint8_t visibleIndex) {
    uint8_t visible = 0;
    for (ItemId item : HOME_BAG_ITEMS) {
        if (count(state, item) == 0) continue;
        if (visible++ == visibleIndex) return item;
    }
    return ItemId::COUNT;
}

}  // namespace ItemInventory
}  // namespace Game
