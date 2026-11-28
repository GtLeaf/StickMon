// Real production sprite loader and renderer acceptance driver; no SDK needed.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "assets/PokemonSprites.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"

namespace {
struct Profile { uint16_t width, height; int asset, clip; bool swapped; };
// Full raster, one dirty row, bounded rectangle, and one dirty column.
constexpr Profile profiles[] = {
    {160, 128, 1, 0, false}, {160, 128, 1, 0, true},
    {160, 128, 1, 1, false}, {160, 128, 2, 2, true},
    {160, 128, 2, 0, false}, {160, 128, 2, 3, true},
    {384, 448, 1, 0, false}, {384, 448, 2, 0, true},
};
constexpr std::array<float, 7> scales{{.5f, .75f, 1.f, 1.25f, 1.5f, 2.f, 2.5f}};

// Hash every stored pixel, including the untouched area outside clipping.
// Independent lanes avoid one serial dependency per pixel in the hot loop.
std::array<uint64_t, 2> rasterHash(const std::vector<uint16_t>& pixels) {
    uint64_t a = 0xcbf29ce484222325ULL, b = 0x9e3779b97f4a7c15ULL;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        uint64_t word;
        std::memcpy(&word, pixels.data() + i, sizeof(word));
        a = (a ^ word) * 0x100000001b3ULL;
        b ^= word + 0x9e3779b97f4a7c15ULL + (b << 6) + (b >> 2);
    }
    return {{a, b}};
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 5 || argc > 7)
            throw std::runtime_error("usage: host RESOURCE_ROOT FRAMES OUTPUT large|small [ONLY_CASE [RAW_OUTPUT]]");
        DesktopPlatform desktop(argv[1]);
        Platform::bind(desktop.serviceBundle());
        if (!desktop.begin() || !Platform::resources().mount())
            throw std::runtime_error("resource mount failed");
        std::ifstream frames(argv[2]);
        std::ofstream output(argv[3]);
        if (!frames || !output) throw std::runtime_error("cannot open input/output");
        const bool large = std::string(argv[4]) == "large";
        const int64_t only = argc > 5 ? std::stoll(argv[5]) : -1;
        uint64_t caseNumber = 0, frameCount = 0;
        unsigned species, kind;
        while (frames >> species >> kind) {
            const auto* frame = PokemonSprites::findSpeciesSprite(
                species, static_cast<PokemonSprites::SpriteKind>(kind));
            if (!frame || frame->speciesId != species || frame->kind != kind)
                throw std::runtime_error("missing frame " + std::to_string(species) + ":" + std::to_string(kind));
            const auto bounds = PokemonSprites::frameVisibleBounds(frame);
            output << "M " << species << ' ' << kind << ' ' << unsigned(frame->width)
                << ' ' << unsigned(frame->height) << ' ' << PokemonSprites::frameGroundOffsetY(frame)
                << ' ' << unsigned(bounds.left) << ' ' << unsigned(bounds.top)
                << ' ' << unsigned(bounds.width) << ' ' << unsigned(bounds.height)
                << ' ' << unsigned(PokemonSprites::frameVisibleWidth(frame));
            for (bool flip : {false, true}) {
                const auto shadow = PokemonSprites::frameShadow(frame, flip);
                output << ' ' << shadow.offsetX << ' ' << shadow.offsetY
                    << ' ' << unsigned(shadow.radiusX) << ' ' << unsigned(shadow.radiusY);
            }
            output << '\n';
            ++frameCount;
            for (unsigned p = 0; p < (large ? 8u : 6u); ++p) {
                const auto& profile = profiles[p];
                std::vector<uint16_t> pixels(profile.width * profile.height);
                auto& canvas = PixelRenderer::canvas();
                canvas.attach({pixels.data(), profile.width, profile.height, profile.swapped});
                canvas.setCoordinateScale(1);
                canvas.setAssetScale(profile.asset);
                const int cx = profile.width / 2, cy = profile.height / 2;
                for (bool flip : {false, true}) {
                    // normal, silhouette, scaled coverage, scaled nearest, tinted scaled.
                    for (unsigned op = 0; op < 5; ++op) {
                        const unsigned count = op < 2 ? 1 : scales.size();
                        for (unsigned s = 0; s < count; ++s, ++caseNumber) {
                            if (only >= 0 && caseNumber != static_cast<uint64_t>(only)) continue;
                            const float scale = op < 2 ? 1.f : scales[s];
                            // Logical origin stays identical even when physical assetScale is 2.
                            const int x = cx / profile.asset - std::lround(frame->width * scale) / 2;
                            const int y = cy / profile.asset - std::lround(frame->height * scale) / 2;
                            for (size_t i = 0; i < pixels.size(); ++i)
                                pixels[i] = canvas.encodeColor(static_cast<uint16_t>(0x18e3u ^ ((i * 13u) & 0x07ffu)));
                            canvas.clearClipRect();
                            if (profile.clip == 1) canvas.setClipRect(0, cy + 1, profile.width, 1);
                            if (profile.clip == 2) canvas.setClipRect(cx - 19, cy - 23, 31, 47);
                            if (profile.clip == 3) canvas.setClipRect(cx - 1, 0, 1, profile.height);
                            bool ok = false;
                            if (op == 0) ok = PokemonSprites::drawFrame(frame, x, y, flip);
                            if (op == 1) ok = PokemonSprites::drawFrameSilhouette(frame, x, y, 0xf81f, flip);
                            if (op == 2 || op == 3) ok = PokemonSprites::drawFrameScaled(frame, x, y, scale, flip, op == 3);
                            if (op == 4) ok = PokemonSprites::drawFrameTintedScaled(frame, x, y, scale, 0x07e0, 117, flip);
                            if (!ok) throw std::runtime_error("draw failed at case " + std::to_string(caseNumber));
                            const auto hash = rasterHash(pixels);
                            output << "R " << caseNumber << ' ' << species << ' ' << kind << ' '
                                << p << ' ' << flip << ' ' << op << ' ' << scale << ' '
                                << std::hex << hash[0] << ' ' << hash[1] << std::dec << '\n';
                            if (argc > 6) {
                                std::ofstream raw(argv[6], std::ios::binary);
                                raw.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(uint16_t));
                            }
                        }
                    }
                }
            }
        }
        if (!frameCount) throw std::runtime_error("no logical frames");
        output << "END " << frameCount << ' ' << caseNumber << '\n';
        std::cerr << frameCount << " logical frames, " << caseNumber << " raster cases\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
