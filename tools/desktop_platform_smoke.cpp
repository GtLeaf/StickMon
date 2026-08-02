#include "platform/api/PlatformServices.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "core/GameClockService.h"
#include "core/SaveCoordinator.h"
#include "hardware/EspNowLink.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(Platform::services().lifecycle.begin());

    const std::array<uint8_t, 4> saved = {4, 8, 15, 16};
    assert(Platform::blobs().initialize());
    assert(Platform::blobs().writeBlob(
        "smoke", "state", saved.data(), saved.size()));
    std::array<uint8_t, 4> loaded{};
    assert(Platform::blobs().blobSize("smoke", "state") == loaded.size());
    assert(Platform::blobs().readBlob(
        "smoke", "state", loaded.data(), loaded.size()));
    assert(saved == loaded);
    assert(Platform::blobs().removeBlob("smoke", "state"));
    assert(Platform::blobs().blobSize("smoke", "state") == 0);

    assert(Platform::resources().mount());
    Platform::ResourceFile file = Platform::resources().open("/fixture.bin");
    assert(file && file.size() == saved.size());
    std::array<uint8_t, 2> fileBytes{};
    assert(file.read(fileBytes.data(), fileBytes.size()) == fileBytes.size());
    assert(fileBytes[0] == 1 && fileBytes[1] == 2);
    assert(file.seek(3));
    uint8_t tail = 0;
    assert(file.read(&tail, 1) == 1 && tail == 4);

    Canvas565 canvas;
    canvas.attach(Platform::display().frameBuffer());
    canvas.fillSprite(0);
    canvas.fillRect(10, 10, 20, 15, 0xF800);
    assert(canvas.readPixel(10, 10) == 0xF800);
    assert(canvas.readPixel(0, 0) == 0);
    Platform::display().present();
    assert(desktop.presentCount() == 1);

    const uint8_t pcm[] = {128, 140, 120, 128};
    assert(Platform::audio().playPcmU8(pcm, sizeof(pcm), 8000));
    assert(desktop.audioPlayCount() == 1);
    assert(desktop.lastAudio().size() == sizeof(pcm));
    Platform::audio().setChannelVolume(0, 37);
    assert(desktop.channelVolume(0) == 37);
    Platform::audio().stop();

    const uint8_t destination[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t message[] = {0x5A, 0xA5, 0x03};
    assert(Platform::peers().enable());
    assert(Platform::peers().send(
        destination, message, sizeof(message)));
    Platform::PeerPacket packet;
    assert(Platform::peers().receive(packet));
    assert(packet.length == sizeof(message));
    assert(std::memcmp(packet.payload, message, sizeof(message)) == 0);
    Platform::peers().end();

    GameClockService gameClock;
    gameClock.start(1000, 100);
    assert(gameClock.minutesAt(61000, 2.0f) == 102);
    uint32_t gameMinutes = 100;
    assert(gameClock.sync(61000, 2.0f, gameMinutes));
    assert(gameMinutes == 102);

    SaveCoordinator saves;
    saves.reset(0);
    saves.mark(0, SaveCoordinator::Priority::DEFERRED);
    assert(!saves.due(299999));
    assert(saves.due(300000));
    saves.recordAttempt(300000, true);
    assert(!saves.dirty());
    saves.mark(301000, SaveCoordinator::Priority::SOON);
    assert(!saves.due(302999));
    assert(saves.due(303000));

    assert(EspNowLink::ins().begin());
    assert(EspNowLink::ins().isEnabled());
    EspNowLink::ins().end();
    return 0;
}
