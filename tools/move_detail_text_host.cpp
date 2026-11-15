#include "ui/TeamScreens.h"
#include "ui/TeamScreensInternal.h"
#include "ui/ProgressionScreens.h"
#include "ui/UiCommon.h"
#include "presentation/TextBlock.h"
#include "core/FontResource.h"
#include "platform/desktop/DesktopPlatform.h"
#include "assets/GameAssets.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace AmoledV1;
using Frame = std::vector<uint16_t>;
constexpr int WIDTH = 368, HEIGHT = 448;
constexpr uint16_t SENTINEL = 0xf81f;

static Frame pixels(Canvas565& canvas) {
    Frame result;
    result.reserve(WIDTH * HEIGHT);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            result.push_back(canvas.readPixel(x, y));
    return result;
}

static void capture(Canvas565& canvas, const char* directory, const char* name) {
    if (!directory) return;
    FILE* file = std::fopen((std::string(directory) + "/" + name + ".ppm").c_str(), "wb");
    assert(file);
    std::fprintf(file, "P6\n368 448\n255\n");
    for (uint16_t p : pixels(canvas)) {
        const unsigned char rgb[] = {
            uint8_t(((p >> 11) & 31) * 255 / 31),
            uint8_t(((p >> 5) & 63) * 255 / 63),
            uint8_t((p & 31) * 255 / 31)};
        assert(std::fwrite(rgb, 1, 3, file) == 3);
    }
    std::fclose(file);
}

static Ui::TextBlock::Options bodyOptions() {
    Ui::TextBlock::Options options;
    options.width = 320;
    options.maxHeight = 192;
    options.font = BitmapText::Font::PX32;
    options.lineGap = 8;
    return options;
}

static uint16_t fadeColor(uint16_t ink, uint8_t alpha) {
    return uint16_t(((((ink >> 11) & 31) * alpha / 255) << 11) |
                    ((((ink >> 5) & 63) * alpha / 255) << 5) |
                    ((ink & 31) * alpha / 255));
}

static void checkBody(Canvas565& canvas, const MoveInfo& move, int base, uint8_t alpha) {
    const uint16_t background = UiCommon::rgb(17, 27, 34);
    canvas.clearClipRect();
    canvas.fillSprite(background);
    drawMoveDetailContent(canvas, move, base, alpha);
    const auto actual = pixels(canvas);

    Ui::TextBlock expected;
    expected.setOptions(bodyOptions());
    expected.setText(move.description);
    assert(!expected.measure().overflow);
    canvas.fillSprite(background);
    expected.draw(canvas, 24, base + 64,
                  fadeColor(UiCommon::rgb(178, 194, 190), alpha));
    for (int y = base + 64; y < std::min(HEIGHT, base + 256); ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (actual[y * WIDTH + x] != canvas.readPixel(x, y)) {
                std::fprintf(stderr, "Description mismatch: move %u (%s), alpha %u, pixel %d,%d\n",
                             move.id, move.name, alpha, x, y);
                assert(false);
            }
        }
    }
}

static void checkPunctuationRows(Canvas565& canvas, MoveInfo move) {
    move.description = "一二三四五六七八九十，继续。";
    const uint16_t background = UiCommon::rgb(17, 27, 34);
    const uint16_t ink = UiCommon::rgb(178, 194, 190);
    canvas.clearClipRect();
    canvas.fillSprite(background);
    drawMoveDetailContent(canvas, move, 84, 255);
    const auto actual = pixels(canvas);
    canvas.fillSprite(background);
    // Hand-written expected breaks: keep the comma with its preceding glyph.
    PixelRenderer::bitmapText(canvas, 24, 148, "一二三四五六七八九", ink, BitmapText::Font::PX32);
    PixelRenderer::bitmapText(canvas, 24, 188, "十，继续。", ink, BitmapText::Font::PX32);
    for (int y = 148; y < 340; ++y)
        for (int x = 0; x < WIDTH; ++x)
            assert(actual[y * WIDTH + x] == canvas.readPixel(x, y));
}

