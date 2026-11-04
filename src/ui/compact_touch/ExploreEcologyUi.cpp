#include "ui/compact_touch/ExploreUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cmath>
#include "assets/GameAssets.h"
#include "game/ExploreRouteGeometry.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/PokemonShadow.h"
#include "ui/common/ExploreRouteUiContent.h"

namespace Ui {
namespace {
constexpr int TILE = ExploreRouteGeometry::TILE_SIZE;
constexpr uint16_t TRANSPARENT = 0xf81f;
constexpr float SPRITE_SCALE = 0.8f;
static_assert(ExploreEcology::CAPACITY == 3, "route overlay has three wildlife slots");

struct GrassArtwork {
    uint16_t tiles[3][TILE * TILE]{};
    uint16_t floor[64]{};
    uint8_t floorCount = 0;
    bool ready = false;
    bool isFloor(uint16_t color) const {
        return std::find(floor, floor + floorCount, color) != floor + floorCount;
    }
    bool load() {
        if (ready) return true;
        Canvas565 canvas;
        uint16_t scratch[TILE * TILE];
        canvas.attach({scratch, TILE, TILE, false});
        floorCount = 0;
        for (uint16_t id = 385; id <= 389; ++id) {
            if (!GameAssets::drawExploreTileTo(canvas, id, 0, 0, 0, 1)) return false;
            for (uint16_t color : scratch)
                if (!isFloor(color) && floorCount < 64) floor[floorCount++] = color;
        }
        const uint16_t ids[] = {390, 399, 391};
        for (int i = 0; i < 3; ++i) {
            canvas.attach({tiles[i], TILE, TILE, false});
            canvas.fillSprite(TRANSPARENT);
            if (!GameAssets::drawExploreTileTo(canvas, ids[i], 0, 0, 0, 1)) return false;
        }
        ready = true;
        return true;
    }
} artwork;

ExploreGrass::Frame grassFrame(const ExploreMapGenerator::Map& map,
                               const ExploreGrass::Simulation& grass, int cell, uint16_t id) {
    if (id == 391 && cell + ExploreMapGenerator::WIDTH < ExploreMapGenerator::CELL_COUNT &&
        map.layers[0][cell + ExploreMapGenerator::WIDTH] == 399)
        cell += ExploreMapGenerator::WIDTH;
    return grass.frame(cell);
}

void drawGrass(uint16_t id, int x, int y, ExploreGrass::Frame frame, bool cover) {
    if (!artwork.ready) return;
    auto& canvas = PixelRenderer::canvas();
    const int index = id == 391 ? 2 : id == 399 ? 1 : 0;
    const int bend = ExploreGrass::Simulation::bend(frame);
    for (int row = 0; row < TILE; ++row) {
        if (cover && id == 390 && row < 10) continue;
        const int spread = id == 391 ? bend * (2 * TILE - 1 - row) / (2 * TILE - 1)
            : id == 399 ? bend * (TILE - 1 - row) / (2 * TILE - 1)
                        : bend * (TILE - 1 - row) / (TILE - 1);
        for (int column = 0; column < TILE; ++column) {
            const int source = column - (column < frame.contactX ? -spread : spread);
            const uint16_t color = source < 0 || source >= TILE
                ? (id == 391 ? TRANSPARENT : artwork.floor[0])
                : artwork.tiles[index][row * TILE + source];
            if (color != TRANSPARENT && !(cover && id != 391 && artwork.isFloor(color)))
                canvas.drawPixel(x + column, y + row, color);
        }
    }
}

const PokemonSprites::SpriteFrame* wildlifeFrame(const ExploreEcology::Pose& pose, bool& flip) {
    using namespace PokemonSprites;
    PetAnimationProfile profile{};
    if (petAnimationProfile(pose.speciesId, profile)) {
        const bool sleeping = pose.activity == ExploreEcology::Activity::DOZE ||
            (pose.cover == ExploreEcology::Cover::GRASS &&
             (pose.activity == ExploreEcology::Activity::HIDE ||
              pose.activity == ExploreEcology::Activity::BURROW));
        const bool flapping = pose.airborne || pose.activity == ExploreEcology::Activity::TAKEOFF ||
            pose.activity == ExploreEcology::Activity::LAND;
        if (sleeping || (!pose.walking && !flapping)) {
            constexpr uint8_t directions[] = {0, 2, 4, 6};
            uint8_t direction = directions[pose.direction % 4];
            flip = !sleeping && direction == 6 && profile.mirrorRightDirections;
            if (flip) direction = 2;
            const uint8_t count = sleeping ? profile.sleepingFrames : profile.idleFrames;
            if (count) {
                const auto kind = static_cast<SpriteKind>(
                    static_cast<uint16_t>(sleeping ? profile.sleepingBase : profile.idleBase) +
                    (sleeping ? 0 : direction * count) + pose.frame % count);
                if (const auto* frame = findCachedSpeciesSprite(pose.speciesId, kind)) return frame;
            }
        }
    }
    WalkingAnimation animation{};
    if (walkingAnimation(pose.speciesId, static_cast<WalkDirection>(pose.direction % 4), animation) &&
        animation.frameCount) {
        flip = animation.flipX;
        const auto kind = static_cast<SpriteKind>(static_cast<uint16_t>(animation.base) +
            ((pose.airborne || pose.walking || pose.activity == ExploreEcology::Activity::TAKEOFF ||
              pose.activity == ExploreEcology::Activity::LAND) ? pose.frame % animation.frameCount : 0));
        if (const auto* frame = findCachedSpeciesSprite(pose.speciesId, kind)) return frame;
    }
    flip = false;
    return findCachedSpeciesSprite(pose.speciesId, SpriteKind::ICON_0);
}

void ripple(int x, int y, int radius, uint16_t color) {
    auto& canvas = PixelRenderer::canvas();
    int previousX = x + radius, previousY = y;
    for (int i = 1; i <= 20; ++i) {
        const float angle = i * 6.2831853f / 20;
        const int nextX = x + int(std::lround(std::cos(angle) * radius));
        const int nextY = y + int(std::lround(std::sin(angle) * 2));
        canvas.drawLine(previousX, previousY, nextX, nextY, color);
        previousX = nextX; previousY = nextY;
    }
}

void habitatHint(const ExploreEcology::Pose& pose, int x, int y) {
    auto& canvas = PixelRenderer::canvas();
    const int phase = pose.frame % 3;
    if (pose.surface == ExploreEcology::Surface::WATER) {
        ripple(x, y, 5 + phase * 2, PixelRenderer::rgb(171, 217, 225));
        if (phase) canvas.fillCircle(x + 2, y - 4, 1, PixelRenderer::rgb(195, 233, 239));
    } else if (pose.surface == ExploreEcology::Surface::ROCK ||
               pose.surface == ExploreEcology::Surface::ICE) {
        const auto color = pose.surface == ExploreEcology::Surface::ICE
            ? PixelRenderer::rgb(183, 219, 231) : PixelRenderer::rgb(146, 137, 125);
        canvas.fillEllipse(x, y + 1, 5 + phase * 2, 1, color);
        if (phase) {
            canvas.drawPixel(x - 3 - phase, y - 2, color);
            canvas.drawPixel(x + 3 + phase, y - 4, color);
        }
    } else {
        const int bend = phase == 1 ? -2 : phase == 2 ? 2 : 0;
        for (int blade = -1; blade <= 1; ++blade)
            canvas.drawLine(x + blade * 3, y + 1, x + blade * 5 + bend,
                y - 5 - (blade == 0 ? 2 : 0), PixelRenderer::rgb(110, 163, 85));
    }
}
}  // namespace

uint8_t ExploreUi::routeSpriteSpecies(const ExplorePool::Pool& pool, uint16_t* ids,
                                      uint8_t capacity) const {
    if (!ids) return 0;
    uint8_t count = ExplorePool::appendUniqueSpecies(pool, ids, 0, capacity);
    for (uint8_t i = 0; i < pool.count && i < ExplorePool::POOL_CAP; ++i) {
        const auto* family = ExploreEcology::groupFamily(pool.entries[i].speciesId);
        if (!family) continue;
        for (uint8_t member = 0; member < family->count && count < capacity; ++member) {
            const auto species = family->species[member];
            if (std::find(ids, ids + count, species) == ids + count) ids[count++] = species;
        }
    }
    return count;
}

void ExploreUi::resetRouteVisual(const ExplorePool::Pool& pool, uint8_t hiddenMask) {
    const auto frame = host.exploreRouteFrame();
    if (!frame.generatedMap) return;
    ecology.reset(*frame.generatedMap, frame.currentRoutePath, pool, hiddenMask, frame.nowMs);
    grass.reset(frame.nowMs);
    routeVisualActive = true;
    // Warm artwork at map entry. Drawing never decodes grass or species packs.
    artwork.load();
}

void ExploreUi::revealRouteSpecies(uint16_t species) { ecology.revealSpecies(species); }

void ExploreUi::updateRouteVisual(uint32_t nowMs, bool active) {
    const auto frame = host.exploreRouteFrame();
    if (!frame.generatedMap) return;
    const int cameraX = std::clamp(int(std::lround(frame.routeWorldX)) - Ui::Page::WIDTH / 2,
        0, ExploreMapGenerator::WIDTH * TILE - Ui::Page::WIDTH);
    const int cameraY = std::clamp(int(std::lround(frame.routeWorldY)) - Ui::Page::HEIGHT / 2,
        0, ExploreMapGenerator::HEIGHT * TILE - Ui::Page::HEIGHT);
    // Static child pages can stop scene updates for a long time. Rebase their
    // elapsed time on the first resumed frame instead of catching up movement.
    const bool advance = active && routeVisualActive;
    routeVisualActive = active;
    ecology.update(nowMs, advance, frame.routeWorldX, frame.routeWorldY,
        {float(cameraX), float(cameraY), float(Ui::Page::WIDTH), float(Ui::Page::HEIGHT)});
    const auto grounded = [](uint16_t species) {
        return ExploreEcology::locomotionForSpecies(species) == ExploreEcology::Locomotion::GROUND;
    };
    ExploreGrass::Contact contacts[ExploreGrass::ACTORS]{};
    contacts[0] = {frame.routeWorldX, frame.routeWorldY, frame.leader && grounded(frame.leader->id)};
    contacts[1] = {frame.routeFollowerWorldX, frame.routeFollowerWorldY,
        frame.follower && grounded(frame.follower->id)};
    for (uint8_t i = 0; i < ecology.count(); ++i) {
        const auto& pose = ecology.pose(i);
        contacts[i + 2] = {pose.x, pose.y, !pose.hidden && !pose.airborne &&
            !pose.absent && pose.lift <= 0 && grounded(pose.speciesId)};
    }
    grass.update(*frame.generatedMap, nowMs, advance, contacts, ExploreGrass::ACTORS);
}

uint8_t ExploreUi::appendActors(ExploreTileSemantics::ActorDepth* actors,
                                uint8_t count, uint8_t capacity) const {
    for (uint8_t i = 0; i < ecology.count() && count < capacity; ++i) {
        const auto& pose = ecology.pose(i);
        if (!pose.absent) actors[count++] = {ExploreTileSemantics::ActorRole::WILDLIFE,
            pose.y, i, pose.surface == ExploreEcology::Surface::WATER && !pose.airborne};
    }
    return count;
}

void ExploreUi::drawGround(const ExploreRouteFrame& frame, int cameraX, int cameraY,
                           const ExploreRouteLayout& layout) const {
    const auto& map = *frame.generatedMap;
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        const int x = cell % ExploreMapGenerator::WIDTH * TILE - cameraX;
        const int y = cell / ExploreMapGenerator::WIDTH * TILE - cameraY;
        if (x + TILE <= 0 || x >= layout.width || y + TILE <= 0 || y >= layout.height) continue;
        const auto base = grassFrame(map, grass, cell, map.layers[0][cell]);
        const auto top = grassFrame(map, grass, cell, 391);
        const bool movingBase = base.phase && ExploreTileSemantics::denseGrass(map.layers[0][cell]);
        const bool movingTop = top.phase && map.layers[1][cell] == 391;
        if (!movingBase && !movingTop) continue;
        for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
            const auto id = map.layers[layer][cell];
            if (!id) continue;
            if (artwork.ready && ((layer == 0 && movingBase) || (layer == 1 && movingTop)))
                drawGrass(id, x, y, layer ? top : base, false);
            else ExploreRouteUiContent::drawMapTile(id, x, y, layer, frame.fieldColor,
                uint8_t((frame.nowMs / 140U) % 8U));
        }
    }
}

