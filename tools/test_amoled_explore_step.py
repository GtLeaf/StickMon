#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
STICK_EXPLORE = ROOT / "src" / "scenes" / "ExploreScene.cpp"


class AmoledExploreStepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app_source = AMOLED_APP.read_text(encoding="utf-8")
        cls.stick_source = STICK_EXPLORE.read_text(encoding="utf-8")

    def test_player_route_tap_walks_to_next_interaction(self):
        start = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE)"
        )
        end = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE)", start
        )
        touch = self.app_source[start:end]
        map_start = touch.index("if (exploreRouteMapAt(x, y))")
        map_branch = touch[map_start:]

        self.assertIn("exploreRoutePlayerWalkActive = true;", map_branch)
        self.assertIn("exploreRouteAutoWalk = false;", map_branch)
        self.assertIn("if (!exploreRouteMoving)", map_branch)
        self.assertIn("beginExploreRouteStep(nowMs);", map_branch)
        self.assertNotIn("exploreRouteAutoWalk = !exploreRouteAutoWalk", map_branch)

    def test_step_guard_matches_stick_walk_guard(self):
        start = self.app_source.index("bool AmoledApp::beginExploreRouteStep(")
        end = self.app_source.index("void AmoledApp::updateExploreRoute(", start)
        amoled_step = self.app_source[start:end]
        self.assertIn("if (exploreRouteMoving", amoled_step)
        self.assertIn("++exploreRouteIndex;", amoled_step)

        stick_start = self.stick_source.index("void ExploreScene::walk()")
        stick_end = self.stick_source.index(
            "void ExploreScene::beginAutoWalk()", stick_start
        )
        stick_walk = self.stick_source[stick_start:stick_end]
        self.assertIn("if (routeMoving || pendingFrostFall) return;", stick_walk)
        self.assertIn("exploreRoutePendingFrostFall", amoled_step)
        self.assertIn("++routeIndex;", stick_walk)

    def test_wild_encounters_share_route_pool_and_level_rules(self):
        route_start = self.app_source.index("bool AmoledApp::startExploreRoute(")
        route_end = self.app_source.index("bool AmoledApp::", route_start + 10)
        self.assertIn(
            "exploreRoutePool = buildExplorePreviewPool(gameState, selectedExploreArea);",
            self.app_source[route_start:route_end],
        )

        start = self.app_source.index("bool AmoledApp::beginExploreEncounter(")
        end = self.app_source.index("\nvoid AmoledApp::", start)
        encounter = self.app_source[start:end]
        self.assertIn("ExplorePool::entryForRoll(", encounter)
        self.assertIn("exploreRoutePool, GameRandom::range(0, totalWeight)", encounter)
        self.assertIn("ExploreEncounterRules::targetLevel(", encounter)
        self.assertIn("ExploreEncounterRules::levelForRoll(", encounter)
        self.assertNotIn("gameState.team[0].level", encounter)

        stick_start = self.stick_source.index("void ExploreScene::rollEncounter()")
        stick_end = self.stick_source.index("\nvoid ExploreScene::", stick_start + 10)
        stick_encounter = self.stick_source[stick_start:stick_end]
        self.assertIn("rollPoolEntry(activePool)", stick_encounter)
        self.assertIn("ExploreEncounterRules::targetLevel(", stick_encounter)
        self.assertIn("ExploreEncounterRules::levelForRoll(", stick_encounter)
        self.assertIn("ExploreEncounters::poolForArea(", self.app_source)
        self.assertIn("ExploreEncounters::poolForArea(", self.stick_source)

    def test_route_update_keeps_auto_walk_and_player_walk_independent(self):
        start = self.app_source.index("void AmoledApp::updateExploreRoute(")
        end = self.app_source.index("bool AmoledApp::finishExploreRouteAtEnd(", start)
        update = self.app_source[start:end]
        self.assertIn("if (exploreRouteAutoWalk || exploreRoutePlayerWalkActive)", update)
        self.assertNotIn("exploreRoutePrompt", update)

    def test_player_walk_stops_before_regional_boss(self):
        update_start = self.app_source.index("void AmoledApp::updateExploreRoute(")
        start = self.app_source.index(
            "if (exploreRouteIndex + 1 >= path.pointCount)", update_start
        )
        end = self.app_source.index("requestExploreRouteDynamicRender();", start)
        resolution = self.app_source[start:end]
        self.assertIn("exploreRoutePlayerWalkActive", resolution)
        self.assertIn("exploreRouteBossPending", resolution)
        self.assertIn("exploreRouteIndex + 1 == exploreRouteBossIndex", resolution)
        self.assertIn("exploreRoutePlayerWalkActive = false;", resolution)

    def test_pickup_preserves_automatic_walk_but_stops_manual_walk(self):
        start = self.app_source.index("void AmoledApp::resolveExploreRoutePickup(")
        end = self.app_source.index("void AmoledApp::", start + 10)
        pickup = self.app_source[start:end]
        self.assertIn("const bool resumeAutoWalk = exploreRouteAutoWalk;", pickup)
        self.assertIn("exploreRouteAutoWalk = false;", pickup)
        self.assertIn("exploreRouteAutoWalk = resumeAutoWalk;", pickup)
        self.assertIn("exploreRoutePlayerWalkActive = false;", pickup)

        stick_start = self.stick_source.index("void ExploreScene::finishCompletedWalkStep()")
        stick_end = self.stick_source.index(
            "void ExploreScene::recoverTeamForCompletedSteps()", stick_start
        )
        stick_resolution = self.stick_source[stick_start:stick_end]
        self.assertIn("autoWalkActive = false;", stick_resolution)

    def test_pickup_is_placed_on_route_and_resolved_at_that_point(self):
        self.assertIn("placeExploreRoutePickup();", self.app_source)
        self.assertIn("ExploreIceSlide::nearestNonIceIndex", self.app_source)
        self.assertIn("exploreRouteIndex == exploreRoutePickupIndex", self.app_source)
        self.assertIn("resolveExploreRoutePickup(nowMs);", self.app_source)

    def test_pickup_tables_match_stick_area_weights(self):
        for pickup_name in (
            "GRASS_PATH_PICKUPS",
            "CREEK_SLOPE_PICKUPS",
            "TALL_GRASS_PARK_PICKUPS",
            "FROST_CRYSTAL_CAVE_PICKUPS",
            "MIST_FOREST_PATH_PICKUPS",
            "ANCIENT_WATERFALL_VALLEY_PICKUPS",
        ):
            self.assertIn(f"{pickup_name}[]", self.app_source)
            self.assertIn(f"{pickup_name}[]", self.stick_source)
        self.assertIn("stepsToday >= 5000", self.app_source)

    def test_route_generation_matches_stick_map_count_ranges(self):
        self.assertIn(
            "EXPLORE_MAP_MIN_COUNT[] = {3, 4, 4, 5, 6, 7}",
            self.app_source,
        )
        self.assertIn(
            "EXPLORE_MAP_MAX_COUNT[] = {4, 5, 6, 7, 8, 9}",
            self.app_source,
        )
        self.assertIn("exploreMapCountForRoll(", self.app_source)
        self.assertIn("exploreRouteMapBlockCount", self.app_source)
        self.assertIn("generateExploreRouteMap(nowMs)", self.app_source)

    def test_route_event_frequency_matches_stick(self):
        self.assertIn(
            "EXPLORE_ENCOUNTER_CHANCE[] = {\n"
            "    500, 600, 700, 900, 1100, 1300,\n"
            "}",
            self.app_source,
        )
        self.assertIn("EXPLORE_ENCOUNTER_COOLDOWN_STEPS = 5", self.app_source)
        self.assertIn("EXPLORE_MAX_ENCOUNTERS_PER_MAP = 2", self.app_source)
        self.assertIn("exploreRouteEncounterCooldownSteps", self.app_source)
        self.assertIn("exploreRouteMapEncounterCount", self.app_source)
        self.assertIn("exploreItemEffects.completeWalkStep();", self.app_source)

    def test_completed_tenth_step_recovers_team_before_events(self):
        update_start = self.app_source.index("void AmoledApp::updateExploreRoute(")
        update_end = self.app_source.index(
            "void AmoledApp::requestExploreRouteDynamicRender()", update_start
        )
        update = self.app_source[update_start:update_end]
        step_increment = update.index("++exploreRouteSteps;")
        recovery = update.index("recoverExploreTeamForCompletedSteps();")
        event_resolution = update.index("resolveExploreStepEvent(")
        self.assertLess(step_increment, recovery)
        self.assertLess(recovery, event_resolution)

        recover_start = self.app_source.index(
            "bool AmoledApp::recoverExploreTeamForCompletedSteps()"
        )
        recover_end = self.app_source.index(
            "void AmoledApp::requestExploreRouteDynamicRender()", recover_start
        )
        recover = self.app_source[recover_start:recover_end]
        self.assertIn(
            "ExploreRunRules::isRecoveryStep(exploreRouteSteps)", recover
        )
        self.assertIn("slot < gameState.teamCount", recover)
        self.assertIn("monster.fainted", recover)
        self.assertIn("monster.hpCur == 0", recover)
        self.assertIn("ExploreRunRules::recoveryAmount(monster.hpMax)", recover)
        self.assertIn("monster.hpMax", recover)

    def test_home_recovery_is_paused_for_the_full_explore_session(self):
        care_start = self.app_source.index("void AmoledApp::updateClockAndCare(")
        care_end = self.app_source.index(
            "void AmoledApp::updateMoodHearts(", care_start
        )
        care = self.app_source[care_start:care_end]
        self.assertIn("!exploreSessionActive", care)
        self.assertIn(
            "sceneFlow.current() != AppSceneFlow::Scene::BATTLE", care
        )
        self.assertIn("gameSpeed(), homeRecoveryActive", care)

        queue_start = self.app_source.index("bool AmoledApp::queueExploreDeparture(")
        queue_end = self.app_source.index(
            "void AmoledApp::cancelExploreDeparture()", queue_start
        )
        self.assertIn(
            "exploreSessionActive = true;",
            self.app_source[queue_start:queue_end],
        )

        return_update_start = self.app_source.index(
            "bool AmoledApp::updateExploreDeparture("
        )
        return_update_end = self.app_source.index(
            "void AmoledApp::beginExploreReturn(", return_update_start
        )
        return_update = self.app_source[return_update_start:return_update_end]
        self.assertGreaterEqual(
            return_update.count("exploreSessionActive = false;"), 2
        )

        defeat_start = self.app_source.index("void AmoledApp::finishBattleDefeat(")
        defeat_end = self.app_source.index(
            "void AmoledApp::closeBattle(", defeat_start
        )
        self.assertIn(
            "exploreSessionActive = false;",
            self.app_source[defeat_start:defeat_end],
        )

    def test_step_recovery_is_persisted_when_the_expedition_settles(self):
        settle_start = self.app_source.index("void AmoledApp::settleExploreReturn()")
        settle_end = self.app_source.index(
            "void AmoledApp::leaveExploreRoute()", settle_start
        )
        settle = self.app_source[settle_start:settle_end]
        self.assertIn("bool stateChanged = exploreRecoveryPendingSave;", settle)
        self.assertIn(
            "if (stateChanged && saveState()) exploreRecoveryPendingSave = false;",
            settle,
        )

    def test_pickup_roll_is_per_map_and_keeps_guaranteed_battle_slot(self):
        self.assertIn("EXPLORE_MAP_PICKUP_CHANCE = 6500", self.app_source)
        self.assertIn(
            "exploreCanScheduleGuaranteedEncounter(", self.app_source
        )
        self.assertIn(
            "exploreRouteGuaranteedEncounterPending = true;",
            self.app_source,
        )
        self.assertIn(
            "exploreRouteMapBlock + 1 == exploreRouteMapBlockCount",
            self.app_source,
        )


if __name__ == "__main__":
    unittest.main()
