#pragma once

#include <cstddef>
#include <cstdint>

#include "core/MainSceneViewState.h"
#include "game/GameState.h"

// Portable snapshot codec shared by the StickS3 and AMOLED firmwares.
// The legacy SaveManager envelope remains supported for migration, but new
// writes use this fixed-endian format instead of copying C++ object layouts.
namespace SaveCodec {

static constexpr uint32_t MAGIC = 0x32435653; // SVC2
// Schema 1 = v3 state payload; schema 2 appends debugMotionFlags (v4).
static constexpr uint16_t SCHEMA_VERSION = 2;
static constexpr uint16_t MIN_SUPPORTED_SCHEMA_VERSION = 1;
// State version stored by schema-1 payloads (Game::SAVE_VERSION at the time).
static constexpr uint16_t SCHEMA_1_STATE_VERSION = 3;
static constexpr size_t HEADER_BYTES = 16;
static constexpr size_t MAX_ENCODED_BYTES = 4096;

struct Snapshot {
    Game::GameState state;
    MainSceneViewState view;
};

size_t encodedSizeUpperBound();

bool encode(const Game::GameState& state,
            const MainSceneViewState& view,
            uint32_t sequence,
            uint8_t* output,
            size_t capacity,
            size_t& written);

bool decode(const uint8_t* input,
            size_t length,
            Snapshot& snapshot,
            uint32_t* sequence = nullptr);

} // namespace SaveCodec
