#include "core/AudioManager.h"
#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cstddef>

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform platform(argv[1]);
    Platform::bind(platform.serviceBundle());
    assert(platform.begin());

    AudioManager& audio = AudioManager::ins();
    audio.setMusic(MusicTrack::HOME);
    assert(audio.playingMusic() == MusicTrack::HOME);

    size_t guard = 0;
    while (audio.playingMusic() == MusicTrack::HOME) {
        platform.consumeQueuedPcm(AudioManager::MUSIC_CHANNEL);
        platform.advanceMs(20);
        audio.update();
        assert(++guard < 10000);
    }

    assert(audio.requestedMusic() == MusicTrack::HOME);
    assert(platform.queuedPcm(AudioManager::MUSIC_CHANNEL) == 0);
    const size_t playsAtSilence = platform.audioPlayCount();

    platform.advanceMs(2UL * 60UL * 1000UL - 1UL);
    audio.update();
    assert(audio.playingMusic() == MusicTrack::NONE);
    assert(platform.audioPlayCount() == playsAtSilence);

    platform.advanceMs(2UL * 60UL * 1000UL + 2UL);
    audio.update();
    assert(audio.playingMusic() == MusicTrack::HOME);
    assert(platform.audioPlayCount() > playsAtSilence);

    audio.setMusic(MusicTrack::EXPLORE);
    assert(audio.playingMusic() == MusicTrack::EXPLORE);
    for (size_t block = 0; block < 1000; ++block) {
        platform.consumeQueuedPcm(AudioManager::MUSIC_CHANNEL);
        platform.advanceMs(20);
        audio.update();
        assert(audio.playingMusic() == MusicTrack::EXPLORE);
    }

    audio.stopAll();
    return 0;
}
