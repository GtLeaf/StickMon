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
        state.marketTutorGifted = 1; // This journey exercises lessons after the first-visit gift.
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
        int openedSceneShift = -100000;
        const auto shot = [&](const char* name) {
            const auto& panel = app.marketTutorUi_;
            if (panel.active() && panel.page() != Page::GREET && !panel.animating()) {
                assert(panel.panelTop() == 208);
                const int shift = panel.sceneShift(*app.market_, app.marketViewport());
                if (openedSceneShift == -100000) openedSceneShift = shift;
                assert(shift == openedSceneShift); // Switching pages never moves the scene.
            }
            app.render(PixelRenderer::canvas()); capture(name);
        };
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
        const auto leaveAndReopen = [&]() {
            tap(184, 414);
            assert(ui().animating());
            advance(6);
            assert(!ui().active() && !app.market_->conversing());
            assert(marketStandNear(*app.market_, 1));
            app.marketUi_.chatTarget = 1; app.marketUi_.refresh(*app.market_);
            tap(184, 399);
            for (int i = 0; i < 30 && !ui().active(); ++i) advance(1);
            advance(4); assert(ui().page() == Page::GREET);
            tap(184, 380); advance(5); assert(ui().page() == Page::MENU);
            openedSceneShift = -100000;
        };
        // Chat button on Gengar turns both sides, then opens the stall.
        tap(184, 399);
        assert(app.market_->conversing() && !app.marketUi_.dialogue().active());
        for (int n = 0; n < 30 && !ui().active(); ++n) advance(1);
        assert(ui().active() && ui().page() == Page::GREET);
        assert(state.marketTutor.move == 247 && state.marketTutor.drawnSpecies == 133);
        const auto period = state.marketTutor;
        advance(3); shot("01-greet");
        advance(600); // A full minute never advances the greeting on its own.
        assert(ui().page() == Page::GREET && !ui().animating());
        drag(184, 360, 184, 260);
        assert(ui().page() == Page::GREET);
        TouchEvent greetDown{}; greetDown.type = TouchEventType::DOWN;
        greetDown.x = 184; greetDown.y = 380; greetDown.timestampMs = touchTime;
        app.handleTouch(greetDown); app.setDisplayLocked(true);
        app.setDisplayLocked(false);
        greetDown.type = TouchEventType::UP; greetDown.timestampMs += 60;
        app.handleTouch(greetDown); touchTime += 200;
        advance(30); assert(ui().page() == Page::GREET);
        tap(184, 380);
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
        assert(partialTop > Ui::MarketTutorUi::MENU_Y && partialTop < 328 && partialShift > greetingShift);
        shot("01b-opening-100ms");
        // Both the camera and panel pause while locked, even after a long gap.
        app.setDisplayLocked(true); advance(20);
        assert(ui().panelTop() == partialTop && ui().sceneShift(*app.market_, viewport) == partialShift);
        app.setDisplayLocked(false); advance(1);
        assert(ui().panelTop() < partialTop && ui().panelTop() > Ui::MarketTutorUi::MENU_Y);
        shot("01c-opening-200ms");
        const int standbyTop = ui().panelTop();
        app.setDisplayStandby(true, t); advance(40);
        assert(ui().panelTop() == standbyTop);
        app.setDisplayStandby(false, t); app.update(t);
        assert(ui().panelTop() == standbyTop); // Wake never catches up the slide.
        for (int n = 0; n < 5 && ui().animating(); ++n) advance(1);
        assert(!ui().animating() && ui().panelTop() == Ui::MarketTutorUi::MENU_Y && ui().hudOffset() == 0);
        assert(ui().sceneShift(*app.market_, viewport) > partialShift);
        shot("02-menu");
        // Currency uses the shop glyphs at 28px and keeps the stall palette.
        {
            const auto pixels = std::vector<uint16_t>(PixelRenderer::canvas().rawPixels(),
                PixelRenderer::canvas().rawPixels() + 368 * 448);
            ShopViewModel model{}; model.coins = state.coins;
            renderShopScreen(PixelRenderer::canvas(), model, 0, 64);
            const int w = UiCommon::textWidth("C1774") * 28 / 32;
            for (int y = 0; y < 28; ++y) for (int x = 0; x < w; ++x) {
                const int shopY = (UiMetrics::PAGE_HEADER_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
                const auto shop = PixelRenderer::canvas().readPixel(184 - UiCommon::textWidth("C1774") / 2 + x * 32 / 28, shopY + y * 32 / 28);
                const auto tutor = pixels[(18 + y) * 368 + 340 - w + x];
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
        tap(184, Ui::MarketTutorUi::MENU_Y); assert(ui().page() == Page::MENU && !ui().animating());
        drag(184, Ui::MarketTutorUi::MENU_Y - 3, 184, Ui::MarketTutorUi::MENU_Y + 3);
        drag(184, Ui::MarketTutorUi::MENU_Y + 3, 184, Ui::MarketTutorUi::MENU_Y - 3);
        drag(184, 100, 184, 244);
        assert(ui().page() == Page::MENU && !ui().animating());

        tap(184, 216 + 28); assert(ui().page() == Page::TUTOR); shot("03-tutor");
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 244);
        tap(98, 312); assert(ui().page() == Page::TUTOR); // Disabled left card cannot learn.
        // A single partner is centered; the empty right position is inert.
        const auto first = state.team[0];
        state.team[0] = state.team[1]; state.teamCount = 1;
        shot("03a-tutor-single");
        tap(300, 312); assert(ui().page() == Page::TUTOR);
        tap(184, 312); assert(ui().page() == Page::SLOT && ui().member() == 0);
        tap(184, 100); assert(ui().page() == Page::TUTOR);
        state.team[0] = first; state.teamCount = 2;
        tap(270, 312); assert(ui().page() == Page::SLOT && ui().member() == 1); shot("04-slot");
        tap(184, 100); assert(ui().page() == Page::TUTOR);
        tap(270, 312);
        tap(184, 216 + 28); assert(ui().page() == Page::SLOT); // Header space does not expose a basic-move slot.
        tap(184, 294); assert(ui().page() == Page::CONFIRM); shot("05-confirm");
        drag(272, 414, 184, 100); assert(ui().page() == Page::CONFIRM && state.coins == 1774);
        tap(184, 100); assert(ui().page() == Page::SLOT && state.coins == 1774 && !state.marketTutor.taught);
        tap(184, 300);
        // A failed save keeps coins, moves and the period untouched.
        app.saveWritesBlocked = true;
        tap(184, 414);
        assert(ui().page() == Page::CONFIRM && state.coins == 1774 && state.team[1].move2Id == 129 && !state.marketTutor.taught);
        shot("06-confirm-save-failed");
        app.saveWritesBlocked = false;
        tap(184, 414);
        assert(ui().page() != Page::RESULT && state.coins == 1374);
        shot("learn-00");
        tap(184,330); assert(ui().page()!=Page::RESULT);
        advance(3);
        app.setDisplayLocked(true);advance(30);app.setDisplayLocked(false);app.update(t);
        app.setDisplayStandby(true,t);advance(30);app.setDisplayStandby(false,t);app.update(t);
        assert(ui().page()!=Page::RESULT && state.coins==1374);
        for(int i=3;i<56;++i){
            char name[40];std::snprintf(name,sizeof(name),"learn-%02d",i);shot(name);
            assert(state.coins==1374 && state.team[1].move2Id==247 && state.marketTutor.taught==1);
            advance(1);
        }
        assert(ui().page()==Page::RESULT);
        assert(state.team[1].move2Id == 247 && state.team[1].moveProficiency[1] == 0 && state.marketTutor.taught == 1);
        shot("07-learned");
        {
            SaveManager manager; Game::GameState loaded; MainSceneViewState view;
            assert(manager.load(loaded, view));
            assert(loaded.coins == 1374 && loaded.team[1].move2Id == 247 && loaded.marketTutor.taught == 1);
            assert(loaded.marketTutor.periodDay == period.periodDay && loaded.marketTutor.move == 247);
        }
        leaveAndReopen(); assert(state.coins == 1374);
        tap(184, 216 + 28); assert(ui().page() == Page::MENU); // lesson sold this period
        shot("07b-menu-unavailable");

        // Training shares the horizontal partner cards and compact move rows.
        tap(184, 380); assert(ui().page() == Page::TRAIN_MEMBER); shot("08-train-member");
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 380);
        tap(98, 328); assert(ui().page() == Page::TRAIN_MOVE); shot("09-train-move");
        // Selecting a row stays here, and full slots cannot replace selection.
        tap(184, 294); assert(ui().page() == Page::TRAIN_MOVE);
        tap(184, 354); assert(ui().page() == Page::TRAIN_MOVE);
        assert(state.coins == 1374 && state.team[0].moveProficiency[1] == 64);
        app.saveWritesBlocked = true;
        tap(184, 414);
        assert(ui().page() == Page::TRAIN_MOVE && state.coins == 1374);
        assert(state.team[0].moveProficiency[1] == 64);
        shot("09a-train-save-failed");
        app.saveWritesBlocked = false;
        tap(184, 414);
        assert(ui().page() != Page::RESULT && state.team[0].moveProficiency[1] == 72 && state.coins == 1174);
        shot("10-training-start");
        tap(184, 330); assert(ui().page() != Page::RESULT); // Payment finger cannot skip.
        advance(3); shot("10a-training-demonstrate");
        app.setDisplayLocked(true); advance(30);
        app.setDisplayLocked(false); app.update(t);
        assert(ui().page() != Page::RESULT && state.coins == 1174);
        app.setDisplayStandby(true,t); advance(30); app.setDisplayStandby(false,t); app.update(t);
        assert(ui().page() != Page::RESULT && state.coins == 1174);
        for (int i = 3; i < 48; ++i) {
            char name[48]; std::snprintf(name,sizeof(name),"animation-%02d",i);
            shot(name);
            if(i==9) shot("10b-training-practice");
            if(i==18) shot("10c-training-finish");
            advance(1);
        }
        assert(ui().page() == Page::RESULT); shot("10d-trained");
        leaveAndReopen();
        tap(184, 380); tap(98, 328);
        assert(ui().page() == Page::TRAIN_MEMBER); // Already trained this period.
        shot("10e-train-member-used");
        tap(184, 100); assert(ui().page() == Page::MENU);


        // Recall: short lists are stationary; long lists reach every move.
        tap(184, 310); assert(ui().page() == Page::RECALL_MEMBER); shot("11a-recall-member");
        tap(184, 100); assert(ui().page() == Page::MENU);
        tap(184, 310);
        state.team[1].level = 1;
        Game::MoveId shortMoves[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
        const int shortCount = Game::MoveManagementService::collectRecallable(*findSpecies(133), state.team[1], shortMoves,
            Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
        assert(shortCount > 0 && shortCount <= 3);
        tap(270, 328); assert(ui().page() == Page::RECALL_LIST); shot("11-recall-list");
        assert(ui().scrollOffset() == 0);
        drag(184, 365, 184, 225);
        assert(ui().page() == Page::RECALL_LIST && ui().scrollOffset() == 0);
        drag(184, 260, 184, 100); assert(ui().page() == Page::RECALL_LIST);
        tap(184, 100); assert(ui().page() == Page::RECALL_MEMBER);
        state.team[1].level = 100;
        tap(270, 328); assert(ui().page() == Page::RECALL_LIST);
        Game::MoveId recallable[Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT];
        const int recallCount = Game::MoveManagementService::collectRecallable(*findSpecies(133), state.team[1], recallable,
            Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
        assert(recallCount > 4);
        const int maxScroll = (recallCount - 4) * 56;
        shot("11b-recall-long-top");
        TouchEvent slow{}; slow.type=TouchEventType::DOWN; slow.x=184; slow.y=365; slow.timestampMs=touchTime;
        app.handleTouch(slow); assert(ui().page()==Page::RECALL_LIST);
        advance(2); assert(ui().page()==Page::RECALL_LIST);
        slow.type=TouchEventType::MOVE; slow.y=345; slow.timestampMs+=200; app.handleTouch(slow);
        assert(ui().page()==Page::RECALL_LIST && ui().scrollOffset()==20);
        slow.type=TouchEventType::UP; slow.timestampMs+=100; app.handleTouch(slow);
        assert(ui().page()==Page::RECALL_LIST);
        drag(184,225,184,365); assert(ui().scrollOffset()==0);
        touchTime+=500;
        shot("11b-recall-long-top");
        const auto topPixels = std::vector<uint16_t>(PixelRenderer::canvas().rawPixels(),
            PixelRenderer::canvas().rawPixels() + 368 * 448);
        drag(184, 365, 184, 225); assert(ui().scrollOffset() == 140 && ui().page() == Page::RECALL_LIST);
        shot("11c-recall-long-scrolled");
        int changedPixels = 0;
        for (int y = 216; y < 440; ++y) for (int x = 10; x < 358; ++x)
            changedPixels += topPixels[y * 368 + x] != PixelRenderer::canvas().readPixel(x, y);
        assert(changedPixels > 0);
        drag(184, 240, 184, 285); assert(ui().scrollOffset() == 95);
        for (int i = 0; i < maxScroll / 140 + 2; ++i) drag(184, 365, 184, 225);
        assert(ui().scrollOffset() == maxScroll && ui().page() == Page::RECALL_LIST);
        shot("11d-recall-last");
        // Overscroll stays clamped, and dragging outside never activates back.
        drag(184, 365, 184, 100); assert(ui().scrollOffset() == maxScroll && ui().page() == Page::RECALL_LIST);
        TouchEvent scrollDown{}; scrollDown.type = TouchEventType::DOWN;
        scrollDown.x = 184; scrollDown.y = 300; scrollDown.timestampMs = touchTime;
        app.handleTouch(scrollDown); app.setDisplayLocked(true); app.setDisplayLocked(false);
        scrollDown.type = TouchEventType::UP; scrollDown.y = 350; scrollDown.timestampMs += 60;
        app.handleTouch(scrollDown); touchTime += 200;
        assert(ui().scrollOffset() == maxScroll && ui().page() == Page::RECALL_LIST);
        tap(184, 412); assert(ui().page() == Page::SLOT);
        tap(184, 100); assert(ui().page() == Page::RECALL_LIST && ui().scrollOffset() == maxScroll);
        for (int i = 0; i < maxScroll / 140 + 2; ++i) drag(184, 225, 184, 365);
        assert(ui().scrollOffset() == 0 && ui().page() == Page::RECALL_LIST);
        // Explicit back and re-entry resets the scroll position.
        drag(184, 365, 184, 225); assert(ui().scrollOffset() == 140);
        tap(184, 100); assert(ui().page() == Page::RECALL_MEMBER);
        tap(270, 328); assert(ui().page() == Page::RECALL_LIST && ui().scrollOffset() == 0);
        for (int i = 0; i < maxScroll / 140 + 2; ++i) drag(184, 365, 184, 225);
        tap(184, 412); assert(ui().page() == Page::SLOT);
        tap(184, 354); assert(ui().page() == Page::CONFIRM); shot("12-recall-confirm");
        tap(184, 100); assert(ui().page() == Page::SLOT && state.bag.heartScale == 1);
        tap(184, 354);
        tap(184, 414); assert(ui().page() == Page::RESULT && state.bag.heartScale == 0);
        // Verify the actual saved move is the last item, not its visible row index.
        assert(state.team[1].move3Id == recallable[recallCount - 1]);
        {
            SaveManager manager; Game::GameState loaded; MainSceneViewState view;
            assert(manager.load(loaded, view));
            assert(loaded.team[1].move3Id == recallable[recallCount - 1] && loaded.bag.heartScale == 0 && loaded.coins == 1174);
        }
        shot("12a-recalled");
        leaveAndReopen(); shot("13-menu-after");
        std::printf("recall-scroll: %d moves, top/bottom clamp, %d changed pixels, saved final move %u OK\n",
                    recallCount, changedPixels, unsigned(recallable[recallCount - 1]));

        // Lock cancels the gesture and keeps the page; leave resumes walking.
        TouchEvent down{}; down.type = TouchEventType::DOWN; down.x = 184; down.y = 100; down.timestampMs = touchTime;
        app.handleTouch(down); app.setDisplayLocked(true);
        TouchEvent up = down; up.type = TouchEventType::UP; up.timestampMs = touchTime + 50; app.handleTouch(up);
        app.setDisplayLocked(false); advance(3);
        assert(ui().page() == Page::MENU && app.market_->conversing());
        touchTime += 200;
        tap(184, 100); // Main-page outside tap closes with the panel animation.
        assert(ui().active() && ui().animating() && app.market_->conversing());
        const int openShift = ui().sceneShift(*app.market_, viewport);
        advance(1);
        assert(ui().panelTop() > Ui::MarketTutorUi::MENU_Y && ui().panelTop() < 448);
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
        std::puts("market-tutor-amoled: outside back on every subpage, animated close, tap-only greeting, uniform panel/camera, horizontal members, actual recall scrolling and final move, boundary drags, compact currency, lock pause and save rollback OK");
    }
};
}

// Public UI replay for prices, rank boundaries and one-shot performance.
static void trainingCases() {
    namespace UiCommon = AmoledV1::UiCommon;
    using UI = Ui::MarketTutorUi;
    using Type = Platform::IInputDevice::TouchType;
    auto& canvas = PixelRenderer::canvas();
    const auto centeredText = [&](const char* label, int y, int left, int right, uint16_t color, int size = 32) {
        const auto background = color == UiCommon::rgb(248,248,224) ? UiCommon::rgb(48,64,112) : UiCommon::rgb(248,248,224);
        std::vector<uint16_t> pixels(368 * 448, background);
        Canvas565 expected; expected.attach({pixels.data(),368,448,false}); expected.setNativeText(size == 32);
        PixelRenderer::text(expected, (left + right - UiCommon::textWidth(label) * size / 32) / 2, y, label, color);
        for (int yy=y; yy<y+size; ++yy) for(int x=left; x<right; ++x)
            if (canvas.readPixel(x, yy) != expected.readPixel(x, yy)) {
                std::fprintf(stderr,"text mismatch %s at %d,%d got %04x expected %04x native=%d scale=%d\n",label,x,yy,canvas.readPixel(x,yy),expected.readPixel(x,yy),canvas.nativeText(),canvas.renderScale());
                capture("failed-text"); std::abort();
            }
    };
    for (const int before : {0, 39, 40, 55, 60, 79, 80, 85, 90, 95, 99}) {
        auto state = Game::GameState{};
        state.teamCount = 1; state.coins = 3000;
        state.team[0] = Game::MonsterFactory::create(25, 18);
        state.team[0].ivPacked = 12; state.team[0].metAt = 24;
        state.team[0].move2Id = 486; state.team[0].moveProficiency[1] = uint8_t(before);
        state.team[0].move3Id = 98; state.team[0].moveProficiency[2] = 80;
        Game::MarketTutor::ensurePeriod(state);
        UI ui; uint32_t clock = 100;
        const auto tap = [&](int x, int y) {
            ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},state);
            const auto request = ui.touch({Type::UP,int16_t(x),int16_t(y),clock+40},state);
            clock += 200; return request;
        };
        ui.open(state); ui.update(250); ui.update(250); tap(184,380); ui.update(250); ui.update(250);
        tap(184,380); tap(184,328); assert(ui.page()==UI::Page::TRAIN_MOVE);
        ui.render(state);
        char label[24]; const unsigned price = before < 40 ? 100 : before < 80 ? 200 : 300;
        std::snprintf(label,sizeof(label),"特训%u",price);
        centeredText(label,398,16,352,UiCommon::rgb(248,248,224));
        if(before==0) capture("training-price-100");
        // Selecting another move changes the footer price and never trains it.
        assert(tap(184,354).kind==UI::Request::NONE); ui.render(state);
        centeredText("特训300",398,16,352,UiCommon::rgb(248,248,224));
        if(before==0) capture("training-price-300");
        assert(state.coins==3000);
        tap(184,294); ui.render(state); centeredText(label,398,16,352,UiCommon::rgb(248,248,224));
        const auto rich = state;
        state.coins = 0;
        assert(tap(184,414).kind==UI::Request::NONE && state.team[0].moveProficiency[1]==before);
        state = rich;
        auto request = tap(184,414);
        assert(request.kind==UI::Request::TRAIN && request.slot==1 && ui.pending());
        assert(tap(184,414).kind==UI::Request::NONE); // pending payment cannot repeat
        ui.committed(false,state); ui.render(state);
        centeredText("保存失败，未完成，请重试",230,24,344,UiCommon::rgb(183,56,56),16);
        request = tap(184,414); assert(request.kind==UI::Request::TRAIN);
        assert(Game::MarketTutor::train(state,0,request.slot)==Game::MarketTutor::Result::OK);
        assert(state.coins==3000-price);
        ui.committed(true,state); assert(ui.page()==UI::Page::TRAINING);
        ui.render(state); char name[64]; std::snprintf(name,sizeof(name),"training-rank-%02d-start",before); capture(name);
        tap(184,330); assert(ui.page()==UI::Page::TRAINING);
        ui.update(250); ui.update(150); // 400ms skip boundary
        tap(184,330); assert(ui.page()==UI::Page::RESULT && state.coins==3000-price);
        ui.render(state);
        const auto after = state.team[0].moveProficiency[1];
        const char* ending = before < 90 && after >= 90 ? "已经精通了！" :
            before < 60 && after >= 60 ? "已经熟练了！" : "用得更熟练了！";
        centeredText(ending,312,24,344,UiCommon::rgb(40,56,80));
        for(int y=350;y<380;++y) for(int x=24;x<344;++x)
            assert(canvas.readPixel(x,y)==UiCommon::rgb(248,248,224));
        centeredText("离开",398,16,352,UiCommon::rgb(248,248,224));
        std::snprintf(name,sizeof(name),"training-rank-%02d-result",before); capture(name);
        tap(184,414); ui.update(250); ui.update(250); ui.update(20); assert(!ui.active());
    }
    std::puts("training public UI: 11 boundary values, dynamic prices, insufficient funds, saved one-shot animation, 400ms skip, centered rank copy and exit OK");
}

