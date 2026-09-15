#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"


class AmoledMusicContextTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = APP.read_text()

    def test_music_is_not_derived_from_every_page_each_update(self):
        update_start = self.source.index("void AmoledApp::update(")
        update_end = self.source.index("void AmoledApp::", update_start + 10)
        update = self.source[update_start:update_end]
        self.assertNotIn("musicForAmoledScene", update)
        self.assertNotIn("AudioManager::ins().setMusic(", update)
        self.assertIn("AudioManager::ins().update();", update)

    def test_explore_selector_does_not_own_explore_music(self):
        selector_start = self.source.index(
            "sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS)"
        )
        selector_end = self.source.index("requestFullRender();", selector_start)
        selector = self.source[selector_start:selector_end]
        self.assertNotIn("setMusicContext", selector)

    def test_battle_bag_does_not_change_music(self):
        bag_start = self.source.index("void AmoledApp::performBattleBag(")
        bag_end = self.source.index("void AmoledApp::performBattleBagItem(", bag_start)
        bag = self.source[bag_start:bag_end]
        self.assertNotIn("setMusicContext", bag)
        self.assertNotIn("setMusic(", bag)

    def test_root_transitions_update_music_context(self):
        route_start = self.source.index("bool AmoledApp::startExploreRoute(")
        route_end = self.source.index("bool AmoledApp::generateExploreRouteMap(", route_start)
        self.assertIn("setMusicContext(MusicContext::EXPLORE)",
                      self.source[route_start:route_end])

        battle_start = self.source.index("bool AmoledApp::beginExploreEncounter(")
        battle_end = self.source.index("void AmoledApp::pushBattleLog(", battle_start)
        self.assertIn("MusicContext::BATTLE_SPECIAL", self.source[battle_start:battle_end])
        self.assertIn("MusicContext::BATTLE", self.source[battle_start:battle_end])

        return_start = self.source.index("void AmoledApp::completeExploreReturn(")
        return_end = self.source.index("void AmoledApp::updateClockAndCare(", return_start)
        self.assertIn("setMusicContext(MusicContext::HOME)",
                      self.source[return_start:return_end])


if __name__ == "__main__":
    unittest.main()
