#include "game/HomeActorController.h"

#include <algorithm>

#include "game/SpeciesBehavior.h"

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

IdleGoal ActorController::idleGoal(MonsterDesire desire,
                                   ActorIntent survival) {
    switch (desire) {
    case MonsterDesire::EAT:
        return survival == ActorIntent::SEEK_FOOD
            ? IdleGoal::SEEK_FOOD : IdleGoal::IDLE;
    case MonsterDesire::REST:
        return survival == ActorIntent::SEEK_SLEEP
            ? IdleGoal::SEEK_SLEEP : IdleGoal::WANDER;
    case MonsterDesire::WANDER:
        return IdleGoal::WANDER;
    case MonsterDesire::STARE:
        return IdleGoal::STARE;
    }
    return IdleGoal::IDLE;
}

float ActorController::routeSpeed(Task task,
                                 const MonsterBehaviorProfile& behavior,
                                 const Game::MonsterRuntime& monster,
                                 float minimum) {
    float speed = task == Task::SEEK_FOOD ? 19.0f
        : task == Task::WANDER ? 10.5f : 12.5f;
    speed *= behavior.moveSpeedScale;
    if (monster.mood < 40 || monster.satiety < 20) speed *= 0.72f;
    return std::max(minimum, speed);
}

bool ActorController::ambientEligible(const Game::MonsterRuntime& monster,
                                      MonsterDesire desire, bool sleepTime,
                                      bool bowlHasFood) {
    if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 ||
        monster.majorStatus == Game::MajorStatus::SLEEP ||
        static_cast<uint32_t>(monster.hpCur) * 100UL <=
            static_cast<uint32_t>(monster.hpMax) * 25UL ||
        monster.satiety <= 25 || sleepTime ||
        (bowlHasFood && monster.satiety < MONSTER_FEED_TARGET_SATIETY)) {
        return false;
    }
    return desire != MonsterDesire::EAT && desire != MonsterDesire::REST;
}

bool ActorController::pairEligible(const Game::MonsterRuntime& first,
                                    const Game::MonsterRuntime& second,
                                    bool firstSleepTime, bool secondSleepTime) {
    auto ready = [](const Game::MonsterRuntime& monster) {
        if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 ||
            monster.majorStatus == Game::MajorStatus::SLEEP ||
            static_cast<uint32_t>(monster.hpCur) * 100UL <=
                static_cast<uint32_t>(monster.hpMax) * 20UL) {
            return false;
        }
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(monster.speciesId);
        return !care.needsFood || monster.satiety > 25;
    };
    if (firstSleepTime || secondSleepTime ||
        !ready(first) || !ready(second)) {
        return false;
    }
    return Game::speciesCareProfileFor(first.speciesId).canMove ||
           Game::speciesCareProfileFor(second.speciesId).canMove;
}

PairActivity ActorController::pairActivity(bool firstCanMove,
                                           bool secondCanMove,
                                           bool forceChase, bool forcePlay,
                                           bool forceTalk, uint8_t roll) {
    if (!firstCanMove || !secondCanMove || forceTalk) {
        return PairActivity::TALK;
    }
    if (forceChase || (forcePlay && roll < 75) ||
        (!forcePlay && roll >= 45)) {
        return PairActivity::CHASE;
    }
    return PairActivity::TALK;
}

bool ActorController::pairLeaderMain(bool firstCanMove,
                                     bool secondCanMove, uint8_t roll) {
    if (!secondCanMove) return true;
    if (!firstCanMove) return false;
    return roll == 0;
}

}  // namespace Home
