#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "platform/desktop/DesktopPlatform.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "presentation/PixelRenderer.h"
#define private public
#define protected public
#include "hardware/Hal.h"
#include "core/GameEngine.h"
#include "core/CryPlayer.h"
#include "core/ProgressionUi.h"
#include "scenes/MenuScene.h"
#include "scenes/MainScene.h"
#include "scenes/ExploreScene.h"
#undef protected
#undef private

#if STICKMON_FINAL_UI_BASELINE
#define DEBUG_UI(scene) (scene)
using DebugPage = MenuScene;
#else
#define DEBUG_UI(scene) ((scene).debugUi)
#if STICKMON_ENABLE_DEBUG_FEATURES
using DebugPage = Ui::DebugUiBase;
#endif
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
    const auto bundle = desktop.serviceBundle();
    TouchInput input;
    Platform::Services services{bundle.lifecycle, bundle.clock, bundle.logger,
        input, bundle.display, bundle.audio, bundle.imu, bundle.power,
        bundle.blobs, bundle.resources, bundle.memory, bundle.peers};
    Platform::bind(services);
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& game = engine.gameState();
    std::unique_ptr<MenuScene> menu;
    unsigned checks = 0;
    const ButtonEvent a{0, BtnAction::PRESSED}, b{1, BtnAction::PRESSED};
    const ButtonEvent longA{0, BtnAction::LONG_PRESS}, longB{1, BtnAction::LONG_PRESS};
    auto setup = [&]() -> MenuScene& {
        menu.reset();
        engine.currentScene.reset();
        CryPlayer::ins().stop();
        CryPlayer::ins().update();
        ProgressionUi::resetEvolutionAnimation();
        engine.contactVisit = {};
        engine.exploreTravel = ExploreTravelPhase::NONE;
        engine.sceneFade = GameEngine::SceneFadePhase::NONE;
        engine.evolutionEventHead = engine.evolutionEventCount = 0;
        engine.moveReplacementEventHead = engine.moveReplacementEventCount = 0;
        engine.debugBattleRequested = engine.debugMenuReturnRequested = false;
        engine.debugPairInteractionRequested = false;
        engine.currentId = SceneID::MENU;
        engine.prevId = SceneID::MAIN;
        engine.clearMainSceneViewState();
        GameRandom::seed(1234);
        game = Game::GameState{};
        game.oobeDone = true;
        game.coins = 0;
        game.tutorialFlags = 0xff;
        game.teamCount = 1;
        game.team[0] = Game::MonsterFactory::create(1, 50);
        game.team[0].move2Id = 45;
        game.team[0].move3Id = 22;
        game.team[0].moveProficiency[0] = 100;
        game.team[0].moveProficiency[1] = 37;
        game.team[0].moveProficiency[2] = 92;
        game.gameMinutesTotal = 1147;
        engine.gameClock.set(desktop.millis(), 1147);
        engine.saveCoordinator.reset(desktop.millis());
        menu = std::make_unique<MenuScene>();
        menu->onEnter();
        return *menu;
    };
    auto pixels = [&](const std::string& name) {
        const auto fb = desktop.frameBuffer();
        FILE* file = std::fopen((std::string(argv[2]) + "/" + name + ".rgb565").c_str(), "wb");
        assert(file);
        assert(std::fwrite(fb.pixels, sizeof(uint16_t), fb.width * fb.height, file) == size_t(fb.width) * fb.height);
        std::fclose(file);
    };
    auto pending = [&](bool full = true) {
        setup();
        game.pendingMoveLearn = true;
        game.pendingMoveSlot = 0;
        game.pendingMoveId = 402;
        game.pendingMoveCursor = 65535;
        if (!full) game.team[0].move3Id = 0;
    };
    ProgressionUi::MoveLearnState state;
    for (int stage = 0; stage < 2; ++stage)
        for (int slot = 0; slot <= 4; ++slot)
            for (int confirm = 0; confirm < 3; ++confirm)
                for (int age : {0, 4000}) {
                    pending(); desktop.advanceMs(uint32_t(5000 + age) - desktop.millis());
                    state = {}; state.stage = ProgressionUi::MoveLearnStage(stage);
                    state.promptCursor = slot % 2; state.replacementSlot = slot;
                    state.confirmOpen = confirm > 0; state.confirmYes = confirm == 2;
                    PixelRenderer::clear(0); ProgressionUi::renderMoveLearn(state);
                    pixels("learn-" + std::to_string(stage) + "-slot-" + std::to_string(slot) + "-confirm-" + std::to_string(confirm) + "-age-" + std::to_string(age));
                }
    for (int kind = 0; kind < 5; ++kind) {
        pending(false);
        if (kind == 0) game.team[0].move2Id = 0;
        if (kind == 1) game.pendingMoveSlot = Game::TEAM_CAP;
        if (kind == 2) game.pendingMoveId = 65535;
        if (kind == 3) game.teamCount = 0;
        if (kind == 4) game.team[0].speciesId = 65535;
        state = {}; PixelRenderer::clear(0); ProgressionUi::renderMoveLearn(state);
        pixels("learn-edge-" + std::to_string(kind));
        state.stage = ProgressionUi::MoveLearnStage::REPLACEMENT;
        ProgressionUi::renderMoveLearn(state); pixels("replacement-edge-" + std::to_string(kind));
    }
    for (int level : {1, 50, 100}) for (const char* name : {static_cast<const char*>(nullptr), "Bulbasaur", "A VERY LONG SPECIES NAME"}) {
        PixelRenderer::clear(0x39e7); ProgressionUi::renderLevelUp(name, level);
        pixels("level-" + std::to_string(level) + "-name-" + std::to_string(name ? std::strlen(name) : 0));
    }
    for (int oldId : {0, 33, 45, 65535}) {
        setup(); engine.queueMoveReplacementEvent(0, oldId, 402);
        PixelRenderer::clear(0); ProgressionUi::renderMoveReplacement(); pixels("replaced-" + std::to_string(oldId));
    }
    for (uint32_t age : {0u, 1u, 599u, 600u, 879u, 880u, 2100u, 2839u, 2840u, 3009u, 3010u, 3179u, 3180u, 3579u, 3580u, 3999u, 4000u, 6000u}) {
        setup(); ProgressionUi::evolutionAnimationComplete(1, 2, 10000);
        PixelRenderer::clear(0); ProgressionUi::renderEvolution(1, 2, 10000 + age);
        pixels("evolution-" + std::to_string(age));
        assert(ProgressionUi::evolutionAnimationComplete(1, 2, 10000 + age) == (age >= 4000));
    }
    for (uint32_t age : {0u, 89u, 90u, 419u, 420u, 649u, 650u, 999u, 1000u, 2000u}) {
        setup(); ProgressionUi::beginEvolutionCancellation(1, 2, 12000);
        PixelRenderer::clear(0); ProgressionUi::renderEvolutionCancelled(1, 12000 + age);
        pixels("evolution-cancel-" + std::to_string(age));
        assert(ProgressionUi::evolutionCancellationComplete(12000 + age) == (age >= 1000));
    }
    setup(); assert(ProgressionUi::evolutionCancellationComplete(0));
    assert(!ProgressionUi::evolutionAnimationComplete(1, 2, 0xffffff00u));
    assert(ProgressionUi::evolutionAnimationComplete(1, 2, 0xffffff00u + 4000u));
    assert(!ProgressionUi::evolutionAnimationComplete(4, 5, 6000)); ++checks;
    setup(); ProgressionUi::evolutionAnimationComplete(1, 2, 0);
    ProgressionUi::renderEvolution(1, 2, 3579); assert(CryPlayer::ins().pcm_ == nullptr);
    ProgressionUi::renderEvolution(1, 2, 3580);
    const auto* pcm = CryPlayer::ins().pcm_;
    assert(pcm && CryPlayer::ins().speciesId_ == 2);
    ProgressionUi::renderEvolution(1, 2, 4000); assert(CryPlayer::ins().pcm_ == pcm);
    ProgressionUi::beginEvolutionCancellation(1, 2, 4100); CryPlayer::ins().update(); assert(CryPlayer::ins().pcm_ == nullptr); ++checks;
    pending(false); ProgressionUi::resetMoveLearnState(state);
    assert(state.replacementSlot == 1 && !state.confirmYes);
    assert(!ProgressionUi::handleMoveLearnInput(state, 2));
    assert(ProgressionUi::handleMoveLearnInput(state, 0));
    assert(game.team[0].move3Id == 402 && game.team[0].moveProficiency[2] == 0 && !game.pendingMoveLearn); ++checks;
    pending(); const auto old = game.team[0]; state = {};
    assert(!ProgressionUi::handleMoveLearnInput(state, 1));
    assert(ProgressionUi::handleMoveLearnInput(state, 0));
    assert(std::memcmp(&old, &game.team[0], sizeof(old)) == 0 && !game.pendingMoveLearn); ++checks;
    for (int slot : {1, 2, 3}) {
        pending(); const auto before = game.team[0]; state = {};
        assert(!ProgressionUi::handleMoveLearnInput(state, 0));
        assert(state.stage == ProgressionUi::MoveLearnStage::REPLACEMENT && state.replacementSlot == 1);
        for (int i = 1; i < slot; ++i) assert(!ProgressionUi::handleMoveLearnInput(state, 1));
        assert(!ProgressionUi::handleMoveLearnInput(state, 0) && state.confirmOpen && !state.confirmYes);
        assert(!ProgressionUi::handleMoveLearnInput(state, 0) && !state.confirmOpen);
        assert(!ProgressionUi::handleMoveLearnInput(state, 0));
        assert(!ProgressionUi::handleMoveLearnInput(state, 1) && state.confirmYes);
        assert(ProgressionUi::handleMoveLearnInput(state, 0));
        if (slot == 3) assert(std::memcmp(&before, &game.team[0], sizeof(before)) == 0);
        else {
            assert((slot == 1 ? game.team[0].move2Id : game.team[0].move3Id) == 402);
            assert(game.team[0].moveProficiency[slot] == 0);
            assert(game.team[0].moveProficiency[3 - slot] == before.moveProficiency[3 - slot]);
            assert(engine.hasPendingMoveReplacement() && engine.pendingMoveReplacementNewId() == 402);
        }
        ++checks;
    }
    pending(); state = {}; state.stage = ProgressionUi::MoveLearnStage::REPLACEMENT;
    state.confirmOpen = state.confirmYes = true; state.replacementSlot = 0;
    assert(!ProgressionUi::handleMoveLearnInput(state, 0));
    assert(game.pendingMoveLearn && !state.confirmOpen && !state.confirmYes); ++checks;
    pending(); state = {}; state.stage = ProgressionUi::MoveLearnStage::REPLACEMENT; state.replacementSlot = 3;
    assert(!ProgressionUi::handleMoveLearnInput(state, 1) && state.replacementSlot == 1); ++checks;
    setup(); state = {}; assert(ProgressionUi::handleMoveLearnInput(state, 0)); ++checks;

