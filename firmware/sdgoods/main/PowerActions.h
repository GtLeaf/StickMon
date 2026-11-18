// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "PowerButton.h"

namespace Sdgoods {
// Sample the final key level before returning, including after the last pause.
template<class ReadDown, class Clock, class Pause>
void waitForPowerKeyRelease(ReadDown readDown, Clock clock, Pause pause) {
    bool released = false;
    uint32_t releasedAt = 0;
    for (;;) {
        const bool down = readDown();
        const uint32_t now = clock();
        if (down) released = false;
        else if (!released) { released = true; releasedAt = now; }
        else if (now - releasedAt >= Stickmon::PowerButton::debounceMs) return;
        pause();
    }
}

// Runs before simulation so the held key cannot advance past its snapshot.
template<class Board, class Session, class Engine>
void processPowerActions(Board& board, Session& session, Engine& engine) {
    // The key has been released. A failed save cannot cancel a confirmed shutdown.
    if (board.takeShutdownRequest()) {
        board.takeSaveRequest();
        board.takeUnlockRequest();
        session.save();
        board.powerOff();
        return;
    }
    if (board.takeSaveRequest()) session.save();
    if (board.takeUnlockRequest()) {
        engine.wakeFromIdle();
        engine.invalidateScene();
    }
    if(board.lockRequestReady()&&engine.prepareDisplayLock()){
        board.completeLockRequest();
        session.save();
    }
}

// Recovery must remain operable without a save session or game engine.
template<class Board>
void pollRecoveryPowerActions(Board& board) {
    board.pollPowerButton(false);
    board.takeSaveRequest();
    board.takeUnlockRequest();
    if (board.takeShutdownRequest()) board.powerOff();
}
}  // namespace Sdgoods