void ExploreUi::drawCover(const ExploreRouteFrame& frame, float groundY,
    int left, int top, int right, int bottom, int cameraX, int cameraY,
    const ExploreRouteLayout& layout, bool airborne, bool swimming) const {
    left = std::max(0, left); top = std::max(0, top);
    right = std::min(layout.width, right); bottom = std::min(layout.height, bottom);
    if (left >= right || top >= bottom) return;
    const auto& map = *frame.generatedMap;
    auto& canvas = PixelRenderer::canvas();
    canvas.setClipRect(left, top, right - left, bottom - top);
    const int firstX = std::max(0, (cameraX + left) / TILE);
    const int firstY = std::max(0, (cameraY + top) / TILE);
    const int lastX = std::min<int>(ExploreMapGenerator::WIDTH - 1, (cameraX + right - 1) / TILE);
    const int lastY = std::min<int>(ExploreMapGenerator::HEIGHT - 1, (cameraY + bottom - 1) / TILE);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int ty = firstY; ty <= lastY; ++ty) for (int tx = firstX; tx <= lastX; ++tx) {
            const int cell = ty * ExploreMapGenerator::WIDTH + tx;
            const auto id = map.layers[layer][cell];
            if (airborne && !ExploreTileSemantics::upwardLadder(id, layer) &&
                ExploreTileSemantics::foregroundDepthOffsetRows(id) <= 1) continue;
            const int x = tx * TILE - cameraX, y = ty * TILE - cameraY;
            if (artwork.ready && !airborne && layer == 0 && ExploreTileSemantics::denseGrass(id) &&
                groundY < (ty + 1) * TILE)
                drawGrass(id, x, y, grassFrame(map, grass, cell, id), true);
            const bool bridge = swimming && !airborne && layer == 2 && ExploreTileSemantics::bridgeDeck(id);
            if (!bridge && !ExploreTileSemantics::objectOccludes(id, layer, ty, groundY, TILE)) continue;
            if (artwork.ready && id == 391)
                drawGrass(id, x, y, grassFrame(map, grass, cell, id), true);
            else ExploreRouteUiContent::drawMapTile(id, x, y, layer, frame.fieldColor,
                uint8_t((frame.nowMs / 140U) % 8U));
        }
    }
    canvas.clearClipRect();
}

