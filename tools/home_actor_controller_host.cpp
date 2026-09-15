#include <cassert>

#include "game/HomeActorController.h"

int main() {
    Home::HouseholdObservation home;
    home.actorCount = 2;
    home.bowlHasFood = true;
    for (Home::ActorObservation& actor : home.actors) {
        actor.active = true;
        actor.satiety = 20;
        actor.feedTarget = MONSTER_FEED_TARGET_SATIETY;
        actor.desire = MonsterDesire::EAT;
    }

    home.actors[0].satiety = 40;
    Home::FoodSelection food = Home::ActorController::selectBowlActor(home);
    assert(food.actorId == 1);

    home.actors[0].satiety = 20;
    home.actors[1].satiety = 20;
    food = Home::ActorController::selectBowlActor(home);
    assert(food.actorId == 0);

    home.actors[1].task = Home::Task::SEEK_FOOD;
    food = Home::ActorController::selectBowlActor(home);
    assert(food.actorId == 1);

    home.bowlOwner = 0;
    food = Home::ActorController::selectBowlActor(home);
    assert(food.actorId == 0);

    home.bowlClearing = true;
    food = Home::ActorController::selectBowlActor(home);
    assert(food.actorId == Home::Coordinator::NO_ACTOR);
    assert(food.waitingForClearance);

    Home::ActorObservation sleeper = home.actors[0];
    sleeper.task = Home::Task::SLEEPING;
    sleeper.sleeping = true;
    sleeper.sleepTime = true;
    sleeper.wakeForFood = false;
    assert(Home::ActorController::survivalIntent(sleeper, true) ==
           Home::ActorIntent::HOLD_SLEEP);
    sleeper.wakeForFood = true;
    assert(Home::ActorController::survivalIntent(sleeper, true) ==
           Home::ActorIntent::WAKE_FOR_FOOD);
    sleeper.foodRetryReady = false;
    assert(Home::ActorController::survivalIntent(sleeper, true) ==
           Home::ActorIntent::HOLD_SLEEP);
    sleeper.foodRetryReady = true;
    sleeper.sleepTime = false;
    assert(Home::ActorController::survivalIntent(sleeper, false) ==
           Home::ActorIntent::WAKE);

    Home::ActorObservation actor = home.actors[0];
    actor.desire = MonsterDesire::REST;
    actor.sleepTime = true;
    assert(Home::ActorController::survivalIntent(actor, true) ==
           Home::ActorIntent::SEEK_SLEEP);
    actor.fainted = true;
    assert(Home::ActorController::survivalIntent(actor, true) ==
           Home::ActorIntent::FAINT_REST);

    actor = home.actors[0];
    actor.satiety = 70;
    actor.desire = MonsterDesire::EAT;
    assert(!Home::ActorController::foodEligible(actor, true));
    actor.satiety = 69;
    assert(Home::ActorController::foodEligible(actor, true));

    actor = home.actors[0];
    actor.controlAvailable = false;
    assert(Home::ActorController::survivalIntent(actor, true) ==
           Home::ActorIntent::NONE);
    assert(!Home::ActorController::foodEligible(actor, true));
    actor.task = Home::Task::FEEDING;
    assert(Home::ActorController::foodEligible(actor, true));

    actor = home.actors[0];
    actor.visitor = true;
    assert(!Home::ActorController::foodEligible(actor, true));
    return 0;
}
