#include "core/CryPlayer.h"
#include "core/AudioManager.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 4 || argc == 5);
    DesktopPlatform platform(argv[1]);
    Platform::bind(platform.serviceBundle());
    assert(platform.begin());
    auto& player = CryPlayer::ins();
    const bool wanted = std::atoi(argv[2]) != 0;
    const unsigned species = std::atoi(argv[3]);
    assert(player.play(species) == wanted);
    if (!wanted) { assert(platform.audioPlayCount() == 0); return 0; }
    assert(!platform.lastAudio().empty());
    if (argc == 4 && species == 25) {
        // Hand-calculated IMA code 7 ramp, including predictor saturation.
        assert(platform.lastAudio() == std::vector<uint8_t>(
            {128,128,128,128,128,130,132,137,149,173,225,255,255}));
    }
    if (argc == 4 && species == 26) assert(platform.lastAudio().size() == 2042);
    if (argc == 4 && species == 27) assert(platform.lastAudio() == std::vector<uint8_t>({0,128,255}));
    if (argc == 5) assert(platform.lastAudio().size() == std::strtoul(argv[4], nullptr, 10));
    assert(platform.queuedPcm(AudioManager::CRY_CHANNEL) == 1);
    assert(!player.play(species));
    assert(player.replay(species));
    platform.setVolume(0); assert(!player.replay(species));
    platform.setVolume(50); player.stop();
    assert(platform.queuedPcm(AudioManager::CRY_CHANNEL) == 0);
    player.update();
    assert(player.play(species));
    player.stop(); player.update();
}
