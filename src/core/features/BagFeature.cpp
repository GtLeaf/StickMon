#include "core/features/BagFeature.h"
#include "game/ItemInventory.h"

namespace Features { namespace Bag {

Game::ItemId itemId(uint8_t sourceIndex) {
    switch (sourceIndex) {
    case 0: return Game::ItemId::POTION;
    case 1: return Game::ItemId::SUPER_POTION;
    case 2: return Game::ItemId::ANTIDOTE;
    case 3: return Game::ItemId::CANDY;
    case SOURCE_HEAL_BASE + 0: return Game::ItemId::PARALYZE_HEAL;
    case SOURCE_HEAL_BASE + 1: return Game::ItemId::AWAKENING;
    case SOURCE_HEAL_BASE + 2: return Game::ItemId::BURN_HEAL;
    case SOURCE_HEAL_BASE + 3: return Game::ItemId::ICE_HEAL;
    case SOURCE_EXTRA_BASE + 0: return Game::ItemId::MAX_POTION;
    case SOURCE_EXTRA_BASE + 1: return Game::ItemId::FULL_RESTORE;
    case SOURCE_EXTRA_BASE + 2: return Game::ItemId::FULL_HEAL;
    case SOURCE_EXTRA_BASE + 3: return Game::ItemId::FIRE_STONE;
    case SOURCE_EXTRA_BASE + 4: return Game::ItemId::WATER_STONE;
    case SOURCE_EXTRA_BASE + 5: return Game::ItemId::THUNDER_STONE;
    case SOURCE_EXTRA_BASE + 6: return Game::ItemId::REVIVE;
    case SOURCE_EXTRA_BASE + 7: return Game::ItemId::MAX_REPEL;
    case SOURCE_EXTRA_BASE + 8: return Game::ItemId::HONEY;
    case SOURCE_HEART_SCALE: return Game::ItemId::HEART_SCALE;
    default:
        if (sourceIndex >= SOURCE_FOOD_BASE &&
            sourceIndex < SOURCE_FOOD_BASE + Game::ROOM_FOOD_COUNT) {
            return Game::itemIdForFoodIndex(sourceIndex - SOURCE_FOOD_BASE);
        }
        return Game::ItemId::COUNT;
    }
}

bool visible(uint8_t sourceIndex, bool battleOnly) {
    if (sourceIndex >= SOURCE_FOOD_BASE &&
        sourceIndex < SOURCE_FOOD_BASE + Game::ROOM_FOOD_COUNT) {
        return battleOnly;
    }
    if (sourceIndex >= SOURCE_HEAL_BASE && sourceIndex < SOURCE_HEAL_BASE + 4) {
        return true;
    }
    if (!battleOnly) {
        return sourceIndex < 4 ||
               (sourceIndex >= SOURCE_EXTRA_BASE &&
                sourceIndex < SOURCE_EXTRA_BASE + SOURCE_EXTRA_COUNT);
    }
    switch (sourceIndex) {
    case 0:
    case 1:
    case 2:
    case SOURCE_EXTRA_BASE + 0:
    case SOURCE_EXTRA_BASE + 1:
    case SOURCE_EXTRA_BASE + 2:
    case SOURCE_EXTRA_BASE + 6:
        return true;
    default:
        return false;
    }
}

uint8_t targetSlot(uint8_t sourceIndex, const Context& context,
                   const Game::GameState& state) {
    uint8_t fallback =
        context.targetTeamSlot < state.teamCount && context.targetTeamSlot < Game::TEAM_CAP
            ? context.targetTeamSlot : 0;
    if (sourceIndex != SOURCE_EXTRA_BASE + 6) return fallback;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (state.team[slot].fainted || state.team[slot].hpCur == 0) return slot;
    }
    return context.battle ? fallback : 0;
}


uint8_t itemCount(const Game::GameState& state, uint8_t source) {
    return source == SOURCE_BACK ? 1 : Game::ItemInventory::count(state, itemId(source));
}

uint8_t collect(const Game::GameState& state, bool battle, Row* rows, uint8_t capacity) {
    if (!rows || !capacity) return 0;
    uint8_t count = 0;
    for (uint8_t source = 0; source < SOURCE_BACK && count < capacity; ++source) {
        const auto stock = itemCount(state, source);
        if (visible(source, battle) && stock) rows[count++] = {source, stock};
    }
    if (count < capacity) rows[count++] = {SOURCE_BACK, 1};
    return count;
}

uint8_t visibleCount(const Game::GameState& state, bool battle) {
    Row rows[ITEM_COUNT];
    return collect(state, battle, rows, ITEM_COUNT);
}

uint8_t sourceAt(const Game::GameState& state, bool battle, uint8_t index) {
    Row rows[ITEM_COUNT];
    const auto count = collect(state, battle, rows, ITEM_COUNT);
    return index < count ? rows[index].source : SOURCE_BACK;
}

Selection select(uint8_t source, bool battle) {
    if (source == SOURCE_BACK) return Selection::BACK;
    if (source == SOURCE_HEART_SCALE && !battle) return Selection::RECALL;
    if (source < 3 ||
        (source >= SOURCE_HEAL_BASE && source < SOURCE_HEAL_BASE + 4) ||
        (source >= SOURCE_EXTRA_BASE && source < SOURCE_EXTRA_BASE + SOURCE_EXTRA_COUNT &&
         (!battle || source == SOURCE_EXTRA_BASE || source == SOURCE_EXTRA_BASE + 1 ||
          source == SOURCE_EXTRA_BASE + 2 || source == SOURCE_EXTRA_BASE + 6)) ||
        (battle && source >= SOURCE_FOOD_BASE && source < SOURCE_FOOD_BASE + Game::ROOM_FOOD_COUNT))
        return Selection::CONFIRM;
    return Selection::NONE;
}

Outcome use(const Game::GameState& state, uint8_t source, const Context& context, ApplyItem apply) {
    Outcome out;
    if (!apply) return out;
    bool& used = out.used;
    auto& result = out.battleResult;
    auto& notice = out.notice;
    uint8_t slot = targetSlot(source, context, state);
    if (source == 0) {
        const auto& mon = state.team[slot];
        if (mon.fainted || mon.hpCur == 0) {
            notice = Notice::FAINTED_CANNOT_HEAL;
        } else {
            used = apply(itemId(source), slot);
            notice = used ? Notice::USED_POTION : Notice::HP_FULL;
            result = BattleResult::POTION;
        }
    } else if (source == 1) {
        const auto& mon = state.team[slot];
        if (mon.fainted || mon.hpCur == 0) {
            notice = Notice::FAINTED_CANNOT_HEAL;
        } else {
            used = apply(itemId(source), slot);
            notice = used ? Notice::USED_SUPER_POTION : Notice::HP_FULL;
            result = BattleResult::SUPER_POTION;
        }
    } else if (source == 2) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_ANTIDOTE : Notice::STATUS_NORMAL;
        result = BattleResult::ANTIDOTE;
    } else if (source == SOURCE_HEAL_BASE + 0) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_PARALYZE_HEAL : Notice::STATUS_NORMAL;
        result = BattleResult::PARALYZE_HEAL;
    } else if (source == SOURCE_HEAL_BASE + 1) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_AWAKENING : Notice::STATUS_NORMAL;
        result = BattleResult::AWAKENING;
    } else if (source == SOURCE_HEAL_BASE + 2) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_BURN_HEAL : Notice::STATUS_NORMAL;
        result = BattleResult::BURN_HEAL;
    } else if (source == SOURCE_HEAL_BASE + 3) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_ICE_HEAL : Notice::STATUS_NORMAL;
        result = BattleResult::ICE_HEAL;
    } else if (source == SOURCE_EXTRA_BASE + 0) {
        const auto& mon = state.team[slot];
        if (mon.fainted || mon.hpCur == 0) {
            notice = Notice::FAINTED_CANNOT_HEAL;
        } else {
            used = apply(itemId(source), slot);
            notice = used ? Notice::USED_MAX_POTION : Notice::HP_FULL;
            if (used) result = BattleResult::MAX_POTION;
        }
    } else if (source == SOURCE_EXTRA_BASE + 1) {
        const auto& mon = state.team[slot];
        if (mon.fainted || mon.hpCur == 0) {
            notice = Notice::FAINTED_CANNOT_HEAL;
        } else {
            used = apply(itemId(source), slot);
            notice = used ? Notice::USED_FULL_RESTORE
                         : Notice::FULL_RESTORE_NOT_NEEDED;
            if (used) result = BattleResult::FULL_RESTORE;
        }
    } else if (source == SOURCE_EXTRA_BASE + 2) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_FULL_HEAL : Notice::STATUS_NORMAL;
        result = BattleResult::FULL_HEAL;
    } else if (source >= SOURCE_EXTRA_BASE + 3 &&
               source < SOURCE_EXTRA_BASE + 6) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::STONE_EVOLUTION_BEGIN
                     : Notice::STONE_NO_EFFECT;
    } else if (source == SOURCE_EXTRA_BASE + 6) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_REVIVE
                     : Notice::REVIVE_NO_FAINTED;
        result = BattleResult::REVIVE;
    } else if (source == SOURCE_EXTRA_BASE + 7) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_MAX_REPEL
                     : Notice::MAX_REPEL_ACTIVE;
    } else if (source == SOURCE_EXTRA_BASE + 8) {
        used = apply(itemId(source), slot);
        notice = used ? Notice::USED_HONEY
                     : Notice::HONEY_ACTIVE;
    } else if (context.battle && source >= SOURCE_FOOD_BASE &&
               source < SOURCE_FOOD_BASE + Game::ROOM_FOOD_COUNT) {
        uint8_t foodIndex = source - SOURCE_FOOD_BASE;
        used = apply(itemId(source), slot);
        if (used) {
            out.foodIndex = foodIndex;
            notice = Notice::THREW_FOOD;
        } else {
            notice = Notice::FOOD_NO_STOCK;
        }
        result = BattleResult::FOOD_THROWN;
    }
    return out;
}

} }  // namespace Features::Bag
