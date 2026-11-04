#include "ui/common/HomeUiContent.h"

#include <algorithm>
#include <cstdlib>
#include "assets/GameAssets.h"
#include "core/MathUtil.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"
#include "presentation/PokemonShadow.h"
#include "presentation/TutorialOverlay.h"

namespace Ui::HomeUiContent {
namespace {

uint8_t rgb565R(uint16_t color) {
    return static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
}

uint8_t rgb565G(uint16_t color) {
    return static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
}

uint8_t rgb565B(uint16_t color) {
    return static_cast<uint8_t>((color & 0x1F) * 255 / 31);
}

uint16_t blendRgb565(uint16_t destination, uint16_t source, uint8_t alpha) {
    const uint8_t inverse = 255 - alpha;
    const auto r = static_cast<uint8_t>(
        (rgb565R(source) * alpha + rgb565R(destination) * inverse) / 255);
    const auto g = static_cast<uint8_t>(
        (rgb565G(source) * alpha + rgb565G(destination) * inverse) / 255);
    const auto b = static_cast<uint8_t>(
        (rgb565B(source) * alpha + rgb565B(destination) * inverse) / 255);
    return PixelRenderer::rgb(r, g, b);
}

void fillRadialLightAlpha(int centerX, int centerY, int radiusX, int radiusY,
                          uint16_t color, uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    auto& canvas = PixelRenderer::canvas();
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        const float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            const float dx = static_cast<float>(px - centerX) / radiusX;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared > 1.0f) continue;
            const auto alpha = static_cast<uint8_t>(maxAlpha * (1.0f - distanceSquared));
            if (alpha == 0) continue;
            const auto background = static_cast<uint16_t>(canvas.readPixel(px, py));
            canvas.drawPixel(px, py, blendRgb565(background, color, alpha));
        }
    }
}

void fillEllipseAlpha(int centerX, int centerY, int radiusX, int radiusY,
                      uint16_t color, uint8_t alpha) {
    if (radiusX <= 0 || radiusY <= 0 || alpha == 0) return;
    auto& canvas = PixelRenderer::canvas();
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        const float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            const float dx = static_cast<float>(px - centerX) / radiusX;
            if (dx * dx + dy * dy > 1.0f) continue;
            const auto background = static_cast<uint16_t>(canvas.readPixel(px, py));
            canvas.drawPixel(px, py, blendRgb565(background, color, alpha));
        }
    }
}