static void learningSkipCase() {
    using UI=Ui::MarketTutorUi;
    using Type=Platform::IInputDevice::TouchType;
    Game::GameState state{};state.teamCount=1;state.coins=3000;
    state.team[0]=Game::MonsterFactory::create(133,18);
    state.team[0].move2Id=129;state.team[0].move3Id=44;
    Game::MarketTutor::ensurePeriod(state);
    state.marketTutor.move=247;state.marketTutor.drawnSpecies=133;
    UI ui;ui.open(state);ui.update(250);ui.update(250);uint32_t clock=0;
    const auto tap=[&](int x,int y){
        ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},state);
        auto result=ui.touch({Type::UP,int16_t(x),int16_t(y),clock+50},state);
        clock+=200;return result;
    };
    tap(184,380);ui.update(250);ui.update(250);tap(184,244);tap(184,328);tap(184,294);
    assert(ui.page()==UI::Page::CONFIRM);
    auto request=tap(184,414);assert(request.kind==UI::Request::TEACH);
    ui.committed(false,state);
    assert(ui.page()==UI::Page::CONFIRM && state.coins==3000 && state.team[0].move2Id==129);
    request=tap(184,414);assert(request.kind==UI::Request::TEACH);
    assert(Game::MarketTutor::teach(state,0,1)==Game::MarketTutor::Result::OK);
    ui.committed(true,state);assert(ui.page()==UI::Page::TEACHING);
    const auto coins=state.coins;assert(coins==2600 && state.team[0].move2Id==247);
    // An ignored DOWN before400ms cannot turn into a skip on later UP.
    ui.touch({Type::DOWN,184,330,clock},state);ui.update(250);ui.update(150);
    ui.touch({Type::UP,184,330,clock+500},state);clock+=600;
    assert(ui.page()==UI::Page::TEACHING);
    ui.touch({Type::DOWN,184,330,clock},state);ui.cancelTouch();
    ui.touch({Type::UP,184,330,clock+50},state);clock+=200;
    assert(ui.page()==UI::Page::TEACHING);
    tap(184,330);assert(ui.page()==UI::Page::RESULT);
    assert(state.coins==coins && state.team[0].move2Id==247 && state.marketTutor.taught==1);
    for(int i=0;i<70;++i)ui.update(100);
    assert(ui.page()==UI::Page::RESULT && state.coins==coins);
    tap(184,414);ui.update(250);ui.update(250);ui.update(20);assert(!ui.active());
    std::puts("learning performance: failed save stays confirmation, saved move before show,400ms gate, canceled gesture, one-shot skip and exit OK");
}

