#!/usr/bin/env python3

import unittest
from pathlib import Path

from amoled_source import read_app_source, read_home_source

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
        cls.app_source = read_app_source(ROOT)
        cls.app_header = AMOLED_APP_HEADER.read_text()
        cls.screen_source = read_home_source(ROOT)
        cls.screen_header = HOME_SCREEN_HEADER.read_text()
        cls.stick_explore_source = STICK_EXPLORE.read_text()

    def test_application_uses_extracted_battle_and_shared_services(self):
        session = (ROOT / "firmware/amoled_1_8_v1/main/app/BattleSession.cpp").read_text()
        for service in ("BattleActionService::prepare(", "BattleActionService::commit(",
                        "battleTurnController.planAiTurn(", "Game::ExperienceService::add(",
                        "Game::EffortService::grant(", "Game::FriendshipService::evaluateOffer("):
            self.assertIn(service, session)
        self.assertLess(session.index("BattleActionService::prepare("),
                        session.index("BattleActionService::commit("))
        app = AMOLED_APP.read_text()
        self.assertIn("battleSession_.updateGauges(Platform::clock().millis())", app)
        self.assertIn("battleSession_.updateAttack(Platform::clock().millis())", app)
        self.assertIn("battleSession_.switchPreloaded(Platform::clock().millis())", app)
        self.assertIn("battleSession_.markPresented()", app)
        self.assertNotIn("BattleActionService::commit(", app)
        self.assertNotIn("BattleSystem::resolveEndTurn(", app)

    def test_battle_status_icons_share_hp_bar_row(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]
        self.assertIn("drawBattleStatusIcon(canvas, model.wildStatus, 12, 66);", render)
        self.assertIn("drawBattleHpBar(canvas, 44, 72, 128, model.wildHp);", render)
        self.assertIn("drawBattleStatusIcon(canvas, model.playerStatus, 184, 284);", render)
        self.assertIn("drawBattleHpBar(canvas, 216, 290, 136, model.playerHp);", render)

    def test_renderer_has_lunge_shake_and_hit_effect(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]

        self.assertIn("static constexpr int LUNGE[]", render)
        self.assertIn("if (model.animationHit", render)
        self.assertIn("drawBattleHitEffect(", render)

    def test_player_sprite_and_long_names_use_corrected_battle_layout(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]

        self.assertIn("static constexpr int PLAYER_GROUND_Y = 344;", render)
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

    def test_only_wild_battle_sprites_use_airborne_body_lift(self):
        start = self.screen_source.index("int drawBattleSprite(")
        end = self.screen_source.index("void drawBattleStatusIcon", start)
        sprite = self.screen_source[start:end]
        self.assertIn("fitBattleSprite(width, height, groundPadding,", sprite)
        self.assertIn("fitBattleFrontSprite(speciesId, width, height, groundPadding,", sprite)
        self.assertIn("return layout.verticalOffset;", sprite)
        self.assertIn("layout.rect.x, layout.rect.y, layout.scale", sprite)
        self.assertNotIn("canvas.fillEllipse(", sprite)
        self.assertIn("drawFallbackPet(canvas, centerX, groundY, false);", sprite)
        self.assertNotIn("MAX_BATTLE_SPRITE_SCALE = 1.5f", sprite)
        self.assertIn("WILD_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT, false",
                      self.screen_source)
        self.assertIn("PLAYER_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT, true",
                      self.screen_source)

    def test_hit_effect_keeps_flash_but_no_damage_text(self):
        start = self.screen_source.index("void drawBattleHitEffect(")
        end = self.screen_source.index("void drawBattleSceneText", start)
        effect = self.screen_source[start:end]
        self.assertIn("canvas.drawCircle", effect)
        self.assertNotIn("damage", effect)
        self.assertNotIn("text(", effect)

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