#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_FINAL_UI_BASELINE
    // Exercise both real consumers, including replacement event continuation.
    using LearnTouch = Platform::IInputDevice::TouchType;
    std::unique_ptr<Scene> learner;
    auto prepareLearner = [&](bool explore, bool full) -> Scene& {
        learner.reset();
        pending(full);
        if (explore) {
            auto page = std::make_unique<ExploreScene>();
            page->onEnter();
            page->phase = ExploreScene::Phase::LEARN_MOVE;
            page->progressionReturnPhase = ExploreScene::Phase::SELECT;
            ProgressionUi::resetMoveLearnState(page->moveLearnState);
            learner = std::move(page);
        } else {
            auto page = std::make_unique<MainScene>();
            page->onEnter();
            page->progressionModal = MainScene::ProgressionModal::LEARN_MOVE;
            ProgressionUi::resetMoveLearnState(page->progressionMoveLearn);
            learner = std::move(page);
        }
        return *learner;
    };
    auto learnState = [&](bool explore) -> ProgressionUi::MoveLearnState& {
        return explore ? static_cast<ExploreScene&>(*learner).moveLearnState
                       : static_cast<MainScene&>(*learner).progressionMoveLearn;
    };
    auto learnTouch = [&](LearnTouch type, int x, int y) {
        desktop.advanceMs(10);
        return learner->onTouch({type, int16_t(x), int16_t(y), desktop.millis()});
    };
    auto learnTap = [&](int x, int y) {
        learnTouch(LearnTouch::DOWN, x, y);
        return learnTouch(LearnTouch::UP, x, y);
    };
    for (bool explore : {false, true}) {
        prepareLearner(explore, false);
        learnTap(92, 164);
        assert(!game.pendingMoveLearn && game.team[0].move3Id == 402);
        // Repeated release cannot perform a second semantic action.
        learnTouch(LearnTouch::UP, 92, 164);
        assert(game.team[0].move3Id == 402);
        prepareLearner(explore, false);
        const auto original = game.team[0];
        learnTap(204, 164);
        assert(!game.pendingMoveLearn && std::memcmp(&original, &game.team[0], sizeof(original)) == 0);
        prepareLearner(explore, true);
        learnTap(92, 164);
        assert(learnState(explore).stage == ProgressionUi::MoveLearnStage::REPLACEMENT);
        learnTap(44, 34); // Permanent basic move is disabled.
        learnTap(44, 109); // Six-pixel gap is not a row.
        assert(!learnState(explore).confirmOpen);
        learnTap(44, 84);
        assert(learnState(explore).confirmOpen && !learnState(explore).confirmYes);
        learnTap(290, 158); // Confirmation blocks the page underneath.
        assert(learnState(explore).confirmOpen);
        learnTap(204, 158);
        assert(!learnState(explore).confirmOpen && game.pendingMoveLearn);
        learnTap(44, 134);
        assert(learnState(explore).replacementSlot == 2 && learnState(explore).confirmOpen);
        learnTap(92, 158);
        assert(!game.pendingMoveLearn && game.team[0].move3Id == 402);
        assert(engine.hasPendingMoveReplacement());
        learnTouch(LearnTouch::UP, 92, 158);
        assert(engine.hasPendingMoveReplacement());
        learnTap(148, 158);
        assert(!engine.hasPendingMoveReplacement());
        prepareLearner(explore, true);
        const auto beforeGiveUp = game.team[0];
        learnTap(92, 164); learnTap(44, 184);
        assert(learnState(explore).replacementSlot == Game::MOVE_SLOT_COUNT);
        learnTap(92, 158);
        assert(!game.pendingMoveLearn && !engine.hasPendingMoveReplacement());
        assert(std::memcmp(&beforeGiveUp, &game.team[0], sizeof(beforeGiveUp)) == 0);
        for (int kind = 0; kind < 7; ++kind) {
            prepareLearner(explore, false);
            learnTouch(LearnTouch::DOWN, 92, 164);
            if (kind == 0) learnTouch(LearnTouch::CANCEL, 92, 164);
            if (kind == 1) { learnTouch(LearnTouch::MOVE, 120, 164); learnTouch(LearnTouch::MOVE, 92, 164); }
            if (kind == 2) desktop.advanceMs(1100);
            if (kind == 3) learner->cancelTouch();
            if (kind == 4) learner->onButton(b);
            if (kind == 5) learner->onExit();
            if (kind == 6) game.pendingMoveId = 73;
            learnTouch(LearnTouch::UP, 92, 164);
            assert(game.pendingMoveLearn && game.team[0].move3Id == 0);
        }
        prepareLearner(explore, false);
        learnTouch(LearnTouch::DOWN, 92, 164);
        learnTouch(LearnTouch::UP, 204, 164);
        assert(game.pendingMoveLearn);
    }
    learner.reset();
