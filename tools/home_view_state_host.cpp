#include "AmoledApp.h"
#include "app/HomeViewState.h"
#include "core/SaveCodec.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using AmoledV1::HomeViewState;
using Direction = PokemonSprites::WalkDirection;
using MainMotion = HomeViewState::MainMotion;

bool equal(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.0001f;
}

Game::MonsterRuntime secondaryMonster() {
    Game::MonsterRuntime monster = Game::MonsterFactory::create(4, 12);
    monster.ivPacked = 0x10203040U;
    monster.metAt = 0x50607080U;
    monster.nature = 7;
    monster.metArea = 9;
    monster.origin = Game::Origin::STARTER;
    return monster;
}

Home::Actor secondaryActor() {
    Home::Actor actor{};
    actor.active = true;
    actor.speciesId = 4;
    actor.x = 70.0f;
    actor.y = 100.0f;
    actor.targetX = 110.0f;
    actor.targetY = 130.0f;
    actor.sleepX = 76.0f;
    actor.sleepY = 112.0f;
    actor.sleepSpotValid = true;
    actor.task = Home::Task::WANDER;
    return actor;
}

void checkMainMapping() {
    Game::MonsterRuntime monster = Game::MonsterFactory::create(25, 20);
    Home::Actor actor{};
    actor.speciesId = 25;
    actor.x = 80.0f;
    actor.y = 100.0f;
    actor.targetX = 120.0f;
    actor.targetY = 140.0f;
    actor.task = Home::Task::TURNING;
    actor.resumeTask = Home::Task::SEEK_FOOD;
    constexpr uint32_t now = 1000;
    actor.nextDecisionMs = now + 5000;

    MainSceneViewState saved{};
    HomeViewState::encodeMain(saved, monster, actor, MainMotion::TURNING,
        false, false, false, 18.0f, Direction::RIGHT, 7, now);
    assert(saved.valid && saved.speciesId == 25);
    assert(saved.aiMode == 3);
    assert(saved.pmdAction == 0 && saved.pmdDirection == 6);
    assert(saved.pmdFrame == 7 && saved.facingRight);
    assert(saved.nextDecisionRemainingMs == 5000);
    assert(equal(saved.monsterX, 80.0f) && equal(saved.monsterY, 82.0f));
    assert(equal(saved.targetX, 120.0f) && equal(saved.targetY, 122.0f));

    HomeViewState::MainRestore restored{};
    assert(HomeViewState::decodeMain(saved, actor, 25, false, false,
        18.0f, now, restored));
    assert(restored.motion == MainMotion::IDLE);
    assert(equal(restored.actor.targetX, restored.actor.x));
    assert(equal(restored.actor.targetY, restored.actor.y));
    assert(restored.frame == 0 && restored.direction == Direction::RIGHT);
    assert(restored.actor.nextDecisionMs == now + 5000);
    assert(HomeViewState::decodeMain(saved, actor, 25, false, true,
        18.0f, now, restored));
    assert(restored.motion == MainMotion::SEEKING_FOOD);
    assert(restored.frame == 7);
    assert(equal(restored.actor.targetX, 120.0f));
    assert(equal(restored.actor.targetY, 140.0f));

    HomeViewState::encodeMain(saved, monster, actor, MainMotion::TURNING,
        true, false, false, 18.0f, Direction::LEFT, 7, now);
    assert(saved.aiMode == 0 && saved.pmdFrame == 0);
    assert(saved.pmdDirection == 2 && !saved.facingRight);
    assert(saved.nextDecisionRemainingMs == 0);
    assert(equal(saved.targetX, actor.x));
    assert(equal(saved.targetY, actor.y - 18.0f));

    for (MainMotion motion : {MainMotion::EATING, MainMotion::STOPPING}) {
        HomeViewState::encodeMain(saved, monster, actor, motion,
            false, true, false, 18.0f, Direction::UP, 5, now);
        assert(saved.aiMode == 0 && saved.nextDecisionRemainingMs == 0);
        assert(saved.pmdDirection == 4 && saved.pmdFrame == 5);
        assert(equal(saved.targetX, actor.x));
        assert(equal(saved.targetY, actor.y - 18.0f));
    }

    HomeViewState::encodeMain(saved, monster, actor, MainMotion::SLEEPING,
        false, false, true, 18.0f, Direction::DOWN, 3, now);
    assert(saved.aiMode == 0);
    HomeViewState::encodeMain(saved, monster, actor, MainMotion::IDLE,
        false, false, true, 18.0f, Direction::DOWN, 3, now);
    assert(saved.aiMode == 0);
    HomeViewState::encodeMain(saved, monster, actor, MainMotion::SLEEPING,
        false, true, true, 18.0f, Direction::DOWN, 3, now);
    assert(saved.aiMode == 8);
    assert(HomeViewState::decodeMain(saved, actor, 25, true, false,
        18.0f, now, restored));
    assert(restored.motion == MainMotion::SLEEPING);

    actor.resumeTask = Home::Task::WANDER;
    HomeViewState::encodeMain(saved, monster, actor, MainMotion::TURNING,
        false, false, false, 18.0f, Direction::DOWN, 3, now);
    assert(saved.aiMode == 1);
    actor.resumeTask = Home::Task::SEEK_SLEEP;
    HomeViewState::encodeMain(saved, monster, actor, MainMotion::TURNING,
        false, true, false, 18.0f, Direction::DOWN, 3, now);
    assert(saved.aiMode == 4);
    assert(HomeViewState::decodeMain(saved, actor, 25, false, false,
        18.0f, now, restored));
    assert(restored.motion == MainMotion::IDLE);
    assert(equal(restored.actor.targetX, restored.actor.x));
    assert(restored.frame == 0);

    const MainSceneViewState mismatch = saved;
    restored.actor.x = 1234.0f;
    assert(!HomeViewState::decodeMain(mismatch, actor, 4, true, false,
        18.0f, now, restored));
    assert(equal(restored.actor.x, 1234.0f));

    for (uint8_t pmd = 0; pmd < 8; ++pmd) {
        const Direction expected = pmd == 3 || pmd == 4 || pmd == 5
            ? Direction::UP
            : pmd == 1 || pmd == 2 ? Direction::LEFT
            : pmd == 6 || pmd == 7 ? Direction::RIGHT : Direction::DOWN;
        assert(HomeViewState::mainDirectionFromView(pmd) == expected);
    }
}