int textPixelWidth(const char* value) {
    int width = 0;
    const auto* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

void drawFood(const HomeFrame& frame, const Camera& camera) {
    if (!frame.foodPresent) return;
    const uint8_t maxBites = Game::roomFoodBitesPerServing(frame.foodIndex);
    const uint8_t remaining = MathUtil::min<uint8_t>(maxBites, frame.foodBites);
    const int cx = screenX(frame.foodX, camera);
    const int foodY = screenY(frame.foodY, camera);
    static constexpr int FOOD_LEFT = -9 * RoomRenderer::PIXEL_SCALE;
    static constexpr int FOOD_WIDTH = 18 * RoomRenderer::PIXEL_SCALE;
    const int visibleWidth = maxBites == 0 ? 0
        : (FOOD_WIDTH * remaining + maxBites - 1) / maxBites;
    if (visibleWidth <= 0) return;
    auto& canvas = PixelRenderer::canvas();
    canvas.setClipRect(cx + FOOD_LEFT, foodY - 12 * RoomRenderer::PIXEL_SCALE,
        visibleWidth, 20 * RoomRenderer::PIXEL_SCALE);
    if (!GameAssets::drawCentered(
            GameAssets::itemKind(Game::itemIdForFoodIndex(frame.foodIndex)),
            cx, foodY - 3 * RoomRenderer::PIXEL_SCALE, 0.5f * RoomRenderer::PIXEL_SCALE)) {
        canvas.fillEllipse(cx, foodY, 8 * RoomRenderer::PIXEL_SCALE,
            3 * RoomRenderer::PIXEL_SCALE, PixelRenderer::rgb(238, 76, 56));
    }
    canvas.clearClipRect();
}

void drawShadow(const HomeActorVisual& actor, bool night, const Camera& camera) {
    if (actor.hidden || actor.shadowHidden || !actor.frame) return;
    const auto shadow = PokemonShadow::atGround(actor.frame,
        screenX(actor.x, camera) + actor.offsetX * RoomRenderer::PIXEL_SCALE,
        spriteGroundY(actor.y, actor.groundOffsetY, actor.frame, camera) +
            actor.offsetY * RoomRenderer::PIXEL_SCALE,
        RoomRenderer::PIXEL_SCALE, actor.shadowFlip);
    int rx = shadow.radiusX;
    int ry = shadow.radiusY;
    if (night) {
        rx = (rx * 11 + 5) / 10;
        ry = (ry * 11 + 5) / 10;
    }
    const uint16_t color = night ? PixelRenderer::rgb(18, 16, 24)
        : PixelRenderer::rgb(36, 29, 24);
    const uint8_t outerAlpha = night ? (actor.floating ? 84 : 122)
        : (actor.floating ? 68 : 116);
    fillRadialLightAlpha(shadow.x, shadow.y, rx, ry, color, outerAlpha);
    if (!actor.floating) {
        fillEllipseAlpha(shadow.x, shadow.y,
            MathUtil::max(5 * RoomRenderer::PIXEL_SCALE, rx / 2),
            MathUtil::max(2 * RoomRenderer::PIXEL_SCALE, ry / 2), color,
            night ? 92 : 86);
    }
}

void drawSleepZ(int x, int spriteTopY, uint32_t nowMs) {
    static constexpr uint16_t CYCLE_MS = 1600;
    for (uint8_t i = 0; i < 2; ++i) {
        const uint32_t phase = (nowMs + i * (CYCLE_MS / 2)) % CYCLE_MS;
        const float t = static_cast<float>(phase) / CYCLE_MS;
        const int zx = x + 8 + static_cast<int>(t * 4.0f) + i * 3;
        const int zy = spriteTopY - 7 - static_cast<int>(t * 9.0f) - i * 2;
        const uint16_t color = t < 0.34f ? PixelRenderer::rgb(255, 255, 255)
            : (t < 0.67f ? PixelRenderer::rgb(176, 176, 184)
                         : PixelRenderer::rgb(112, 112, 124));
        PixelRenderer::text(zx, zy, "Z", color, 1);
    }
}

void drawActor(const HomeActorVisual& actor, uint32_t nowMs, const Camera& camera) {
    if (actor.hidden || !actor.frame) return;
    const int x = screenX(actor.x, camera) + actor.offsetX * RoomRenderer::PIXEL_SCALE;
    const int y = spriteGroundY(actor.y - actor.drop - actor.lift - actor.hop,
        actor.groundOffsetY, actor.frame, camera) +
        (actor.offsetY - PokemonSprites::frameGroundOffsetY(actor.frame)) *
            RoomRenderer::PIXEL_SCALE;
    const int width = FlashStorage::readByte(&actor.frame->width);
    const int height = FlashStorage::readByte(&actor.frame->height);
    const int top = y - (height / 2) * RoomRenderer::PIXEL_SCALE;
    PokemonSprites::drawFrameScaled(actor.frame,
        x - (width / 2) * RoomRenderer::PIXEL_SCALE, top,
        RoomRenderer::PIXEL_SCALE, actor.flip);
    if (actor.sleepZ) drawSleepZ(x, top, nowMs);
}

void drawHearts(const HomeFrame& frame, const Camera& camera) {
    if (frame.main.hidden || frame.hearts == 0 || !frame.main.frame) return;
    auto& canvas = PixelRenderer::canvas();
    const int baseX = screenX(frame.main.x, camera) + 17 * RoomRenderer::PIXEL_SCALE;
    const int baseY = screenY(frame.main.y - frame.lightLift - frame.heartLift, camera)
        - 30 * RoomRenderer::PIXEL_SCALE - (frame.heartPulse ? 2 : 0);
    const uint16_t color = PixelRenderer::rgb(255, 103, 135);
    const uint16_t outline = PixelRenderer::rgb(204, 204, 204);
    const auto fillHeart = [&](int x, int y, bool small, uint16_t fill) {
        const int radius = small ? 2 : 3;
        const int spread = small ? 4 : 5;
        const int tipY = small ? y + 7 : y + 9;
        canvas.fillCircle(x, y, radius, fill);
        canvas.fillCircle(x + spread, y, radius, fill);
        canvas.fillTriangle(x - radius, y + 2, x + spread + radius, y + 2,
            x + spread / 2, tipY, fill);
    };
    const auto drawHeart = [&](int x, int y, bool small) {
        for (int dy = -2; dy <= 2; ++dy) {
            const int horizontal = 2 - std::abs(dy);
            for (int dx = -horizontal; dx <= horizontal; ++dx)
                fillHeart(x + dx, y + dy, small, outline);
        }
        fillHeart(x, y, small, color);
    };
    drawHeart(baseX, baseY, false);
    if (frame.hearts == 2) drawHeart(baseX - 11, baseY + 7, true);
}

void drawLight(const HomeFrame& frame, const Camera& camera) {
    if (frame.lightSource == 0) return;
    int x = screenX(frame.main.x, camera);
    int y = screenY(frame.main.y - frame.lightLift - 10.0f, camera);
    switch (frame.lightSource) {
    case 1: x = 44; y = 30; break;
    case 2: x = Hal::DISPLAY_W / 2; y = 20; break;
    case 3: x = Hal::DISPLAY_W - 44; y = 30; break;
    case 4: x = 34; y = Hal::DISPLAY_H / 2; break;
    case 5: x = Hal::DISPLAY_W - 34; y = Hal::DISPLAY_H / 2; break;
    default: break;
    }
    if (frame.night) {
        fillRadialLightAlpha(x, y, 58, 42, PixelRenderer::rgb(255, 210, 128), 88);
        fillRadialLightAlpha(x, y, 112, 76, PixelRenderer::rgb(255, 151, 92), 30);
    } else {
        fillRadialLightAlpha(x, y, 68, 46, PixelRenderer::rgb(255, 226, 150), 30);
        fillRadialLightAlpha(x, y, 118, 76, PixelRenderer::rgb(255, 176, 96), 10);
    }
    auto& canvas = PixelRenderer::canvas();
    canvas.fillCircle(x, y, 3, PixelRenderer::rgb(255, 236, 158));
    canvas.drawCircle(x, y, 5, PixelRenderer::rgb(255, 169, 79));
}

void drawContactAt(const HomeFrame& frame, int X, int BOTTOM, int WIDTH) {
    if (!frame.contactVisible) return;
    static constexpr int RADIUS = 4;
    const int height = frame.contactChoice ? 45 : 28;
    const int y = BOTTOM - height;
    auto& canvas = PixelRenderer::canvas();
    fillRoundRectAlpha(X, y, WIDTH, height, RADIUS, PixelRenderer::rgb(20, 24, 31), 153);
    canvas.drawRoundRect(X, y, WIDTH, height, RADIUS, PixelRenderer::rgb(236, 239, 230));
    const int textX = X + MathUtil::max(6, (WIDTH - textPixelWidth(frame.contactText)) / 2);
    PixelRenderer::text(textX, y + 6, frame.contactText, PixelRenderer::rgb(241, 242, 232), 1);
    if (frame.contactChoice) {
        const uint16_t selected = PixelRenderer::rgb(255, 216, 72);
        const uint16_t normal = PixelRenderer::rgb(148, 156, 169);
        PixelRenderer::text(X + 71, y + 27, Ui::ContactVisit::YES,
            frame.contactYes ? selected : normal, 1);
        PixelRenderer::text(X + 149, y + 27, Ui::ContactVisit::NO,
            frame.contactYes ? normal : selected, 1);
    }
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void drawWalkBoundary(const HomeFrame& frame, const Camera& camera) {
    if (!frame.walkBoundaryVisible) return;
    auto& room = RoomResource::ins();
    const uint8_t count = room.walkPolygonCount();
    if (count < 2) return;
    auto& canvas = PixelRenderer::canvas();
    const uint16_t outline = PixelRenderer::rgb(0, 0, 0);
    const uint16_t red = PixelRenderer::rgb(255, 32, 32);
    int16_t previousX = screenX(room.walkPoint(0).x, camera);
    int16_t previousY = screenY(room.walkPoint(0).y, camera);
    for (uint8_t i = 1; i <= count; ++i) {
        const uint8_t index = i == count ? 0 : i;
        const int16_t x = screenX(room.walkPoint(index).x, camera);
        const int16_t y = screenY(room.walkPoint(index).y, camera);
        canvas.drawLine(previousX - 1, previousY, x - 1, y, outline);
        canvas.drawLine(previousX + 1, previousY, x + 1, y, outline);
        canvas.drawLine(previousX, previousY - 1, x, y - 1, outline);
        canvas.drawLine(previousX, previousY + 1, x, y + 1, outline);
        canvas.drawLine(previousX, previousY, x, y, red);
        canvas.fillCircle(x, y, 2, red);
        previousX = x;
        previousY = y;
    }
    const uint16_t routeColor = PixelRenderer::rgb(255, 216, 72);
    int routePreviousX = screenX(frame.main.x, camera);
    int routePreviousY = screenY(frame.main.y, camera);
    for (uint8_t i = frame.routeIndex; i < frame.routeCount; ++i) {
        const int x = screenX(frame.routeX[i], camera);
        const int y = screenY(frame.routeY[i], camera);
        canvas.drawLine(routePreviousX, routePreviousY, x, y, routeColor);
        canvas.fillCircle(x, y, 2, routeColor);
        routePreviousX = x;
        routePreviousY = y;
    }
    canvas.drawCircle(screenX(frame.targetX, camera), screenY(frame.targetY, camera), 4,
        PixelRenderer::rgb(64, 210, 255));
    PixelRenderer::text(2, 2, frame.debugText, 0xFFFF, 1);
}
#endif

}  // namespace

int16_t screenX(float x, const Camera& camera) {
    return RoomRenderer::screenX(x, camera.x);
}

int16_t screenY(float y, const Camera& camera) {
    return RoomRenderer::screenY(y, camera.y);
}

int16_t spriteGroundY(float y, float groundOffset,
                     const PokemonSprites::SpriteFrame* frame, const Camera& camera) {
    const float offset = RoomRenderer::PIXEL_SCALE == 1
        ? PokemonSprites::frameGroundOffsetY(frame) : groundOffset;
    return screenY(y + offset, camera);
}

void fillRoundRectAlpha(int x, int y, int w, int h, int radius,
                        uint16_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;
    auto& canvas = PixelRenderer::canvas();
    radius = MathUtil::max(0, MathUtil::min(radius, MathUtil::min(w, h) / 2));
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        for (int px = x; px < x + w; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            const int localX = px - x;
            const int localY = py - y;
            bool inside = radius == 0 || (localX >= radius && localX < w - radius) ||
                (localY >= radius && localY < h - radius);
            if (!inside) {
                const int centerX = localX < radius ? radius : w - radius - 1;
                const int centerY = localY < radius ? radius : h - radius - 1;
                const int dx = localX - centerX;
                const int dy = localY - centerY;
                inside = dx * dx + dy * dy <= radius * radius;
            }
            if (!inside) continue;
            const auto background = static_cast<uint16_t>(canvas.readPixel(px, py));
            canvas.drawPixel(px, py, blendRgb565(background, color, alpha));
        }
    }
}

