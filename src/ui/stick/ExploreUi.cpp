#include "ui/stick/ExploreUi.h"

#include "hardware/Hal.h"

namespace Ui {
void ExploreUi::render() {
    renderWith(ExploreRouteLayout{Hal::DISPLAY_W, Hal::DISPLAY_H},
               ExploreBattleLayout{Hal::DISPLAY_W, Hal::DISPLAY_H});
}
}  // namespace Ui