// Compare the production performance with independently drawn, directional
// market sprites. Ground anchors are a public presentation contract.
static void trainingFacingCases() {
    namespace Sprites = PokemonSprites;
    namespace Colors = AmoledV1::UiCommon;
    using UI = Ui::MarketTutorUi;
    using Type = Platform::IInputDevice::TouchType;
    auto& canvas = PixelRenderer::canvas();
    for (const uint16_t species : {25, 133, 143, 130}) {
        Game::GameState state{}; state.teamCount=1; state.coins=3000;
        state.team[0]=Game::MonsterFactory::create(species,30);
        state.team[0].move2Id=98; state.team[0].moveProficiency[1]=40;
        Game::MarketTutor::ensurePeriod(state);
        const uint16_t ids[]={94,species};
        assert(Sprites::preloadDynamicSpecies(ids,2));
        UI ui; ui.open(state); ui.update(250); ui.update(250);
        uint32_t clock=0;
        const auto tap=[&](int x,int y) {
            ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},state);
            auto result=ui.touch({Type::UP,int16_t(x),int16_t(y),clock+50},state);
            clock+=200;return result;
        };
        tap(184,380);ui.update(250);ui.update(250);tap(184,380);tap(184,328);
        assert(tap(184,414).kind==UI::Request::TRAIN);
        assert(Game::MarketTutor::train(state,0,1)==Game::MarketTutor::Result::OK);
        ui.committed(true,state);
        for(int ms=0;ms<4800;ms+=100) {
            ui.render(state);
            if(ms==0) for(int role=0;role<2;++role) {
                Sprites::WalkingAnimation animation{};
                const auto direction=role==0?Sprites::WalkDirection::RIGHT:Sprites::WalkDirection::LEFT;
                assert(Sprites::walkingAnimation(ids[role],direction,animation));
                const auto spriteTime=0;
                const auto kind=Sprites::SpriteKind(uint16_t(animation.base)+(spriteTime/150)%animation.frameCount);
                const auto* frame=Sprites::findSpeciesSprite(ids[role],kind);assert(frame);
                const float scale=std::min(2.0f,80.0f/std::max(frame->width,frame->height));
                const int footX=role==0?90:278;
                const int hop=0;
                const int footY=366+hop;
                std::vector<uint16_t> pixels(368*448,Colors::rgb(248,248,224));
                Canvas565 expected;expected.attach({pixels.data(),368,448,false});
                const Canvas565 savedCanvas=canvas; canvas=expected;
                assert(Sprites::drawFrameScaled(frame,int(footX-frame->width*scale/2),
                    int(footY-(Sprites::frameGroundOffsetY(frame)+frame->height/2.0f)*scale),
                    scale,animation.flipX,true));
                expected=canvas; canvas=savedCanvas;
                int checked=0;
                // Check sprite pixels above the floor and away from particles.
                for(int y=270;y<352;++y)for(int x=footX-40;x<footX+40;++x) {
                    const auto color=expected.readPixel(x,y);
                    if(color==Colors::rgb(248,248,224))continue;
                    const auto actual=canvas.readPixel(x,y);
                    if(actual==Colors::rgb(240,168,48) || actual==Colors::rgb(138,92,200))continue;
                    assert(actual==color);++checked;
                }
                assert(checked>100);
            }
            for(int y=390;y<430;++y)for(int x=24;x<344;++x)
                assert(canvas.readPixel(x,y)==Colors::rgb(248,248,224));
            char name[64];std::snprintf(name,sizeof(name),"facing-%03u-%02d",species,ms/100);capture(name);
            ui.update(100);
        }
        assert(ui.page()==UI::Page::RESULT);
    }
    std::puts("training facing: 4 species x 48 frames, initial directional pixels, target timeline and blank footer OK");
}