#endif

#if STICKMON_ENABLE_DEBUG_FEATURES
    using Category = DebugPage::DebugCategory;
    auto debug = [&](int category = 0, int cursor = 0) -> MenuScene& {
        auto& m = setup(); m.pushView(MenuScene::ViewMode::DEBUG);
        DEBUG_UI(m).debugCategory = Category(category); DEBUG_UI(m).debugCursor = cursor;
        return m;
    };
    auto capture = [&](const std::string& name) { PixelRenderer::clear(0); menu->render(); pixels(name); };
    for (int category = 0; category < 7; ++category) {
        auto& m = debug(category); const int count = DEBUG_UI(m).debugItemCount();
        for (int cursor = 0; cursor <= count; ++cursor) for (int scroll : {0, 19, 85}) {
            DEBUG_UI(m).debugCursor = cursor; DEBUG_UI(m).debugScroll = float(scroll);
            capture("debug-" + std::to_string(category) + "-cursor-" + std::to_string(cursor) + "-scroll-" + std::to_string(scroll));
        }
    }
    for (int focus = 0; focus < 5; ++focus) for (int id : {0, 1, 25, 999}) {
        auto& m = debug(1, 2); DEBUG_UI(m).openDebugSwitchPopup();
        DEBUG_UI(m).debugSwitchFocus = focus;
        DEBUG_UI(m).debugSwitchDigits[0] = id / 100; DEBUG_UI(m).debugSwitchDigits[1] = id / 10 % 10; DEBUG_UI(m).debugSwitchDigits[2] = id % 10;
        capture("debug-species-" + std::to_string(id) + "-focus-" + std::to_string(focus));
    }
    for (int focus = 0; focus < 6; ++focus) for (int minute : {0, 599, 1439}) {
        auto& m = debug(3); game.gameMinutesTotal = minute; engine.gameClock.set(desktop.millis(), minute);
        DEBUG_UI(m).openDebugTimePopup(); DEBUG_UI(m).debugTimeFocus = focus;
        capture("debug-time-" + std::to_string(minute) + "-focus-" + std::to_string(focus));
    }
    for (bool enabled : {false, true}) {
        debug(4); engine.debugTiltControl = engine.debugShowWalkBoundary = enabled;
        capture("debug-motion-enabled-" + std::to_string(enabled));
        debug(5); engine.debugShowBattleDrawBounds = enabled;
        capture("debug-battle-bounds-" + std::to_string(enabled));
    }
    for (int category = 0; category < 7; ++category) {
        auto& m = debug(category);
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_FINAL_UI_BASELINE
        const int count = m.debugUi.visibleItemCount();
#else
        const int count = DEBUG_UI(m).debugItemCount();
#endif
        for (int i = 1; i <= count; ++i) { assert(m.onButton(b)); assert(DEBUG_UI(m).debugCursor == i % count); }
        assert(DEBUG_UI(m).debugScroll == 0); ++checks;
    }
    for (int category = 1; category < 7; ++category) {
        auto& m = debug(0, category - 1); m.onButton(a); assert(DEBUG_UI(m).debugCategory == Category(category));
        m.onButton(longB); assert(DEBUG_UI(m).debugCategory == Category::ROOT); ++checks;
    }
    for (int category = 1; category < 7; ++category) {
        auto& m = debug(category); DEBUG_UI(m).debugCursor = DEBUG_UI(m).debugItemCount() - 1;
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_FINAL_UI_BASELINE
        m.onButton(longB);
#else
        m.onButton(a);
#endif
        assert(DEBUG_UI(m).debugCategory == Category::ROOT && DEBUG_UI(m).debugCursor == 0); ++checks;
    }
    for (int cursor : {6, 7}) {
        auto& m = debug(0, cursor);
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_FINAL_UI_BASELINE
        if (cursor == 7) m.onButton(longB); else m.onButton(a);
#else
        m.onButton(a);
#endif
        assert(m.viewMode == MenuScene::ViewMode::MENU); ++checks;
    }
    debug(); assert(!menu->onButton({0, BtnAction::DOWN}) && !menu->onButton({2, BtnAction::PRESSED}));
    menu->onButton(longB); assert(menu->viewMode == MenuScene::ViewMode::MENU); ++checks;
    debug(1, 2); menu->onButton(a); assert(DEBUG_UI(*menu).debugSwitchOpen); menu->onButton(longB);
    assert(!DEBUG_UI(*menu).debugSwitchOpen && DEBUG_UI(*menu).debugCategory == Category::MONSTER); ++checks;
    debug(3); menu->onButton(a); assert(DEBUG_UI(*menu).debugTimeOpen); menu->onButton(longB); assert(!DEBUG_UI(*menu).debugTimeOpen); ++checks;
    debug(1, 2); menu->onButton(a); for (int i = 0; i < 3; ++i) menu->onButton(b);
    DEBUG_UI(*menu).debugSwitchDigits[0] = DEBUG_UI(*menu).debugSwitchDigits[1] = DEBUG_UI(*menu).debugSwitchDigits[2] = 0;
    menu->onButton(a); assert(DEBUG_UI(*menu).debugSwitchOpen && menu->toast);
    capture("debug-invalid-species-toast");
    DEBUG_UI(*menu).debugSwitchDigits[2] = 4; menu->onButton(a);
    assert(!DEBUG_UI(*menu).debugSwitchOpen && game.team[0].speciesId == 4); ++checks;
    debug(1, 2); menu->onButton(a); DEBUG_UI(*menu).debugSwitchDigits[0] = 9; menu->onButton(a);
    assert(DEBUG_UI(*menu).debugSwitchDigits[0] == 0); DEBUG_UI(*menu).debugSwitchFocus = 4;
    menu->onButton(a); assert(!DEBUG_UI(*menu).debugSwitchOpen); ++checks;
    debug(3); menu->onButton(a); DEBUG_UI(*menu).debugTimeDigits[0] = 1; DEBUG_UI(*menu).debugTimeDigits[1] = 9;
    menu->onButton(a); assert(DEBUG_UI(*menu).debugTimeDigits[0] == 2 && DEBUG_UI(*menu).debugTimeDigits[1] == 3);
    for (int focus = 1; focus < 4; ++focus) {
        DEBUG_UI(*menu).debugTimeFocus = focus;
        DEBUG_UI(*menu).debugTimeDigits[focus] = focus == 1 ? 3 : (focus == 2 ? 5 : 9);
        menu->onButton(a); assert(DEBUG_UI(*menu).debugTimeDigits[focus] == 0);
    }
    DEBUG_UI(*menu).debugTimeFocus = 4; menu->onButton(a);
    assert(!DEBUG_UI(*menu).debugTimeOpen && engine.gameMinutesOfDay() == 1200); ++checks;
    debug(2); menu->onButton(a); assert(game.coins == 1000 && menu->toastUntil == desktop.millis() + 1100); ++checks;
    debug(1); game.team[0].hpCur = 1; menu->onButton(a); assert(game.team[0].hpCur == game.team[0].hpMax); ++checks;
    debug(1); game.teamCount = 0; menu->onButton(a); capture("debug-recover-empty"); assert(menu->toast); ++checks;
    debug(1, 1); const auto level = game.team[0].level; menu->onButton(a); assert(game.team[0].level == level + 1 && game.pendingLevelUp); ++checks;
    debug(1, 1); game.team[0].level = Game::LEVEL_MAX; menu->onButton(a); capture("debug-level-max"); assert(game.team[0].level == Game::LEVEL_MAX); ++checks;
    debug(3, 1); const auto light = engine.debugLightSourceLabel(); menu->onButton(a); assert(engine.debugLightSourceLabel() != light); ++checks;
    debug(4); const bool tilt = engine.debugTiltControlEnabled(); menu->onButton(a); assert(engine.debugTiltControlEnabled() != tilt); ++checks;
    debug(4, 1); const bool walk = engine.debugWalkBoundaryVisible(); menu->onButton(a); assert(engine.debugWalkBoundaryVisible() != walk); ++checks;
    debug(4, 3); menu->onButton(a); assert(menu->toastUntil == desktop.millis() + 1400); capture("debug-pair-needs-two"); ++checks;
    debug(5, 1); const bool bounds = engine.debugBattleDrawBoundsVisible(); menu->onButton(a); assert(engine.debugBattleDrawBoundsVisible() != bounds); ++checks;
    debug(5); game.team[0].hpCur = 0; menu->onButton(a); assert(engine.currentId == SceneID::MENU); capture("debug-fainted"); ++checks;
    debug(6); menu->onButton(a); assert(menu->toastUntil == desktop.millis() + 1400); capture("debug-no-contact"); ++checks;
    debug(6); game.teamCount = 2; menu->onButton(a); capture("debug-team-not-solo"); assert(menu->toast); ++checks;
    debug(6); engine.contactVisit.active = true; menu->onButton(a); capture("debug-event-busy"); assert(menu->toast); ++checks;
    debug(1, 1); game.teamCount = 0; menu->onButton(a); capture("debug-level-empty"); assert(menu->toast); ++checks;
    debug(5); game.oobeDone = false; menu->onButton(a); capture("debug-battle-unhatched"); assert(engine.currentId == SceneID::MENU); ++checks;
    debug(3); menu->onButton(a);
    for (int i = 0; i < 6; ++i) menu->onButton(b);
    assert(DEBUG_UI(*menu).debugTimeFocus == 0);
    DEBUG_UI(*menu).debugTimeFocus = 5; const auto minutes = engine.gameMinutesOfDay();
    menu->onButton(a); assert(!DEBUG_UI(*menu).debugTimeOpen && engine.gameMinutesOfDay() == minutes); ++checks;
    debug(1, 2); DEBUG_UI(*menu).openDebugTimePopup(); DEBUG_UI(*menu).openDebugSwitchPopup();
    assert(!DEBUG_UI(*menu).debugTimeOpen && DEBUG_UI(*menu).debugSwitchOpen);
    DEBUG_UI(*menu).openDebugTimePopup(); assert(DEBUG_UI(*menu).debugTimeOpen && !DEBUG_UI(*menu).debugSwitchOpen); ++checks;
    debug(4, 2); const auto unchanged = game; menu->onButton(a);
    assert(std::memcmp(&game, &unchanged, sizeof(game)) == 0 && !menu->toast); ++checks;
    debug(); DEBUG_UI(*menu).debugCursor = 7; auto demand = menu->update(desktop.millis(), 0.02f);
    if (Hal::DISPLAY_H < 200) assert(demand.redraw && demand.nextUpdateDelayMs != SceneUpdateResult::NO_UPDATE);
    for (int i = 0; i < 100; ++i) menu->update(desktop.millis(), 0.02f);
    demand = menu->update(desktop.millis(), 0.02f); assert(!demand.redraw && demand.nextUpdateDelayMs == SceneUpdateResult::NO_UPDATE); ++checks;
    debug(2); menu->onButton(a); desktop.advanceMs(1100); demand = menu->update(desktop.millis(), 0.02f);
    assert(!menu->toast && demand.redraw); ++checks;
    auto owned = [&](int category, int cursor) -> MenuScene* {
        debug(category, cursor); menu.reset(); engine.currentScene = std::make_unique<MenuScene>();
        auto* m = static_cast<MenuScene*>(engine.currentScene.get()); m->onEnter(); m->pushView(MenuScene::ViewMode::DEBUG);
        DEBUG_UI(*m).debugCategory = Category(category); DEBUG_UI(*m).debugCursor = cursor;
        return m;
    };
    using TouchType = Platform::IInputDevice::TouchType;
