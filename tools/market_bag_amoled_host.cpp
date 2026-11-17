#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MarketItemCatalog.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include "ui/ItemShopScreens.h"
#include "ui/UiMetrics.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const char* output = nullptr;
static void capture(const char* name) {
    if (!output) return;
    const auto path = std::string(output) + "/" + name + ".ppm";
    auto* file = std::fopen(path.c_str(), "wb"); assert(file);
    std::fprintf(file, "P6\n368 448\n255\n");
    for (int y = 0; y < 448; ++y) for (int x = 0; x < 368; ++x) {
        const auto c = PixelRenderer::canvas().readPixel(x, y);
        const unsigned char rgb[] = {uint8_t(((c >> 11) & 31) * 255 / 31),
            uint8_t(((c >> 5) & 63) * 255 / 63), uint8_t((c & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    assert(std::fclose(file) == 0);
}

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app) {
        using Scene = AppSceneFlow::Scene;
        auto& state = app.gameState;
        state = {}; state.oobeDone = true; state.teamCount = 2;
        state.team[0] = Game::MonsterFactory::create(25, 30);
        state.team[1] = Game::MonsterFactory::create(133, 30);
        // Keep all legacy recovery stacks, so new items really lie beyond
        // the former first-four list and require the normal scrolling bag.
        for (int id = 0; id < int(Game::ItemId::COUNT); ++id)
            Game::ItemInventory::add(state, static_cast<Game::ItemId>(id), 2);
        app.syncHomeActors(0);
        uint32_t time = 1000;
        const auto tap = [&](int x, int y) {
            TouchEvent e{}; e.x = x; e.y = y; e.timestampMs = time;
            e.type = TouchEventType::DOWN; app.handleTouch(e);
            e.type = TouchEventType::UP; e.timestampMs = time + 50; app.handleTouch(e);
            time += 200;
        };
        const auto choose = [&](Game::ItemId item) {
            auto& inventory = app.inventoryController_;
            const auto model = inventory.bagModel(state);
            int row = -1;
            for (uint8_t i = 0; i < model.itemCount; ++i)
                if (inventory.itemAt(state, Scene::BAG, i) == item) row = i;
            assert(row >= 0);
            const float maximum = itemListMaxScroll(model.dailyItemCount, model.exploreItemCount, model.itemCount, false);
            for (int scroll = 0; scroll <= int(maximum) + 96; scroll += 48) {
                inventory.stopScroll();
                inventory.dragList(-48, maximum);
                for (int y = UiMetrics::PAGE_HEADER_HEIGHT + 20; y < 428; y += 8)
                    for (int x : {64, 176, 288})
                        if (itemListItemAt(x, y, inventory.state().itemScroll,
                            model.dailyItemCount, model.exploreItemCount, model.itemCount, false) == row) {
                            tap(x, y);
                            assert(inventory.state().pendingItem == item && inventory.state().itemConfirmOpen);
                            return;
                        }
            }
            assert(false && "purchased item must be reachable by scrolling");
        };
        // A read-only detail must preserve the bag's original return target,
        // including after repeated visits and taps where Use would normally be.
        for (const auto parent : {Scene::MAIN_MENU, Scene::EXPLORE_MENU, Scene::HOME}) {
            if (parent == Scene::MAIN_MENU) app.sceneFlow.openMenu(Scene::HOME);
            else app.sceneFlow.enter(parent);
            app.openItemScene(Scene::BAG);
            const auto before = state;
            for (int visit = 0; visit < 2; ++visit) {
                choose(Game::ItemId::HEART_SCALE);
                auto& inventory = app.inventoryController_;
                const auto scroll = inventory.state().itemScroll;
                assert(app.sceneFlow.current() == Scene::BAG);
                assert(app.sceneFlow.subSceneReturn() == parent);
                assert(!inventory.canConfirm(state));
                assert(!inventory.state().selectingItemTarget);
                app.forceFullRender(); app.render(PixelRenderer::canvas());
                capture("heart-scale-readonly");
                tap(UiMetrics::DETAIL_ACTION_RECT.x + 20, UiMetrics::DETAIL_ACTION_RECT.y + 20);
                assert(app.sceneFlow.current() == Scene::BAG && inventory.state().itemConfirmOpen);
                assert(!inventory.state().selectingItemTarget);
                assert(inventory.state().pressedShopDetailAction == -1);
                assert(app.sceneFlow.subSceneReturn() == parent);
                assert(std::memcmp(&before, &state, sizeof(state)) == 0);
                tap(UiMetrics::DETAIL_BACK_RECT.x + 20, UiMetrics::DETAIL_BACK_RECT.y + 20);
                assert(!inventory.state().itemConfirmOpen);
                assert(inventory.state().itemScroll == scroll);
            }
            tap(30, 25);
            assert(app.sceneFlow.current() == parent);
            assert(std::memcmp(&before, &state, sizeof(state)) == 0);
            if (parent == Scene::MAIN_MENU) {
                app.menuScroll = mainMenuMaxScroll();
                const int back = AppSceneFlow::mainMenuItemCount(STICKMON_ENABLE_DEBUG_FEATURES != 0) - 1;
                bool found = false;
                for (int y = 0; y < 448 && !found; y += 8)
                    for (int x = 0; x < 368 && !found; x += 8)
                        if (mainMenuItemAt(x, y, app.menuScroll) == back) {
                            tap(x, y); found = true;
                        }
                assert(found && app.sceneFlow.current() == Scene::HOME);
            }
        }
        for (char32_t cp : U"美丽的鳞片，在爱好者中有着相当高的人气。") {
            if (!cp) continue;
            assert(FontResource::ins().findGlyphBitmap(cp, FontFace::SARASA_CJK));
            assert(FontResource::ins().findLargeGlyphBitmap(cp, FontFace::SARASA_CJK));
        }
        std::puts("heart-scale-bag: read-only detail, inert Use region, unchanged state/scroll and all three return targets OK");
        // Every cultivation purchase can be selected, confirmed and used
        // through the actual home bag and target screen.
        for (const auto& entry : Game::MarketItemCatalog::ITEMS) {
            if (Game::ItemInventory::usableInBattle(entry.id)) continue;
            app.sceneFlow.enter(Scene::HOME); app.openItemScene(Scene::BAG);
            choose(entry.id);
            tap(UiMetrics::DETAIL_ACTION_RECT.x + 20, UiMetrics::DETAIL_ACTION_RECT.y + 20);
            assert(app.inventoryController_.state().selectingItemTarget);
            const auto stock = Game::ItemInventory::count(state, entry.id);
            tap(100, UiMetrics::PAGE_HEADER_HEIGHT + 40);
            assert(Game::ItemInventory::count(state, entry.id) == stock - 1);
            assert(app.inventoryController_.state().teamItemFeedbackActive);
            app.finishTeamItemUse(time);
        }
        for (const auto& entry : Game::MarketItemCatalog::ITEMS) {
            if (!Game::ItemInventory::usableInBattle(entry.id)) continue;
            auto& target = state.team[1];
            target.hpCur = entry.id == Game::ItemId::MAX_REVIVE ? 0 : 1;
            target.fainted = entry.id == Game::ItemId::MAX_REVIVE;
            target.majorStatus = Game::MajorStatus::BURN;
            app.sceneFlow.enter(Scene::BATTLE); app.performBattleBag(time);
            assert(app.sceneFlow.current() == Scene::BAG && app.inventoryController_.state().battleBagMode);
            choose(entry.id);
            tap(UiMetrics::DETAIL_ACTION_RECT.x + 20, UiMetrics::DETAIL_ACTION_RECT.y + 20);
            assert(app.inventoryController_.state().selectingItemTarget);
            const auto stock = Game::ItemInventory::count(state, entry.id);
            tap(100, UiMetrics::PAGE_HEADER_HEIGHT + 40 + 152);
            assert(Game::ItemInventory::count(state, entry.id) == stock - 1);
            assert(app.inventoryController_.state().teamItemFeedbackActive);
            app.finishTeamItemUse(time);
            assert(app.sceneFlow.current() == Scene::BATTLE);
            assert(app.battleSession_.state().battleContinuation == BattleSession::BattleContinuation::WILD_TURN);
            assert(target.hpCur > 0 && !target.fainted);
        }
        // Battle context rejects cultivation even though it is still stocked.
        app.sceneFlow.enter(Scene::BATTLE); app.performBattleBag(time);
        auto& inventory = app.inventoryController_;
        for (uint8_t i = 0; i < inventory.itemCount(state, Scene::BAG); ++i)
            if (inventory.itemAt(state, Scene::BAG, i) == Game::ItemId::HP_UP)
                assert(inventory.selectItem(state, Scene::BAG, i) == InventoryController::Selection::CANNOT_USE);
        std::puts("market-bag-amoled: 12 home purchases +5 battle recoveries selected by scroll, target selection, stock consumption and battle reply OK");
    }
};
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    if (argc == 3) output = argv[2];
    DesktopPlatform desktop(argv[1]); Platform::bind(desktop.serviceBundle()); assert(desktop.begin());
    assert(Platform::resources().mount()); assert(GameAssets::begin()); FontResource::ins().begin();
    std::vector<uint16_t> pixels(368 * 448); auto& canvas = PixelRenderer::canvas();
    canvas.attach({pixels.data(), 368, 448, false}); canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1); canvas.setAssetScale(1); canvas.setNativeText(true);
    AmoledV1::AmoledApp app; app.begin(0); AmoledV1::AmoledAppTestAccess::run(app);
}