// Actual public UI route: selected move determines which approved script plays.
static void trainingTypeCases() {
    using UI=Ui::MarketTutorUi;
    using Type=Platform::IInputDevice::TouchType;
    namespace C=AmoledV1::UiCommon;
    struct Case { uint16_t move; const char* name; bool melee,self; uint16_t species=25; };
    const Case cases[]={{486,"remote",false,false},{98,"melee",true,false},
        {97,"self",false,true},{86,"enemy-status",false,true},
        {402,"seed-bomb",false,false},{479,"smack-down",false,false},
        {97,"self-eevee",false,true,133},{97,"self-snorlax",false,true,143},
        {97,"self-gyarados",false,true,130}};
    auto& canvas=PixelRenderer::canvas();
    const auto yellowPixels=[&](int x0,int x1){
        int count=0;
        for(int y=290;y<371;++y)for(int x=x0;x<x1;++x){
            const auto c=canvas.readPixel(x,y);
            count+= (c>>11)>=23 && ((c>>5)&63)>=35 && (c&31)<=12;
        }
        return count;
    };
    for(const auto& test:cases){
        Game::GameState state{};state.teamCount=1;state.coins=3000;
        state.team[0]=Game::MonsterFactory::create(test.species,30);
        state.team[0].ivPacked=12;state.team[0].metAt=24;
        state.team[0].move2Id=test.move;state.team[0].moveProficiency[1]=40;
        Game::MarketTutor::ensurePeriod(state);
        const uint16_t ids[]={94,test.species};assert(PokemonSprites::preloadDynamicSpecies(ids,2));
        UI ui;ui.open(state);ui.update(250);ui.update(250);uint32_t clock=0;
        const auto tap=[&](int x,int y){
            ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},state);
            auto req=ui.touch({Type::UP,int16_t(x),int16_t(y),clock+50},state);
            clock+=200;return req;
        };
        tap(184,380);ui.update(250);ui.update(250);tap(184,380);tap(184,328);
        auto req=tap(184,414);assert(req.kind==UI::Request::TRAIN && req.slot==1);
        assert(Game::MarketTutor::train(state,0,req.slot)==Game::MarketTutor::Result::OK);
        ui.committed(true,state);assert(ui.page()==UI::Page::TRAINING);
        int changedFrames=0;std::vector<uint16_t> previous;
        for(int ms=0;ms<4800;ms+=100){
            ui.render(state);
            if(ms==1200){
                if(test.species==25){
                    if(test.melee){assert(yellowPixels(180,235)>80);assert(yellowPixels(255,315)==0);}
                    else {assert(yellowPixels(255,315)>80);}
                }
                if(test.self){
                    for(int y=304;y<360;++y)for(int x=150;x<205;++x)assert(canvas.readPixel(x,y)==C::rgb(248,248,224));
                } else { assert(canvas.readPixel(test.melee?180:178,330)!=C::rgb(248,248,224)); }
            }
            for(int y=392;y<430;++y)for(int x=24;x<344;++x)assert(canvas.readPixel(x,y)==C::rgb(248,248,224));
            std::vector<uint16_t> pixels(canvas.rawPixels(),canvas.rawPixels()+368*448);
            changedFrames+=!previous.empty() && pixels!=previous;previous=std::move(pixels);
            char file[80];std::snprintf(file,sizeof(file),"type-%s-%02d",test.name,ms/100);capture(file);
            assert(state.coins==2800 && state.team[0].moveProficiency[1]==48);
            ui.update(100);
        }
        assert(ui.page()==UI::Page::RESULT && changedFrames>25);
        tap(184,414);ui.update(250);ui.update(250);ui.update(20);assert(!ui.active());
    }
    std::puts("training types: real UI remote/melee/self/enemy-status plus ranged overrides, complete timelines, one payment and direct exits OK");
}

