#pragma once

#include <cstddef>
#include <cstdint>

class CryPlayer {
public:
    static CryPlayer& ins();

    bool play(uint16_t speciesId);
    bool replay(uint16_t speciesId);
    void update();
    void stop();

private:
    CryPlayer() = default;

    void releaseBuffer();

    uint8_t* pcm_ = nullptr;
    size_t pcmBytes_ = 0;
    uint16_t speciesId_ = 0;
};
