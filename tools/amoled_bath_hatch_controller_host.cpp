#include "app/HatchController.h"
#include "app/ShowerController.h"
#include "app/ProgressionFlow.h"
#include "app/TeamController.h"
#include "app/InventoryController.h"
#include "game/MonsterFactory.h"

#include <type_traits>
#include <utility>

#include <cassert>
#include <cstdio>

void checkBathHatchControllerEffects() {
    using namespace AmoledV1;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<HatchController&>().state())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<ShowerController&>().state())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<TeamController&>().state())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<InventoryController&>().state())>>);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(std::declval<ProgressionFlow&>().state())>>);
    HatchController hatch;
    const uint32_t start = 0xFFFFFF00U;
    hatch.reset(start);
    hatch.advance(start + 500, 180, 1, true);
    assert(hatch.state().elapsedSeconds == 0.5f);
    hatch.advance(start + 100500, 180, 1, false);
    hatch.advance(start + 101500, 180, 1, true);
    assert(hatch.state().elapsedSeconds == 1.5f);

    // Failed hardware application is retried without requesting or polling every frame.
    auto request = hatch.syncImu(true, start);
    assert(request.apply && request.enabled);
    hatch.didApplyImu(false, start);
    assert(!hatch.syncImu(true, start + 999).apply);
    assert(!hatch.pollImu(start + 999));
    assert(hatch.syncImu(true, start + 1000).apply);
    hatch.didApplyImu(true, start + 1000);
    assert(hatch.pollImu(start + 1000));
    assert(!hatch.pollImu(start + 1019));
    assert(hatch.pollImu(start + 1020));
    assert(hatch.interact(false, start + 1020, 180, false));
    request = hatch.syncImu(false, start + 1021);
    assert(request.apply && !request.enabled && request.roomChanged);
    hatch.didApplyImu(true, start + 1021);
    assert(!hatch.state().heartVisible && !hatch.state().shaking);
    assert(!hatch.syncImu(false, start + 2021).apply);
    assert(!hatch.pollImu(start + 2021));

    // Preparing a save does not acknowledge it: a failed write remains eligible.
    Game::HatchProgress restored;
    restored.elapsedSeconds = 165;
    restored.pokeCount = 7;
    hatch.restoreProgress(restored, 180, start + 2000);
    hatch.advance(start + 17000, 180, 1, true);
    Game::HatchProgress candidate;
    assert(hatch.progressToSave(false, candidate));
    assert(candidate.elapsedSeconds == 180 && candidate.pokeCount == 7);
    assert(hatch.progressToSave(false, candidate));
    hatch.didSaveProgress(candidate.elapsedSeconds);
    assert(!hatch.progressToSave(false, candidate));
    assert(!hatch.interact(true, start + 17000, 180, false));
    assert(!hatch.interact(false, start + 17000, 180, true));
    assert(hatch.state().pokeCount == 7 && hatch.state().wipeCount == 0);

    ShowerController shower;
    shower.reset(1000, 324);
    shower.takeEffects();
    shower.soapConsumed(0, 1000);
    shower.takeEffects();
    assert(shower.beginTouch(184, 212));
    shower.dragTool(140, 212, 1050);
    const auto soap = shower.takeEffects();
    const uint8_t soapMask = 1U << static_cast<uint8_t>(Game::BathService::Stage::SOAP);
    assert(soap.rewardStageMask == soapMask && soap.redraw);
    shower.dragTool(220, 212, 1300);
    assert(shower.takeEffects().rewardStageMask == 0);
    shower.cancelTouch();
    const auto cancelledX = shower.state().showerToolX;
    shower.dragTool(100, 212, 1600);
    assert(shower.state().showerToolX == cancelledX);
    assert(!shower.takeEffects().redraw);
    assert(!shower.requestExit());
    assert(shower.state().showerMode == ShowerMode::EXIT_CONFIRM);
    const Game::GameState game;
    assert(shower.tap(game, 250, 380, 1600).action == ShowerController::Action::NONE);
    assert(shower.state().showerMode == ShowerMode::MENU);
    shower.startRinse(1700);
    shower.takeEffects();
    uint8_t completionCount = 0;
    for (uint32_t now = 1720; now <= 3700; now += 20) {
        shower.update(now, false);
        const auto effects = shower.takeEffects();
        if (effects.completionScore) {
            ++completionCount;
            assert(effects.completionScore == 2);
            const uint8_t rinseMask = 1U << static_cast<uint8_t>(Game::BathService::Stage::RINSE);
            assert(effects.rewardStageMask == rinseMask);
        }
    }
    assert(completionCount == 1 && shower.state().showerMode == ShowerMode::COMPLETE);
    shower.update(5000, false);
    const auto reset = shower.takeEffects();
    assert(reset.sessionReset && reset.rewardStageMask == 0 && reset.completionScore == 0);
    assert(shower.state().showerMode == ShowerMode::MENU);
    const auto drained = shower.takeEffects();
    assert(!drained.redraw && !drained.sessionReset && drained.rewardStageMask == 0);
    // The growth flow consumes effects once, preserves stones on cancellation,
    // and cannot apply a completed evolution again (including clock wrap).
    Game::GameState growth;
    growth.teamCount = 1;
    growth.team[0] = Game::MonsterFactory::create(133, 20);
    assert(Game::ItemInventory::add(growth, Game::ItemId::WATER_STONE));
    ProgressionFlow flow;
    const uint32_t evolutionStart = 0xFFFFFF00U;
    assert(flow.openEvolution(growth, AppSceneFlow::Scene::HOME, 0, 133, 134,
                              evolutionStart, Game::ItemId::WATER_STONE));
    assert(flow.takeEffects().preloadTo == 134);
    assert(flow.takeEffects().preloadTo == 0);
    assert(flow.cancelEvolution(evolutionStart + 100));
    assert(flow.takeEffects().stopCry);
    assert(!flow.takeEffects().stopCry);
    const uint32_t cancelledAt = evolutionStart + 100 + Game::EvolutionSequence::CANCEL_COMPLETE_MS;
    flow.tap(growth, 0, 0, cancelledAt);
    assert(flow.takeEffects().completed);
    assert(growth.team[0].speciesId == 133);
    assert(Game::ItemInventory::count(growth, Game::ItemId::WATER_STONE) == 1);
    assert(flow.openEvolution(growth, AppSceneFlow::Scene::HOME, 0, 133, 134,
                              cancelledAt, Game::ItemId::WATER_STONE));
    flow.takeEffects();
    flow.advance(growth, cancelledAt + Game::EvolutionSequence::COMPLETE_MS);
    const auto evolved = flow.takeEffects();
    assert(evolved.save && evolved.syncTeam && evolved.completed);
    assert(growth.team[0].speciesId == 134);
    assert(Game::ItemInventory::count(growth, Game::ItemId::WATER_STONE) == 0);
    flow.advance(growth, cancelledAt + 10000);
    flow.update(cancelledAt + 10000);
    const auto duplicate = flow.takeEffects();
    assert(!duplicate.save && !duplicate.completed && !duplicate.syncTeam && duplicate.rowEnd == 0);

    std::puts("bath/hatch controllers: one-shot rewards, cancellation, completion, IMU retry, pause/wrap and failed-save acknowledgement verified");
}