void drawContact(const HomeFrame& frame, int x, int bottom, int width) {
    drawContactAt(frame, x, bottom, width);
}

void drawHudRow(const HomeHudRow& row, int statusX, int rowY) {
    auto& canvas = PixelRenderer::canvas();
    HudRenderer::drawHungerIcon(canvas, statusX + 6, rowY + 2, row.hunger);
    const int x = statusX + 29;
    const int y = rowY + 6;
    canvas.fillRect(x, y, 31, 6, PixelRenderer::rgb(39, 45, 50));
    const int fillWidth = 29 * row.hp / 100;
    const uint16_t fillColor = row.hp > 50 ? PixelRenderer::rgb(92, 222, 112)
        : (row.hp > 20 ? PixelRenderer::rgb(246, 204, 72)
                       : PixelRenderer::rgb(232, 80, 84));
    if (fillWidth > 0) canvas.fillRect(x + 1, y + 1, fillWidth, 4, fillColor);
    canvas.drawRect(x, y, 31, 6, PixelRenderer::rgb(220, 224, 218));
}

void drawTutorial(const HomeFrame& frame) {
    if (!frame.tutorialVisible || Platform::input().touchAvailable()) return;
    TutorialOverlay::draw(frame.tutorialButton == 0 ? TutorialOverlay::Button::A
                                                   : TutorialOverlay::Button::B,
        frame.tutorialText, frame.tutorialLongPress);
}

