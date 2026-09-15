// Public renderer contracts. Expected geometry is deliberately independent of UiMetrics.
#include "HomeScreen.h"
#include "core/FontResource.h"
#include "core/UiStrings.h"
#include "game/Species.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"
#include "ui/RenderCaches.h"
#include "ui/UiCommon.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

using namespace AmoledV1;

namespace {

using Frame = std::vector<uint16_t>;

Frame capture(Canvas565& canvas) {
    Frame result;
    for (int y = 0; y < AmoledUi::HEIGHT; ++y) {
        for (int x = 0; x < AmoledUi::WIDTH; ++x) result.push_back(canvas.readPixel(x, y));
    }
    return result;
}

void expectPixel(Canvas565& canvas, int x, int y, uint16_t expected) {
    const auto actual = canvas.readPixel(x, y);
    if (actual != expected) {
        std::fprintf(stderr, "pixel (%d,%d): expected %04x, got %04x\n",
                     x, y, expected, actual);
        std::abort();
    }
}

template<class Hit>
void expectRect(Hit hit, int left, int top, int right, int bottom) {
    // Include the entire perimeter and one pixel outside it, including gaps.
    for (int y = top - 1; y <= bottom; ++y) {
        for (int x = left - 1; x <= right; ++x) {
            const bool expected = x >= left && x < right && y >= top && y < bottom;
            if (hit(x, y) != expected) {
                std::fprintf(stderr, "hit (%d,%d): expected %d\n", x, y, expected);
                std::abort();
            }
        }
    }
}

void exploreButtons(Canvas565& canvas, Game::GameState&) {
    expectRect(exploreStartAt, 20, 388, 176, 436);
    expectRect(exploreSelectionBackAt, 192, 388, 348, 436);
    PixelCache565 cache;
    ExploreViewModel model;
    renderExploreScreen(canvas, model, cache);
    expectPixel(canvas, 98, 430, PixelRenderer::rgb(42, 61, 68));
    expectPixel(canvas, 270, 430, PixelRenderer::rgb(27, 43, 51));
    expectPixel(canvas, 98, 388, PixelRenderer::rgb(115, 226, 183));
    expectPixel(canvas, 270, 388, PixelRenderer::rgb(67, 97, 101));
    // A locked selection changes the departure button, without moving its hit box.
    model.visibleAreaCount = 2;
    model.selectedArea = 1;
    renderExploreScreen(canvas, model, cache);
    expectPixel(canvas, 98, 430, PixelRenderer::rgb(78, 91, 96));
    expectPixel(canvas, 270, 430, PixelRenderer::rgb(27, 43, 51));
}

void pageHeader(Canvas565& canvas, Game::GameState&) {
    expectRect(UiCommon::pageHeaderBackAt, 0, 0, 80, 76);
    const auto untouched = PixelRenderer::rgb(239, 143, 148);
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT, untouched);
    UiCommon::drawPageHeader(canvas, Ui::Amoled::MOVES);
    expectPixel(canvas, 0, 0, PixelRenderer::rgb(0, 0, 0));
    expectPixel(canvas, 0, 75, PixelRenderer::rgb(0, 0, 0));
    expectPixel(canvas, 0, 76, untouched);
    expectPixel(canvas, 40, 38, PixelRenderer::rgb(27, 43, 51));
    expectPixel(canvas, 40, 12, PixelRenderer::rgb(67, 97, 101));
    expectPixel(canvas, 29, 38, PixelRenderer::rgb(235, 183, 239));

    UiCommon::drawPageHeader(canvas, Ui::SHOP, "C100", false);
    expectPixel(canvas, 120, 38, PixelRenderer::rgb(0, 0, 0));
}

