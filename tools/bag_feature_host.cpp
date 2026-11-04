#include <cassert>
#include <cstring>

#include "core/features/BagFeature.h"
#include "game/ItemInventory.h"

namespace {
Game::GameState* currentState = nullptr;
int applyCalls = 0;
Game::ItemId lastItem = Game::ItemId::COUNT;
uint8_t lastSlot = 255;
bool applyItem(Game::ItemId item, uint8_t slot) {
    ++applyCalls;
    lastItem = item;
    lastSlot = slot;
    if (Game::foodIndexForItemId(item) >= 0)
        return Game::ItemInventory::remove(*currentState, item);
    return Game::ItemInventory::useOnTeam(*currentState, item, slot) ==
        Game::ItemInventory::UseResult::USED;
}

void clearInventory(Game::GameState& state) {
    for (int value = 0; value < int(Game::ItemId::COUNT); ++value) {
        const auto item = static_cast<Game::ItemId>(value);
        const auto count = Game::ItemInventory::count(state, item);
        if (count) assert(Game::ItemInventory::remove(state, item, count));
    }
}
}  // namespace

int main() {
    using namespace Features::Bag;
    using Game::ItemId;
    Game::GameState state;
    currentState = &state;
    clearInventory(state);

    Row rows[ITEM_COUNT];
    assert(collect(state, false, rows, ITEM_COUNT) == 1);
    assert(rows[0].source == SOURCE_BACK && rows[0].count == 1);
    assert(collect(state, true, rows, ITEM_COUNT) == 1);
    assert(!collect(state, false, nullptr, ITEM_COUNT));
    assert(!collect(state, false, rows, 0));
    assert(sourceAt(state, false, 255) == SOURCE_BACK);

    for (uint8_t source = 0; source < SOURCE_BACK; ++source)
        assert(Game::ItemInventory::add(state, itemId(source)));
    const auto beforeQueries = state;
    const bool modes[] = {false, true};
    for (bool battle : modes) {
        const auto count = collect(state, battle, rows, ITEM_COUNT);
        assert(count == 19 && visibleCount(state, battle) == count);
        assert(rows[count - 1].source == SOURCE_BACK);
        bool seen[ITEM_COUNT] = {};
        for (uint8_t index = 0; index < count; ++index) {
            assert(rows[index].source == sourceAt(state, battle, index));
            assert(rows[index].count == 1);
            seen[rows[index].source] = true;
        }
        assert(seen[3] == !battle);
        for (uint8_t source = SOURCE_FOOD_BASE; source < SOURCE_HEAL_BASE; ++source)
            assert(seen[source] == battle);
        for (uint8_t source = SOURCE_HEAL_BASE; source < SOURCE_HEAL_BASE + 4; ++source)
            assert(seen[source]);
        assert(seen[SOURCE_EXTRA_BASE + 6]);
        assert(seen[SOURCE_EXTRA_BASE + 7] == !battle);
        assert(seen[SOURCE_HEART_SCALE] == !battle);
        assert(collect(state, battle, rows, 2) == 2);
    }
    assert(std::memcmp(&state, &beforeQueries, sizeof(state)) == 0);
    assert(select(SOURCE_BACK, false) == Selection::BACK);
    assert(select(SOURCE_HEART_SCALE, false) == Selection::RECALL);
    assert(select(SOURCE_HEART_SCALE, true) == Selection::NONE);
    assert(select(0, false) == Selection::CONFIRM);
    assert(select(SOURCE_FOOD_BASE, true) == Selection::CONFIRM);
    assert(select(SOURCE_FOOD_BASE, false) == Selection::NONE);
    assert(select(SOURCE_EXTRA_BASE + 3, true) == Selection::NONE);
    assert(select(SOURCE_EXTRA_BASE + 3, false) == Selection::CONFIRM);
    // Preserve the existing catalog's candy entry without adding a new action.
    assert(select(3, false) == Selection::NONE);

    state.teamCount = 2;
    state.team[0].hpMax = 50;
    state.team[0].hpCur = 10;
    state.team[0].fainted = false;
    state.team[1].hpMax = 40;
    state.team[1].hpCur = 0;
    state.team[1].fainted = true;
    Context context{true, 0, "Wild target"};
    assert(targetSlot(SOURCE_EXTRA_BASE + 6, context, state) == 1);
    assert(targetSlot(0, context, state) == 0);
    context.targetTeamSlot = 255;
    assert(targetSlot(0, context, state) == 0);
    context.targetTeamSlot = 1;
    applyCalls = 0;
    auto outcome = use(state, 0, context, applyItem);
    assert(!outcome.used && outcome.notice == Notice::FAINTED_CANNOT_HEAL);
    assert(applyCalls == 0 && state.bag.potion == 1);

    outcome = use(state, SOURCE_EXTRA_BASE + 6, context, applyItem);
    assert(outcome.used && outcome.battleResult == BattleResult::REVIVE);
    assert(outcome.notice == Notice::USED_REVIVE);
    assert(applyCalls == 1 && lastSlot == 1 && lastItem == ItemId::REVIVE);
    assert(state.team[1].hpCur == 20 && !state.team[1].fainted && state.bag.revive == 0);

    context.targetTeamSlot = 0;
    outcome = use(state, 0, context, applyItem);
    assert(outcome.used && outcome.battleResult == BattleResult::POTION);
    assert(outcome.notice == Notice::USED_POTION);
    assert(state.team[0].hpCur == 30 && state.bag.potion == 0);
    assert(lastItem == ItemId::POTION && lastSlot == 0);
    state.team[0].hpCur = state.team[0].hpMax;
    state.bag.potion = 1;
    outcome = use(state, 0, context, applyItem);
    assert(!outcome.used && outcome.notice == Notice::HP_FULL);
    assert(state.bag.potion == 1);

    state.team[0].majorStatus = Game::MajorStatus::BURN;
    outcome = use(state, SOURCE_HEAL_BASE + 2, context, applyItem);
    assert(outcome.used && outcome.battleResult == BattleResult::BURN_HEAL);
    assert(state.team[0].majorStatus == Game::MajorStatus::NONE && state.bag.burnHeal == 0);

    constexpr uint8_t food = 2;
    outcome = use(state, SOURCE_FOOD_BASE + food, context, applyItem);
    assert(outcome.used && outcome.battleResult == BattleResult::FOOD_THROWN);
    assert(outcome.notice == Notice::THREW_FOOD && outcome.foodIndex == food);
    assert(state.room.food[food] == 0 && lastItem == Game::itemIdForFoodIndex(food));
    outcome = use(state, SOURCE_FOOD_BASE + food, context, applyItem);
    assert(!outcome.used && outcome.notice == Notice::FOOD_NO_STOCK);
    context.battle = false;
    const int callsBefore = applyCalls;
    outcome = use(state, SOURCE_FOOD_BASE, context, applyItem);
    assert(!outcome.used && outcome.notice == Notice::NONE && applyCalls == callsBefore);
    outcome = use(state, 0, context, nullptr);
    assert(!outcome.used && applyCalls == callsBefore);
    return 0;
}
