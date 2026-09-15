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
        self.assertIn("if (routeMoving) return;", stick_walk)
        self.assertIn("++routeIndex;", stick_walk)

    def test_prompt_continue_is_also_one_step(self):
        start = self.app_source.index(
            "if (exploreRoutePrompt != ExploreRouteViewModel::Prompt::NONE)"
        )
        end = self.app_source.index("if (exploreRouteExitConfirm)", start)
        prompt = self.app_source[start:end]
        self.assertIn("exploreRoutePlayerWalkActive = true;", prompt)
        self.assertIn("exploreRouteAutoWalk = false;", prompt)
        self.assertIn("beginExploreRouteStep(nowMs);", prompt)

    def test_agent_mode_keeps_route_auto_walk_in_update_loop(self):
        start = self.app_source.index("void AmoledApp::updateExploreRoute(")
        end = self.app_source.index("bool AmoledApp::finishExploreRouteAtEnd(", start)
        update = self.app_source[start:end]
        self.assertIn("if (autonomousExpedition", update)
        self.assertIn("if (exploreRouteAutoWalk || exploreRoutePlayerWalkActive)", update)

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

    def test_pickup_stops_the_current_walk_run(self):
        start = self.app_source.index("void AmoledApp::resolveExploreRoutePickup(")
        end = self.app_source.index("void AmoledApp::", start + 10)
        pickup = self.app_source[start:end]
        self.assertIn("exploreRouteAutoWalk = false;", pickup)

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