void shopDetail(Canvas565& canvas, Game::GameState& state) {
    assert(shopMenuItemAt(50, 100) == 0);
    assert(shopMenuItemAt(50, 208) == 1);
    assert(shopMenuItemAt(50, 316) == -1);
    expectRect([](int x, int y) { return itemConfirmChoiceAt(x, y) == 0; },
               36, 354, 176, 426);
    expectRect([](int x, int y) { return itemConfirmChoiceAt(x, y) == 1; },
               192, 354, 332, 426);
    ShopViewModel model;
    model.state = &state;
    model.detailItem = Game::ItemId::POTION;
    for (int pressed : {-1, 0, 1}) {
        model.pressedDetailAction = pressed;
        renderShopScreen(canvas, model);
        expectPixel(canvas, 106, 418, pressed == 0 ? PixelRenderer::rgb(48, 74, 68)
                                                               : PixelRenderer::rgb(36, 54, 61));
        expectPixel(canvas, 262, 418, pressed == 1 ? PixelRenderer::rgb(91, 49, 55)
                                                               : PixelRenderer::rgb(46, 37, 44));
    }
    const auto expected = capture(canvas);
    for (float progress : {0.5f, 1.0f}) {
        model.detailProgress = progress;
        renderShopScreen(canvas, model);
        assert(capture(canvas) == expected);
    }
}

void settingsSliders(Canvas565& canvas, Game::GameState& state) {
    SettingsViewModel model;
    model.state = &state;
    const auto track = PixelRenderer::rgb(48, 63, 70);
    const auto active = PixelRenderer::rgb(83, 184, 157);
    const auto knob = PixelRenderer::rgb(226, 238, 233);
    const auto background = PixelRenderer::rgb(0, 0, 0);
    model.brightness = 32;
    model.volume = 0;
    renderSettingsScreen(canvas, model);
    const auto minimum = capture(canvas);
    for (int y : {124, 186}) {
        expectPixel(canvas, 148, y, active);
        expectPixel(canvas, 148, y - 10, knob);
        for (int dy = -8; dy < 8; ++dy) expectPixel(canvas, 240, y + dy, track);
        expectPixel(canvas, 240, y - 9, background);
        expectPixel(canvas, 240, y + 8, background);
    }
    model.brightness = 255;
    model.volume = 100;
    renderSettingsScreen(canvas, model);
    for (int y : {124, 186}) {
        expectPixel(canvas, 332, y, active);
        expectPixel(canvas, 332, y - 10, knob);
        expectPixel(canvas, 240, y, active);
        // The label/value region stays unchanged; values are conveyed by the slider.
        for (int row = y - 28; row < y - 14; ++row) {
            for (int x = 12; x < 356; ++x) {
                expectPixel(canvas, x, row, minimum[row * AmoledUi::WIDTH + x]);
            }
        }
        expectPixel(canvas, 332, y - 13, background);
    }
    for (int index : {0, 1}) {
        model.pressedItem = index;
        renderSettingsScreen(canvas, model);
        const int y = index == 0 ? 124 : 186;
        expectPixel(canvas, 332, y - 13, knob);
        expectPixel(canvas, 332, y, PixelRenderer::rgb(115, 226, 183));
    }
    // Values below/above the supported range stay at the nearest endpoint.
    model.pressedItem = -1;
    model.brightness = 0;
    model.volume = 255;
    renderSettingsScreen(canvas, model);
    expectPixel(canvas, 148, 124, active);
    expectPixel(canvas, 332, 186, active);
}

