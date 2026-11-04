// SPDX-License-Identifier: Apache-2.0
#include "RoomSession.h"
#include "PowerActions.h"
#include "SdgoodsPlatform.h"
#include "game/GameRandom.h"
#include "core/UiStrings.h"
#include "core/BuildConfig.h"
#include "presentation/PixelRenderer.h"

namespace {
Sdgoods::SdgoodsPlatform board;
Sdgoods::RoomSession session;
}

extern "C" void app_main() {
    Platform::bind(board.serviceBundle());
    if (!board.begin()) {
        Platform::logLine("[SDGOODS] board initialization failed; no game started");
        return;
    }
    Platform::logf("[SDGOODS] build debug=%u trace=%u\n",
        unsigned(STICKMON_ENABLE_DEBUG_FEATURES), unsigned(STICKMON_ENABLE_TRACE_LOGS));
    GameRandom::seed(board.hardwareRandom());
    if (!session.begin()) {
        Platform::logLine("[SDGOODS] save unavailable; original data retained");
        PixelRenderer::bind(board.frameBuffer());
        PixelRenderer::clear(0);
        PixelRenderer::text(116, 160, Ui::RoundRoom::START_FAILED, 0xffff);
        PixelRenderer::text(96, 186, Ui::RoundRoom::DATA_RETAINED, 0xffff);
        board.present();
        return;
    }
    for (;;) {
        board.pollInputs();
        Sdgoods::processPowerActions(board, session, GameEngine::ins());
        if (board.powerKeyHeld()) board.sleepMs(5);
        else session.poll();
    }
}