#if STICKMON_UI_COMPACT_TOUCH && !STICKMON_FINAL_UI_BASELINE
    auto touch = [&](Scene& page, TouchType type, int x, int y) {
        desktop.advanceMs(10);
        return page.onTouch({type, int16_t(x), int16_t(y), desktop.millis()});
    };
    auto tap = [&](Scene& page, int x, int y) {
        touch(page, TouchType::DOWN, x, y);
        return touch(page, TouchType::UP, x, y);
    };
    debug(); assert(!menu->touchNavigation());
    // The removed navigation and inter-row gap cannot activate a list item.
    for (int x : {10, Hal::DISPLAY_W / 2, Hal::DISPLAY_W - 10}) tap(*menu, x, 230);
    tap(*menu, 40, 99); assert(DEBUG_UI(*menu).debugCategory == Category::ROOT);
    touch(*menu, TouchType::DOWN, 40, 70); menu->cancelTouch();
    assert(!touch(*menu, TouchType::UP, 40, 70));
    touch(*menu, TouchType::DOWN, 40, 70); menu->onButton(b);
    touch(*menu, TouchType::UP, 40, 70);
    assert(DEBUG_UI(*menu).debugCategory == Category::ROOT);
    assert(tap(*menu, 40, 120) && DEBUG_UI(*menu).debugCategory == Category::RESOURCE);
    assert(!touch(*menu, TouchType::UP, 40, 120)); ++checks;
    debug(1, 2); menu->onButton(a);
    const auto species = game.team[0].speciesId;
    tap(*menu, 10, 70); assert(DEBUG_UI(*menu).debugSwitchOpen && game.team[0].speciesId == species);
    tap(*menu, 48, 24);
    assert(!DEBUG_UI(*menu).debugSwitchOpen && DEBUG_UI(*menu).debugCategory == Category::MONSTER);
    tap(*menu, 48, 24); assert(DEBUG_UI(*menu).debugCategory == Category::ROOT); ++checks;
    // Manual scrolling stays put when idle, exposes the last row, and never activates on drag.
    debug(); touch(*menu, TouchType::DOWN, 150, 200);
    touch(*menu, TouchType::MOVE, 150, 60); touch(*menu, TouchType::UP, 150, 60);
    assert(DEBUG_UI(*menu).debugScroll == 140 && DEBUG_UI(*menu).debugCategory == Category::ROOT);
    for (int i = 0; i < 20; ++i) menu->update(desktop.millis(), 0.02f);
    assert(DEBUG_UI(*menu).debugScroll == 140); capture("debug-touch-scrolled");
    // Physical selection reveals the current row; Back is absent from the cycle and hit map.
    debug(); for (int i = 0; i < 6; ++i) menu->onButton(b);
    assert(DEBUG_UI(*menu).debugCursor == 6 && DEBUG_UI(*menu).debugScroll > 0);
    capture("debug-touch-last"); menu->onButton(b);
    assert(DEBUG_UI(*menu).debugCursor == 0 && DEBUG_UI(*menu).debugScroll == 0);
    for (int category = 1; category <= 6; ++category) {
        debug(category); const auto coins = game.coins;
        assert(menu->debugUi.touchTarget(40, 52 + menu->debugUi.visibleItemCount() * 50 + 10) == -1);
        tap(*menu, 48, 24); assert(DEBUG_UI(*menu).debugCategory == Category::ROOT && game.coins == coins);
    }
    // A cancelled, moved, or expired contact cannot become a click.
    for (int kind = 0; kind < 3; ++kind) {
        debug(); touch(*menu, TouchType::DOWN, 40, 70);
        if (kind == 0) touch(*menu, TouchType::CANCEL, 40, 70);
        if (kind == 1) { touch(*menu, TouchType::MOVE, 70, 70); touch(*menu, TouchType::MOVE, 40, 70); }
        if (kind == 2) desktop.advanceMs(1001);
        touch(*menu, TouchType::UP, 40, 70);
        assert(DEBUG_UI(*menu).debugCategory == Category::ROOT);
    }
    // Digits and confirm/cancel remain operable after removing virtual buttons.
    debug(1); tap(*menu, 40, 170); assert(DEBUG_UI(*menu).debugSwitchOpen);
    tap(*menu, 192, 140); assert(DEBUG_UI(*menu).debugSwitchDigits[2] == 2);
    tap(*menu, 200, 190); assert(!DEBUG_UI(*menu).debugSwitchOpen && game.team[0].speciesId == 1);
    tap(*menu, 40, 170); DEBUG_UI(*menu).debugSwitchDigits[2] = 4;
    tap(*menu, 90, 190); assert(!DEBUG_UI(*menu).debugSwitchOpen && game.team[0].speciesId == 4);
    debug(3); tap(*menu, 40, 70); assert(DEBUG_UI(*menu).debugTimeOpen);
    tap(*menu, 72, 140); assert(DEBUG_UI(*menu).debugTimeDigits[0] == 2);
    const auto targetTime = DEBUG_UI(*menu).debugTimeTargetMinutes();
    tap(*menu, 90, 190); assert(!DEBUG_UI(*menu).debugTimeOpen && engine.gameMinutesOfDay() == targetTime);
    auto* tapped = owned(5, 0); tap(*tapped, 40, 70);
    assert(engine.currentId == SceneID::EXPLORE && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr); ++checks;
