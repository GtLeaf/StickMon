// Real AmoledApp journey through the Gengar stall (秘技小屋): entry from the
// chat button, teach / save failure / train / recall / leave, and lock
// handling. Frames are written when an output directory is given.
#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"
#include "ui/ItemShopScreens.h"
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
    using Page = Ui::MarketTutorUi::Page;
    static void run(AmoledApp& app) {
        auto& state = app.gameState;
        state = {}; state.oobeDone = true; state.teamCount = 2; state.coins = 1774; state.bag.heartScale = 1;
        state.team[0] = Game::MonsterFactory::create(25, 18);
        state.team[1] = Game::MonsterFactory::create(133, 18);
        state.team[0].ivPacked = 0x1234; state.team[0].metAt = 100;
        state.team[1].ivPacked = 0x5678; state.team[1].metAt = 200;
        state.team[0].move2Id = 486; state.team[0].moveProficiency[1] = 64;
        state.team[0].move3Id = 98; state.team[0].moveProficiency[2] = 100;
        state.team[1].move2Id = 129; state.team[1].moveProficiency[1] = 72;
        state.team[1].move3Id = 44; state.team[1].moveProficiency[2] = 15;
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
        assert(app.startMarket(0));
        advance(40);
        assert(marketStandNear(*app.market_, 1) && app.market_->actor(1).species == 94);
        // The app clock owns gameMinutesTotal; pin this day's lesson to Shadow
        // Ball (Eevee-only) so the journey is deterministic. ensurePeriod keeps
        // a record that already belongs to the current day.
        {
            auto drawn = state; Game::MarketTutor::ensurePeriod(drawn);
            assert(drawn.marketTutor.move && drawn.marketTutor.periodDay == Game::MarketTutor::periodDay(state));
            state.marketTutor = Game::MarketTutorState{};
            state.marketTutor.periodDay = Game::MarketTutor::periodDay(state);
            state.marketTutor.move = 247; state.marketTutor.drawnSpecies = 133;
        }
        app.marketUi_.chatTarget = 1; app.marketUi_.refresh(*app.market_);
        assert(app.marketUi_.chatTarget == 1);
        const auto ui = [&]() -> Ui::MarketTutorUi& { return app.marketTutorUi_; };
        // Chat button on Gengar turns both sides, then opens the stall.
        tap(184, 399);
        assert(app.market_->conversing() && !app.marketUi_.dialogue().active());
        for (int n = 0; n < 30 && !ui().active(); ++n) advance(1);
        assert(ui().active() && ui().page() == Page::GREET);
        assert(state.marketTutor.move == 247 && state.marketTutor.drawnSpecies == 133);
        const auto period = state.marketTutor;
        advance(3); shot("01-greet");
        for (int n = 0; n < 40 && ui().page() == Page::GREET; ++n) advance(1);
        // The panel grows from the greeting box edge, not from the bottom.
        assert(ui().page() == Page::MENU && ui().animating() && ui().panelTop() == 328 && ui().hudOffset() < -40);
        const auto viewport = app.marketViewport();
        const int greetingShift = ui().sceneShift(*app.market_, viewport);
        shot("01a-opening-start");
        tap(184, 244); // A tap during the slide cannot choose a service.
        assert(ui().page() == Page::MENU);
        advance(1);
        const int partialTop = ui().panelTop();
        const int partialShift = ui().sceneShift(*app.market_, viewport);
        assert(partialTop > 168 && partialTop < 328 && partialShift > greetingShift);
        shot("01b-opening-100ms");
        // Both the camera and panel pause while locked, even after a long gap.
        app.setDisplayLocked(true); advance(20);
        assert(ui().panelTop() == partialTop && ui().sceneShift(*app.market_, viewport) == partialShift);
        app.setDisplayLocked(false); advance(1);
        assert(ui().panelTop() < partialTop && ui().panelTop() > 168);
        shot("01c-opening-200ms");
        const int standbyTop = ui().panelTop();
        app.setDisplayStandby(true, t); advance(40);
        assert(ui().panelTop() == standbyTop);
        app.setDisplayStandby(false, t); app.update(t);
        assert(ui().panelTop() == standbyTop); // Wake never catches up the slide.
        for (int n = 0; n < 5 && ui().animating(); ++n) advance(1);
        assert(!ui().animating() && ui().panelTop() == 168 && ui().hudOffset() == 0);
        assert(ui().sceneShift(*app.market_, viewport) > partialShift);
        shot("02-menu");
        // Currency keeps the shop glyphs/font and uses the stall panel palette.
        {
            const auto pixels = std::vector<uint16_t>(PixelRenderer::canvas().rawPixels(),
                PixelRenderer::canvas().rawPixels() + 368 * 448);
            ShopViewModel model{}; model.coins = state.coins;
            renderShopScreen(PixelRenderer::canvas(), model, 0, 64);
            const int w = UiCommon::textWidth("C1774");
            for (int y = 0; y < 32; ++y) for (int x = 0; x < w; ++x) {
                const int shopY = (UiMetrics::PAGE_HEADER_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
                const auto shop = PixelRenderer::canvas().readPixel(184 - w / 2 + x, shopY + y);
                const auto tutor = pixels[(16 + y) * 368 + 340 - w + x];
                const auto gold = UiCommon::rgb(248, 210, 105);
                const auto ink = UiCommon::rgb(40, 56, 80);
                const auto paper = UiCommon::rgb(248, 248, 224);
                assert((shop == gold) == (tutor == ink));
                assert(tutor == ink || tutor == paper);
            }
            shot("02-menu");
        }
        // Walking and chat stay frozen while the stall is open.
        const auto pose = app.market_->playerPose();
        tap(20, 435); advance(5); // Unused panel space is not outside.
        assert(app.market_->playerPose().x == pose.x && ui().page() == Page::MENU);
        tap(184, 168); assert(ui().page() == Page::MENU && !ui().animating());
        drag(184, 165, 184, 171);
        drag(184, 171, 184, 165);
        drag(184, 100, 184, 244);
        assert(ui().page() == Page::MENU && !ui().animating());

        tap(184, 216 + 28); assert(ui().page() == Page::TUTOR); shot("03-tutor");
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 244);
        tap(184, 248 + 28); assert(ui().page() == Page::TUTOR); // Pikachu cannot learn
        tap(184, 248 + 56 + 28); assert(ui().page() == Page::SLOT && ui().member() == 1); shot("04-slot");
        tap(184, 100); assert(ui().page() == Page::TUTOR);
        tap(184, 332);
        tap(184, 216 + 28); assert(ui().page() == Page::SLOT); // basic move is locked
        tap(184, 216 + 56 + 28); assert(ui().page() == Page::CONFIRM); shot("05-confirm");
        drag(272, 414, 184, 100); assert(ui().page() == Page::CONFIRM && state.coins == 1774);
        tap(184, 100); assert(ui().page() == Page::SLOT && state.coins == 1774 && !state.marketTutor.taught);
        tap(184, 300);
        // A failed save keeps coins, moves and the period untouched.
        app.saveWritesBlocked = true;
        tap(272, 414);
        assert(ui().page() == Page::CONFIRM && state.coins == 1774 && state.team[1].move2Id == 129 && !state.marketTutor.taught);
        shot("06-confirm-save-failed");
        app.saveWritesBlocked = false;
        tap(272, 414);
        assert(ui().page() == Page::RESULT && state.coins == 1374);
        assert(state.team[1].move2Id == 247 && state.team[1].moveProficiency[1] == 0 && state.marketTutor.taught == 1);
        shot("07-learned");
        {
            SaveManager manager; Game::GameState loaded; MainSceneViewState view;
            assert(manager.load(loaded, view));
            assert(loaded.coins == 1374 && loaded.team[1].move2Id == 247 && loaded.marketTutor.taught == 1);
            assert(loaded.marketTutor.periodDay == period.periodDay && loaded.marketTutor.move == 247);
        }
        tap(184, 100); assert(ui().page() == Page::MENU && state.coins == 1374);
        tap(184, 216 + 28); assert(ui().page() == Page::MENU); // lesson sold this period
        shot("07b-menu-unavailable");

        // Training: Pikachu's Electro Ball 64 -> 72.
        tap(184, 216 + 112 + 28); assert(ui().page() == Page::TRAIN_MEMBER); shot("08-train-member");
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 356);
        tap(184, 216 + 28); assert(ui().page() == Page::TRAIN_MOVE); shot("09-train-move");
        tap(184, 100); assert(ui().page() == Page::TRAIN_MEMBER);
        tap(184, 244);
        tap(184, 216 + 56 + 28); assert(ui().page() == Page::TRAIN_MOVE); // 100 is maxed
        tap(184, 216 + 28); assert(ui().page() == Page::CONFIRM); shot("10-train-confirm");
        tap(184, 100); assert(ui().page() == Page::TRAIN_MOVE && state.coins == 1374 && state.team[0].moveProficiency[1] == 64);
        tap(184, 244);
        tap(272, 414); assert(ui().page() == Page::RESULT && state.team[0].moveProficiency[1] == 72 && state.coins == 1174);
        tap(184, 414);
        tap(184, 216 + 112 + 28); tap(184, 216 + 28);
        assert(ui().page() == Page::TRAIN_MEMBER); // already trained this period
        tap(184, 414); assert(ui().page() == Page::MENU);

        // Recall with the heart scale.
        tap(184, 216 + 56 + 28); assert(ui().page() == Page::RECALL_MEMBER);
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 300);
        tap(184, 216 + 56 + 28); assert(ui().page() == Page::RECALL_LIST); shot("11-recall-list");
        drag(184, 260, 184, 100); assert(ui().page() == Page::RECALL_LIST);
        tap(184, 100); assert(ui().page() == Page::RECALL_MEMBER);
        tap(184, 300);
        tap(184, 216 + 28); assert(ui().page() == Page::SLOT);
        tap(184, 100); assert(ui().page() == Page::RECALL_LIST);
        tap(184, 244);
        tap(184, 216 + 112 + 28); assert(ui().page() == Page::CONFIRM); shot("12-recall-confirm");
        tap(184, 100); assert(ui().page() == Page::SLOT && state.bag.heartScale == 1);
        tap(184, 356);
        tap(272, 414); assert(ui().page() == Page::RESULT && state.bag.heartScale == 0 && state.team[1].move3Id != 44);
        tap(184, 414); assert(ui().page() == Page::MENU); shot("13-menu-after");

        // Lock cancels the gesture and keeps the page; leave resumes walking.
        TouchEvent down{}; down.type = TouchEventType::DOWN; down.x = 336; down.y = 194; down.timestampMs = touchTime;
        app.handleTouch(down); app.setDisplayLocked(true);
        TouchEvent up = down; up.type = TouchEventType::UP; up.timestampMs = touchTime + 50; app.handleTouch(up);
        app.setDisplayLocked(false); advance(3);
        assert(ui().page() == Page::MENU && app.market_->conversing());
        touchTime += 200;
        tap(184, 100); // Main-page outside tap closes with the cross animation.
        assert(ui().active() && ui().animating() && app.market_->conversing());
        const int openShift = ui().sceneShift(*app.market_, viewport);
        advance(1);
        assert(ui().panelTop() > 168 && ui().panelTop() < 448);
        assert(ui().sceneShift(*app.market_, viewport) < openShift);
        shot("14-closing-100ms");
        tap(184, 244); assert(ui().page() == Page::MENU);
        advance(1); shot("15-closing-200ms");
        for (int n = 0; n < 5 && ui().active(); ++n) advance(1);
        assert(!ui().active() && !app.market_->conversing());
        shot("16-closed");
        advance(5);
        TouchEvent walk{}; walk.type = TouchEventType::DOWN; walk.x = 100; walk.y = 200; walk.timestampMs = touchTime;
        app.handleTouch(walk); walk.type = TouchEventType::MOVE; walk.y = 240; app.handleTouch(walk);
        advance(4); assert(app.market_->playerPose().y != pose.y || app.market_->playerPose().x != pose.x);
        walk.type = TouchEventType::UP; app.handleTouch(walk);
        // Same period on re-entry: no redraw, no second lesson.
        auto again = state; Game::MarketTutor::ensurePeriod(again);
        assert(again.marketTutor.move == 247 && again.marketTutor.taught == 1);
        std::puts("market-tutor-amoled: outside back on every subpage, animated close, boundary/scroll drags, shop currency, lock pause and save rollback OK");
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
