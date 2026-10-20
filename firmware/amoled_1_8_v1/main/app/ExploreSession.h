#pragma once

#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"
#include "game/ExploreItemEffects.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"

namespace AmoledV1 {

// Route decisions and rewards. The caller owns presentation, storage and clocks.
class ExploreSession {
public:
    struct State {
        ExploreMapGenerator::Map exploreRouteMap;
        ExplorePool::Pool exploreRoutePool{};
        uint8_t exploreRouteMapBlock = 0;
        uint8_t exploreRouteMapBlockCount = 1;
        uint8_t exploreRouteMapEncounterCount = 0;
        uint8_t exploreRouteEncounterCooldownSteps = 0;
        uint8_t exploreRouteGuaranteedEncounterIndex = 0;
        uint32_t exploreRouteExpeditionSeed = 0;
        ExploreMapGenerator::Edge exploreRoutePendingEntryEdge =
            ExploreMapGenerator::Edge::TOP;
        bool exploreRoutePendingFrostLadder = false;
        bool exploreRoutePendingFrostFall = false;
        uint32_t exploreRouteFrostFallAtMs = 0;
        uint8_t exploreRoutePath = 0;
        uint8_t exploreRouteIndex = 0;
        uint16_t exploreRouteSteps = 0;
        bool exploreRouteMoving = false;
        bool exploreRouteExiting = false;
        bool exploreRouteAutoWalk = false;
        bool exploreRoutePlayerWalkActive = false;
        bool exploreRoutePaused = false;
        bool exploreRouteComplete = false;
        bool exploreRouteExitConfirm = false;
        bool exploreRouteIceSliding = false;
        int8_t exploreRouteIceDx = 0;
        int8_t exploreRouteIceDy = 0;
        uint8_t exploreRoutePickupIndex = 0;
        uint8_t exploreRoutePickupItem = 0;
        bool exploreRoutePickupAvailable = false;
        bool exploreRouteGuaranteedEncounterPending = false;
        bool exploreRouteBossScheduled = false;
        bool exploreRouteBossPending = false;
        uint8_t exploreRouteBossIndex = 0;
        bool exploreRoutePityEligible = false;
        uint16_t exploreRouteBossSpeciesId = 0;
        uint8_t exploreRouteBossLevel = 0;
        uint16_t exploreRouteBossExperiencePercent = 100;
        ExploreSpecial::Kind exploreRouteSpecialKind = ExploreSpecial::Kind::NONE;
        Game::ExploreItemEffects exploreItemEffects;
        uint32_t exploreRoutePausedAtMs = 0;
    };
    enum class Notice : uint8_t { NONE, MAP_FAILED, PET_REST, BAG_FULL, COINS, ITEM };
    struct Encounter {
        Game::MonsterRuntime wild;
        uint8_t leaderSlot = 0;
        bool boss = false;
        uint16_t experiencePercent = 100;
        ExploreSpecial::Kind special = ExploreSpecial::Kind::NONE;
    };
    struct Effects {
        bool save = false, fullRender = false, statusChanged = false;
        bool mapChanged = false, invalidateWorld = false;
        bool startStep = false, startExit = false, completed = false, startBattle = false;
        Notice notice = Notice::NONE;
        uint32_t coins = 0;
        Game::ItemId item = Game::ItemId::COUNT;
        Encounter encounter;
    };
    const State& state() const { return state_; }
    static bool healthy(const Game::MonsterRuntime& monster);
    static uint8_t leaderSlot(const Game::GameState& gameState);
    static bool hasCompanion(const Game::GameState& gameState);
    bool begin(Game::GameState& gameState, uint8_t area);
    bool generateMap(Game::GameState& gameState, uint8_t area);
    bool beginStep(Game::GameState& gameState);
    void completeStep(Game::GameState& gameState, uint32_t nowMs);
    bool beginExit();
    void finishExit(Game::GameState& gameState);
    void updatePendingFall(Game::GameState& gameState, uint32_t nowMs);
    bool beginEncounter(Game::GameState& gameState, uint8_t area, bool boss = false,
        uint16_t speciesOverride = 0, uint8_t levelOverride = 0,
        uint16_t experiencePercent = 100,
        ExploreSpecial::Kind special = ExploreSpecial::Kind::NONE);
    void pause(uint32_t nowMs);
    uint32_t resume(uint32_t nowMs);
    void stopRoute();
    void battleClosed();
    void finishReturn(Game::GameState& gameState);
    bool useItem(Game::GameState& gameState, Game::ItemId item);
    // A player command ends at interactions and remains distinct from Agent mode.
    void walkByPlayer() { state_.exploreRoutePlayerWalkActive = true; state_.exploreRouteAutoWalk = false; }
    void cancelPlayerWalk() { state_.exploreRoutePlayerWalkActive = false; }
    void setAutoWalk(bool enabled) { state_.exploreRouteAutoWalk = enabled; }
    void stopWalking() { cancelPlayerWalk(); setAutoWalk(false); }
    void confirmExit(bool open) { state_.exploreRouteExitConfirm = open; }
    Effects takeEffects();
private:
    bool finishAtEnd(Game::GameState& gameState);
    void resolveStepEvent(Game::GameState& gameState, bool blocked, bool repelActive);
    void resolvePickup(Game::GameState& gameState);
    bool recoverTeam(Game::GameState& gameState);
    void placePickup(const Game::GameState& gameState);
    void settleReturn(Game::GameState& gameState);
    friend struct AmoledAppTestAccess;
    friend struct AmoledEcologyTestAccess;
    State state_;
    Effects effects_;
    uint8_t area_ = 0;
    bool battleActive_ = false;
    bool returnSettled_ = false;
};

}  // namespace AmoledV1
