#pragma once

#include <cstdint>

#include "game/HomeCoordinator.h"

namespace Home {

enum class ActorIntent : uint8_t {
    NONE,
    FAINT_REST,
    HOLD_SLEEP,
    WAKE,
    WAKE_FOR_FOOD,
    SEEK_FOOD,
    SEEK_SLEEP,
};

struct ActorObservation {
    bool active = false;
    bool controlAvailable = true;
    bool foodActionAvailable = true;
    bool visitor = false;
    bool canMove = true;
    bool needsFood = true;
    bool usesBed = true;
    bool fainted = false;
    bool statusSleeping = false;
    bool sleepTime = false;
    bool sleeping = false;
    bool wakeForFood = false;
    bool foodRetryReady = true;
    uint8_t satiety = 100;
    uint8_t feedTarget = 85;
    Task task = Task::IDLE;
    Task resumeTask = Task::IDLE;
    MonsterDesire desire = MonsterDesire::STARE;
};

struct HouseholdObservation {
    ActorObservation actors[ACTOR_CAP];
    uint8_t actorCount = 0;
    bool bowlHasFood = false;
    bool bowlClearing = false;
    int8_t bowlOwner = Coordinator::NO_ACTOR;
};

struct FoodSelection {
    int8_t actorId = Coordinator::NO_ACTOR;
    bool waitingForClearance = false;
};

// Shared intent policy for room actors. It deliberately knows nothing about
// coordinates, sprites, input or platform APIs; scene adapters execute the
// returned intent with their own navigation and presentation code.
class ActorController {
public:
    static bool committedToFood(const ActorObservation& actor);
    static bool foodEligible(const ActorObservation& actor,
                             bool bowlHasFood);
    static FoodSelection selectBowlActor(
        const HouseholdObservation& home);
    static ActorIntent survivalIntent(const ActorObservation& actor,
                                      bool bowlHasFood);
};

}  // namespace Home