#else
    auto touch = [&](Scene& page, TouchType type, int x) {
        desktop.advanceMs(10);
        return page.onTouch({type, int16_t(x), int16_t(Hal::DISPLAY_H - 10), desktop.millis()});
    };
    debug(); assert(menu->touchNavigation());
    touch(*menu, TouchType::DOWN, Hal::DISPLAY_W / 2); assert(touch(*menu, TouchType::UP, Hal::DISPLAY_W / 2));
    assert(DEBUG_UI(*menu).debugCursor == 1);
    assert(!touch(*menu, TouchType::UP, Hal::DISPLAY_W / 2));
    touch(*menu, TouchType::DOWN, Hal::DISPLAY_W / 2); menu->cancelTouch();
    assert(!touch(*menu, TouchType::UP, Hal::DISPLAY_W / 2) && DEBUG_UI(*menu).debugCursor == 1); ++checks;
    debug(1, 2); menu->onButton(a); touch(*menu, TouchType::DOWN, 10); touch(*menu, TouchType::UP, 10);
    assert(!DEBUG_UI(*menu).debugSwitchOpen && DEBUG_UI(*menu).debugCategory == Category::MONSTER);
    touch(*menu, TouchType::DOWN, 10); touch(*menu, TouchType::UP, 10);
    assert(DEBUG_UI(*menu).debugCategory == Category::ROOT); ++checks;
    auto* tapped = owned(5, 0); touch(*tapped, TouchType::DOWN, Hal::DISPLAY_W - 10);
    touch(*tapped, TouchType::UP, Hal::DISPLAY_W - 10);
    assert(engine.currentId == SceneID::EXPLORE && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr); ++checks;
