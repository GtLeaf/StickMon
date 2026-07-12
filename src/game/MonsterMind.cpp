#include "game/MonsterMind.h"

#include <Arduino.h>
#include <algorithm>

namespace {
uint8_t clampScore(uint16_t value) {
    return value > 255 ? 255 : static_cast<uint8_t>(value);
}

float clampScale(float value, float lo, float hi) {
    return std::max(lo, std::min(hi, value));
}
}

void MonsterMind::reset(uint32_t nowMs) {
    topDesire_ = MonsterDesire::STARE;
    for (uint8_t& score : scores_) score = 0;
    lastActivityMs_ = nowMs;
}

void MonsterMind::update(const Game::MonsterRuntime& monster, bool isNight,
                         bool bowlHasFood, uint32_t nowMs) {
    uint8_t hpPercent = monster.hpMax == 0
        ? 0
        : static_cast<uint8_t>(std::min<uint32_t>(100, monster.hpCur * 100UL / monster.hpMax));
    uint32_t inactiveSeconds = lastActivityMs_ == 0 ? 0 : (nowMs - lastActivityMs_) / 1000UL;
    uint16_t boredom = std::min<uint32_t>(90, inactiveSeconds * 2UL);

    uint16_t base[4] = {};
    if (bowlHasFood && monster.satiety < 85) {
        base[static_cast<uint8_t>(MonsterDesire::EAT)] =
            55 + static_cast<uint16_t>(85 - monster.satiety) * 2 +
            (monster.satiety < 35 ? 40 : 0);
    }

    uint16_t rest = isNight ? 150 : 15;
    if (hpPercent < 50) rest += static_cast<uint16_t>(50 - hpPercent) * 2;
    if (monster.mood < 35) rest += 30;
    base[static_cast<uint8_t>(MonsterDesire::REST)] = rest;

    uint16_t wander = (isNight ? 15 : 42) + monster.mood / 3 + boredom;
    if (monster.satiety < 25) wander /= 2;
    base[static_cast<uint8_t>(MonsterDesire::WANDER)] = wander;
    base[static_cast<uint8_t>(MonsterDesire::STARE)] = 50 + (isNight ? 20 : 0);

    for (uint8_t i = 0; i < 4; ++i) {
        uint16_t score = base[i] + static_cast<uint16_t>(random(0, 9));
        if (i == static_cast<uint8_t>(topDesire_)) score += INERTIA_BONUS;
        scores_[i] = clampScore(score);
    }

    uint8_t winner = 0;
    for (uint8_t i = 1; i < 4; ++i) {
        if (scores_[i] > scores_[winner]) winner = i;
    }
    uint8_t current = static_cast<uint8_t>(topDesire_);
    if (winner != current && scores_[winner] < clampScore(scores_[current] + SWITCH_MARGIN)) {
        winner = current;
    }
    topDesire_ = static_cast<MonsterDesire>(winner);
}

void MonsterMind::onActivity(uint32_t nowMs) {
    lastActivityMs_ = nowMs;
    topDesire_ = MonsterDesire::STARE;
}

void MonsterMind::onAte(uint32_t nowMs) {
    lastActivityMs_ = nowMs;
    topDesire_ = MonsterDesire::STARE;
}

void MonsterMind::onRested(uint32_t nowMs) {
    lastActivityMs_ = nowMs;
}

MonsterBehaviorProfile behaviorProfileFor(const Species& species,
                                          const Game::MonsterRuntime& monster) {
    MonsterBehaviorProfile profile;
    float statScale = 0.72f + static_cast<float>(species.stats.spe) / 280.0f;
    if (natureBoostStat(monster.nature) == 5) statScale *= 1.08f;
    if (natureLowerStat(monster.nature) == 5) statScale *= 0.90f;
    profile.moveSpeedScale = clampScale(statScale, 0.70f, 1.25f);

    if (profile.moveSpeedScale >= 1.10f) {
        profile.idleMinMs = 1900;
        profile.idleMaxMs = 5200;
        profile.wanderRadiusX = 34;
        profile.wanderRadiusY = 20;
        profile.turnPauseMs = 170;
    } else if (profile.moveSpeedScale <= 0.82f) {
        profile.idleMinMs = 3600;
        profile.idleMaxMs = 8600;
        profile.wanderRadiusX = 22;
        profile.wanderRadiusY = 13;
        profile.turnPauseMs = 280;
    }

    switch (species.id) {
    case 129:
        profile.moveSpeedScale = 0.35f;
        profile.idleMinMs = 4200;
        profile.idleMaxMs = 9200;
        break;
    case 143:
        profile.moveSpeedScale *= 0.72f;
        profile.idleMinMs = 4800;
        profile.idleMaxMs = 9800;
        break;
    case 92:
    case 93:
    case 151:
    case 380:
    case 381:
        profile.wanderRadiusY = static_cast<uint8_t>(profile.wanderRadiusY + 5);
        break;
    default:
        break;
    }
    return profile;
}
