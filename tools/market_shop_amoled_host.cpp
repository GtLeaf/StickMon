// Real AmoledApp journey through the Kecleon stall (奇珍小铺).
#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "market_dialogue_fixture.h"

static const char* gOutput = nullptr;
static void capture(const char* name) {
    if (!gOutput) return;
    const auto path = std::string(gOutput) + "/" + name + ".ppm";
    auto* file = std::fopen(path.c_str(), "wb"); assert(file);
    std::fprintf(file, "P6\n368 448\n255\n");
    for (int y = 0; y < 448; ++y) for (int x = 0; x < 368; ++x) {
        const auto c = PixelRenderer::canvas().readPixel(x, y);
        const unsigned char rgb[] = {uint8_t(((c >> 11) & 31) * 255 / 31), uint8_t(((c >> 5) & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    assert(std::fclose(file) == 0);
}

namespace AmoledV1 {
struct AmoledAppTestAccess {
    using Page = Ui::MarketShopUi::Page;
    static void run(AmoledApp& app) {
        auto& state = app.gameState;
        state = {}; state.oobeDone = true; state.teamCount = 2; state.coins = 1774;
        for (int i = 0; i < Game::EXPLORE_AREA_COUNT; ++i) state.explorePoolRerollCounts[i] = 1;
        state.team[0] = Game::MonsterFactory::create(25, 18);
        state.team[1] = Game::MonsterFactory::create(133, 18);
        app.syncHomeActors(0);
        uint32_t t = 0, touchTime = 1000;
        const auto advance = [&](int n) { while (n--) { t += 100; app.update(t); } };
        const auto tap = [&](int x, int y) {
            TouchEvent e{}; e.x = int16_t(x); e.y = int16_t(y);
            e.type = TouchEventType::DOWN; e.timestampMs = touchTime; app.handleTouch(e);
            e.type = TouchEventType::UP; e.timestampMs = touchTime + 60; app.handleTouch(e);
            touchTime += 200;
        };
        const auto drag = [&](int x, int y, int endX, int endY) {
            TouchEvent e{}; e.x = x; e.y = y; e.type = TouchEventType::DOWN; e.timestampMs = touchTime;
            app.handleTouch(e); e.x = endX; e.y = endY; e.type = TouchEventType::MOVE;
            app.handleTouch(e); e.type = TouchEventType::UP; e.timestampMs += 60;
            app.handleTouch(e); touchTime += 200;
        };
        const auto shot = [&](const char* name) { app.render(PixelRenderer::canvas()); capture(name); };
        const auto settle = [&]() { for (int n = 0; n < 6 && app.marketShopUi_.animating(); ++n) advance(1); };
        const auto rowY = [](int i) { return 216 + 72 * i + 36; };
        assert(app.startMarket(0));
        advance(40);
        assert(app.market_->actor(0).species == 352 && marketStandNear(*app.market_, 0));
        // Fixed shelf for this game day: heart scale / full restore / water stone.
        state.marketShop = Game::MarketShopState{};
        state.marketShop.periodDay = Game::MarketShop::periodDay(state);
        const uint8_t shelf[] = {uint8_t(Game::ItemId::HEART_SCALE), uint8_t(Game::ItemId::FULL_RESTORE), uint8_t(Game::ItemId::WATER_STONE)};
        for (int i = 0; i < 3; ++i) { state.marketShop.items[i] = shelf[i]; state.marketShop.stock[i] = 1; }
        app.marketUi_.chatTarget = 0; app.marketUi_.refresh(*app.market_);
        assert(app.marketUi_.chatTarget == 0);
        auto& ui = app.marketShopUi_;
        tap(184, 399);
        assert(app.market_->conversing() && !app.marketUi_.dialogue().active());
        for (int n = 0; n < 30 && !ui.active(); ++n) advance(1);
        assert(ui.active() && ui.page() == Page::GREET && !app.marketTutorUi_.active());
        // The greeting box rises from the bottom edge; the coins stay hidden.
        assert(ui.animating() && ui.panelTop() > 328 && ui.hudOffset() < -40);
        settle(); advance(3); shot("01-greet");
        assert(ui.panelTop() == 328);
        // Tap skips the greeting; the lifting finger never buys. The panel
        // grows from the greeting box edge instead of jumping to the bottom.
        tap(184, 280);
        assert(ui.page() == Page::SHELF && ui.animating() && ui.panelTop() == 328);
        int lastTop = ui.panelTop(), lastHud = ui.hudOffset(), steps = 0;
        while (ui.animating()) {
            advance(1); ++steps;
            assert(ui.panelTop() <= lastTop && ui.panelTop() >= 168 && ui.hudOffset() >= lastHud);
            assert(steps > 3 || (ui.panelTop() < lastTop && ui.hudOffset() > lastHud));
            lastTop = ui.panelTop(); lastHud = ui.hudOffset();
            if (steps == 2) shot("01b-opening-200ms");
        }
        assert(steps == 5 && ui.panelTop() == 168 && ui.hudOffset() == 0);
        shot("02-shelf");
        // The old footer area is now the last shelf row's lower edge or empty.
        tap(184, 440); assert(ui.page() == Page::SHELF && !ui.animating());
        tap(184, 168); assert(ui.page() == Page::SHELF && !ui.animating()); // Panel edge is inside.
        drag(184, 165, 184, 171); // Crossing the boundary within tap slop is not a click.
        drag(184, 171, 184, 165);
        drag(184, 100, 272, 414); // An outside press cannot buy on release.
        assert(ui.page() == Page::SHELF && !ui.animating() && state.coins == 1774);
        const auto pose = app.market_->playerPose();
        tap(184, rowY(0)); assert(ui.page() == Page::CONFIRM && ui.selected() == 0); shot("03-confirm");
        drag(272, 414, 184, 100); // Dragging the buy button outside cannot back out or buy.
        assert(ui.page() == Page::CONFIRM && state.coins == 1774);
        tap(184, 100); assert(ui.page() == Page::SHELF && state.coins == 1774 && state.marketShop.stock[0] == 1);
        tap(184, rowY(0));
        tap(96, 414); assert(ui.page() == Page::SHELF && state.coins == 1774);
        tap(184, rowY(0));
        app.saveWritesBlocked = true;
        tap(272, 414);
        assert(ui.page() == Page::CONFIRM && state.coins == 1774 && state.bag.heartScale == 0 && state.marketShop.stock[0] == 1);
        shot("04-save-failed");
        app.saveWritesBlocked = false;
        tap(272, 414);
        assert(ui.page() == Page::RESULT && state.coins == 1174 && state.bag.heartScale == 1 && !state.marketShop.stock[0]);
        shot("05-success");
        {
            SaveManager manager; Game::GameState loaded; MainSceneViewState view;
            assert(manager.load(loaded, view));
            assert(loaded.coins == 1174 && loaded.bag.heartScale == 1 && !loaded.marketShop.stock[0]);
        }
        tap(184, 100); assert(ui.page() == Page::SHELF && state.coins == 1174); shot("06-shelf-sold");
        tap(184, rowY(0)); assert(ui.page() == Page::CONFIRM); shot("07-sold-confirm");
        tap(272, 414); assert(ui.page() == Page::CONFIRM && state.coins == 1174); // disabled
        tap(96, 414);
        // Not enough coins for the 900 stone after buying the restore (360).
        tap(184, rowY(1)); tap(272, 414);
        assert(ui.page() == Page::RESULT && state.coins == 814 && state.bag.fullRestore == 1);
        tap(184, 414);
        tap(184, rowY(2)); shot("08-no-coins");
        tap(272, 414); assert(ui.page() == Page::CONFIRM && state.coins == 814 && state.bag.waterStone == 0);
        tap(96, 414); assert(ui.page() == Page::SHELF);
        assert(app.market_->playerPose().x == pose.x && app.market_->playerPose().y == pose.y);
        // Leave from the top-right cross: the panel sinks away, then walking
        // resumes. A drag off the cross does not close.
        {
            TouchEvent e{}; e.x = 336; e.y = 194; e.type = TouchEventType::DOWN; e.timestampMs = touchTime;
            app.handleTouch(e); e.type = TouchEventType::MOVE; e.y = 300; app.handleTouch(e);
            e.type = TouchEventType::UP; e.timestampMs = touchTime + 60; app.handleTouch(e); touchTime += 200;
            assert(ui.page() == Page::SHELF && !ui.animating());
        }
        tap(184, 100); // Outside the shelf closes with the same animation as the cross.
        assert(ui.active() && ui.animating() && app.market_->conversing());
        lastTop = ui.panelTop(); lastHud = ui.hudOffset(); steps = 0;
        while (ui.active()) {
            advance(1); ++steps;
            if (!ui.active()) break;
            assert(ui.panelTop() > lastTop && ui.hudOffset() <= lastHud);
            // Smoothstep keeps every closing step well under the old cubic peak.
            assert(ui.panelTop() - lastTop <= 90);
            lastTop = ui.panelTop(); lastHud = ui.hudOffset();
            if (steps == 2) shot("09-closing-200ms");
        }
        assert(steps == 6 && !app.market_->conversing());
        shot("10-closed");
        TouchEvent walk{}; walk.type = TouchEventType::DOWN; walk.x = 100; walk.y = 200; walk.timestampMs = touchTime;
        app.handleTouch(walk); walk.type = TouchEventType::MOVE; walk.x = 140; app.handleTouch(walk);
        advance(4); assert(app.market_->playerPose().x != pose.x || app.market_->playerPose().y != pose.y);
        walk.type = TouchEventType::UP; app.handleTouch(walk);
        // Re-entry the same day keeps the sold-out state and greets accordingly.
        auto again = state; assert(!Game::MarketShop::ensurePeriod(again));
        std::puts("market-shop-amoled: outside back/animated close, boundary drags, greet-rise panel-grow, save-failure buy sold-out coins OK");
    }
};
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    gOutput = argc == 3 ? argv[2] : nullptr;
    DesktopPlatform desktop(argv[1]); Platform::bind(desktop.serviceBundle()); assert(desktop.begin());
    assert(Platform::resources().mount()); assert(GameAssets::begin()); FontResource::ins().begin();
    std::vector<uint16_t> pixels(368 * 448); auto& c = PixelRenderer::canvas(); c.attach({pixels.data(), 368, 448, false});
    c.setCoordinateScale(1); c.setLayoutScale(1); c.setAssetScale(1); c.setNativeText(true);
    AmoledV1::AmoledApp app; app.begin(0); AmoledV1::AmoledAppTestAccess::run(app);
}