#endif
    for (int kind = 0; kind < 3; ++kind) {
        auto* m = owned(6, kind); game.storageCount = 1;
        game.storage[0] = Game::MonsterFactory::create(4, 20); game.storage[0].metAt = 999; game.storage[0].origin = Game::Origin::BEFRIENDED;
        m->onButton(a); assert(engine.currentId == SceneID::MAIN && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr);
        assert(engine.contactVisit.pendingKnock && engine.contactVisit.kind == ContactVisitKind(kind + 1)); ++checks;
    }
    auto* m = owned(4, 3); game.teamCount = 2; game.team[1] = Game::MonsterFactory::create(4, 20);
    m->onButton(a); assert(engine.currentId == SceneID::MAIN && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr); ++checks;
    m = owned(5, 0); m->onButton(a); assert(engine.currentId == SceneID::EXPLORE && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr); ++checks;
    engine.endDebugBattle(); m = static_cast<MenuScene*>(engine.currentScene.get());
    assert(m->viewMode == MenuScene::ViewMode::DEBUG && DEBUG_UI(*m).debugCategory == Category::BATTLE); ++checks;
    m = owned(3, 0); m->onButton(longA); assert(engine.currentId == SceneID::MAIN && dynamic_cast<MenuScene*>(engine.currentScene.get()) == nullptr); ++checks;
#else
    setup(); assert(!BuildConfig::DEBUG_FEATURES); ++checks;
#endif
    engine.currentScene.reset(); menu.reset(); CryPlayer::ins().stop();
    std::printf("final-ui-checkpoints=%u\n", checks);
}
