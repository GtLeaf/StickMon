#pragma once
#include <memory>
#include <new>
#include "StandbyCheckpoint.h"
#include "StandbyPolicy.h"

namespace Sdgoods {
enum class StandbyWake { OTHER, TIMER, KEY };
struct StandbyBoot {
    bool restored = false;
    Game::StandbyCare::Context context{};
    // A user wake can retry a failed early write after verified normal startup.
    // Keep the old retained anchor until that candidate is durable.
    std::unique_ptr<Game::GameState> pendingState;
};

template<class Board, class Storage, class Retention>
StandbyBoot resumeStandbyBootFlow(Board& board, Storage& storage, Retention& retention,
                                  StandbyWake wake, bool deepReset) {
    StandbyBoot result;
    auto retained = retention.read();
    const bool timer = deepReset && wake == StandbyWake::TIMER;
    const bool deep = deepReset && wake != StandbyWake::OTHER;
    const auto retry = [&] { board.enterDeepSleep(30ULL * 60 * 1000000, true); };
    if (!retained.valid(retained.snapshotHash, deep, board.standbyMicros())) {
        retention.clear();
        if (timer) { storage.begin(); retry(); }
        return result;
    }
    if (!storage.begin()) {
        if (timer) retry();
        else retention.clear();
        return result;
    }
    Game::GameState state{};
    MainSceneViewState view{};
    if (!storage.load(state, view) ||
        !retained.valid(standbySnapshotHash(state, view), true, board.standbyMicros())) {
        // Never overwrite an unreadable, newer or unmatched snapshot.
        if (timer) retry();
        else retention.clear();
        return result;
    }
    auto next = retained;
    advanceStandbyCare(state, next.context, next.appliedUs, board.standbyMicros());
    if (!storage.save(state, view)) {
        if (timer) { retry(); return result; }
        result.pendingState.reset(new (std::nothrow) Game::GameState(state));
        if (!result.pendingState) { retry(); return result; }
        result.restored = true;
        result.context = next.context;
        return result;
    }
    next.snapshotHash = standbySnapshotHash(state, view);
    next.seal();
    retention.store(next);
    if (timer && !board.powerKeyActive()) {
        const auto age = (board.standbyMicros() - next.enteredUs) / 1000;
        board.enterDeepSleep(uint64_t(StandbyPolicy::waitMs(age)) * 1000, true);
        return result;
    }
    result.restored = true;
    result.context = next.context;
    retention.clear();
    return result;
}
}
