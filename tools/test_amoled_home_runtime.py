#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
APP_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start + len(signature))
    return source[start:end]


class AmoledHomeRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP.read_text(encoding="utf-8")
        cls.header = APP_HEADER.read_text(encoding="utf-8")

    def test_companion_owns_independent_runtime_and_animation_state(self):
        for token in (
            "Home::Actor homeMainActor;",
            "Home::Actor homeCompanionActor;",
            "PokemonSprites::WalkDirection companionDirection",
            "uint8_t companionFrame = 0;",
        ):
            self.assertIn(token, self.header)

        update = function_body(
            self.app,
            "void AmoledApp::updateCompanion(uint32_t nowMs)",
            "bool AmoledApp::petFootprintInsideWalkArea(",
        )
        self.assertIn("homeRuntime.advanceRoute(\n            1,", update)
        self.assertIn("homeCompanionActor.x", update)
        self.assertIn("homeCompanionActor.y", update)
        self.assertIn("companionDirection = petDirectionForDelta(", update)
        self.assertIn("++companionFrame;", update)
        for copied_state in (
            "homeCompanionActor.x = petX",
            "homeCompanionActor.y = petY",
            "companionDirection = petDirection;",
            "companionFrame = petFrame;",
        ):
            self.assertNotIn(copied_state, update)

    def test_claw_ownership_precedes_arbitration_without_starving_companion(self):
        update = function_body(
            self.app,
            "void AmoledApp::updatePet(uint32_t nowMs)",
            "bool AmoledApp::petFootprintInsideWalkArea(",
        )
        ownership_sync = update.index("homeRuntime.setControlOwner(")
        arbitration = update.index("serviceHomeFoodArbitration(nowMs);")
        companion_call = update.index("updateCompanion(nowMs);")
        claw_guard = update.index("if (!homeRuntime.autonomousAllowed(0))")
        self.assertLess(ownership_sync, arbitration)
        self.assertLess(arbitration, companion_call)
        self.assertLess(companion_call, claw_guard)

    def test_home_view_model_uses_authoritative_render_snapshot(self):
        for token in (
            "const Home::RenderSnapshot homeSnapshot =",
            "homeRuntime.renderSnapshot()",
            "homeSnapshot.actors[0].x",
            "homeSnapshot.actors[0].y",
            "homeSnapshot.actors[1].x",
            "homeSnapshot.actors[1].y",
            "model.companionFrame = companionFrame;",
            "model.companionDirection = companionDirection;",
        ):
            self.assertIn(token, self.app)

    def test_companion_pose_is_persisted_without_schema_change(self):
        persist = function_body(
            self.app,
            "void AmoledApp::persistHomeViewState(uint32_t nowMs)",
            "bool AmoledApp::saveState()",
        )
        self.assertIn("secondary.x = homeCompanionActor.x;", persist)
        self.assertIn("secondary.y = homeCompanionActor.y - companionGroundOffset;", persist)
        self.assertIn("secondary.direction = static_cast<uint8_t>(companionDirection);", persist)
        self.assertIn(
            "secondary.frameIndex = transientCompanion ? 0 : companionFrame;",
            persist,
        )

    def test_both_amoled_boards_link_shared_home_runtime(self):
        for board in ("amoled_1_8_v1", "amoled_1_8_v2"):
            cmake = (ROOT / "firmware" / board / "main" / "CMakeLists.txt").read_text(
                encoding="utf-8"
            )
            self.assertIn('"${STICKMON_ROOT}/src/game/HomeActor.cpp"', cmake)
            self.assertIn('"${STICKMON_ROOT}/src/game/HomeActorController.cpp"', cmake)
            self.assertIn('"${STICKMON_ROOT}/src/game/HomeCoordinator.cpp"', cmake)
            self.assertIn('"${STICKMON_ROOT}/src/game/HomeRuntime.cpp"', cmake)
            self.assertIn('"${STICKMON_ROOT}/src/game/HomeSimulation.cpp"', cmake)
            self.assertIn('"${STICKMON_ROOT}/src/core/RoomNavigator.cpp"', cmake)

    def test_amoled_uses_authoritative_home_simulation(self):
        self.assertIn("Home::Simulation homeRuntime;", self.header)
        self.assertIn("homeRuntime.beginTick(nowMs);", self.app)
        self.assertIn("homeRuntime.beginBowlClearance(0, nowMs);", self.app)
        self.assertIn("homeRuntime.beginBowlClearance(1, nowMs);", self.app)
        self.assertIn("homeRuntime.renderSnapshot()", self.app)


if __name__ == "__main__":
    unittest.main()
