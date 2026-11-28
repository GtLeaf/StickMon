// Exercise the six production scripts, observing their final sprite selection.
// Resource/font boundaries are replaced by a fixed-size sprite probe; stage
// effects still use the production Canvas565. XML expectations come from Python.
#include "ui/common/MarketTutorPerformance.h"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
uint8_t partnerFrames = 1;
uint8_t instructorFrames = 3;
uint16_t drawnSpecies = 0;
int drawnFrame = -1;
int drawnInstructorFrame = -1;
unsigned draws = 0;
}
Canvas565& PixelRenderer::canvas() { static Canvas565 canvas; return canvas; }
void PixelRenderer::text(int, int, const char*, uint16_t, uint8_t) {}
void PixelRenderer::textSmooth(int, int, const char*, uint16_t, uint8_t) {}
namespace PokemonSprites {
bool walkingAnimation(uint16_t species, WalkDirection direction, WalkingAnimation& a) {
    assert(direction == WalkDirection::RIGHT || direction == WalkDirection::LEFT);
    assert(direction != WalkDirection::RIGHT || species == 94);
    a = {SpriteKind(0), direction == WalkDirection::RIGHT ? instructorFrames : partnerFrames, false};
    return true;
}
const SpriteFrame* findCachedSpeciesSprite(uint16_t species, SpriteKind kind) {
    static SpriteFrame frame{};
    frame.speciesId = species; frame.kind = uint16_t(kind);
    frame.width = frame.height = 20;
    return &frame;
}
const SpriteFrame* findSpeciesSprite(uint16_t species, SpriteKind kind) {
    return findCachedSpeciesSprite(species, kind);
}
int16_t frameGroundOffsetY(const SpriteFrame*) { return 0; }
bool drawFrameScaled(const SpriteFrame* frame, int, int, float, bool, bool) {
    if (!draws) {
        assert(frame->speciesId == 94);
        drawnInstructorFrame = frame->kind;
    }
    drawnSpecies = frame->speciesId; drawnFrame = frame->kind; ++draws;
    return true;
}
}
int main() {
    std::vector<uint16_t> pixels(368 * 448);
    PixelRenderer::canvas().attach({pixels.data(), 368, 448, false});
    unsigned species, style, ms, count, expected, teacherCount, teacherExpected, checked = 0;
    while (std::scanf("%u %u %u %u %u %u %u", &species, &style, &ms, &count, &expected,
                     &teacherCount, &teacherExpected) == 7) {
        instructorFrames = uint8_t(teacherCount);
        partnerFrames = uint8_t(count); draws = 0;
        Ui::MarketTutorPerformance::render(Ui::MarketTutorPerformance::Style(style), ms, species);
        if (draws != 2 || drawnSpecies != species || drawnFrame != int(expected) ||
            drawnInstructorFrame != int(teacherExpected)) {
            std::fprintf(stderr, "species=%u style=%u ms=%u expected=%u actual=%d instructor=%d expected=%u draws=%u\n",
                         species, style, ms, expected, drawnFrame, drawnInstructorFrame, teacherExpected, draws);
            return 1;
        }
        ++checked;
    }
    assert(checked > 1000);
    std::printf("tutor timing: %u script samples match XML, including partner/instructor cycling windows and explicit holds\n", checked);
}
