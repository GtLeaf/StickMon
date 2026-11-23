#include "core/DeflateDecoder.h"
#include "core/RoomResource.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 6);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& room = RoomResource::ins();
    assert(room.begin());
    const bool night = std::string(argv[2]) == "night";
    const bool supported = std::string(argv[3]) == "valid";
    std::vector<uint16_t> pixels(room.artPixelCount());
    const bool nightLayer = night && room.layered() && room.furniture().nightBaseLength();
    assert(DeflateDecoder::inflateMemory(
        nightLayer ? room.furniture().nightBase() : room.baseCompressedData(),
        nightLayer ? room.furniture().nightBaseLength() : room.baseCompressedLen(),
        reinterpret_cast<uint8_t*>(pixels.data()), room.baseRawBytes()));
    if (night) {
        for (uint32_t i = 0; i < room.nightPatchRunCount(); ++i) {
            auto run = room.nightPatchRun(i);
            for (int j = 0; j < run.len; ++j)
                pixels[run.y * room.artWidth() + run.x + j] = room.nightPatchPixel(run.colorOffset + j);
        }
    }
    const auto original = pixels;
    assert(room.applyWallpaper(pixels.data(), night,
                               static_cast<Game::MarketWorkshop::Wallpaper>(std::stoi(argv[5]))) == supported);
    if (!supported) assert(pixels == original);
    std::ofstream out(argv[4], std::ios::binary);
    out.write(reinterpret_cast<const char*>(pixels.data()), room.baseRawBytes());
    assert(out.good());
}
