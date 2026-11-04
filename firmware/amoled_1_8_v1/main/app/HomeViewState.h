#pragma once

#include <cstdint>

#include "assets/PokemonSprites.h"
#include "core/MainSceneViewState.h"
#include "game/GameState.h"
#include "game/HomeActor.h"

namespace AmoledV1 {

class HomeViewState {
public:
    using Direction = PokemonSprites::WalkDirection;

    enum class MainMotion : uint8_t {
        IDLE,
        TURNING,
        WANDERING,
        SEEKING_FOOD,
        SEEKING_SLEEP,
        SLEEPING,
        STOPPING,
        EATING,
    };

    struct MainRestore {
        Home::Actor actor{};
        Direction direction = Direction::DOWN;
        uint8_t frame = 0;
        MainMotion motion = MainMotion::IDLE;
    };

    enum class SecondaryMotion : uint8_t { IDLE, WANDERING, SLEEPING };

    struct SecondaryRestore {
        Home::Actor actor{};
        Direction direction = Direction::DOWN;
        uint8_t frame = 0;
        SecondaryMotion motion = SecondaryMotion::IDLE;
    };

    static uint8_t mainDirectionForView(Direction direction);
    static Direction mainDirectionFromView(uint8_t direction);

    static void encodeMain(MainSceneViewState& saved,
                           const Game::MonsterRuntime& monster,
                           const Home::Actor& actor, MainMotion motion,
                           bool transient, bool sleepTime, bool resting,
                           float groundOffset, Direction direction,
                           uint8_t frame, uint32_t nowMs);
    static bool decodeMain(const MainSceneViewState& saved,
                           const Home::Actor& current, uint16_t speciesId,
                           bool sleepTime,
                           bool foodResumeAllowed, float groundOffset,
                           uint32_t nowMs, MainRestore& restored);

    static void encodeSecondary(SecondarySceneViewState& saved,
                                const Game::MonsterRuntime& monster,
                                const Home::Actor& actor, bool transient,
                                bool sleepTime, float groundOffset,
                                Direction direction, uint8_t frame,
                                uint32_t nowMs);
    static bool decodeSecondary(const SecondarySceneViewState& saved,
                                const Game::MonsterRuntime& monster,
                                const Home::Actor& current, bool sleepTime,
                                float groundOffset, uint32_t nowMs,
                                SecondaryRestore& restored);
};

}  // namespace AmoledV1
