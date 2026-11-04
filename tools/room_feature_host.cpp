#include <cassert>
#include <cstring>

#include "core/features/RoomFeature.h"
#include "core/UiStrings.h"
#include "game/ItemInventory.h"
#include "ui/common/RoomUiBase.h"

namespace {
class Host : public Ui::RoomHost, public Ui::FoodHost {
public:
    Game::GameState state;
    int foodOpens = 0, showerOpens = 0, backs = 0, selections = 0;
    uint8_t lastSelection = 255;
    bool selectionResult = false;
    char notice[64] = {};
    uint32_t noticeDuration = 0;

    const Game::GameState& pageState() const override { return state; }
    bool pageExploreContext() const override { return false; }
    void pageBack() override { ++backs; }
    void pageHome() override {}
    void pageNotice(const char* text, uint32_t durationMs) override {
        assert(text && std::strlen(text) < sizeof(notice));
        std::strcpy(notice, text);
        noticeDuration = durationMs;
    }
    void pageBackTutorial() override {}
    void pageNavigationTutorial() override {}
    void openRoomFood() override { ++foodOpens; }
    void openRoomShower() override { ++showerOpens; }
    bool selectPageFood(uint8_t index) override {
        ++selections;
        lastSelection = index;
        return selectionResult;
    }
};

class RoomPage : public Ui::RoomUiBase {
public:
    using RoomUiBase::RoomUiBase;
    using RoomUiBase::activateRoom;
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override {}
};

class FoodPage : public Ui::FoodUiBase {
public:
    using FoodUiBase::FoodUiBase;
    using FoodUiBase::activateFood;
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override {}
};

void unchanged(const Game::GameState& state, const Game::GameState& before) {
    assert(std::memcmp(&state, &before, sizeof(state)) == 0);
}
}

int main() {
    using namespace Features::Room;
    Game::GameState state;
    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index)
        state.room.food[index] = 0;
    assert(totalFoodStock(state) == 0 && displayedFoodStock(state) == 0);
    state.room.food[0] = 12;
    state.room.food[Game::ROOM_FOOD_COUNT - 1] = 99;
    state.room.selectedFood = Game::ROOM_FOOD_COUNT - 1;
    const auto before = state;
    assert(selectedFoodIndex(state) == Game::ROOM_FOOD_COUNT - 1);
    for (int value = 0; value <= 255; ++value) {
        const auto index = static_cast<uint8_t>(value);
        const bool valid = index < Game::ROOM_FOOD_COUNT;
        assert(normalizedFoodIndex(index) == (valid ? index : 0));
        assert(foodBack(index) == !valid);
        assert(foodStock(state, index) == state.room.food[valid ? index : 0]);
        if (valid)
            assert(foodStock(state, index) == Game::ItemInventory::count(state, Game::itemIdForFoodIndex(index)));
    }
    assert(totalFoodStock(state) == 111 && displayedFoodStock(state) == 111);
    assert(roomAction(0) == Action::FOOD && roomAction(1) == Action::SHOWER);
    assert(roomAction(2) == Action::BACK && roomAction(3) == Action::NONE);
    assert(roomAction(255) == Action::NONE);
    unchanged(state, before);

    // Corrupt selected indices normalize for querying, without repairing saves.
    state.room.selectedFood = 255;
    const auto invalidSelected = state;
    assert(selectedFoodIndex(state) == 0 && foodStock(state, 255) == 12);
    unchanged(state, invalidSelected);
    for (auto& stock : state.room.food) stock = 255;
    assert(totalFoodStock(state) == Game::ROOM_FOOD_COUNT * 255);
    assert(displayedFoodStock(state) == 255);
    for (auto& stock : state.room.food) stock = 0;
    state.room.food[0] = 255;
    assert(totalFoodStock(state) == 255 && displayedFoodStock(state) == 255);
    state.room.food[1] = 1;
    assert(totalFoodStock(state) == 256 && displayedFoodStock(state) == 255);

    state.oobeDone = false; state.teamCount = 1;
    assert(!canShower(state));
    state.oobeDone = true; state.teamCount = 0;
    assert(!canShower(state));
    state.teamCount = 1;
    assert(canShower(state));

    // The common flow dispatches actions and notices; the host owns mutations.
    Host host;
    RoomPage room(host);
    FoodPage food(host);
    const auto initialHostState = host.state;
    room.activateRoom(0);
    assert(host.foodOpens == 1 && host.showerOpens == 0 && host.backs == 0);
    room.activateRoom(255);
    assert(host.foodOpens == 1 && host.showerOpens == 0 && host.backs == 0);
    host.state.oobeDone = false;
    host.state.teamCount = 1;
    room.activateRoom(1);
    assert(host.showerOpens == 0 && host.noticeDuration == 1100);
    assert(std::strcmp(host.notice, Ui::Menu::HATCH_FIRST) == 0);
    host.state.oobeDone = true;
    host.state.teamCount = 0;
    room.activateRoom(1);
    assert(host.showerOpens == 0);
    host.state.teamCount = 1;
    room.activateRoom(1);
    assert(host.showerOpens == 1);
    room.activateRoom(2);
    assert(host.backs == 1);

    host.state = initialHostState;
    food.activateFood(Game::ROOM_FOOD_COUNT);
    food.activateFood(255);
    assert(host.backs == 3 && host.selections == 0);
    host.selectionResult = false;
    food.activateFood(0);
    assert(host.selections == 1 && host.lastSelection == 0 && host.noticeDuration == 900);
    assert(std::strcmp(host.notice, Ui::Room::FOOD_NO_STOCK) == 0);
    host.selectionResult = true;
    food.activateFood(Game::ROOM_FOOD_COUNT - 1);
    assert(host.selections == 2 && host.lastSelection == Game::ROOM_FOOD_COUNT - 1);
    assert(host.noticeDuration == 900 && host.backs == 3);
    assert(std::strcmp(host.notice, Ui::Room::FOOD_SELECTED) == 0);
    unchanged(host.state, initialHostState);
}
