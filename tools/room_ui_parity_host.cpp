#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#define private public
#include "scenes/MenuScene.h"
#undef private

#if STICKMON_ROOM_UI_BASELINE || STICKMON_MAIN_MENU_UI_BASELINE
#define MAIN_MENU(scene) (scene)
#else
#define MAIN_MENU(scene) ((scene).mainMenuUi)
#endif

class TouchInput final : public Platform::IInputDevice {
public:
    void update() override {}
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return STICKMON_UI_COMPACT_TOUCH; }
};

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    auto bundle = desktop.serviceBundle();
    TouchInput input;
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, bundle.power,
        bundle.blobs, bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    GameRandom::seed(1234);
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    for (int i = 0; i < Game::ROOM_FOOD_COUNT; ++i) state.room.food[i] = i * 3;
    state.room.selectedFood = 3;
    MenuScene menu; menu.onEnter();
#if STICKMON_ROOM_UI_BASELINE
    auto& room = menu;
    auto& food = menu;
#else
    auto& room = menu.roomUi;
    auto& food = menu.foodUi;
#endif
    auto capture = [&](const std::string& name) {
        PixelRenderer::clear(0);
        menu.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file);
        std::fclose(file);
    };
    menu.viewMode = MenuScene::ViewMode::ROOM;
    for (int i = 0; i < 3; ++i) {
        room.roomCursor = i;
        capture("room-choice-" + std::to_string(i));
    }
    for (auto& stock : state.room.food) stock = Game::ITEM_STACK_CAP;
    room.roomCursor = 0;
    capture("room-full-stock");
    for (auto& stock : state.room.food) stock = 0;
    capture("room-empty-stock");
    menu.viewMode = MenuScene::ViewMode::FOOD;
    for (int i = 0; i < Game::ROOM_FOOD_COUNT; ++i) state.room.food[i] = i * 3;
    for (int i = 0; i <= Game::ROOM_FOOD_COUNT; ++i) {
        food.foodCursor = i;
#if STICKMON_UI_COMPACT_TOUCH
        food.foodScroll = MenuGrid::fit({0, TouchUi::SAFE_HEADER_H, Hal::DISPLAY_W,
            Hal::DISPLAY_H - TouchUi::SAFE_HEADER_H}, Hal::DISPLAY_W - 16, 44, 0, 6, 8, 4)
            .reveal(i, 0, Game::ROOM_FOOD_COUNT);
#else
        const int visible = TouchUi::CONTENT_H - 12;
        food.foodScroll = std::clamp(i * 22 - visible / 2 + 11, 0,
            std::max(0, (Game::ROOM_FOOD_COUNT + 1) * 22 - visible));
        food.descScroll = 0;
#endif
        capture("food-choice-" + std::to_string(i));
#if !STICKMON_UI_COMPACT_TOUCH
        if (i < Game::ROOM_FOOD_COUNT) {
            food.descScroll = 24;
            capture("food-description-" + std::to_string(i));
        }
#endif
    }
    food.foodCursor = 3;
    food.foodScroll = 17;
    capture("food-partial-scroll");
    state.room.selectedFood = 255;
    capture("food-invalid-selected");
    for (auto& stock : state.room.food) stock = 0;
    capture("food-empty-stock");
    menu.toast = Ui::Room::FOOD_SELECTED;
    capture("food-selected-notice");
    menu.toast = Ui::Room::FOOD_NO_STOCK;
    capture("food-empty-notice");
    menu.viewMode = MenuScene::ViewMode::ROOM;
    menu.toast = Ui::Menu::HATCH_FIRST;
    capture("room-hatch-notice");
    std::puts("Room pages captured");

    // Exercise production scene entry points with the same state in both UIs.
    state = Game::GameState{};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    state.room.food[0] = 2;
    state.room.food[2] = 4;
    state.room.selectedFood = 0;
    menu.onEnter();
    menu.toast = nullptr; // Pixel captures above inject notices without deadlines.
    MAIN_MENU(menu).cursor = 0;
    auto press = [&](int button, BtnAction action = BtnAction::PRESSED) {
        assert(menu.onButton({uint8_t(button), action}));
    };
    auto chooseFood = [&](uint8_t index) {
        for (int i = 0; food.foodCursor != index && i <= Game::ROOM_FOOD_COUNT; ++i) press(1);
        assert(food.foodCursor == index);
    };
    auto expireNotice = [&](const char* text, uint32_t duration) {
        assert(menu.toast && std::strcmp(menu.toast, text) == 0);
        assert(menu.toastUntil == Hal::ins().millis() + duration);
        menu.update(Hal::ins().millis() + duration - 1, 0.0f);
        assert(menu.toast);
        desktop.advanceMs(duration);
        assert(menu.update(Hal::ins().millis(), 0.0f).redraw);
        assert(!menu.toast);
    };
    auto assertParked = [&](const char* context) {
        const auto result = menu.update(Hal::ins().millis(), 0.0f);
        if (result.redraw || result.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE)
            std::fprintf(stderr, "%s: redraw=%d next=%u\n", context, result.redraw, result.nextUpdateDelayMs);
        assert(!result.redraw && result.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    };
    press(1); press(1); press(0); // Main menu -> Room.
    assert(menu.viewMode == MenuScene::ViewMode::ROOM && menu.navDepth == 1);
    assertParked("Room");
    press(0); // Room -> Food.
    assert(menu.viewMode == MenuScene::ViewMode::FOOD && menu.navDepth == 2);
    assert(food.foodCursor == 0);
    const auto beforeSelection = state;
    chooseFood(2); press(0);
    auto selected = beforeSelection;
    selected.room.selectedFood = 2;
    assert(std::memcmp(&state, &selected, sizeof(state)) == 0);
    expireNotice(Ui::Room::FOOD_SELECTED, 900);
    chooseFood(1); press(0); // Empty stock cannot change selection or inventory.
    assert(std::memcmp(&state, &selected, sizeof(state)) == 0);
    expireNotice(Ui::Room::FOOD_NO_STOCK, 900);

#if STICKMON_UI_COMPACT_TOUCH
    using Type = Platform::IInputDevice::TouchType;
    auto touch = [&](Type type, int x, int y) {
        menu.onTouch({type, int16_t(x), int16_t(y), Hal::ins().millis()});
    };
    assert(food.touchTarget(22,19)==-1 && food.touchTarget(275,24)==-1);
    assert(food.touchTarget(48,24)==230 && food.touchTarget(10,60)==0);
    touch(Type::DOWN,22,19);touch(Type::UP,22,19);
    assert(menu.viewMode==MenuScene::ViewMode::FOOD);
    touch(Type::DOWN,48,24);touch(Type::MOVE,80,24);touch(Type::UP,48,24);
    assert(menu.viewMode==MenuScene::ViewMode::FOOD);
    touch(Type::DOWN,48,24);menu.cancelTouch();touch(Type::UP,48,24);
    assert(menu.viewMode==MenuScene::ViewMode::FOOD);
    touch(Type::DOWN,48,24);touch(Type::UP,48,24);
    assert(menu.viewMode==MenuScene::ViewMode::ROOM && menu.navDepth==1);
    press(0);assert(menu.viewMode==MenuScene::ViewMode::FOOD && menu.navDepth==2);
    chooseFood(0); // Wrapping to the first item also resets its scroll.
    assert(food.foodScroll == 0);
    touch(Type::DOWN, 100, 60);
    touch(Type::MOVE, 100, 15);
    touch(Type::UP, 100, 15);
    assert(food.foodScroll > 0 && !menu.toast);
    assert(std::memcmp(&state, &selected, sizeof(state)) == 0);
    for (int i = 0; i < Game::ROOM_FOOD_COUNT; ++i) press(1);
    assert(food.foodScroll == 0);
    touch(Type::DOWN, 100, 60);
    touch(Type::CANCEL, 100, 60);
    touch(Type::UP, 100, 60);
    assert(!menu.toast && std::memcmp(&state, &selected, sizeof(state)) == 0);
    touch(Type::DOWN, 100, 60);
    menu.cancelTouch(); // Locking or changing the input source cancels the press.
    touch(Type::UP, 100, 60);
    assert(!menu.toast && std::memcmp(&state, &selected, sizeof(state)) == 0);
    touch(Type::DOWN, 100, 60);
    press(1, BtnAction::LONG_PRESS); // Leaving must also cancel a pending touch.
    touch(Type::UP, 100, 60);
    assert(menu.viewMode == MenuScene::ViewMode::ROOM && menu.navDepth == 1);
    assert(!menu.toast && std::memcmp(&state, &selected, sizeof(state)) == 0);
    press(0);
#endif

    chooseFood(6);
    const auto scrolling = menu.update(Hal::ins().millis(), 0.0f);
#if STICKMON_UI_COMPACT_TOUCH
    assert(!scrolling.redraw && scrolling.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
#else
    assert(scrolling.redraw && scrolling.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE);
    for (int i = 0; i < 40; ++i) {
        desktop.advanceMs(66);
        menu.update(Hal::ins().millis(), 0.066f);
    }
#endif
    assertParked("Settled food list");
    press(1, BtnAction::LONG_PRESS);
    assert(menu.viewMode == MenuScene::ViewMode::ROOM && menu.navDepth == 1);
    press(1); press(1); press(0); // Physical Back remains the third Room option.
    assert(menu.viewMode == MenuScene::ViewMode::MENU && menu.navDepth == 0);
    assert(MAIN_MENU(menu).cursor == 2 && std::memcmp(&state, &selected, sizeof(state)) == 0);

    engine.requestScene(SceneID::MENU, false);
    press(0); press(1); // Reopen Room and choose Shower.
    const auto previousScene = engine.previousScene();
    state.oobeDone = false;
    const auto beforeHatch = state;
    press(0);
    assert(menu.viewMode == MenuScene::ViewMode::ROOM);
    assert(std::memcmp(&state, &beforeHatch, sizeof(state)) == 0);
    assert(engine.previousScene() == previousScene);
    expireNotice(Ui::Menu::HATCH_FIRST, 1100);
    state.oobeDone = true;
    state.teamCount = 0;
    const auto beforeEmptyTeam = state;
    press(0);
    assert(std::memcmp(&state, &beforeEmptyTeam, sizeof(state)) == 0);
    assert(engine.previousScene() == previousScene);
    expireNotice(Ui::Menu::HATCH_FIRST, 1100);
    assertParked("Bath gate notice expired");
    state.teamCount = 1;
    press(0); // The valid entry still uses the original engine scene switch.
    engine.requestScene(engine.homeScene(), false);
    assert(engine.previousScene() == SceneID::SHOWER);
    assert(state.room.selectedFood == 2 && state.room.food[0] == 2 && state.room.food[2] == 4);
    std::puts("Room selection, cancellation, navigation, scheduling and bath checks passed");
}
