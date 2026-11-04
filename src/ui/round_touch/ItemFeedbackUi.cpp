// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ItemFeedbackUi.h"
#include "assets/PokemonSprites.h"
#include "core/ProgressionUi.h"
#include "core/UiStrings.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoundPage.h"
#include "ui/SceneSurface.h"

namespace Ui::ItemFeedback {
void renderLevelUp(const Game::MonsterRuntime& monster, const char* name, uint8_t level) {
    PixelRenderer::clear(0);
    const auto* sprite = PokemonSprites::findSpeciesSprite(monster.speciesId, PokemonSprites::SpriteKind::FRONT);
    if (sprite) PokemonSprites::drawFrame(sprite,
        180 - FlashStorage::readByte(&sprite->width) / 2,
        176 - FlashStorage::readByte(&sprite->height) / 2);
    RoundPage::nativeHeader(Ui::BAG, false);
    ::ProgressionUi::renderLevelUp(name, level);
}
void renderEvolutionNavigation(const char* message, bool complete) {
    SceneSurface::pageHeader(Ui::BAG, false);
    if (!complete) return;
    auto& canvas = PixelRenderer::canvas();
    // Replace the shared button-key hint with the round device's touch action.
    const auto panel = PixelRenderer::rgb(24, 38, 43);
    canvas.fillRect(8, 103, 232, 28, panel);
    MenuGrid::label(canvas, 120, 109, message, 216, 0xffff);
    canvas.fillRoundRect(48, 184, 144, 36, 6, panel);
    MenuGrid::label(canvas, 120, 194, Ui::Common::CONTINUE, 128, 0xffff);
}
int evolutionTarget(int16_t x, int16_t y, bool cancellable) {
    if (RoundPage::NATIVE_BACK.contains(x + 60, y + 60)) return cancellable ? 1 : 0;
    return x >= 0 && x < 240 && y >= 0 && y < 240 ? 0 : -1;
}
}