template<class Model, class Render>
static void checkPage(Canvas565& canvas, Frame& storage, const Model& model, Render render) {
    canvas.clearClipRect();
    canvas.fillSprite(SENTINEL);
    render(canvas, model, 0, HEIGHT);
    const auto full = pixels(canvas);

    // A partial paint must not touch other rows, including while expanding.
    for (int top = 0; top < HEIGHT; top += 17) {
        const int bottom = std::min(HEIGHT, top + 17);
        canvas.fillSprite(SENTINEL);
        render(canvas, model, uint16_t(top), uint16_t(bottom));
        const auto band = pixels(canvas);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                const uint16_t expected = y >= top && y < bottom
                    ? full[y * WIDTH + x] : SENTINEL;
                if (band[y * WIDTH + x] != expected) {
                    std::fprintf(stderr, "Dirty-row mismatch: band %d..%d, pixel %d,%d\n",
                                 top, bottom, x, y);
                    assert(false);
                }
            }
        }
        assert(canvas.clipLeft() == 0 && canvas.clipTop() == 0 &&
               canvas.clipRight() == WIDTH && canvas.clipBottom() == HEIGHT);
    }
    canvas.fillSprite(SENTINEL);
    for (int top = 0; top < HEIGHT; top += 17)
        render(canvas, model, uint16_t(top), uint16_t(std::min(HEIGHT, top + 17)));
    assert(pixels(canvas) == full);

    canvas.attach({storage.data(), WIDTH, HEIGHT, true});
    canvas.fillSprite(SENTINEL);
    render(canvas, model, 0, HEIGHT);
    assert(pixels(canvas) == full);
    canvas.attach({storage.data(), WIDTH, HEIGHT, false});
}

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    const char* output = argc == 3 ? argv[2] : nullptr;
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount());
    assert(GameAssets::begin() && FontResource::ins().begin());
    Frame storage(WIDTH * HEIGHT);
    auto& canvas = PixelRenderer::canvas();
    canvas.attach({storage.data(), WIDTH, HEIGHT, false});
    canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1);
    canvas.setAssetScale(1);
    canvas.setNativeText(true);

    std::vector<const MoveInfo*> moves;
    const MoveInfo* longest = nullptr;
    const MoveInfo* longestPlayable = nullptr;
    int maxLines = 0;
    int maxPlayableLines = 0;
    for (uint32_t id = 1; id <= 65535; ++id) {
        const auto* move = findMove(Game::MoveId(id));
        if (!move) continue;
        moves.push_back(move);
        Ui::TextBlock audit;
        audit.setOptions(bodyOptions());
        audit.setText(move->description);
        const auto& measured = audit.measure();
        if (measured.overflow)
            std::fprintf(stderr, "Move %u (%s) exceeds reserved body: %d lines\n",
                         id, move->name, measured.lineCount);
        assert(!measured.overflow);
        if (!longest || measured.lineCount > maxLines) {
            longest = move;
            maxLines = measured.lineCount;
        }
        if (isMoveBattleSupported(move->id) &&
            (!longestPlayable || measured.lineCount > maxPlayableLines)) {
            longestPlayable = move;
            maxPlayableLines = measured.lineCount;
        }
    }
    assert(longest && longestPlayable && findMove(85));
    std::printf("Catalog: %zu moves fit; maximum %d lines (%s)\n", moves.size(), maxLines, longest->name);
    std::printf("Longest playable showcase: %s (%d lines)\n", longestPlayable->name, maxPlayableLines);
    std::fflush(stdout);

    Game::GameState state{};
    state.teamCount = 1;
    auto& monster = state.team[0];
    monster.speciesId = 25;
    monster.level = 50;
    monster.move2Id = longestPlayable->id;
    monster.move3Id = 85;
    TeamMovesViewModel manage;
    manage.state = &state;
    manage.selectedItem = 1;
    manage.detailProgress = 1;
    manage.scrollOffsetY = 100;
    ProgressionViewModel learn;
    learn.state = &state;
    learn.mode = ProgressionViewModel::Mode::MOVE_REPLACE;
    learn.moveId = longestPlayable->id;
    learn.oldMove2 = 85;
    learn.oldMove3 = 182;
    learn.selectedItem = 0;
    learn.detailProgress = 1;

    // Capture old and new production pages through the same reproducible inputs.
    renderTeamMovesScreen(canvas, manage);
    int bodyInkPixels = 0;
    for (int y = 172; y < 364; ++y)
        for (int x = 24; x < 344; ++x)
            bodyInkPixels += canvas.readPixel(x, y) == UiCommon::rgb(178, 194, 190);
    assert(bodyInkPixels > 0);
    capture(canvas, output, "manage-longest");
    renderProgressionScreen(canvas, learn);
    capture(canvas, output, "learn-longest");
    monster.move2Id = 85;
    renderTeamMovesScreen(canvas, manage);
    capture(canvas, output, "manage-thunderbolt");
    monster.move2Id = longestPlayable->id;

    // The old byte wrapper puts the comma at the beginning of the next line.
    MoveInfo synthetic = *findMove(85);
    synthetic.description = "一二三四五六七八九十，继续练习。HP恢复20点，PP增加。";
    checkBody(canvas, synthetic, 84, 255);
    checkPunctuationRows(canvas, synthetic);
    synthetic.description = "恢复HP 20点\n使用后，继续练习。";
    checkBody(canvas, synthetic, 84, 255);
    for (const auto* move : moves) checkBody(canvas, *move, 84, 255);
    for (uint8_t alpha : {uint8_t(0), uint8_t(96), uint8_t(255)})
        checkBody(canvas, *longest, 100, alpha);
    // Empty or null descriptions must clear the cache, then allow another move.
    synthetic.description = "";
    checkBody(canvas, synthetic, 84, 255);
    synthetic.description = nullptr;
    checkBody(canvas, synthetic, 84, 255);
    checkBody(canvas, *findMove(85), 84, 255);

    for (float progress : {0.0f, 0.2f, 0.55f, 1.0f}) {
        manage.detailProgress = progress;
        learn.detailProgress = progress;
        checkPage(canvas, storage, manage, renderTeamMovesScreen);
        checkPage(canvas, storage, learn, renderProgressionScreen);
    }
    manage.detailProgress = 1;
    learn.detailProgress = 1;
    for (uint8_t selected : {uint8_t(0), uint8_t(1), uint8_t(2)}) {
        manage.selectedItem = selected;
        manage.scrollOffsetY = int16_t(66 * selected);
        learn.selectedItem = selected;
        learn.scrollOffsetY = int16_t(66 * selected);
        checkPage(canvas, storage, manage, renderTeamMovesScreen);
        checkPage(canvas, storage, learn, renderProgressionScreen);
    }
    manage.mode = TeamMovesViewModel::Mode::RECALL_SELECT;
    manage.recallCount = 1;
    manage.recallIds[0] = longestPlayable->id;
    manage.recallSelected = 0;
    manage.selectedItem = 0;
    manage.scrollOffsetY = 80;
    checkPage(canvas, storage, manage, renderTeamMovesScreen);
    manage.mode = TeamMovesViewModel::Mode::RECALL_REPLACE;
    manage.toast = "操作成功";
    checkPage(canvas, storage, manage, renderTeamMovesScreen);
    learn.toast = "操作成功";
    checkPage(canvas, storage, learn, renderProgressionScreen);
    assert(teamMovesBackAt(40, 38));
    assert(teamMovesItemAt(160, 106, TeamMovesViewModel::Mode::MANAGE, 0, 0, 0, 1) == 0);
    assert(teamMovesItemAt(160, 180, TeamMovesViewModel::Mode::MANAGE, 0, 0, 0, 1) == -1);
    std::puts("move-detail-text: punctuation/ASCII/empty/cache/fade, manage/recall/learn, dirty/swapped and hits OK");
}