// Learning follows the chosen move as well, using actual eligible lessons.
static void learningTypeCases() {
    using UI=Ui::MarketTutorUi;
    using Type=Platform::IInputDevice::TouchType;
    namespace C=AmoledV1::UiCommon;
    struct Case { uint16_t move; const char* name; bool melee,self; uint16_t species=25; };
    const Case cases[]={{85,"remote",false,false},{98,"melee",true,false},
                        {97,"self",false,true},{86,"enemy-status",false,true},
        {104,"self-eevee",false,true,133},{111,"self-snorlax",false,true,143},
        {349,"self-gyarados",false,true,130}};
    auto& canvas=PixelRenderer::canvas();
    for(const auto& test:cases){
        Game::GameState state{};state.teamCount=1;state.coins=3000;
        state.team[0]=Game::MonsterFactory::create(test.species,1);
        state.team[0].ivPacked=12;state.team[0].metAt=24;
        Game::MarketTutor::ensurePeriod(state);
        state.marketTutor.move=test.move;state.marketTutor.drawnSpecies=test.species;
        assert(Game::MarketTutor::eligibility(state.team[0],test.move)==Game::MarketTutor::Eligibility::CAN_LEARN);
        const auto old=state.team[0].move2Id;
        const uint16_t ids[]={94,test.species};assert(PokemonSprites::preloadDynamicSpecies(ids,2));
        UI ui;ui.open(state);ui.update(250);ui.update(250);uint32_t clock=0;
        const auto tap=[&](int x,int y){
            ui.touch({Type::DOWN,int16_t(x),int16_t(y),clock},state);
            auto req=ui.touch({Type::UP,int16_t(x),int16_t(y),clock+50},state);clock+=200;return req;
        };
        tap(184,380);ui.update(250);ui.update(250);tap(184,244);tap(184,328);tap(184,294);
        assert(tap(184,414).kind==UI::Request::TEACH);
        ui.committed(false,state);
        assert(ui.page()==UI::Page::CONFIRM && state.coins==3000 && state.team[0].move2Id==old);
        assert(tap(184,414).kind==UI::Request::TEACH);
        assert(Game::MarketTutor::teach(state,0,1)==Game::MarketTutor::Result::OK);
        const auto paid=state.coins;
        assert(paid==3000-Game::MarketTutor::lessonPrice(test.move));
        ui.committed(true,state);assert(ui.page()==UI::Page::TEACHING);
        int changed=0;std::vector<uint16_t> previous;
        for(int ms=0;ms<5600;ms+=100){
            ui.render(state);
            if(ms==800){
                int originInk=0;
                for(int y=300;y<360;++y)for(int x=65;x<115;++x)originInk+=canvas.readPixel(x,y)!=C::rgb(248,248,224);
                if(test.melee)assert(originInk==0);else assert(originInk>100);
                if(test.self){for(int y=304;y<360;++y)for(int x=150;x<205;++x)assert(canvas.readPixel(x,y)==C::rgb(248,248,224));}
                else assert(canvas.readPixel(178,330)!=C::rgb(248,248,224));
            }
            for(int y=392;y<430;++y)for(int x=24;x<344;++x)assert(canvas.readPixel(x,y)==C::rgb(248,248,224));
            std::vector<uint16_t> pixels(canvas.rawPixels(),canvas.rawPixels()+368*448);
            changed+=!previous.empty()&&pixels!=previous;previous=std::move(pixels);
            char file[80];std::snprintf(file,sizeof(file),"learning-%s-%02d",test.name,ms/100);capture(file);
            assert(state.coins==paid&&state.team[0].move2Id==test.move&&state.marketTutor.taught==1);
            ui.update(100);
        }
        assert(ui.page()==UI::Page::RESULT&&changed>35);
        tap(184,414);ui.update(250);ui.update(250);ui.update(20);assert(!ui.active());
    }
    std::puts("learning types: eligible remote/melee/self/enemy-status lessons, teacher movement, rollback, complete timings, one payment and exit OK");
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    gOutput = argc == 3 ? argv[2] : nullptr;
    DesktopPlatform desktop(argv[1]); Platform::bind(desktop.serviceBundle()); assert(desktop.begin());
    assert(Platform::resources().mount()); assert(GameAssets::begin()); FontResource::ins().begin();
    std::vector<uint16_t> pixels(368 * 448); auto& c = PixelRenderer::canvas(); c.attach({pixels.data(), 368, 448, false});
    c.setCoordinateScale(1); c.setLayoutScale(1); c.setAssetScale(1); c.setNativeText(true);
    AmoledV1::AmoledApp app; app.begin(0); AmoledV1::AmoledAppTestAccess::run(app);
    trainingCases();
    trainingFacingCases();
    learningSkipCase();
    trainingTypeCases();
    learningTypeCases();
}
