#pragma once

#include "ui/common/ExploreRouteUiContent.h"
#include "presentation/PokemonAnimationView.h"
#include "presentation/PokemonShadow.h"
#include "presentation/PixelRenderer.h"

inline void checkSdgoodsRouteAnimationTiming() {
    using PokemonAnimationTiming::Action;
    ExploreMapGenerator::Map map{};
    map.pathCount = 1;
    map.paths[0].pointCount = 2;
    map.paths[0].points[0] = {2, 2};
    map.paths[0].points[1] = {5, 2};
    Ui::ExploreRouteFrame view{};
    view.generatedMap = &map;
    view.routeLeaderMoveDurationMs = 500;
    Ui::ExploreRouteVisualState visual{};
    Ui::ExploreRouteLayout layout{};
    const uint16_t ids[] = {264, 355};
    assert(PokemonSprites::syncTeamCache(ids, 2));
    const auto draw = [&](uint16_t id, uint32_t now, uint8_t direction = 0) {
        view.nowMs = now;
        Ui::ExploreRouteUiContent::drawRouteMonster(view, visual, *findSpecies(id),
            100, 100, direction, false, 0, 0, layout);
    };
    for (uint32_t now = 0; now <= 1800; now += 100) {
        draw(264, now, (now / 100) % 8);
        assert(visual.partyAnimation[0].elapsedMs() == now);
        assert(visual.partyAction[0] == Action::Idle);
        draw(264, now);
        assert(visual.partyAnimation[0].elapsedMs() == now);
    }
    PokemonSprites::WalkingAnimation artwork{};
    PokemonSprites::PetAnimationProfile profile{};
    assert(PokemonSprites::petAnimationProfile(264, profile));
    assert(PokemonAnimationView::animationFor(264, PokemonSprites::WalkDirection::LEFT, false, artwork));
    assert(artwork.frameCount == 14);
    assert(static_cast<uint16_t>(artwork.base) == static_cast<uint16_t>(profile.idleBase) + 2 * 14);
    view.routeMoving = true;
    for (uint32_t now = 2000; now <= 2900; now += 100) {
        view.routeMoveStarted = now - (now - 2000) % 300;
        view.routeIndex = 1 + (now - 2000) / 300;
        draw(264, now, (now / 100) % 4);
        assert(visual.partyAnimation[0].elapsedMs() == now - 2000);
        assert(visual.partyAction[0] == Action::Walk);
    }
    visual.partyAnimationActive = false;
    view.routeMoving = false; // Touch pause masks movement in the scene view.
    draw(264, 3000);
    draw(264, 10000);
    assert(visual.partyAnimation[0].elapsedMs() == 900);
    assert(visual.partyAction[0] == Action::Walk);
    visual.partyAnimationActive = true;
    view.routeMoving = true;
    view.routeMoveStarted = 11000;
    draw(264, 11000);
    assert(visual.partyAnimation[0].elapsedMs() == 900);
    draw(264, 11100);
    assert(visual.partyAnimation[0].elapsedMs() == 1000);
    view.routeMoving = false;
    draw(264, 11200);
    assert(visual.partyAnimation[0].elapsedMs() == 0 && visual.partyAction[0] == Action::Idle);
    draw(355, 11300);
    assert(visual.partyAnimation[0].elapsedMs() == 0);

    // Compare the real route renderer with source-registered artwork placed
    // directly on its ground anchor. Legacy slither offsets and delayed turns
    // must not be layered on top of complete original frames.
    layout.actorScale = 1;
    layout.feetOnRouteCenter = true;
    auto& canvas = PixelRenderer::canvas();
    for (const uint16_t species : {1, 11, 41, 93, 94, 147, 148}) {
        const uint16_t cached[] = {species};
        assert(PokemonSprites::syncTeamCache(cached, 1));
        for (const bool walking : {false, true}) {
            visual = {};
            view.routeMoving = walking;
            view.routeMoveStarted = 0;
            for (uint32_t now = 0; now <= 2000; now += 67) {
                const uint8_t direction = (now / 67) % 8;
                canvas.fillSprite(0);
                draw(species, now, direction);
                assert(visual.routeVisualWalkDirection == direction);
                std::vector<uint16_t> actual(canvas.rawPixels(), canvas.rawPixels() + canvas.pixelCount());
                const auto action = walking ? Action::Walk : Action::Idle;
                assert(visual.partyAnimation[0].elapsedMs() == now);
                PokemonSprites::WalkingAnimation art{};
                assert(PokemonAnimationView::animationFor(species,
                    static_cast<PokemonSprites::WalkDirection>(direction), walking, art));
                const auto* frame = PokemonSprites::findSpeciesSprite(species,
                    static_cast<PokemonSprites::SpriteKind>(static_cast<uint16_t>(art.base) +
                    PokemonAnimationTiming::frameAt(species, action, now)));
                assert(frame);
                const auto air = PokemonMotion::airProfileForSpecies(species);
                const float phase = static_cast<float>((now + species * 97U) % 1600U) * 0.00392699f;
                const float lift = air.height + sinf(phase) * air.bobAmplitude;
                const auto shadow = PokemonShadow::atGround(frame, 100, 100, 1, art.flipX);
                canvas.fillSprite(0);
                canvas.fillEllipse(shadow.x, shadow.y, shadow.radiusX, shadow.radiusY,
                    PixelRenderer::rgb(68, 87, 74));
                PokemonSprites::drawFrameScaled(frame, 100 - frame->width / 2,
                    static_cast<int>(roundf(100 - lift)) - frame->height / 2 -
                    PokemonSprites::frameGroundOffsetY(frame), 1, art.flipX);
                assert(std::equal(actual.begin(), actual.end(), canvas.rawPixels()));
            }
        }
    }
    std::puts("PASS SDGOODS real route rendering: full idle art, step/direction continuity, repeated draws, pause/resume, action/species resets");
}