void drawLayer(HomeLayer layer, const HomeFrame& frame, const Camera& camera) {
    switch (layer) {
    case HomeLayer::BACKGROUND: RoomRenderer::draw(camera.y, frame.night, camera.x); break;
    case HomeLayer::FOOD: drawFood(frame, camera); break;
    case HomeLayer::MAIN_SHADOW: drawShadow(frame.main, frame.night, camera); break;
    case HomeLayer::VISITOR_SHADOW: drawShadow(frame.visitor, frame.night, camera); break;
    case HomeLayer::MAIN_ACTOR: drawActor(frame.main, frame.nowMs, camera); break;
    case HomeLayer::VISITOR_ACTOR: drawActor(frame.visitor, frame.nowMs, camera); break;
    case HomeLayer::HEARTS: drawHearts(frame, camera); break;
    case HomeLayer::LIGHT: drawLight(frame, camera); break;
#if STICKMON_ENABLE_DEBUG_FEATURES
    case HomeLayer::WALK_BOUNDARY: drawWalkBoundary(frame, camera); break;
#endif
    case HomeLayer::CONTACT: drawContact(frame); break;
    case HomeLayer::TUTORIAL: drawTutorial(frame); break;
    default: break;
    }
}

int touchTarget(int16_t x, int16_t y, const HomeFrame& frame,
                const Camera& camera, bool menuHit) {
    if (frame.touchBlocked) return -1;
    if (menuHit) return 2;
    const auto view = RoomRenderer::viewport();
    if (x < view.x || y < view.y || x >= view.x + view.width || y >= view.y + view.height)
        return -1;
    if (std::abs(x - screenX(frame.foodX, camera)) <= 19 &&
        std::abs(y - screenY(frame.foodY, camera)) <= 18) return 1;
    const HomeActorVisual& actor = frame.main;
    if (actor.hidden || !actor.frame) return -1;
    const int cx = screenX(actor.x, camera) + actor.offsetX * RoomRenderer::PIXEL_SCALE;
    const int cy = spriteGroundY(actor.y - actor.drop - actor.lift - actor.hop,
        actor.groundOffsetY, actor.frame, camera) +
        (actor.offsetY - PokemonSprites::frameGroundOffsetY(actor.frame)) *
            RoomRenderer::PIXEL_SCALE;
    const int width = FlashStorage::readByte(&actor.frame->width) * RoomRenderer::PIXEL_SCALE;
    const int height = FlashStorage::readByte(&actor.frame->height) * RoomRenderer::PIXEL_SCALE;
    const auto bounds = PokemonSprites::frameVisibleBounds(actor.frame);
    const int left = actor.flip
        ? FlashStorage::readByte(&actor.frame->width) - bounds.left - bounds.width : bounds.left;
    const int visibleX = cx - width / 2 + (left + bounds.width / 2) * RoomRenderer::PIXEL_SCALE;
    const int visibleY = cy - height / 2 + (bounds.top + bounds.height / 2) * RoomRenderer::PIXEL_SCALE;
    const int rx = std::max(20, bounds.width * RoomRenderer::PIXEL_SCALE / 2 + 4);
    const int ry = std::max(22, bounds.height * RoomRenderer::PIXEL_SCALE / 2 + 4);
    return std::abs(x - visibleX) <= rx && std::abs(y - visibleY) <= ry ? 0 : -1;
}

}  // namespace Ui::HomeUiContent
