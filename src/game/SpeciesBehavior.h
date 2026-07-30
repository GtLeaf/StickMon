#pragma once

#include <cstdint>

namespace Game {

struct SpeciesCareProfile {
    bool canMove = true;
    bool needsFood = true;
    bool satietyDecays = true;
    bool usesBed = true;
};

inline bool isFixedCocoonSpecies(uint16_t speciesId) {
    switch (speciesId) {
    case 11:   // Metapod
    case 14:   // Kakuna
    case 266:  // Silcoon
    case 268:  // Cascoon
        return true;
    default:
        return false;
    }
}

inline SpeciesCareProfile speciesCareProfileFor(uint16_t speciesId) {
    SpeciesCareProfile profile;
    if (isFixedCocoonSpecies(speciesId)) {
        profile.canMove = false;
        profile.needsFood = false;
        profile.satietyDecays = false;
        profile.usesBed = false;
    }
    return profile;
}

}  // namespace Game
