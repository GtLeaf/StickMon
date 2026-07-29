#pragma once

#include <cstdint>

namespace Game {

static constexpr uint16_t ENCOUNTERED_SPECIES_CAP = 256;

struct EncounterHistory {
    uint16_t count = 0;
    uint16_t speciesIds[ENCOUNTERED_SPECIES_CAP] = {};

    bool contains(uint16_t speciesId) const {
        if (speciesId == 0) return false;
        uint16_t validCount =
            count < ENCOUNTERED_SPECIES_CAP ? count : ENCOUNTERED_SPECIES_CAP;
        for (uint16_t i = 0; i < validCount; ++i) {
            if (speciesIds[i] == speciesId) return true;
        }
        return false;
    }

    bool add(uint16_t speciesId) {
        if (speciesId == 0 || contains(speciesId) ||
            count >= ENCOUNTERED_SPECIES_CAP) {
            return false;
        }
        speciesIds[count++] = speciesId;
        return true;
    }

    bool sanitize() {
        bool changed = false;
        uint16_t sourceCount = count;
        if (sourceCount > ENCOUNTERED_SPECIES_CAP) {
            sourceCount = ENCOUNTERED_SPECIES_CAP;
            changed = true;
        }

        uint16_t write = 0;
        for (uint16_t read = 0; read < sourceCount; ++read) {
            uint16_t speciesId = speciesIds[read];
            if (speciesId == 0) {
                changed = true;
                continue;
            }
            bool duplicate = false;
            for (uint16_t i = 0; i < write; ++i) {
                if (speciesIds[i] == speciesId) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                changed = true;
                continue;
            }
            if (write != read) {
                speciesIds[write] = speciesId;
                changed = true;
            }
            ++write;
        }
        for (uint16_t i = write; i < ENCOUNTERED_SPECIES_CAP; ++i) {
            if (speciesIds[i] != 0) changed = true;
            speciesIds[i] = 0;
        }
        if (count != write) changed = true;
        count = write;
        return changed;
    }

    void clear() {
        count = 0;
        for (uint16_t& speciesId : speciesIds) speciesId = 0;
    }
};

} // namespace Game
