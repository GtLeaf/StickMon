#include <cassert>
#include <cstdio>
#include <string>
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/ItemInventory.h"
#include "presentation/PixelRenderer.h"
#include "scenes/ShopScene.h"
#include "scenes/MenuScene.h"

// Exercise the same semantic journey through each production UI using only
// scene entry points. Captures also allow comparison to the pre-refactor build.
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
    state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(1, 5);
    state.team[0].hpCur = 1;
    state.coins = 10000;
    for (auto& stock : state.room.food) stock = 0;
    state.bag.potion = 0;
    Game::ItemInventory::add(state, Game::ItemId::NORMAL_FOOD, 2);
    Game::ItemInventory::add(state, Game::ItemId::POTION, 2);
    auto settle = [&](Scene& scene) {
        for (int i = 0; i < 100; ++i) {
            desktop.advanceMs(20);
            scene.update(desktop.millis(), .02f);
        }
        const auto demand = scene.update(desktop.millis(), .02f);
        assert(!demand.redraw && demand.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE);
    };
    auto capture = [&](Scene& scene, const char* name) {
        PixelRenderer::clear(0);
        scene.render();
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        for (int y = 0; y < fb.height; ++y)
            std::fwrite(fb.pixels + y * fb.width, sizeof(uint16_t), fb.width, file);
        std::fclose(file);
    };
    const ButtonEvent confirm{0, BtnAction::PRESSED};
    const ButtonEvent next{1, BtnAction::PRESSED};
    const ButtonEvent back{1, BtnAction::LONG_PRESS};
    ShopScene shop;
    shop.onEnter();
    settle(shop);
    capture(shop, "shop-entry");
    assert(shop.onButton(confirm));
    settle(shop);
    capture(shop, "shop-detail");
    assert(state.coins == 10000);
    assert(shop.onButton(confirm));
    assert(state.coins == 9980 && engine.itemCount(Game::ItemId::NORMAL_FOOD) == 3);
    settle(shop);
    capture(shop, "shop-bought");
#if STICKMON_UI_COMPACT_TOUCH
    assert(shop.onButton(back));
    using Type = Platform::IInputDevice::TouchType;
    shop.onTouch({Type::DOWN, 32, 162, desktop.millis()});
    assert(shop.onTouch({Type::UP, 32, 162, desktop.millis() + 40}));
    assert(shop.onButton(confirm));
#else
    assert(shop.onButton(back));
    settle(shop);
    assert(shop.onButton(next));
    assert(shop.onButton(next));
    assert(shop.onButton(confirm));
#endif
    settle(shop);
    capture(shop, "shop-sell");
    assert(shop.onButton(confirm));
    assert(state.coins == 9990 && engine.itemCount(Game::ItemId::NORMAL_FOOD) == 2);
    settle(shop);
    capture(shop, "shop-sold");
    MenuScene menu;
    menu.onEnter();
    menu.openExploreBagView();
    settle(menu);
    capture(menu, "bag-entry");
    assert(menu.onButton(confirm));
#if STICKMON_UI_COMPACT_TOUCH
    capture(menu, "bag-detail");
    assert(menu.onButton(confirm));
#endif
    capture(menu, "bag-confirm");
    assert(engine.itemCount(Game::ItemId::POTION) == 2 && state.team[0].hpCur == 1);
    assert(menu.onButton(back));
    assert(engine.itemCount(Game::ItemId::POTION) == 2 && state.team[0].hpCur == 1);
    assert(menu.onButton(confirm));
    assert(menu.onButton(confirm));
    assert(engine.itemCount(Game::ItemId::POTION) == 1 && state.team[0].hpCur > 1);
    settle(menu);
    capture(menu, "bag-used");
    // Reusing a potion on a full target does not consume stock.
    state.team[0].hpCur = state.team[0].hpMax;
    assert(menu.onButton(confirm));
    assert(menu.onButton(confirm));
    assert(engine.itemCount(Game::ItemId::POTION) == 1);
    std::printf("coins=%lu food=%u potion=%u hp=%u/%u\n",
        (unsigned long)state.coins, engine.itemCount(Game::ItemId::NORMAL_FOOD),
        engine.itemCount(Game::ItemId::POTION), state.team[0].hpCur, state.team[0].hpMax);
}