void checkSecondaryMappingAndWrap() {
    Game::MonsterRuntime monster = secondaryMonster();
    Home::Actor actor = secondaryActor();
    constexpr uint32_t now = 0xFFFFFFF0U;
    actor.nextDecisionMs = 20;
    actor.foodWakeRetryAfterMs = 10;
    SecondarySceneViewState saved{};
    HomeViewState::encodeSecondary(saved, monster, actor, false, false,
        18.0f, Direction::RIGHT, 6, now);
    assert(saved.valid);
    assert(saved.speciesId == monster.speciesId);
    assert(saved.ivPacked == monster.ivPacked);
    assert(saved.metAt == monster.metAt);
    assert(saved.nature == monster.nature);
    assert(saved.metArea == monster.metArea);
    assert(saved.origin == static_cast<uint8_t>(monster.origin));
    assert(saved.state == 1 && saved.stateRemainingMs == 36);
    assert(saved.foodRetryRemainingMs == 26);
    assert(saved.direction == static_cast<uint8_t>(Direction::RIGHT));
    assert(saved.frameIndex == 6 && saved.facingRight && saved.sleepSpotValid);
    assert(equal(saved.x, 70.0f) && equal(saved.y, 82.0f));
    assert(equal(saved.targetX, 110.0f) && equal(saved.targetY, 112.0f));
    assert(equal(saved.sleepX, 76.0f) && equal(saved.sleepY, 94.0f));

    HomeViewState::SecondaryRestore restored{};
    assert(HomeViewState::decodeSecondary(saved, monster, actor, false,
        18.0f, now, restored));
    assert(restored.motion == HomeViewState::SecondaryMotion::WANDERING);
    assert(restored.actor.nextDecisionMs == 20);
    assert(restored.actor.foodWakeRetryAfterMs == 10);
    assert(restored.direction == Direction::RIGHT && restored.frame == 6);
    assert(equal(restored.actor.targetY, 130.0f));

    saved.foodRetryRemainingMs = 0;
    assert(HomeViewState::decodeSecondary(saved, monster, actor, false,
        18.0f, now, restored));
    assert(restored.actor.foodWakeRetryAfterMs == 0);

    actor.foodWakeRetryAfterMs = now - 1;
    HomeViewState::encodeSecondary(saved, monster, actor, true, false,
        18.0f, Direction::LEFT, 6, now);
    assert(saved.state == 0 && saved.stateRemainingMs == 0);
    assert(saved.foodRetryRemainingMs == 0 && saved.frameIndex == 0);
    assert(equal(saved.targetX, actor.x));
    assert(equal(saved.targetY, actor.y - 18.0f));

    actor.task = Home::Task::SLEEPING;
    actor.nextDecisionMs = now + 80;
    actor.foodWakeRetryAfterMs = now + 90;
    HomeViewState::encodeSecondary(saved, monster, actor, false, true,
        18.0f, Direction::UP, 4, now);
    assert(saved.state == 6 && saved.stateRemainingMs == 80);
    assert(saved.foodRetryRemainingMs == 90);
    assert(HomeViewState::decodeSecondary(saved, monster, actor, true,
        18.0f, now, restored));
    assert(restored.motion == HomeViewState::SecondaryMotion::SLEEPING);
    assert(restored.actor.sleepSpotValid);
    assert(equal(restored.actor.sleepX, 76.0f));
    assert(equal(restored.actor.sleepY, 112.0f));
    assert(HomeViewState::decodeSecondary(saved, monster, actor, false,
        18.0f, now, restored));
    assert(restored.motion == HomeViewState::SecondaryMotion::IDLE);

    actor.task = Home::Task::IDLE;
    HomeViewState::encodeSecondary(saved, monster, actor, false, true,
        18.0f, Direction::DOWN, 4, now);
    assert(saved.state == 0);
    assert(equal(saved.targetX, actor.x));
    assert(equal(saved.targetY, actor.y - 18.0f));

    HomeViewState::encodeSecondary(saved, monster, actor, false, true,
        18.0f, Direction::DOWN, 4, now);
    const SecondarySceneViewState identity = saved;
    auto rejected = [&](const SecondarySceneViewState& candidate) {
        restored.actor.x = 4321.0f;
        assert(!HomeViewState::decodeSecondary(candidate, monster, actor, true,
            18.0f, now, restored));
        assert(equal(restored.actor.x, 4321.0f));
    };
    SecondarySceneViewState changed = identity;
    changed.valid = false;
    rejected(changed);
    changed = identity;
    ++changed.speciesId;
    rejected(changed);
    changed = identity;
    ++changed.ivPacked;
    rejected(changed);
    changed = identity;
    ++changed.metAt;
    rejected(changed);
    changed = identity;
    ++changed.nature;
    rejected(changed);
    changed = identity;
    ++changed.metArea;
    rejected(changed);
    changed = identity;
    ++changed.origin;
    rejected(changed);
}

}  // namespace

