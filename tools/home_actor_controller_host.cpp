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

    using Home::ActorController;
    using Home::ActorIntent;
    using Home::IdleGoal;
    using Home::PairActivity;
    assert(ActorController::idleGoal(MonsterDesire::EAT,
                                     ActorIntent::SEEK_FOOD) ==
           IdleGoal::SEEK_FOOD);
    assert(ActorController::idleGoal(MonsterDesire::EAT,
                                     ActorIntent::NONE) == IdleGoal::IDLE);
    assert(ActorController::idleGoal(MonsterDesire::REST,
                                     ActorIntent::SEEK_SLEEP) ==
           IdleGoal::SEEK_SLEEP);
    assert(ActorController::idleGoal(MonsterDesire::REST,
                                     ActorIntent::NONE) == IdleGoal::WANDER);
    assert(ActorController::idleGoal(MonsterDesire::WANDER,
                                     ActorIntent::NONE) == IdleGoal::WANDER);
    assert(ActorController::idleGoal(MonsterDesire::STARE,
                                     ActorIntent::NONE) == IdleGoal::STARE);

    Game::MonsterRuntime first{};
    first.speciesId = 1;
    first.hpMax = 100;
    first.hpCur = 80;
    first.satiety = 90;
    Game::MonsterRuntime second = first;
    assert(ActorController::ambientEligible(
        first, MonsterDesire::WANDER, false, false));
    assert(!ActorController::ambientEligible(
        first, MonsterDesire::EAT, false, false));
    assert(!ActorController::ambientEligible(
        first, MonsterDesire::WANDER, true, false));
    first.satiety = 50;
    assert(!ActorController::ambientEligible(
        first, MonsterDesire::STARE, false, true));
    first.satiety = 90;
    assert(ActorController::pairEligible(first, second, false, false));
    second.hpCur = 20;
    assert(!ActorController::pairEligible(first, second, false, false));
    second.hpCur = 80;
    assert(!ActorController::pairEligible(first, second, false, true));
    second.satiety = 25;
    assert(!ActorController::pairEligible(first, second, false, false));

    assert(ActorController::pairActivity(true, true, false, false,
                                         false, 44) == PairActivity::TALK);
    assert(ActorController::pairActivity(true, true, false, false,
                                         false, 45) == PairActivity::CHASE);
    assert(ActorController::pairActivity(true, true, false, true,
                                         false, 74) == PairActivity::CHASE);
    assert(ActorController::pairActivity(true, true, false, true,
                                         false, 75) == PairActivity::TALK);
    assert(ActorController::pairActivity(true, true, true, false,
                                         false, 0) == PairActivity::CHASE);
    assert(ActorController::pairActivity(false, true, true, false,
                                         false, 99) == PairActivity::TALK);
    assert(ActorController::pairActivity(true, true, true, false,
                                         true, 99) == PairActivity::TALK);
    assert(ActorController::pairLeaderMain(true, true, 0));
    assert(!ActorController::pairLeaderMain(true, true, 1));
    assert(!ActorController::pairLeaderMain(false, true, 0));
    assert(ActorController::pairLeaderMain(true, false, 1));
    return 0;
}