void ExploreUi::drawWildlife(const ExploreRouteFrame& frame, uint8_t index,
    int cameraX, int cameraY, const ExploreRouteLayout& layout) const {
    if (index >= ecology.count()) return;
    const auto& pose = ecology.pose(index);
    if (pose.absent) return;
    const int x = int(std::lround(pose.x)) - cameraX;
    const int ground = int(std::lround(pose.y)) - cameraY;
    // Frame lookup only touches the entry-preloaded species cache.
    bool flip = false;
    const auto* sprite = pose.hidden ? nullptr : wildlifeFrame(pose, flip);
    const int width = sprite ? int(std::lround(sprite->width * SPRITE_SCALE)) : 16;
    const int height = sprite ? int(std::lround(sprite->height * SPRITE_SCALE)) : 16;
    const int offset = sprite ? int(std::lround(PokemonSprites::frameGroundOffsetY(sprite) * SPRITE_SCALE)) : 0;
    const int baseTop = ground - height / 2 - offset;
    const int bodyX = x + pose.sway, bodyTop = baseTop - pose.lift;
    const auto shadow = PokemonShadow::atGround(sprite, bodyX, ground, SPRITE_SCALE, flip);
    const int left = std::min({bodyX - width / 2, x - 14, shadow.x - shadow.radiusX}) - 2;
    const int top = std::min({bodyTop - 8, ground - 10, shadow.y - shadow.radiusY}) - 2;
    const int right = std::max({bodyX + width / 2 + 1, x + 15, shadow.x + shadow.radiusX + 1}) + 2;
    const int bottom = std::max({bodyTop + height, ground + 5, shadow.y + shadow.radiusY + 1}) + 2;
    if (right <= 0 || left >= layout.width || bottom <= 0 || top >= layout.height) return;
    auto& canvas = PixelRenderer::canvas();
    if (pose.hidden) habitatHint(pose, x, ground);
    else if (sprite) {
        if (pose.surface == ExploreEcology::Surface::WATER && !pose.airborne)
            ripple(x, ground, 9, PixelRenderer::rgb(144, 201, 217));
        else canvas.fillEllipse(shadow.x, shadow.y, shadow.radiusX, shadow.radiusY,
            PixelRenderer::rgb(68, 87, 74));
        PokemonSprites::drawFrameScaled(sprite, bodyX - width / 2, bodyTop, SPRITE_SCALE, flip);
        if (pose.activity == ExploreEcology::Activity::DOZE) {
            const int zx = bodyX + 4, zy = bodyTop - 5;
            const auto color = PixelRenderer::rgb(208, 226, 231);
            canvas.drawFastHLine(zx, zy, 4, color);
            canvas.drawLine(zx + 3, zy, zx, zy + 3, color);
            canvas.drawFastHLine(zx, zy + 3, 4, color);
        }
        if (pose.activity == ExploreEcology::Activity::SPLASH ||
            pose.activity == ExploreEcology::Activity::GATHER) {
            ripple(x, ground, 9 + pose.frame % 3 * 2, PixelRenderer::rgb(185, 224, 233));
            if (pose.activity == ExploreEcology::Activity::SPLASH) {
                const int spread = 6 + pose.frame % 3 * 2;
                canvas.drawPixel(x - spread, ground - 4, PixelRenderer::rgb(194, 231, 239));
                canvas.drawPixel(x + spread, ground - 6, PixelRenderer::rgb(194, 231, 239));
            }
        }
    }
    drawCover(frame, pose.y, left, top, right, bottom, cameraX, cameraY, layout,
        pose.airborne, pose.surface == ExploreEcology::Surface::WATER);
}

}  // namespace Ui
