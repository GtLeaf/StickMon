#pragma once

#include "ui/common/ExploreRouteUiModel.h"

namespace Ui::ExploreRouteUiContent {

void drawMapTile(uint16_t tileId, int x, int y, uint8_t layer,
                 uint16_t fieldColor, uint8_t animationFrame);

void drawAreaMenu(const ExploreRouteFrame& frame, ExploreRouteVisualState& state,
                  const ExploreRouteLayout& layout);
void drawWalking(const ExploreRouteFrame& frame, ExploreRouteVisualState& state,
                 const ExploreRouteLayout& layout);
void drawRouteBoss(const ExploreRouteFrame& frame, int cameraX, int cameraY,
                  const ExploreRouteLayout& layout);
void drawRouteMonster(const ExploreRouteFrame& frame, ExploreRouteVisualState& state,
                      const Species& species, float worldX, float worldY,
                      uint8_t walkDirection, bool follower, int cameraX, int cameraY,
                      const ExploreRouteLayout& layout);
void drawRoutePickup(const ExploreRouteFrame& frame, int cameraX, int cameraY,
                    const ExploreRouteLayout& layout);
void drawExploreMenu(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout);
void drawSpecialPrompt(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout);
void drawPickupPrompt(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout);
void drawResult(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout);
void drawEndPrompt(const ExploreRouteFrame& frame, const ExploreRouteLayout& layout);

}  // namespace Ui::ExploreRouteUiContent
