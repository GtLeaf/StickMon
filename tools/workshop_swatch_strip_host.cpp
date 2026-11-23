// Drive the production workshop controller with the expanded catalog.
#include <cassert>
#include "ui/common/MarketWorkshopUi.h"

using namespace Game::MarketWorkshop;
using Workshop = Ui::MarketWorkshopUi;
using Type = TouchTap::Type;

// This host exercises input. The application journeys cover actual assets and saves.
void Ui::HomePhotoFrame::enter(bool) {}
bool RoomRenderer::preparePreview(bool, uint16_t, Wallpaper) { return true; }

static void run(Ui::MarketWorkshopView layout) {
    static_assert(WALLPAPER_COUNT == 6);
    const int pitch = layout.swatchPitch();
    const int slots = layout.round ? 3 : 4;
    const auto viewport = layout.swatchViewport();
    assert(viewport.w > pitch * slots && viewport.w < pitch * (slots + 1));
    for (int offset=0; offset<=layout.maxSwatchOffset(); ++offset) {
        for (int i=0; i<layout.swatchCount; ++i) {
            const int settled=layout.settledOffset(offset,i);
            assert(settled>=0 && settled<=layout.maxSwatchOffset());
            assert(settled==layout.maxSwatchOffset() || settled%pitch==0);
            const auto tile=layout.swatch(i,settled);
            assert(tile.x>=viewport.x && tile.x+tile.w<=viewport.x+viewport.w);
        }
    }
    const int y = viewport.y + viewport.h/2;
    const int x = viewport.x + viewport.w - 5;
    uint32_t time = 0;
    Workshop ui(layout);
    Game::GameState state{};
    state.coins = 1000;
    grantFrame(state);
    auto event = [&](Type type, int px, int py) {
        time += 20;
        return ui.touch({type, int16_t(px), int16_t(py), time}, state);
    };
    auto tap = [&](int px, int py) {
        assert(event(Type::DOWN, px, py) == Workshop::Request::NONE);
        return event(Type::UP, px, py);
    };
    auto at = [&](int i) { assert(ui.selectedWallpaper() == WALLPAPERS[i].id); };
    auto drag = [&](int start, int end) {
        event(Type::DOWN, start, y);
        event(Type::MOVE, end, y);
        assert(event(Type::UP, end, y) == Workshop::Request::NONE);
        ui.update(250);
    };
    auto open = [&] {
        ui.open(state);
        ui.update(400);
        const auto row = layout.row();
        tap(row.x + row.w/2, row.y + row.h/2);
        assert(ui.page() == Workshop::Page::WALLPAPER);
    };

    open();
    at(0);
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - 18, y);
    assert(ui.swatchOffset() == 18);
    event(Type::MOVE, x - pitch - 12, y);
    assert(ui.swatchOffset() == pitch + 12);
    at(0);
    event(Type::UP, x - pitch - 12, y);
    at(1);
    assert(!ui.update(1));
    assert(ui.update(40));
    assert(ui.swatchOffset() > pitch && ui.swatchOffset() < pitch+12);
    ui.update(250);
    assert(!ui.update(20) && ui.swatchOffset()==pitch);

    drag(x, x - 2*pitch);
    at(3);
    drag(x, x - 2*pitch);
    at(5);
    drag(x, x - pitch);
    at(5);
    // Reversing at the limit selects the adjacent catalog entry.
    drag(x - pitch, x);
    at(4);

    open();
    tap(viewport.x+viewport.w+3, y);
    at(0);
    tap(viewport.x+pitch-2, y);
    at(0);
    tap(x, y);  // The partial swatch is visible and selectable.
    at(slots);
    ui.update(250);
    open();
    // A captured drag must never become a purchase, even on release over the button.
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - pitch, y);
    const auto buy = layout.action();
    assert(event(Type::UP, buy.x + buy.w/2, buy.y + 15) == Workshop::Request::NONE);
    at(0);
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - pitch, y);
    ui.cancelTouch();
    assert(ui.swatchOffset()==0);
    event(Type::UP, x - pitch, y);
    at(0);
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - pitch, y);
    event(Type::CANCEL, 0, 0);
    event(Type::UP, x - pitch, y);
    at(0);
    event(Type::DOWN, x, y);
    event(Type::UP, -1, y);
    event(Type::UP, x - pitch, y);
    at(0);
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - pitch, y);
    // A new DOWN supersedes the lost contact before receiving its own release.
    event(Type::DOWN, viewport.x+15, y);
    assert(ui.swatchOffset() == 0);
    event(Type::UP, viewport.x+15, y);
    at(0);
    // A drag back to its origin cannot turn into a tap on the partial item.
    event(Type::DOWN, x, y);
    event(Type::MOVE, x - pitch, y);
    event(Type::MOVE, x, y);
    event(Type::UP, x, y);
    at(0);
    assert(state.coins == 1000 && equippedWallpaper(state) == WALLPAPERS[0].id);

    applyWallpaper(state, WALLPAPERS[5].id);
    open();
    at(5);
    tap(x - pitch, y);  // Reentry reveals the last swatch in full.
    at(4);
    const auto room = layout.room();
    event(Type::DOWN, 110, room.y+35);
    event(Type::UP, 230, room.y+35);
    at(3);
    tap(layout.back().x + 22, layout.back().y + 20);
    assert(ui.page() == Workshop::Page::MENU);
    open();
    at(5);
}

int main() {
    run({368,448,false});
    run({360,360,true});
}
