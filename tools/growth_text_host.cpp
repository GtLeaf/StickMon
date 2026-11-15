#include "ui/ProgressionScreens.h"
#include "ui/UiCommon.h"
#include "presentation/TextBlock.h"
#include "core/FontResource.h"
#include "core/UiStrings.h"
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
        const unsigned char rgb[] = {uint8_t(((p >> 11) & 31) * 255 / 31),
            uint8_t(((p >> 5) & 63) * 255 / 63), uint8_t((p & 31) * 255 / 31)};
        assert(std::fwrite(rgb, 1, 3, file) == 3);
    }
    std::fclose(file);
}

static Ui::TextBlock::Options options(int width, int height, int gap = 2) {
    Ui::TextBlock::Options result;
    result.width = width;
    result.maxHeight = height;
    result.font = BitmapText::Font::PX32;
    result.lineGap = uint16_t(gap);
    result.align = Ui::TextBlock::Align::CENTER;
    return result;
}

static void auditFits(const char* value, Ui::TextBlock::Options opt) {
    Ui::TextBlock block;
    block.setOptions(opt);
    block.setText(value);
    if (block.measure().overflow) std::fprintf(stderr, "Growth message overflow: %s\n", value);
    assert(!block.measure().overflow);
}

static void checkLevelDialog(Canvas565& canvas, ProgressionViewModel model,
                             const char* first, const char* second, bool ambientNative) {
    canvas.setNativeText(ambientNative);
    renderProgressionScreen(canvas, model);
    const auto actual = pixels(canvas);
    assert(canvas.nativeText() == ambientNative);
    canvas.fillSprite(UiCommon::rgb(12, 26, 31));
    PixelRenderer::fillRectAlpha(20, 328, 328, 100, UiCommon::rgb(0, 0, 0), 190);
    canvas.drawRect(20, 328, 328, 100, UiCommon::rgb(126, 175, 175));
    // Literal authored rows: retain semantic breaks, centering and old anchors.
    PixelRenderer::bitmapText(canvas, (WIDTH - UiCommon::textWidth(first)) / 2,
        second ? 336 : 346, first, UiCommon::rgb(226, 238, 233), BitmapText::Font::PX32);
    if (second) PixelRenderer::bitmapText(canvas, (WIDTH - UiCommon::textWidth(second)) / 2,
        368, second, UiCommon::rgb(226, 238, 233), BitmapText::Font::PX32);
    for (int y = 328; y < 428; ++y)
        for (int x = 20; x < 348; ++x)
            if (actual[y * WIDTH + x] != canvas.readPixel(x, y)) {
                std::fprintf(stderr, "Level message mismatch: nativeText=%d, pixel %d,%d\n", ambientNative, x, y);
                assert(false);
            }
    canvas.setNativeText(true);
}