namespace AmoledV1 {

struct AmoledAppTestAccess {
    static void prepare(AmoledApp& app) {
        app.begin(1000);
        app.gameState = Game::GameState{};
        app.gameState.oobeDone = true;
        app.gameState.gameMinutesTotal = 12 * 60;
        app.gameState.teamCount = 2;
        app.gameState.team[0] = Game::MonsterFactory::create(1, 8);
        app.gameState.team[1] = secondaryMonster();
        app.sceneFlow.goHome();
        app.syncHomeActors(1000);
        app.updatePetFootprint();
        app.homeRuntime.stop(0, 1000, 1000);
        app.homeRuntime.stop(1, 1000, 1000);
    }

    static void checkInvalidSecondaryDoesNotCommit() {
        DesktopPlatform desktop("data");
        Platform::bind(desktop.serviceBundle());
        AmoledApp app;
        prepare(app);
        app.mainViewState.secondary = SecondarySceneViewState{};
        SecondarySceneViewState& saved = app.mainViewState.secondary;
        saved.valid = true;
        saved.speciesId = app.gameState.team[1].speciesId;
        saved.ivPacked = app.gameState.team[1].ivPacked;
        saved.metAt = app.gameState.team[1].metAt;
        saved.nature = app.gameState.team[1].nature;
        saved.metArea = app.gameState.team[1].metArea;
        saved.origin = static_cast<uint8_t>(app.gameState.team[1].origin);
        saved.x = -1000.0f;
        saved.y = -1000.0f;
        saved.targetX = 777.0f;
        saved.targetY = 888.0f;
        saved.sleepX = 999.0f;
        saved.sleepY = 999.0f;
        saved.sleepSpotValid = true;
        saved.state = 1;
        saved.direction = 3;
        saved.frameIndex = 9;
        saved.stateRemainingMs = 8000;
        saved.foodRetryRemainingMs = 9000;
        app.initializeCompanionActor(1000);
        assert(!equal(app.homeCompanionActor.targetX, 777.0f));
        assert(!equal(app.homeCompanionActor.targetY, 906.0f));
        assert(!equal(app.homeCompanionActor.sleepX, 999.0f));
        assert(!app.homeCompanionActor.sleepSpotValid);
        assert(app.homeCompanionActor.foodWakeRetryAfterMs != 10000);
        assert(app.homeCompanionActor.task != Home::Task::WANDER);
        assert(app.companionDirection == Direction::DOWN);
        assert(app.companionFrame == 0);
    }

