// Real AmoledApp journey through the Chansey stall (暖心小屋).
#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "game/MarketMapData.h"
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
    using Page = Ui::MarketCareUi::Page;
    using Mode = Ui::MarketCareUi::Mode;
    static void run(AmoledApp& app) {
        auto& state = app.gameState;
        state = {}; state.oobeDone = true; state.teamCount = 2; state.coins = 1774;
        state.team[0] = Game::MonsterFactory::create(25, 18);
        state.team[1] = Game::MonsterFactory::create(133, 18);
        state.team[0].ivPacked = 0x1234; state.team[0].metAt = 100; state.team[0].affection = 100;
        state.team[1].ivPacked = 0x5678; state.team[1].metAt = 200; state.team[1].affection = 215;
        // Roster: Charmander (bond 23), a never-invited Squirtle, three more.
        const uint16_t roster[] = {4, 7, 1, 10, 16};
        state.storageCount = 5;
        for (uint8_t i = 0; i < 5; ++i) {
            auto& m = state.storage[i];
            m = Game::MonsterFactory::create(roster[i], 12);
            m.origin = Game::Origin::BEFRIENDED; m.ivPacked = 0x9000 + i; m.metAt = 300 + i;
            m.lastExploredAt = m.metAt + 5; m.bond = 50;
        }
        state.storage[0].bond = 23;
        state.storage[1].bond = Game::Bond::NEW_CONTACT_VALUE; state.storage[1].lastExploredAt = 0;
        app.syncHomeActors(0);
        uint32_t t = 0, touchTime = 1000;
        const auto advance = [&](int n) { while (n--) { t += 100; app.update(t); } };
        const auto tap = [&](int x, int y) {
            TouchEvent e{}; e.x = int16_t(x); e.y = int16_t(y);
            e.type = TouchEventType::DOWN; e.timestampMs = touchTime; app.handleTouch(e);
            e.type = TouchEventType::UP; e.timestampMs = touchTime + 60; app.handleTouch(e);
            touchTime += 200;
        };
        const auto drag = [&](int x, int y0, int y1) {
            TouchEvent e{}; e.x = int16_t(x); e.y = int16_t(y0);
            e.type = TouchEventType::DOWN; e.timestampMs = touchTime; app.handleTouch(e);
            e.type = TouchEventType::MOVE; e.y = int16_t((y0 + y1) / 2); e.timestampMs = touchTime + 40; app.handleTouch(e);
            e.y = int16_t(y1); e.timestampMs = touchTime + 80; app.handleTouch(e);
            e.type = TouchEventType::UP; e.timestampMs = touchTime + 120; app.handleTouch(e);
            touchTime += 300;
        };
        const auto shot = [&](const char* name) { app.render(PixelRenderer::canvas()); capture(name); };
        auto& ui = app.marketCareUi_;
        const auto settle = [&]() { for (int n = 0; n < 8 && ui.animating(); ++n) advance(1); };
        const auto menuRow = [](int i) { return 216 + 68 * i + 34; };
        const auto listRow = [](int i) { return 216 + 56 * i + 28; };
        const auto& care = state.marketCare;

        if (!BuildConfig::DEBUG_FEATURES) {
            // Release currently hides the whole market. Exercise its NPC
            // interaction independently so future entry changes stay safe.
            assert(!app.startMarket(0));
            app.openMarketCare(0);
            assert(!ui.active());
            app.market_.reset(new Market::Session());
            assert(app.market_->reset(MarketMapData::get(), 1234, 25, 133, 0));
            app.market_->enableManual();
            app.marketUi_.reset(); app.marketUi_.enableDialogue();
            app.marketUi_.setServiceSpecies(Game::MarketCare::STALL_SPECIES);
            app.sceneFlow.enter(AppSceneFlow::Scene::MARKET);
            advance(40);
            int chansey = -1;
            for (int i = 0; i < app.market_->data().residentCount; ++i)
                if (app.market_->actor(i).species == Game::MarketCare::STALL_SPECIES) chansey = i;
            assert(chansey >= 0 && marketStandNear(*app.market_, chansey));
            app.marketUi_.chatTarget = chansey; app.marketUi_.refresh(*app.market_);
            tap(184, 399);
            assert(app.marketUi_.dialogue().active());
            advance(30);
            assert(!ui.active() && !app.marketTutorPending_);
            assert(state.coins == 1774 && care.used == 0);
            for (int i = 0; i < 20 && app.marketUi_.dialogue().active(); ++i) {
                advance(10); tap(184, 400);
            }
            assert(!app.marketUi_.dialogue().active() && !app.market_->conversing());
            std::puts("market-care-release: ordinary dialogue, no service or payment OK");
            return;
        }

        assert(app.startMarket(0));
        advance(40);
        int chansey = -1;
        for (int i = 0; i < app.market_->data().residentCount; ++i)
            if (app.market_->actor(i).species == Game::MarketCare::STALL_SPECIES) chansey = i;
        assert(chansey >= 0 && marketStandNear(*app.market_, chansey));
        app.marketUi_.chatTarget = chansey; app.marketUi_.refresh(*app.market_);
        assert(app.marketUi_.chatTarget == chansey);
        tap(184, 399);
        assert(app.market_->conversing() && !app.marketUi_.dialogue().active());
        for (int n = 0; n < 30 && !ui.active(); ++n) advance(1);
        assert(ui.active() && ui.page() == Page::GREET && !app.marketShopUi_.active() && !app.marketTutorUi_.active());
        assert(ui.hudOffset() < -40);
        settle(); advance(3); shot("01-greet");
        tap(184, 280); // skip the greeting; the lifting finger picks nothing
        assert(ui.page() == Page::MENU && ui.panelTop() == 328);
        settle(); assert(ui.panelTop() == 168 && ui.hudOffset() == 0);
        shot("02-menu");
        const auto pose = app.market_->playerPose();

        // ---- Massage: list, confirm, save failure, success performance.
        tap(184, menuRow(0)); assert(ui.page() == Page::LIST && ui.mode() == Mode::MASSAGE); shot("03-massage-list");
        tap(184, listRow(0)); assert(ui.page() == Page::CONFIRM); shot("04-massage-confirm");
        tap(96, 414); assert(ui.page() == Page::LIST);
        tap(184, listRow(1)); assert(ui.page() == Page::CONFIRM); shot("05-massage-confirm-evolve");
        app.saveWritesBlocked = true;
        tap(272, 414);
        assert(ui.page() == Page::CONFIRM && state.coins == 1774 && state.team[1].affection == 215 && care.used == 0);
        shot("06-massage-save-failed");
        app.saveWritesBlocked = false;
        tap(272, 414);
        assert(ui.page() == Page::SHOW && ui.animating());
        assert(state.coins == 1624 && state.team[1].affection == 225 && care.used == 1);
        {
            SaveManager manager; Game::GameState loaded; MainSceneViewState view;
            assert(manager.load(loaded, view));
            assert(loaded.coins == 1624 && loaded.team[1].affection == 225 && loaded.marketCare.used == 1);
        }
        settle(); assert(ui.panelTop() == 328 && !ui.showDone());
        advance(3); shot("07-massage-performing");
        // Lock pauses the performance; a tap skips it; a second tap reopens.
        app.setDisplayLocked(true); advance(20); app.setDisplayLocked(false);
        assert(!ui.showDone());
        tap(184, 400); assert(ui.page() == Page::SHOW && ui.showDone());
        shot("08-massage-result");
        assert(state.coins == 1624 && care.used == 1); // skipping never pays twice
        tap(184, 400); assert(ui.page() == Page::MENU && ui.animating());
        settle(); assert(ui.panelTop() == 168 && ui.hudOffset() == 0);
        tap(184, menuRow(0)); assert(ui.page() == Page::LIST);
        tap(184, listRow(1)); assert(ui.page() == Page::LIST); // Eevee served this period
        tap(184, 416); assert(ui.page() == Page::MENU);
        // Tapping the market above the panel steps back like the other stalls.
        tap(184, menuRow(2)); tap(184, listRow(0)); assert(ui.page() == Page::EVAL);
        tap(60, 100); assert(ui.page() == Page::LIST);
        tap(60, 100); assert(ui.page() == Page::MENU && !ui.animating());

        // ---- Evaluation recommends tea for Charmander and books it.
        tap(184, menuRow(2)); assert(ui.page() == Page::LIST && ui.mode() == Mode::EVAL);
        tap(184, listRow(2)); assert(ui.page() == Page::EVAL); shot("09-eval-charmander");
        tap(272, 414); assert(ui.page() == Page::CONFIRM && ui.mode() == Mode::TEA); shot("10-tea-confirm");
        tap(96, 414); assert(ui.page() == Page::EVAL);
        tap(272, 414); assert(ui.page() == Page::CONFIRM);
        tap(272, 414);
        assert(ui.page() == Page::SHOW && state.storage[0].bond == 28 && state.coins == 1424 && care.used == 2);
        assert(state.teamCount == 2 && state.storage[0].petCountToday == 0);
        settle(); advance(14); assert(ui.showDone()); shot("11-tea-result");
        tap(184, 400); settle(); assert(ui.page() == Page::MENU);
        shot("12-menu-quota-empty");

        // ---- Tea list scrolls; the never-invited partner is disabled.
        tap(184, menuRow(1)); assert(ui.page() == Page::MENU); // quota used up
        shot("13-menu-quota-error");
        advance(20);
        tap(184, menuRow(2)); assert(ui.page() == Page::LIST);
        drag(184, 360, 220); assert(ui.page() == Page::LIST); shot("14-eval-list-scrolled");
        drag(184, 220, 380);
        tap(184, listRow(0)); assert(ui.page() == Page::EVAL); shot("15-eval-pikachu-no-quota");
        tap(184, 414); assert(ui.page() == Page::LIST); // only "back" when nothing is bookable
        tap(184, 416); assert(ui.page() == Page::MENU);
        assert(app.market_->playerPose().x == pose.x && app.market_->playerPose().y == pose.y);

        // ---- The cross closes; walking resumes. Outside the panel does too.
        {
            auto copy = ui;
            Platform::IInputDevice::TouchEvent e{Platform::IInputDevice::TouchType::DOWN, 60, 100, touchTime};
            copy.touch(e, state); e.type = Platform::IInputDevice::TouchType::UP; copy.touch(e, state);
            assert(copy.animating() && copy.page() == Page::MENU);
        }
        tap(336, 194);
        assert(ui.animating() && app.market_->conversing());
        for (int n = 0; n < 8 && ui.active(); ++n) advance(1);
        assert(!ui.active() && !app.market_->conversing());
        shot("16-closed");
        TouchEvent walk{}; walk.type = TouchEventType::DOWN; walk.x = 100; walk.y = 200; walk.timestampMs = touchTime;
        app.handleTouch(walk); walk.type = TouchEventType::MOVE; walk.x = 140; app.handleTouch(walk);
        advance(4); assert(app.market_->playerPose().x != pose.x || app.market_->playerPose().y != pose.y);
        walk.type = TouchEventType::UP; app.handleTouch(walk);
        std::puts("market-care-amoled: chat entry greet menu massage save-failure show-skip lock eval->tea quota scroll cross-close OK");
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