static void checkPage(Canvas565& canvas, Frame& storage, const ProgressionViewModel& model) {
    canvas.clearClipRect();
    canvas.fillSprite(SENTINEL);
    renderProgressionScreen(canvas, model);
    const auto full = pixels(canvas);
    for (int top = 0; top < HEIGHT; top += 19) {
        const int bottom = std::min(HEIGHT, top + 19);
        canvas.fillSprite(SENTINEL);
        renderProgressionScreen(canvas, model, uint16_t(top), uint16_t(bottom));
        const auto band = pixels(canvas);
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x)
                assert(band[y * WIDTH + x] == (y >= top && y < bottom ? full[y * WIDTH + x] : SENTINEL));
        assert(canvas.clipLeft() == 0 && canvas.clipTop() == 0 &&
               canvas.clipRight() == WIDTH && canvas.clipBottom() == HEIGHT);
    }
    canvas.fillSprite(SENTINEL);
    for (int top = 0; top < HEIGHT; top += 19)
        renderProgressionScreen(canvas, model, uint16_t(top), uint16_t(std::min(HEIGHT, top + 19)));
    assert(pixels(canvas) == full);
    canvas.attach({storage.data(), WIDTH, HEIGHT, true});
    canvas.fillSprite(SENTINEL);
    renderProgressionScreen(canvas, model);
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

    char message[192];
    for (unsigned i = 0; i < speciesCount(); ++i) {
        const auto& species = speciesTable()[i];
        for (unsigned level : {1U, 9U, 10U, 99U, 100U}) {
            Ui::TextBlock levelText;
            levelText.setOptions(options(304, 64, 0));
            std::snprintf(message, sizeof(message), Ui::Common::LEVEL_UP_DIALOG_FMT, species.name, level);
            levelText.setText(message);
            if (levelText.measure().lineCount > 1)
                std::snprintf(message, sizeof(message), "%s等级\n提升到%u", species.name, level);
            auditFits(message, options(304, 64, 0));
        }
        for (const char* format : {Ui::Common::EVOLUTION_FMT,
                Ui::Common::EVOLUTION_COMPLETE_FMT, Ui::Common::EVOLUTION_CANCELLED_FMT}) {
            std::snprintf(message, sizeof(message), format, species.name);
            auditFits(message, options(352, 66));
        }
    }
    auditFits(Ui::Amoled::HOLD_TO_CANCEL_EVOLUTION, options(352, 52));
    auditFits(Ui::Amoled::CANCELLING_EVOLUTION, options(352, 52));
    unsigned moves = 0;
    const MoveInfo* longestMove = nullptr;
    for (uint32_t id = 1; id <= 65535; ++id) {
        const auto* move = findMove(Game::MoveId(id));
        if (!move) continue;
        ++moves;
        auditFits(move->name, options(256, 100));
        if (isMoveBattleSupported(move->id) && (!longestMove ||
            UiCommon::textWidth(move->name) > UiCommon::textWidth(longestMove->name))) longestMove = move;
    }
    assert(longestMove);
    std::printf("Growth catalog: %u species x5 levels/x3 evolution messages, %u move names fit\n", speciesCount(), moves);
    std::fflush(stdout);

    Game::GameState state{};
    state.teamCount = 1;
    state.team[0].speciesId = 1;
    ProgressionViewModel model;
    model.state = &state;
    model.mode = ProgressionViewModel::Mode::LEVEL_UP;
    model.oldLevel = 99;
    model.level = 100;
    model.levelUpElapsedMs = 800;
    renderProgressionScreen(canvas, model);
    capture(canvas, output, "level-up");
    model.mode = ProgressionViewModel::Mode::EVOLUTION;
    model.fromSpeciesId = 1;
    model.toSpeciesId = 3;
    model.evolutionPhase = Game::EvolutionSequence::Phase::COMPLETE;
    model.evolutionReady = true;
    model.evolutionElapsedMs = Game::EvolutionSequence::COMPLETE_MS;
    renderProgressionScreen(canvas, model);
    capture(canvas, output, "evolution-complete");
    model.evolutionPhase = Game::EvolutionSequence::Phase::CANCELLED;
    model.evolutionReady = false;
    model.evolutionElapsedMs = Game::EvolutionSequence::CANCEL_COMPLETE_MS;
    renderProgressionScreen(canvas, model);
    capture(canvas, output, "evolution-cancelled");
    model.mode = ProgressionViewModel::Mode::MOVE_LEARN;
    model.moveId = longestMove->id;
    renderProgressionScreen(canvas, model);
    capture(canvas, output, "new-move");

    model.mode = ProgressionViewModel::Mode::LEVEL_UP;
    checkLevelDialog(canvas, model, "妙蛙种子等级", "提升到100", true);
    checkLevelDialog(canvas, model, "妙蛙种子等级", "提升到100", false);
    state.team[0].speciesId = 25;
    model.level = 5;
    checkLevelDialog(canvas, model, "皮卡丘等级提升到5", nullptr, true);
    checkLevelDialog(canvas, model, "皮卡丘等级提升到5", nullptr, false);
    state.team[0].speciesId = 65535;
    model.level = 100;
    checkLevelDialog(canvas, model, "精灵等级提升到100", nullptr, false);
    state.team[0].speciesId = 1;
    for (uint32_t elapsed : {0U, 400U, 800U}) {
        model.levelUpElapsedMs = elapsed;
        checkPage(canvas, storage, model);
    }

    using Sequence = Game::EvolutionSequence;
    using Phase = Sequence::Phase;
    const struct { Phase phase; uint32_t elapsed; } phases[] = {
        {Phase::IDLE, 0}, {Phase::INTRO, 0}, {Phase::MORPH, Sequence::INTRO_MS + 500},
        {Phase::FLASH, Sequence::MORPH_END_MS + 85}, {Phase::REVEAL, Sequence::FLASH_END_MS + 60},
        {Phase::COMPLETE, Sequence::COMPLETE_MS}, {Phase::CANCEL_MORPH, 200},
        {Phase::CANCEL_REVEAL, Sequence::CANCEL_MORPH_MS + 60},
        {Phase::CANCELLED, Sequence::CANCEL_COMPLETE_MS}};
    model.mode = ProgressionViewModel::Mode::EVOLUTION;
    for (const auto& phase : phases) {
        model.evolutionPhase = phase.phase;
        model.evolutionElapsedMs = phase.elapsed;
        model.evolutionReady = phase.phase == Phase::COMPLETE;
        model.evolutionCancelHoldProgress = phase.phase == Phase::INTRO ? 50 : 0;
        checkPage(canvas, storage, model);
        renderProgressionScreen(canvas, model);
        const auto normal = pixels(canvas);
        canvas.setNativeText(false);
        renderProgressionScreen(canvas, model);
        assert(pixels(canvas) == normal && !canvas.nativeText());
        canvas.setNativeText(true);
    }
    model.fromSpeciesId = model.toSpeciesId = 65535;
    checkPage(canvas, storage, model);
    model.mode = ProgressionViewModel::Mode::MOVE_LEARN;
    model.moveId = 65535;
    model.toast = "操作成功";
    checkPage(canvas, storage, model);
    model.toast = nullptr;
    model.moveId = longestMove->id;
    model.pressedItem = 0;
    checkPage(canvas, storage, model);
    // Without a valid species sprite, the prompt region has a plain black backdrop.
    state.team[0].speciesId = 65535;
    canvas.setNativeText(false);
    renderProgressionScreen(canvas, model);
    const auto actual = pixels(canvas);
    canvas.fillSprite(0);
    Ui::TextBlock name;
    auto nameOptions = options(256, 100);
    nameOptions.align = Ui::TextBlock::Align::LEFT;
    name.setOptions(nameOptions);
    name.setText(longestMove->name);
    name.draw(canvas, 88, 224, UiCommon::rgb(226, 238, 233));
    for (int y = 224; y < 324; ++y)
        for (int x = 88; x < 344; ++x)
            assert(actual[y * WIDTH + x] == canvas.readPixel(x, y));
    assert(!canvas.nativeText());
    assert(progressionItemAt(0, 0, ProgressionViewModel::Mode::LEVEL_UP) == 0);
    assert(progressionItemAt(184, 388, ProgressionViewModel::Mode::MOVE_LEARN) == 0);
    assert(progressionItemAt(184, 300, ProgressionViewModel::Mode::MOVE_LEARN) == -1);
    assert(progressionItemAt(184, 388, ProgressionViewModel::Mode::EVOLUTION) == -1);
    std::puts("growth-text: semantic breaks, explicit fonts/fallback/cache, animation/hold/dirty/swapped and buttons OK");
}
