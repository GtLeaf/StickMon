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
}
