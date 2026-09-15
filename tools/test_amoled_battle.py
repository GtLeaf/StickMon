#!/usr/bin/env python3

import unittest
from pathlib import Path

from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
AMOLED_APP_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
HOME_SCREEN_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.h"
STICK_EXPLORE = ROOT / "src" / "scenes" / "ExploreScene.cpp"
AMOLED_PLATFORMS = (
    ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledPlatform.cpp",
    ROOT / "firmware" / "amoled_1_8_v2" / "main" / "AmoledPlatform.cpp",
)


class AmoledBattleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app_source = AMOLED_APP.read_text()
        cls.app_header = AMOLED_APP_HEADER.read_text()
        cls.screen_source = read_home_source(ROOT)
        cls.screen_header = HOME_SCREEN_HEADER.read_text()
        cls.stick_explore_source = STICK_EXPLORE.read_text()

    def test_animation_uses_fresh_clock_and_redraws_each_frame(self):
        start = self.app_source.index("if (battleAnimationActive &&")
        end = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER)", start
        )
        update = self.app_source[start:end]

        self.assertIn(
            "uint32_t animationNowMs = Platform::clock().millis();", update
        )
        self.assertIn(
            "uint32_t elapsed = animationNowMs - battleAnimationStartedMs;", update
        )
        self.assertNotIn("elapsed = nowMs - battleAnimationStartedMs", update)
        self.assertIn(
            "requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);", update
        )

    def test_both_attackers_start_visible_animation(self):
        player_start = self.app_source.index(
            "void AmoledApp::performBattlePlayerAction("
        )
        wild_start = self.app_source.index(
            "void AmoledApp::performBattleWildAction("
        )
        player = self.app_source[player_start:wild_start]
        wild_end = self.app_source.index(
            "void AmoledApp::performBattlePlannedAction("
        )
        wild = self.app_source[wild_start:wild_end]

        for attack in (player, wild):
            self.assertIn("battleAnimationActive = true;", attack)
            self.assertIn(
                "battleAnimationStartedMs = Platform::clock().millis();", attack
            )
            self.assertIn(
                "pushBattleLog(nowMs, false);", attack
            )
            self.assertIn(
                "requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);", attack
            )
            self.assertIn("battleAudioPending = true;", attack)
            self.assertIn("battleAudioReady = false;", attack)
            self.assertNotIn("CryPlayer::ins().replay(", attack)

    def test_zero_damage_uses_stick_outcome_text_instead_of_damage_zero(self):
        helper_start = self.app_source.index("void formatBattleOutcome(")
        helper_end = self.app_source.index("enum ExplorePickupId", helper_start)
        helper = self.app_source[helper_start:helper_end]

        failed = helper.index("damage.failed")
        missed = helper.index("damage.missed")
        immune = helper.index("damage.effectiveness == 0")
        damage = helper.index("dealt > 0")
        self.assertLess(failed, missed)
        self.assertLess(missed, immune)
        self.assertLess(immune, damage)
        self.assertIn("Ui::Explore::MOVE_FAILED", helper)
        self.assertIn("Ui::Explore::MOVE_MISSED", helper)
        self.assertIn("Ui::Explore::NO_EFFECT", helper)
        self.assertIn("Ui::Explore::WILD_DAMAGE_FMT", helper)
        self.assertNotIn("HIT_FMT", helper)

        for function_name in (
            "void AmoledApp::performBattlePlayerAction(",
            "void AmoledApp::performBattleWildAction(",
        ):
            start = self.app_source.index(function_name)
            end = self.app_source.index("\nvoid AmoledApp::", start + 10)
            attack = self.app_source[start:end]
            self.assertIn("formatBattleOutcome(", attack)
            self.assertIn("damage.effectiveness > 0", attack)

    def test_battle_audio_waits_until_first_frame_is_presented(self):
        mark_start = self.app_source.index("void AmoledApp::markRendered()")
        mark_end = self.app_source.index("void AmoledApp::", mark_start + 10)
        mark_rendered = self.app_source[mark_start:mark_end]
        self.assertIn("battleAudioReady = true;", mark_rendered)

        update_start = self.app_source.index("void AmoledApp::update(")
        update_end = self.app_source.index("void AmoledApp::", update_start + 10)
        update = self.app_source[update_start:update_end]
        self.assertIn("if (battleAudioPending && battleAudioReady)", update)
        self.assertIn("CryPlayer::ins().replay(pendingCrySpecies);", update)

    def test_renderer_has_lunge_shake_and_hit_effect(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]

        self.assertIn("static constexpr int LUNGE[]", render)
        self.assertIn("if (model.animationHit", render)
        self.assertIn("drawBattleHitEffect(", render)

    def test_player_sprite_and_long_names_use_corrected_battle_layout(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]

        self.assertIn("static constexpr int PLAYER_GROUND_Y = 360;", render)
        self.assertIn("static constexpr int WILD_LEVEL_X = 164;", render)
        self.assertIn("static constexpr int NAME_LEVEL_GAP = 8;", render)
        self.assertIn(
            "PLAYER_LEVEL_X - NAME_LEVEL_GAP - textWidth(player->name)",
            render,
        )
        self.assertIn(
            "WILD_LEVEL_X - NAME_LEVEL_GAP - textWidth(wild->name)",
            render,
        )

    def test_battle_command_uses_stick_labels_without_move_picker(self):
        self.assertNotIn("ATTACK_SELECT", self.app_source)
        self.assertNotIn("ATTACK_SELECT", self.screen_source)
        self.assertNotIn("ATTACK_SELECT", self.screen_header)
        self.assertIn(
            "Ui::Explore::CMD_BATTLE, Ui::Explore::CMD_BAG,", self.screen_source
        )
        self.assertIn(
            "Ui::Explore::CMD_SWITCH, Ui::Explore::CMD_FLEE,", self.screen_source
        )

        touch_start = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE)"
        )
        touch_end = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU)",
            touch_start,
        )
        touch = self.app_source[touch_start:touch_end]
        self.assertIn("performBattleAttack(nowMs);", touch)

    def test_player_move_ai_reuses_stick_battle_system(self):
        start = self.app_source.index("void AmoledApp::performBattleAttack(")
        end = self.app_source.index(
            "void AmoledApp::performBattlePlayerAction(", start
        )
        attack = self.app_source[start:end]
        self.assertIn("battleTurnController.planAiTurn(", attack)

        stick_start = self.stick_explore_source.index("void ExploreScene::attackWild()")
        stick_end = self.stick_explore_source.index(
            "void ExploreScene::wildCounterattack()", stick_start
        )
        self.assertIn(
            "battleTurnController.planAiTurn(",
            self.stick_explore_source[stick_start:stick_end],
        )

        controller = (
            ROOT / "src" / "game" / "BattleTurnController.cpp"
        ).read_text()
        self.assertIn("BattleSystem::chooseAiMoveSlot(", controller)

    def test_shared_turn_plan_controls_amoled_action_order(self):
        attack_start = self.app_source.index("void AmoledApp::performBattleAttack(")
        attack_end = self.app_source.index(
            "void AmoledApp::performBattlePlayerAction(", attack_start
        )
        attack = self.app_source[attack_start:attack_end]
        self.assertIn("battleTurnController.planAiTurn(", attack)
        self.assertIn("performBattlePlannedAction(nowMs);", attack)

        planned_start = self.app_source.index(
            "void AmoledApp::performBattlePlannedAction("
        )
        planned_end = self.app_source.index(
            "void AmoledApp::advanceBattleTurn(", planned_start
        )
        planned = self.app_source[planned_start:planned_end]
        self.assertIn("battleTurnPlan.actions[battleTurnActionIndex]", planned)
        self.assertIn("BattleTurnController::Side::WILD", planned)

        animation_start = self.app_source.index("if (battleAnimationActive &&")
        animation_end = self.app_source.index(
            "if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER)",
            animation_start,
        )
        animation = self.app_source[animation_start:animation_end]
        self.assertIn("advanceBattleTurn(animationNowMs);", animation)

    def test_each_log_line_uses_stick_queue_and_minimum_display_time(self):
        self.assertIn("battleLogUntil = nowMs + 1000;", self.stick_explore_source)
        self.assertIn(
            "static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 24;",
            self.app_header,
        )
        self.assertIn(
            "constexpr uint32_t BATTLE_LOG_LINE_MS = 1000;",
            self.app_source,
        )
        service_start = self.app_source.index(
            "bool AmoledApp::serviceBattleLog(uint32_t nowMs)"
        )
        service_end = self.app_source.index(
            "bool AmoledApp::battleLogPlaybackBusy() const", service_start
        )
        service = self.app_source[service_start:service_end]
        self.assertIn("battleLogQueueCount == 0", service)
        self.assertIn("battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP", service)
        self.assertIn(
            "battleLogUntil = nowMs + BATTLE_LOG_LINE_MS;", service
        )
        self.assertIn("serviceBattleLog(nowMs)", self.app_source)
        self.assertIn("!battleLogPlaybackBusy()", self.app_source)
        self.assertIn(
            "model.logCount = battleLogVisibleCount;", self.app_source
        )

    def test_battle_animation_reuses_cached_background(self):
        render_start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[render_start:]
        self.assertIn("drawBattleBackgroundLayer(", render)
        self.assertIn("battleBackgroundCache", self.screen_source)
        self.assertIn("std::memcpy(canvas.rawPixels()", self.screen_source)

    def test_battle_hp_bar_has_black_outline(self):
        start = self.screen_source.index("void drawBattleHpBar(")
        end = self.screen_source.index("void ", start + 10)
        battle_hp = self.screen_source[start:end]
        self.assertIn(
            "canvas.drawRect((x),"
            " (y),"
            " (width),"
            " (12), rgb(0, 0, 0));",
            battle_hp,
        )
        self.assertIn("drawBattleHpBar(canvas, 44, 72", self.screen_source)
        self.assertIn("drawBattleHpBar(canvas, 216, 290", self.screen_source)

    def test_hp_changes_are_animated_before_turn_advance(self):
        self.assertIn("constexpr uint32_t BATTLE_HP_ANIMATION_MS = 420;",
                      self.app_source)
        self.assertIn("void AmoledApp::startBattleHpAnimation(",
                      self.app_source)
        self.assertIn("battleHpPercentForRender(", self.app_source)

        player_start = self.app_source.index(
            "void AmoledApp::performBattlePlayerAction("
        )
        wild_start = self.app_source.index(
            "void AmoledApp::performBattleWildAction("
        )
        player = self.app_source[player_start:wild_start]
        wild_end = self.app_source.index(
            "void AmoledApp::performBattlePlannedAction(", wild_start
        )
        wild = self.app_source[wild_start:wild_end]
        self.assertIn("startBattleHpAnimation(", player)
        self.assertIn("startBattleHpAnimation(", wild)

    def test_victory_replaces_player_hp_with_animated_experience(self):
        faint_start = self.app_source.index(
            "bool AmoledApp::resolveBattleFaint("
        )
        faint_end = self.app_source.index(
            "void AmoledApp::performBattleSwitch(", faint_start
        )
        faint = self.app_source[faint_start:faint_end]
        self.assertIn("startBattleExperienceAnimation(nowMs);", faint)
        self.assertIn("constexpr uint32_t BATTLE_EXP_ANIMATION_MS = 900;",
                      self.app_source)
        self.assertIn("model.showPlayerExperience = battleExperienceVisible;",
                      self.app_source)
        self.assertIn("drawBattleExperienceBar(", self.screen_source)

    def test_defeat_clears_expedition_visibility_before_returning_home(self):
        start = self.app_source.index("void AmoledApp::finishBattleDefeat(")
        end = self.app_source.index("void AmoledApp::closeBattle(", start)
        defeat = self.app_source[start:end]
        self.assertIn("expeditionMainHidden = false;", defeat)
        self.assertIn("expeditionCompanionHidden = false;", defeat)
        self.assertIn("expeditionCompanionDeparting = false;", defeat)
        self.assertLess(
            defeat.index("expeditionMainHidden = false;"),
            defeat.index("sceneFlow.goHome();"),
        )
        self.assertLess(
            defeat.index("expeditionCompanionHidden = false;"),
            defeat.index("sceneFlow.goHome();"),
        )

    def test_battle_bag_reuses_explore_only_inventory_page(self):
        bag_start = self.app_source.index("void AmoledApp::performBattleBag(")
        bag_end = self.app_source.index(
            "void AmoledApp::performBattleBagItem(", bag_start
        )
        bag = self.app_source[bag_start:bag_end]
        self.assertIn("openItemScene(AppSceneFlow::Scene::BAG);", bag)
        self.assertIn("battleBagMode = true;", bag)
        self.assertIn(
            "Game::ItemInventory::homeBagExploreItemAt(gameState, index)",
            self.app_source,
        )
        self.assertIn("model.exploreOnly = battleBagMode;", self.app_source)
        self.assertIn("Game::ItemInventory::usableInBattle(item)",
                      self.app_source)

    def test_audio_submission_does_not_wait_for_codec_task(self):
        for platform_path in AMOLED_PLATFORMS:
            source = platform_path.read_text()
            start = source.index("bool AmoledPlatform::playPcmU8Channel(")
            end = source.index("void AmoledPlatform::setChannelVolume", start)
            submit = source[start:end]
            self.assertNotIn("xSemaphoreTake(audioMutex_", submit)
            self.assertIn("audioGeneration_[channel].fetch_add(1)", submit)
            self.assertNotIn("clearAudioQueue();", submit)

            task_start = source.index("void AmoledPlatform::audioTask()")
            task_end = source.index("void AmoledPlatform::clearAudioQueue()", task_start)
            task = source[task_start:task_end]
            self.assertIn("bsp_audio_codec_speaker_init()", task)
            self.assertIn("esp_codec_dev_open", task)
            self.assertIn("audioGeneration_[item->channel].load()", task)

            stop_start = source.index("void AmoledPlatform::stopChannel(")
            stop_end = source.index("bool AmoledPlatform::beginMicrophone", stop_start)
            stop_channel = source[stop_start:stop_end]
            self.assertIn("audioGeneration_[channel].fetch_add(1)", stop_channel)
            self.assertNotIn("clearAudioQueue();", stop_channel)


if __name__ == "__main__":
    unittest.main()
