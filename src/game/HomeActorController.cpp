#include "game/HomeActorController.h"

namespace Home {

bool ActorController::committedToFood(const ActorObservation& actor) {
    return actor.task == Task::SEEK_FOOD || actor.task == Task::FEEDING ||
           (actor.task == Task::TURNING &&
            (actor.resumeTask == Task::SEEK_FOOD ||
             actor.resumeTask == Task::FEEDING));
}

bool ActorController::foodEligible(const ActorObservation& actor,
                                   bool bowlHasFood) {
    if (!actor.active || !bowlHasFood || actor.visitor ||
        !actor.canMove || !actor.needsFood || actor.fainted ||
        actor.statusSleeping || actor.satiety >= actor.feedTarget ||
        !actor.foodActionAvailable) {
        return false;
    }
    if (!actor.controlAvailable && !committedToFood(actor)) return false;
    if (actor.sleeping && (!actor.wakeForFood || !actor.foodRetryReady)) {
        return false;
    }
    return true;
}

FoodSelection ActorController::selectBowlActor(
    const HouseholdObservation& home) {
    FoodSelection selection;
    if (!home.bowlHasFood || home.actorCount == 0) return selection;
    if (home.bowlClearing) {
        selection.waitingForClearance = true;
        return selection;
    }

    bool eligible[ACTOR_CAP] = {};
    const uint8_t count = home.actorCount > ACTOR_CAP
        ? ACTOR_CAP : home.actorCount;
    for (uint8_t actorId = 0; actorId < count; ++actorId) {
        eligible[actorId] = foodEligible(
            home.actors[actorId], home.bowlHasFood);
    }

    if (home.bowlOwner >= 0 && home.bowlOwner < count &&
        eligible[home.bowlOwner]) {
        selection.actorId = home.bowlOwner;
        return selection;
    }

    for (uint8_t actorId = 0; actorId < count; ++actorId) {
        if (eligible[actorId] && committedToFood(home.actors[actorId])) {
            selection.actorId = static_cast<int8_t>(actorId);
            return selection;
        }
    }

    if (!eligible[0]) {
        selection.actorId = count > 1 && eligible[1]
            ? 1 : Coordinator::NO_ACTOR;
        return selection;
    }
    if (count < 2 || !eligible[1]) {
        selection.actorId = 0;
        return selection;
    }

    const uint8_t firstDeficit = static_cast<uint8_t>(
        home.actors[0].feedTarget - home.actors[0].satiety);
    const uint8_t secondDeficit = static_cast<uint8_t>(
        home.actors[1].feedTarget - home.actors[1].satiety);
    selection.actorId = secondDeficit > firstDeficit ? 1 : 0;
    return selection;
}

ActorIntent ActorController::survivalIntent(
    const ActorObservation& actor, bool bowlHasFood) {
    if (!actor.active) return ActorIntent::NONE;
    if (actor.fainted) return ActorIntent::FAINT_REST;
    if (actor.statusSleeping) return ActorIntent::HOLD_SLEEP;
    if (actor.sleeping) {
        if (!actor.sleepTime) return ActorIntent::WAKE;
        if (foodEligible(actor, bowlHasFood)) {
            return ActorIntent::WAKE_FOR_FOOD;
        }
        return ActorIntent::HOLD_SLEEP;
    }
    if (!actor.controlAvailable) return ActorIntent::NONE;
    if (actor.desire == MonsterDesire::EAT &&
        foodEligible(actor, bowlHasFood)) {
        return ActorIntent::SEEK_FOOD;
    }
    if (actor.desire == MonsterDesire::REST && actor.sleepTime &&
        actor.usesBed && actor.canMove) {
        return ActorIntent::SEEK_SLEEP;
    }
    return ActorIntent::NONE;
}

}  // namespace Home
