#include "HomeRestFlow.h"
#include <cmath>

namespace AmoledV1 {
void HomeRestFlow::positionMain(float x, float y) {
    main_.x = main_.targetX = x;
    main_.y = main_.targetY = y;
    main_.faintRestActive = true;
    main_.motion = Home::MotionPhase::STILL;
}
void HomeRestFlow::positionCompanion(const HomeSleepFlow::Layout &layout) {
    float x = companion_.x, y = companion_.y;
    if (sleep_.chooseCompanionSpot(layout, x, y)) {
        companion_.x = x;
        companion_.y = y;
    }
    companion_.targetX = companion_.x;
    companion_.targetY = companion_.y;
    companion_.faintRestActive = true;
}
uint8_t HomeRestFlow::place(const Game::GameState &game, const HomeSleepFlow::Layout &layout,
                            uint32_t nowMs) {
    uint8_t placed = 0;
    if (game.teamCount > 0 && (game.team[0].fainted || game.team[0].hpCur == 0)) {
        positionMain(layout.bedX, layout.bedY);
        main_.route.clear();
        main_.velocityX = main_.velocityY = 0.0f;
        simulation_.transition(0, Home::Task::FAINTED, nowMs, 0, true);
        placed |= 1;
    }
    if (game.teamCount > 1 && companion_.active &&
        (game.team[1].fainted || game.team[1].hpCur == 0)) {
        positionCompanion(layout);
        companion_.route.clear();
        companion_.velocityX = companion_.velocityY = 0.0f;
        simulation_.transition(1, Home::Task::FAINTED, nowMs, 0, true);
        placed |= 2;
    }
    return placed;
}
bool HomeRestFlow::restMain(float bedX, float bedY, uint32_t nowMs) {
    simulation_.transition(0, Home::Task::FAINTED, nowMs, 0, true);
    const bool changed = !main_.faintRestActive || std::fabs(main_.x - bedX) > 0.01f ||
                         std::fabs(main_.y - bedY) > 0.01f;
    if (changed)
        positionMain(bedX, bedY);
    main_.motion = Home::MotionPhase::STILL;
    return changed;
}
bool HomeRestFlow::restCompanion(const HomeSleepFlow::Layout &layout, uint32_t nowMs) {
    simulation_.transition(1, Home::Task::FAINTED, nowMs, 0, true);
    if (companion_.faintRestActive)
        return false;
    positionCompanion(layout);
    return true;
}
bool HomeRestFlow::recoverMain(uint32_t nowMs) {
    if (!main_.faintRestActive)
        return false;
    main_.faintRestActive = false;
    simulation_.stop(0, nowMs);
    main_.motion = Home::MotionPhase::STILL;
    main_.targetX = main_.x;
    main_.targetY = main_.y;
    main_.mind.reset(nowMs);
    return true;
}
bool HomeRestFlow::recoverCompanion(uint32_t nowMs) {
    if (!companion_.faintRestActive)
        return false;
    companion_.faintRestActive = false;
    simulation_.stop(1, nowMs, 700);
    companion_.mind.reset(nowMs);
    return true;
}
} // namespace AmoledV1
