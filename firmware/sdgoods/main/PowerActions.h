// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace Sdgoods {
// Runs before simulation so the held key cannot advance past its snapshot.
template<class Board, class Session, class Engine>
void processPowerActions(Board& board, Session& session, Engine& engine) {
    if (board.takeSaveRequest()) session.save();
    if (board.takeUnlockRequest()) {
        engine.wakeFromIdle();
        engine.invalidateScene();
    }
    if (board.takeShutdownRequest() && session.save()) board.powerOff();
}
}  // namespace Sdgoods
