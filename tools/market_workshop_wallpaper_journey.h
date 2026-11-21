#pragma once
#include <cassert>
#include <vector>
#include "core/SaveManager.h"
#include "ui/common/MarketWorkshopUi.h"

namespace WorkshopTest {
inline std::vector<uint16_t> roomBand(int top, int bottom) {
    std::vector<uint16_t> pixels;
    auto& canvas = PixelRenderer::canvas();
    for (int y = top; y < bottom; ++y)
        for (int x = 88; x < 270; ++x) pixels.push_back(canvas.readPixel(x, y));
    return pixels;
}

// Decode the persisted snapshot independently of the in-memory transaction.
inline void expectWallpaper(const Game::GameState& state, unsigned coins, unsigned wallpaper) {
    SaveManager saves;
    Game::GameState loaded;
    MainSceneViewState view{};
    assert(saves.begin() && saves.load(loaded, view));
    const auto check = [&](const Game::GameState& actual) {
        assert(actual.coins == coins);
        assert(actual.wallpaperId == wallpaper);
        assert(actual.room.ownedFurniture == 0x4001);
        assert(actual.room.placedFurniture == 0x4000);
    };
    check(state);
    check(loaded);
}

#ifdef STICKMON_TEST_WALLPAPER_CATALOG
inline constexpr auto homeDayShot = "catalog-10-home-third-day";
inline constexpr auto homeNightShot = "catalog-11-home-third-night";
inline int countColor(int x, int y, int width, int height, uint16_t color) {
    int count = 0;
    auto& canvas = PixelRenderer::canvas();
    for (int py = y; py < y + height; ++py)
        for (int px = x; px < x + width; ++px)
            if (canvas.readPixel(px, py) == color) ++count;
    return count;
}

inline void expectFixtureWall(bool visible) {
    // The fixture's authored primary color must survive actual compositing.
    // This region only contains room pixels in both home and workshop layouts.
    const auto pink = PixelRenderer::rgb(240, 40, 136);
    const int pixels = countColor(88, 100, 182, 70, pink);
    if (visible) assert(pixels > 200);
    else assert(pixels == 0);
}

template<class Tap, class Swipe, class Shot, class SetHour, class Advance, class Save, class FailWrites>
void expandedJourney(bool round, Ui::MarketWorkshopUi& ui, Game::GameState& state,
                     Tap tap, Swipe swipe, Shot shot, SetHour setHour, Advance advance,
                     Save save, FailWrites failWrites) {
    static_assert(Game::MarketWorkshop::WALLPAPER_COUNT == 6);
    Ui::MarketWorkshopView layout{round ? 360 : 368, round ? 360 : 448, round};
    const auto strip = layout.swatchViewport();
    const auto menu = layout.row(), back = layout.back(), buy = layout.action();
    const auto first = layout.swatch(0, 0), third = layout.swatch(2, 0);
    const int pitch = (third.x - first.x) / 2;
    const int stripY = first.y + first.h / 2;
    const int right = strip.x + strip.w - 4;
    const auto tapCenter = [&](auto r) { tap(r.x + r.w / 2, r.y + r.h / 2); };
    const auto selected = [&] { return static_cast<uint8_t>(ui.selectedWallpaper()); };
    const auto enter = [&] {
        tapCenter(menu);
        assert(ui.page() == Ui::MarketWorkshopUi::Page::WALLPAPER);
    };
    const auto leave = [&] {
        tapCenter(back);
        assert(ui.page() == Ui::MarketWorkshopUi::Page::MENU);
    };
    const auto pan = [&](int x, int dx, auto during) {
        swipe(x, stripY, x + dx, stripY, during);
        advance(200);
    };
    const auto nothing = [] {};
    const auto gold = PixelRenderer::rgb(250, 218, 116);
    const auto frames = [&](auto tile) { return countColor(tile.x, tile.y, tile.w, tile.h, gold); };
    const auto assertStripClip = [&] {
        for (auto color : {PixelRenderer::rgb(240,40,136), PixelRenderer::rgb(32,168,224),
                           PixelRenderer::rgb(168,200,64), PixelRenderer::rgb(160,112,216), gold}) {
            assert(countColor(0, strip.y, strip.x, strip.h, color) == 0);
            assert(countColor(strip.x + strip.w, strip.y, layout.width - strip.x - strip.w,
                              strip.h, color) == 0);
        }
    };

    setHour(12);
    enter();
    assert(selected() == 0);
    shot("catalog-01-first");
    assert(frames(first) > 10);
    assertStripClip();
    expectFixtureWall(false);
    const auto before = roomBand(strip.y, strip.y + strip.h);
    pan(right, -2 * pitch, [&] {
        shot("catalog-02-drag");
        assert(selected() == 0 && roomBand(strip.y, strip.y + strip.h) != before);
        assertStripClip();
    });
    assert(selected() == 37);
    shot("catalog-03-drag-third");
    expectFixtureWall(true);
    tapCenter(buy);
    expectWallpaper(state, 0, 0);
    leave();

    state.coins = 455;
    assert(save());
    enter();
    assert(selected() == 0);
    tapCenter(third);
    assert(selected() == 37);
    shot("catalog-04-tap-third");
    expectFixtureWall(true);
    failWrites(true);
    tapCenter(buy);
    failWrites(false);
    expectWallpaper(state, 455, 0);
    shot("catalog-05-write-failed");
    tapCenter(buy);
    expectWallpaper(state, 318, 37);
    tapCenter(buy);
    expectWallpaper(state, 318, 37);
    // Start and finish inside the room to move through all remaining entries.
    for (auto id : {91, 8, 64}) {
        swipe(230, 160, 140, 160, nothing);
        advance(200);
        assert(selected() == id);
    }
    tapCenter(buy);
    expectWallpaper(state, 248, 64);
    advance(1500);
    leave();
    enter();
    assert(selected() == 64);
    shot("catalog-06-reopened-last");
    const auto last = layout.swatch(5, layout.maxSwatchOffset());
    assert(last.x >= strip.x && last.x + last.w <= strip.x + strip.w);
    assert(frames(last) > 10);
    assertStripClip();
    // Tapping the name area and dragging farther at the end preserve selection and scroll.
    tap(strip.x + strip.w + 12, stripY);
    pan(right, -2 * pitch, nothing);
    assert(selected() == 64);
    shot("catalog-07-end-clamped");
    assert(frames(last) > 10);
    assertStripClip();
    for (int i = 0; i < 3; ++i) pan(strip.x + 4, 2 * pitch, nothing);
    assert(selected() == 0);
    shot("catalog-08-start-clamped");
    assert(frames(first) > 10);
    assertStripClip();
    tapCenter(buy);
    expectWallpaper(state, 248, 0);
    tapCenter(third);
    assert(selected() == 37);
    tapCenter(buy);
    expectWallpaper(state, 111, 37);
    advance(1500);
    shot("catalog-09-repurchased");
    expectFixtureWall(true);
    leave();
}
#else
inline constexpr auto homeDayShot = "wallpaper-10-home-forest-day";
inline constexpr auto homeNightShot = "wallpaper-11-home-forest-night";
#endif

// Drive both production event handlers with the same actions, including a
// failed durable purchase and a fresh decode after each material state change.
template<class Tap, class Swipe, class Shot, class SetHour, class Advance, class Save, class FailWrites>
void wallpaperJourney(bool round, Ui::MarketWorkshopUi& ui, Game::GameState& state,
                      Tap tap, Swipe swipe, Shot render, SetHour setHour, Advance advance,
                      Save save, FailWrites failWrites) {
    auto shot = [&](const char* filename) {
        render(filename);
        if (ui.page() != Ui::MarketWorkshopUi::Page::WALLPAPER) return;
        const int divider = round ? 211 : 271;
        const int stripY = round ? 216 : 294;
        const int stripHeight = round ? 52 : 60;
        auto& canvas = PixelRenderer::canvas();
        for (int y = stripY+10; y <= stripY+stripHeight-10; ++y) {
            assert(canvas.readPixel(divider, y) == PixelRenderer::rgb(65,99,100));
            assert(canvas.readPixel(divider+1, y) == PixelRenderer::rgb(65,99,100));
            assert(canvas.readPixel(divider+2, y) != PixelRenderer::rgb(65,99,100));
        }
    };
#ifdef STICKMON_TEST_WALLPAPER_CATALOG
    expandedJourney(round, ui, state, tap, swipe, shot, setHour, advance, save, failWrites);
#else
    using P = Ui::MarketWorkshopUi::Page;
    const int menuY = round ? 210 : 286;
    const Ui::MarketWorkshopView layout{round ? 360 : 368, round ? 360 : 448, round};
    const auto strip = layout.swatchViewport();
    const int pitch = layout.swatchPitch();
    const int swatchY = strip.y + strip.h/2;
    const auto selectedX = [&](Game::MarketWorkshop::Wallpaper id) {
        const auto tile = layout.swatch(int(Game::MarketWorkshop::wallpaperIndex(id)), ui.swatchOffset());
        return tile.x + tile.w/2;
    };
    const auto forest = Game::MarketWorkshop::Wallpaper::FOREST;
    const auto base = Game::MarketWorkshop::Wallpaper::ORIGINAL;
    const int applyY = round ? 302 : 400;
    const auto back = layout.back();
    const int backX = back.x + back.w/2, backY = back.y + back.h/2;
    const int band = round ? 130 : 157;
    const auto expect = [&](unsigned coins, unsigned wallpaper) { expectWallpaper(state, coins, wallpaper); };

    assert(ui.page() == P::MENU);
    shot("wallpaper-01-workshop");
    const auto furnitureRow=layout.furnitureRow(), furnitureBack=layout.furnitureBack();
    tap(180, furnitureRow.y+furnitureRow.h/2);
    assert(ui.page() == P::FURNITURE);
    shot("workshop-furniture");
    tap(furnitureBack.x+furnitureBack.w/2, furnitureBack.y+furnitureBack.h/2);
    assert(ui.page() == P::MENU);
    expect(0, 0);
    setHour(12);
    tap(180, menuY);
    assert(ui.page() == P::WALLPAPER);
    shot("wallpaper-02-original");
    const auto original = roomBand(band, band + 20);
    swipe(strip.x + pitch + 5, swatchY, strip.x + 5, swatchY, [] {});
    shot("wallpaper-03-forest-day");
    const auto forestDay = roomBand(band, band + 20);
    assert(forestDay != original && "The preview must draw the selected wallpaper");
    tap(180, applyY);
    expect(0, 0);
    swipe(strip.x + 5, swatchY, strip.x + pitch + 5, swatchY, [] {});
    shot("wallpaper-04-swiped-original");
    assert(roomBand(band, band + 20) == original);
    tap(selectedX(forest), swatchY);
    tap(backX, backY);
    assert(ui.page() == P::MENU);
    tap(180, menuY);
    shot("wallpaper-05-after-back");
    assert(roomBand(band, band + 20) == original && "Backing out discards the trial");
    expect(0, 0);

    state.coins = 850;
    assert(save());
    // Preview each shipped finish using the same touch paths and resource pack as the device.
    const auto& catalog = Game::MarketWorkshop::WALLPAPERS;
    const auto step = [&](int direction) {
        swipe(180, 160, 180 - direction*60, 160, [] {});
        advance(200);
    };
    for (size_t i = Game::MarketWorkshop::wallpaperIndex(base); i > 0; --i) step(-1);
    for (size_t i = 0; i < Game::MarketWorkshop::WALLPAPER_COUNT; ++i) {
        if (i) step(1);
        const auto id = catalog[i].id;
        assert(ui.selectedWallpaper() == id);
        assert(RoomRenderer::preparePreview(false, Game::HomeFurniture::visibleMask(state), id));
        char filename[40];
        std::snprintf(filename, sizeof(filename), "wallpaper-preview-%u", unsigned(id));
        shot(filename);
        expect(850, 0);
    }
    tap(backX, backY);
    tap(180, menuY);
    tap(selectedX(forest), swatchY);
    shot("wallpaper-05b-forest-for-sale");
    setHour(22);
    shot("wallpaper-06-forest-night");
    assert(roomBand(band, band + 20) != forestDay);
    expect(850, 0);
    setHour(12);
    failWrites(true);
    tap(180, applyY);
    failWrites(false);
    assert(ui.page() == P::WALLPAPER);
    expect(850, 0);
    shot("wallpaper-07-save-failed");

    tap(180, applyY);
    expect(550, 1);
    advance(1500);
    shot("wallpaper-08-purchased");
    tap(180, applyY);
    expect(550, 1);
    tap(selectedX(base), swatchY);
    failWrites(true);
    tap(180, applyY);
    failWrites(false);
    expect(550, 1);
    tap(180, applyY);
    expect(550, 0);
    tap(selectedX(forest), swatchY);
    shot("wallpaper-09-buy-again");
    tap(180, applyY);
    expect(250, 1);
    tap(backX, backY);
    assert(ui.page() == P::MENU);
#endif
}
} // namespace WorkshopTest