    static void checkRealSaveBlobFiltersVisitorWithoutMutatingMemory() {
        DesktopPlatform desktop("data");
        Platform::bind(desktop.serviceBundle());
        AmoledApp app;
        prepare(app);
        app.gameState.team[1].origin = Game::Origin::VISITOR;
        app.gameState.activeSlot = 1;
        app.syncHomeActors(1000);
        assert(app.homeCompanionActor.active);
        assert(app.flushSave());
        assert(app.gameState.teamCount == 2);
        assert(app.gameState.activeSlot == 1);
        assert(app.gameState.team[1].origin == Game::Origin::VISITOR);

        const size_t size = desktop.blobSize("stickmon", "state");
        assert(size >= SaveCodec::HEADER_BYTES);
        std::vector<uint8_t> blob(size);
        assert(desktop.readBlob("stickmon", "state", blob.data(), blob.size()));
        SaveCodec::Snapshot snapshot{};
        uint32_t sequence = 0;
        assert(SaveCodec::decode(blob.data(), blob.size(), snapshot, &sequence));
        assert(sequence > 0);
        assert(snapshot.state.teamCount == 1);
        assert(snapshot.state.activeSlot == 0);
        assert(snapshot.state.team[0].origin != Game::Origin::VISITOR);
        assert(snapshot.state.team[0].speciesId == app.gameState.team[0].speciesId);
        assert(snapshot.state.team[1].origin != Game::Origin::VISITOR);
        assert(!snapshot.view.secondary.valid);
    }
};

}  // namespace AmoledV1

int main() {
    checkMainMapping();
    checkSecondaryMappingAndWrap();
    AmoledV1::AmoledAppTestAccess::checkInvalidSecondaryDoesNotCommit();
    AmoledV1::AmoledAppTestAccess::checkRealSaveBlobFiltersVisitorWithoutMutatingMemory();
    return 0;
}