void teamPopup(Canvas565& canvas, Game::GameState& state) {
    TeamViewModel model;
    model.state = &state;
    model.actionPopupOpen = true;
    assert(teamActionPopupItemAt(200, 350, 2) == -1);
    const auto separator = PixelRenderer::rgb(82, 117, 117);
    const auto panel = PixelRenderer::rgb(17, 27, 34);
    for (int slot : {0, 1}) {
        model.actionPopupSlot = slot;
        renderTeamScreen(canvas, model);
        const int top = slot == 0 ? 160 : 208;
        const int rows = slot == 0 ? 2 : 4;
        for (int row = 1; row < rows; ++row) {
            const int separatorY = top + 6 + row * 56 - 1;
            // All 16 pixels between adjacent glyph boxes are clear except the
            // centered 2px separator: neither label may overlap the line.
            for (int y = separatorY - 7; y < separatorY + 9; ++y) {
                for (int x = 220; x < 344; ++x) {
                    expectPixel(canvas, x, y,
                                y == separatorY || y == separatorY + 1 ? separator : panel);
                }
            }
            expectPixel(canvas, 219, separatorY, panel);
            expectPixel(canvas, 344, separatorY, panel);
        }
        const int height = slot == 0 ? 124 : 236;
        expectPixel(canvas, 282, top + height - 1, PixelRenderer::rgb(115, 226, 183));
        assert(top + height <= AmoledUi::HEIGHT - 4);
        assert(teamActionPopupItemAt(200, top + height, slot) == -1);
        assert(teamActionPopupItemAt(200, top + 3, slot) == -1);
        assert(teamActionPopupItemAt(200, top + height - 4, slot) == -1);

        Frame labelPixels(AmoledUi::WIDTH * AmoledUi::HEIGHT, panel);
        Canvas565 labelCanvas;
        labelCanvas.attach({labelPixels.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
        labelCanvas.setNativeText(true);
        const char* leaderLabels[] = {Ui::STATUS, Ui::Amoled::MOVES};
        const char* memberLabels[] = {"首位", Ui::STATUS, Ui::Amoled::MOVES,
                                      Ui::Team::ACTION_LEAVE};
        for (int item = 0; item < rows; ++item) {
            const int rowTop = top + 6 + item * 56;
            expectRect([&](int x, int y) { return teamActionPopupItemAt(x, y, slot) == item; },
                       208, rowTop, 356, rowTop + 56);
            const int textTop = rowTop + 12;
            PixelRenderer::text(labelCanvas, 224, textTop,
                                slot == 0 ? leaderLabels[item] : memberLabels[item],
                                PixelRenderer::rgb(226, 238, 233));
            int inkPixels = 0;
            for (int y = textTop; y < textTop + 32; ++y) {
                for (int x = 224; x < 348; ++x) {
                    const auto expected = labelCanvas.readPixel(x, y);
                    expectPixel(canvas, x, y, expected);
                    if (expected != panel) ++inkPixels;
                }
            }
            assert(inkPixels > 0);  // Compare complete glyphs, not empty regions.
        }
    }
}

void teamNavigation(Canvas565& canvas, Game::GameState& state) {
    // Back lives only in the header; the bottom buttons are gone.
    expectRect(teamStatusBackAt, 0, 0, 80, 76);
    TeamStatusViewModel model;
    model.state = &state;
    for (int page = 0; page < 5; ++page) {
        model.page = page;
        renderTeamStatusScreen(canvas, model);
        for (int dot = 0; dot < 5; ++dot) {
            // Bottom page indicator: five dots centered at y=424, spacing 28.
            expectPixel(canvas, 184 + (dot - 2) * 28, 424,
                        dot == page ? PixelRenderer::rgb(115, 226, 183)
                                    : PixelRenderer::rgb(82, 117, 117));
        }
    }
    // A slide offset reveals the neighbor page beside the current one.
    model.page = 0;
    model.slideOffsetX = 0;
    renderTeamStatusScreen(canvas, model);
    expectPixel(canvas, 300, 358, PixelRenderer::rgb(0, 0, 0));
    model.slideOffsetX = -120;
    renderTeamStatusScreen(canvas, model);
    // The incoming page's HP bar track slides in from the right edge.
    expectPixel(canvas, 300, 358, PixelRenderer::rgb(29, 34, 42));
}

void teamStatusValues(Canvas565& canvas, Game::GameState& state) {
    Game::GameState local = state;
    TeamStatusViewModel model;
    model.state = &local;
    const auto ink = PixelRenderer::rgb(226, 238, 233);
    const auto accent = PixelRenderer::rgb(248, 210, 105);
    const auto background = PixelRenderer::rgb(0, 0, 0);
    const auto headerBackground = PixelRenderer::rgb(0, 0, 0);
    auto expectText = [&](int x, int y, int width, const std::string& label,
                          uint16_t color) {
        Frame expected(AmoledUi::WIDTH * AmoledUi::HEIGHT, background);
        Canvas565 reference;
        reference.attach({expected.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
        reference.setNativeText(true);
        PixelRenderer::text(reference, x, y, label.c_str(), color);
        for (int row = y; row < y + 32; ++row) {
            for (int column = x; column < x + width; ++column) {
                expectPixel(canvas, column, row, reference.readPixel(column, row));
            }
        }
    };
    renderTeamStatusScreen(canvas, model);
    const auto* profileSpecies = findSpecies(local.team[0].speciesId);
    assert(profileSpecies);
    expectText(112, 92, 240, profileSpecies->name, ink);
    expectText(16, 340, 336, "来源初始伙伴", ink);
    expectPixel(canvas, 72, 12, headerBackground);
    expectPixel(canvas, 112, 400, background);
    model.page = 2;
    auto& monster = local.team[0];
    const auto* species = findSpecies(monster.speciesId);
    assert(species);
    const auto* basic = findMove(moveIdForMonster(*species, monster, false));
    assert(basic);
    // Populate both special slots to exercise all three rows with real moves.
    monster.move2Id = monster.move3Id = moveIdForMonster(*species, monster, false);
    const int rowY[] = {100, 180, 280};
    const char* grades[] = {"低", "中", "高", "满"};
    const uint8_t values[] = {0, 25, 60, 90};
    for (int grade = 0; grade < 4; ++grade) {
        for (auto& value : monster.moveProficiency) value = values[grade];
        renderTeamStatusScreen(canvas, model);
        for (int row = 0; row < 3; ++row) {
            expectText(16, rowY[row] + 38, 144,
                       "威力:" + std::to_string(basic->power), accent);
            expectText(176, rowY[row] + 38, 176,
                       std::string("熟练度:") + grades[grade], accent);
        }
    }
}

void teamMovesList(Canvas565& canvas, Game::GameState& state) {
    Game::GameState local = state;
    auto& monster = local.team[0];
    const auto* species = findSpecies(monster.speciesId);
    assert(species);
    const auto* basic = findMove(moveIdForMonster(*species, monster, false));
    assert(basic && basic->type == TypeId::NORMAL);
    monster.move2Id = basic->id;
    monster.move3Id = basic->id;

    TeamMovesViewModel model;
    model.state = &local;
    const auto cell = PixelRenderer::rgb(24, 34, 42);
    const auto proficiency = PixelRenderer::rgb(115, 226, 183);
    auto expectText = [&](int x, int y, const char* label) {
        Frame expected(AmoledUi::WIDTH * AmoledUi::HEIGHT, cell);
        Canvas565 reference;
        reference.attach({expected.data(), AmoledUi::WIDTH,
                          AmoledUi::HEIGHT, false});
        reference.setNativeText(true);
        PixelRenderer::text(reference, x, y, label, proficiency);
        for (int row = y; row < y + FontResource::LARGE_GLYPH_H; ++row) {
            for (int column = x; column < x + 64; ++column) {
                expectPixel(canvas, column, row,
                            reference.readPixel(column, row));
            }
        }
    };
    const uint8_t values[] = {0, 25, 60, 90};
    const char* labels[] = {"入门", "熟悉", "熟练", "精通"};
    for (int grade = 0; grade < 4; ++grade) {
        for (auto& value : monster.moveProficiency) value = values[grade];
        renderTeamMovesScreen(canvas, model);
        for (int row = 0; row < 3; ++row) {
            expectText(280, 90 + row * 66, labels[grade]);
        }
        // The type badge follows the Emerald-style light rim, colored center,
        // and offset dark shadow while retaining the classic Normal olive.
        expectPixel(canvas, 58, 84, PixelRenderer::rgb(232, 236, 218));
        expectPixel(canvas, 24, 88, PixelRenderer::rgb(168, 168, 120));
        expectPixel(canvas, 97, 108, PixelRenderer::rgb(7, 10, 12));
    }
}

void mainMenuGrid(Canvas565& canvas, Game::GameState&) {
    const int count = STICKMON_ENABLE_DEBUG_FEATURES ? 9 : 8;
    const int maxScroll = STICKMON_ENABLE_DEBUG_FEATURES ? 372 : 208;
    assert(MAIN_MENU_ITEM_COUNT == count);
    assert(mainMenuMaxScroll() == maxScroll);
    for (int scroll : {0, 44, maxScroll}) {
        MenuViewModel model;
        model.scroll = scroll;
        renderMainMenu(canvas, model);
        for (int item = 0; item < count; ++item) {
            const int left = item % 2 == 0 ? 8 : 188;
            const int top = (item / 2) * 164 - scroll;
            if (top + 80 < 0 || top + 80 >= 448) continue;
            assert(mainMenuItemAt(left, top + 80, scroll) == item);
            assert(mainMenuItemAt(left + 163, top + 80, scroll) == item);
            assert(mainMenuItemAt(left - 1, top + 80, scroll) == -1);
            assert(mainMenuItemAt(left + 164, top + 80, scroll) == -1);
            expectPixel(canvas, left + 12, top + 80, PixelRenderer::rgb(24, 34, 42));
        }
        // No header, back affordance or scrollbar intrudes into the grid margins.
        for (int y = 0; y < 448; ++y) {
            assert(!mainMenuBackAt(0, y));
            expectPixel(canvas, 0, y, PixelRenderer::rgb(0, 0, 0));
            expectPixel(canvas, 360, y, PixelRenderer::rgb(0, 0, 0));
        }
        if (scroll <= 44) {
            assert(mainMenuItemAt(90, 152 - scroll, scroll) == -1);
            assert(mainMenuItemAt(90, 163 - scroll, scroll) == -1);
            assert(mainMenuItemAt(90, 164 - scroll, scroll) == 2);
        }
        assert(mainMenuItemAt(90, -1, scroll) == -1);
        assert(mainMenuItemAt(90, 448, scroll) == -1);
        if (count == 9 && scroll == maxScroll) {
            assert(mainMenuItemAt(270, 364, scroll) == -1);
            expectPixel(canvas, 270, 364, PixelRenderer::rgb(0, 0, 0));
        }
        if (scroll > 244) continue;
        model.pressedItem = 2;
        renderMainMenu(canvas, model);
        expectPixel(canvas, 20, 244 - scroll, PixelRenderer::rgb(42, 61, 68));
        expectPixel(canvas, 200, 244 - scroll, PixelRenderer::rgb(24, 34, 42));
    }
}

}  // namespace

bool runUiBehaviorCase(const char* name, Canvas565& canvas, Game::GameState& state) {
    struct Case {
        const char* name;
        void (*run)(Canvas565&, Game::GameState&);
    };
    const Case cases[] = {
        {"explore-buttons", exploreButtons},
        {"page-header", pageHeader},
        {"shop-detail", shopDetail},
        {"settings-sliders", settingsSliders},
        {"team-popup", teamPopup},
        {"team-navigation", teamNavigation},
        {"team-status-values", teamStatusValues},
        {"team-moves-list", teamMovesList},
        {"main-menu-grid", mainMenuGrid},
    };
    for (const auto& test : cases) {
        if (std::strcmp(test.name, name) == 0) {
            test.run(canvas, state);
            return true;
        }
    }
    return false;
}
