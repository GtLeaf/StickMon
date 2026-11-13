#include <cassert>
#include <cstdio>
#include <string>
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "core/features/BagFeature.h"
#include "game/MonsterFactory.h"
#include "game/MarketItemCatalog.h"
#include "ui/common/BagUi.h"
#include "scenes/MenuScene.h"

static bool apply(Game::ItemId item, uint8_t slot) {
    return GameEngine::ins().useTeamItem(item, slot) == Game::ItemInventory::UseResult::USED;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]); Platform::bind(desktop.serviceBundle()); assert(desktop.begin());
    auto& engine = GameEngine::ins(); assert(engine.begin());
    auto& state = engine.gameState(); state = {}; state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(25, 30);
    for (const auto& entry : Game::MarketItemCatalog::ITEMS) {
        assert(engine.addItem(entry.id, 2));
        assert(engine.itemCount(entry.id) == 2);
        assert(engine.removeItem(entry.id));
        assert(engine.itemCount(entry.id) == 1);
        const uint8_t source = Features::Bag::SOURCE_MARKET_BASE + uint8_t(entry.id) - uint8_t(Game::ItemId::HP_UP);
        auto& mon = state.team[0];
        mon.hpCur = mon.hpMax; mon.fainted = false;
        mon.majorStatus = Game::MajorStatus::NONE;
        if (entry.id == Game::ItemId::MAX_REVIVE) { mon.hpCur = 0; mon.fainted = true; }
        if (entry.id == Game::ItemId::ORAN_BERRY || entry.id == Game::ItemId::SITRUS_BERRY) mon.hpCur = 1;
        if (entry.id == Game::ItemId::LUM_BERRY || entry.id == Game::ItemId::LAVA_COOKIE) mon.majorStatus = Game::MajorStatus::BURN;
        Features::Bag::Context context{Game::ItemInventory::usableInBattle(entry.id), 0, nullptr};
        const auto result = Features::Bag::use(state, source, context, apply);
        assert(result.used && result.item == entry.id && result.notice == Features::Bag::Notice::USED_ITEM);
        assert(engine.itemCount(entry.id) == 0);
        char notice[80];
        assert(BagUi::notice(result, notice, sizeof(notice)) == notice);
        assert(std::string(notice).find(entry.name) != std::string::npos);
    }
#if !STICKMON_UI_COMPACT_TOUCH
    for (int id = 0; id < int(Game::ItemId::COUNT); ++id) {
        auto item = static_cast<Game::ItemId>(id);
        auto count = engine.itemCount(item);
        if (count) assert(engine.removeItem(item, count));
    }
    // The real Stick menu callback has its own dispatch; reaching only
    // ItemInventory would miss a blocked purchase in this production route.
    MenuScene menu; menu.onEnter(); menu.openExploreBagView();
    assert(engine.addItem(Game::ItemId::PROTEIN));
    const auto beforeAtk = state.team[0].ev.atk;
    menu.onButton({0, BtnAction::PRESSED}); menu.onButton({0, BtnAction::PRESSED});
    assert(state.team[0].ev.atk == beforeAtk + 10 && engine.itemCount(Game::ItemId::PROTEIN) == 0);
    assert(engine.addItem(Game::ItemId::MAX_REVIVE));
    state.team[0].hpCur = 0; state.team[0].fainted = true;
    menu.openBattleBagView("wild", 0);
    menu.onButton({0, BtnAction::PRESSED}); menu.onButton({0, BtnAction::PRESSED});
    assert(state.team[0].hpCur == state.team[0].hpMax && !state.team[0].fainted);
    assert(engine.itemCount(Game::ItemId::MAX_REVIVE) == 0);
    assert(menu.consumeBattleBagResult() == Features::Bag::BattleResult::MARKET_ITEM);
#endif
    std::puts("market-bag-engine: all17 add/count/remove/use, actual effect, per-item notice OK");
}
