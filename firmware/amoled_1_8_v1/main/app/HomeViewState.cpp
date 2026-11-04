#include "app/HomeViewState.h"

namespace AmoledV1 {
namespace {

uint32_t positiveRemaining(uint32_t deadlineMs, uint32_t nowMs) {
    return static_cast<int32_t>(deadlineMs - nowMs) > 0
        ? deadlineMs - nowMs : 0;
}

}  // namespace

uint8_t HomeViewState::mainDirectionForView(Direction direction) {
    switch (direction) {
    case Direction::LEFT: return 2;
    case Direction::UP: return 4;
    case Direction::RIGHT: return 6;
    case Direction::DOWN:
    default: return 0;
    }
}

HomeViewState::Direction HomeViewState::mainDirectionFromView(uint8_t direction) {
    if (direction == 3 || direction == 4 || direction == 5) return Direction::UP;
    if (direction == 1 || direction == 2) return Direction::LEFT;
    if (direction == 6 || direction == 7) return Direction::RIGHT;
    return Direction::DOWN;
}

void HomeViewState::encodeMain(MainSceneViewState& saved,
                               const Game::MonsterRuntime& monster,
                               const Home::Actor& actor, MainMotion motion,
                               bool transient, bool sleepTime, bool resting,
                               float groundOffset, Direction direction,
                               uint8_t frame, uint32_t nowMs) {
    saved.valid = true;
    saved.speciesId = monster.speciesId;
    saved.monsterX = actor.x;
    saved.monsterY = actor.y - groundOffset;
    MainMotion storedMotion = motion;
    if (storedMotion == MainMotion::TURNING) {
        switch (actor.resumeTask) {
        case Home::Task::WANDER:
        case Home::Task::YIELDING:
        case Home::Task::LEAVING_SLEEP:
            storedMotion = MainMotion::WANDERING;
            break;
        case Home::Task::SEEK_FOOD:
            storedMotion = MainMotion::SEEKING_FOOD;
            break;
        case Home::Task::SEEK_SLEEP:
            storedMotion = MainMotion::SEEKING_SLEEP;
            break;
        default:
            storedMotion = MainMotion::IDLE;
            break;
        }
    }
    if (transient || storedMotion == MainMotion::EATING ||
        storedMotion == MainMotion::STOPPING ||
        (storedMotion == MainMotion::SLEEPING && !sleepTime)) {
        storedMotion = MainMotion::IDLE;
    }
    const bool stableRoute = storedMotion == MainMotion::WANDERING ||
        storedMotion == MainMotion::SEEKING_FOOD ||
        storedMotion == MainMotion::SEEKING_SLEEP;
    saved.targetX = stableRoute ? actor.targetX : actor.x;
    saved.targetY = (stableRoute ? actor.targetY : actor.y) - groundOffset;
    switch (storedMotion) {
    case MainMotion::WANDERING: saved.aiMode = 1; break;
    case MainMotion::SEEKING_FOOD: saved.aiMode = 3; break;
    case MainMotion::SEEKING_SLEEP: saved.aiMode = 4; break;
    case MainMotion::SLEEPING: saved.aiMode = 8; break;
    case MainMotion::TURNING:
    case MainMotion::EATING:
    case MainMotion::IDLE:
    case MainMotion::STOPPING:
    default: saved.aiMode = resting ? 8 : 0; break;
    }
    if (saved.aiMode == 8 && !sleepTime) saved.aiMode = 0;
    saved.pmdAction = 0;
    saved.pmdDirection = mainDirectionForView(direction);
    saved.pmdFrame = transient ? 0 : frame;
    saved.facingRight = direction == Direction::RIGHT;
    saved.faintRestActive = monster.fainted || monster.hpCur == 0;
    const bool resetDecision = transient || motion == MainMotion::EATING ||
        motion == MainMotion::STOPPING;
    saved.nextDecisionRemainingMs =
        resetDecision ? 0 : positiveRemaining(actor.nextDecisionMs, nowMs);
    saved.postFeedAwakeRemainingMs = 0;
}

bool HomeViewState::decodeMain(const MainSceneViewState& saved,
                               const Home::Actor& current, uint16_t speciesId,
                               bool sleepTime, bool foodResumeAllowed,
                               float groundOffset, uint32_t nowMs,
                               MainRestore& restored) {
    if (!saved.valid || saved.speciesId != speciesId) return false;
    MainRestore result;
    result.actor = current;
    result.actor.x = saved.monsterX;
    result.actor.y = saved.monsterY + groundOffset;
    result.actor.targetX = saved.targetX;
    result.actor.targetY = saved.targetY + groundOffset;
    result.direction = mainDirectionFromView(saved.pmdDirection);
    result.frame = saved.pmdFrame;
    if (saved.aiMode == 1) {
        result.motion = MainMotion::WANDERING;
    } else if (saved.aiMode == 3 && foodResumeAllowed) {
        result.motion = MainMotion::SEEKING_FOOD;
    } else if (saved.aiMode == 4 && sleepTime) {
        result.motion = MainMotion::SEEKING_SLEEP;
    } else if (saved.aiMode == 8 && sleepTime) {
        result.motion = MainMotion::SLEEPING;
    }
    if (result.motion == MainMotion::IDLE) {
        result.actor.targetX = result.actor.x;
        result.actor.targetY = result.actor.y;
        result.frame = 0;
    }
    result.actor.nextDecisionMs = nowMs + saved.nextDecisionRemainingMs;
    restored = result;
    return true;
}

void HomeViewState::encodeSecondary(SecondarySceneViewState& saved,
                                    const Game::MonsterRuntime& monster,
                                    const Home::Actor& actor, bool transient,
                                    bool sleepTime, float groundOffset,
                                    Direction direction, uint8_t frame,
                                    uint32_t nowMs) {
    saved = SecondarySceneViewState{};
    saved.valid = true;
    saved.speciesId = monster.speciesId;
    saved.ivPacked = monster.ivPacked;
    saved.metAt = monster.metAt;
    saved.nature = monster.nature;
    saved.metArea = monster.metArea;
    saved.origin = static_cast<uint8_t>(monster.origin);
    saved.x = actor.x;
    saved.y = actor.y - groundOffset;
    const bool stableRoute = !transient && actor.task == Home::Task::WANDER;
    saved.targetX = stableRoute ? actor.targetX : actor.x;
    saved.targetY = (stableRoute ? actor.targetY : actor.y) - groundOffset;
    saved.sleepX = actor.sleepX;
    saved.sleepY = actor.sleepY - groundOffset;
    saved.state = stableRoute ? 1
        : (!transient && sleepTime && actor.task == Home::Task::SLEEPING) ? 6 : 0;
    saved.direction = static_cast<uint8_t>(direction);
    saved.frameIndex = transient ? 0 : frame;
    saved.facingRight = direction == Direction::RIGHT;
    saved.sleepSpotValid = actor.sleepSpotValid;
    saved.stateRemainingMs = transient
        ? 0 : positiveRemaining(actor.nextDecisionMs, nowMs);
    saved.foodRetryRemainingMs = positiveRemaining(
        actor.foodWakeRetryAfterMs, nowMs);
}

bool HomeViewState::decodeSecondary(const SecondarySceneViewState& saved,
                                    const Game::MonsterRuntime& monster,
                                    const Home::Actor& current, bool sleepTime,
                                    float groundOffset, uint32_t nowMs,
                                    SecondaryRestore& restored) {
    if (!saved.valid || saved.speciesId != monster.speciesId ||
        saved.ivPacked != monster.ivPacked || saved.metAt != monster.metAt ||
        saved.nature != monster.nature || saved.metArea != monster.metArea ||
        saved.origin != static_cast<uint8_t>(monster.origin)) {
        return false;
    }
    SecondaryRestore result;
    result.actor = current;
    result.actor.x = saved.x;
    result.actor.y = saved.y + groundOffset;
    result.actor.targetX = saved.targetX;
    result.actor.targetY = saved.targetY + groundOffset;
    result.direction = saved.direction <= 3
        ? static_cast<Direction>(saved.direction) : Direction::DOWN;
    result.frame = saved.frameIndex;
    result.actor.nextDecisionMs = nowMs + saved.stateRemainingMs;
    result.actor.foodWakeRetryAfterMs = saved.foodRetryRemainingMs == 0
        ? 0 : nowMs + saved.foodRetryRemainingMs;
    if (saved.state == 1) {
        result.motion = SecondaryMotion::WANDERING;
    } else if (saved.state == 6 && sleepTime) {
        result.motion = SecondaryMotion::SLEEPING;
        result.actor.sleepX = saved.sleepX;
        result.actor.sleepY = saved.sleepY + groundOffset;
        result.actor.sleepSpotValid = saved.sleepSpotValid;
    }
    restored = result;
    return true;
}

}  // namespace AmoledV1
