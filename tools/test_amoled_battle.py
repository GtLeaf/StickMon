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
                "pushBattleLog(nowMs, true, BATTLE_ATTACK_LOG_MS);", attack
            )
            self.assertIn(
                "pushBattleLog(nowMs, false, BATTLE_RESULT_LOG_MS);", attack
            )
            self.assertIn(
                "requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);", attack
            )
            self.assertIn("battleAudioPending = true;", attack)
            self.assertIn("battleAudioReady = false;", attack)
            self.assertNotIn("CryPlayer::ins().replay(", attack)

    def test_effect_outcomes_follow_attack_result_before_next_turn(self):
        start = self.app_source.index("void AmoledApp::enqueueBattleEffectLogs(")
        end = self.app_source.index("\nvoid AmoledApp::", start + 10)
        effects = self.app_source[start:end]
        for kind in ("STAT_CHANGED", "STATUS_APPLIED", "CONFUSED", "CURED",
                     "HEALED", "DRAINED", "ABILITY_ACTIVATED"):
            self.assertIn(f"EffectOutcomeKind::{kind}", effects)
        self.assertIn("Ui::Explore::STAT_FELL_FMT", effects)
        self.assertIn("Ui::Explore::STAT_NAMES[stat]", effects)
        for attacker in ("performBattlePlayerAction", "performBattleWildAction"):
            start = self.app_source.index(f"void AmoledApp::{attacker}(")
            end = self.app_source.index("\nvoid AmoledApp::", start + 10)
            action = self.app_source[start:end]
            self.assertLess(action.index("pushBattleLog(nowMs, false, BATTLE_RESULT_LOG_MS);"),
                            action.index("enqueueBattleEffectLogs(moveEffects,"))
        self.assertIn("!battleLogPlaybackBusy()) {", self.app_source)

    def test_switch_animates_both_sides_before_counterattack(self):
        start = self.app_source.index("void AmoledApp::performBattleSwitch(")
        end = self.app_source.index("\nvoid AmoledApp::performBattleWildTurn(", start)
        switch = self.app_source[start:end]
        self.assertLess(switch.index("BattleSwitchStage::RETREATING"),
                        switch.index("battlePlayerSlot = pendingBattleSwitchSlot;"))
        self.assertLess(switch.index("BattleSwitchStage::ENTERING"),
                        switch.index("battleContinuation = BattleContinuation::WILD_TURN;"))
        self.assertIn("requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);", switch)
        self.assertIn("battleSwitchStage == BattleSwitchStage::NONE", self.app_source)
        self.assertIn("model.playerSwitchOffsetX =", self.app_source)
        self.assertIn("playerX += model.playerSwitchOffsetX;", self.screen_source)

    def test_battle_status_icons_share_hp_bar_row(self):
        start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[start:]
        self.assertIn("drawBattleStatusIcon(canvas, model.wildStatus, 12, 66);", render)
        self.assertIn("drawBattleHpBar(canvas, 44, 72, 128, model.wildHp);", render)
        self.assertIn("drawBattleStatusIcon(canvas, model.playerStatus, 184, 284);", render)
        self.assertIn("drawBattleHpBar(canvas, 216, 290, 136, model.playerHp);", render)

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
        self.assertIn("const int airLift = back ? 0 : battleSpriteAirLift(speciesId);", sprite)
        self.assertIn("layout.rect.x, layout.rect.y - airLift, layout.scale", sprite)
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
        self.assertIn("battleAttackLogHeld = false;", animation)
        self.assertIn(
            "battleContinuation != BattleContinuation::NONE", animation
        )
        self.assertIn("!battleAnimationActive && !battleHpAnimationActive", animation)
        self.assertIn("!battleLogPlaybackBusy()", animation)
        self.assertIn("advanceBattleTurn(nowMs);", animation)

    def test_battle_logs_use_per_entry_display_durations(self):
        self.assertIn("battleLogUntil = nowMs + 1000;", self.stick_explore_source)
        self.assertIn(
            "static constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 24;",
            self.app_header,
        )
        self.assertIn("constexpr uint16_t BATTLE_LOG_DEFAULT_MS = 700;",
                      self.app_source)
        self.assertIn("constexpr uint16_t BATTLE_ATTACK_LOG_MS = 700;",
                      self.app_source)
        self.assertIn("constexpr uint16_t BATTLE_RESULT_LOG_MS = 650;",
                      self.app_source)
        self.assertIn("uint16_t s_battleLogDurations", self.app_source)
        self.assertNotIn("BATTLE_LOG_LINE_MS = 1000", self.app_source)
        service_start = self.app_source.index(
            "bool AmoledApp::serviceBattleLog(uint32_t nowMs)"
        )
        service_end = self.app_source.index(
            "bool AmoledApp::battleLogPlaybackBusy() const", service_start
        )
        service = self.app_source[service_start:service_end]
        self.assertIn("battleLogQueueCount == 0", service)
        self.assertIn("battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP", service)
        self.assertIn("battleLogUntil = nowMs + durationMs;", service)
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

        experience_start = self.app_source.index(
            "void AmoledApp::startBattleExperienceAnimation("
        )
        experience_end = self.app_source.index(
            "uint32_t AmoledApp::battleExperienceForRender(", experience_start
        )
        experience = self.app_source[experience_start:experience_end]
        self.assertIn("BattleSystem::experienceAwards(", experience)
        self.assertIn("awards.active", experience)

    def test_impact_audio_plays_at_damage_timing_node(self):
        update_start = self.app_source.index("void AmoledApp::update(")
        update_end = self.app_source.index(
            "void AmoledApp::", update_start + 10
        )
        update = self.app_source[update_start:update_end]
        self.assertIn("elapsed >= BATTLE_HP_DAMAGE_DELAY_MS", update)
        self.assertIn("battleImpactAudioPlayed = true;", update)
        self.assertIn("static_cast<SfxCue>(battleImpactSfx)", update)

        for function_name in (
            "void AmoledApp::performBattlePlayerAction(",
            "void AmoledApp::performBattleWildAction(",
        ):
            start = self.app_source.index(function_name)
            end = self.app_source.index("\nvoid AmoledApp::", start + 10)
            attack = self.app_source[start:end]
            self.assertIn("battlePendingSfx = 0xFF;", attack)
            self.assertIn("battleImpactSfx = static_cast<uint8_t>(", attack)

    def test_victory_auto_finishes_once_without_continue_button(self):
        update_start = self.app_source.index("void AmoledApp::update(")
        update_end = self.app_source.index(
            "void AmoledApp::", update_start + 10
        )
        update = self.app_source[update_start:update_end]
        victory_gate = update.index(
            "battlePhase == BattleViewModel::Phase::VICTORY"
        )
        victory_finish = update.index("finishBattleVictory(nowMs);", victory_gate)
        victory_block = update[victory_gate:victory_finish]
        self.assertIn("!battleExperienceAnimationActive", victory_block)
        self.assertIn("!battleLogPlaybackBusy()", victory_block)
        self.assertIn("!battleVictoryFinalizePending", victory_block)

        item_start = self.screen_source.index("int battleItemAt(")
        item_end = self.screen_source.index("bool battleBackAt(", item_start)
        self.assertIn(
            "if (phase == BattleViewModel::Phase::VICTORY) return -1;",
            self.screen_source[item_start:item_end],
        )

        render_start = self.screen_source.index("void renderBattleScreen(")
        render = self.screen_source[render_start:]
        self.assertNotIn(
            "model.phase == BattleViewModel::Phase::VICTORY",
            render,
        )

    def test_victory_rewards_reserve_effort_and_serializes_progression(self):
        start = self.app_source.index("void AmoledApp::finishBattleVictory(")
        end = self.app_source.index(
            "void AmoledApp::resolveBattleFriendship(", start
        )
        victory = self.app_source[start:end]
        self.assertIn("BattleSystem::experienceAwards(", victory)
        self.assertGreaterEqual(
            victory.count("Game::EffortService::grant("), 2
        )
        self.assertIn("Ui::Explore::SHARED_EXP_GAIN_FMT", victory)
        self.assertGreaterEqual(victory.count("queueBattleProgression("), 2)
        self.assertIn("battleVictoryFinalizePending = true", victory)

        update_start = self.app_source.index("void AmoledApp::update(")
        update_end = self.app_source.index(
            "void AmoledApp::", update_start + 10
        )
        update = self.app_source[update_start:update_end]
        self.assertIn("battleVictoryFinalizePending", update)
        self.assertIn("finishBattleAfterFriendship(nowMs)", update)

        finish_start = self.app_source.index(
            "void AmoledApp::finishBattleAfterFriendship("
        )
        finish_end = self.app_source.index(
            "void AmoledApp::finishBattleDefeat(", finish_start
        )
        finish = self.app_source[finish_start:finish_end]
        self.assertIn("startNextBattleProgression(nowMs)", finish)

        complete_start = self.app_source.index(
            "void AmoledApp::completeProgression("
        )
        complete_end = self.app_source.index(
            "void AmoledApp::openShowerScene(", complete_start
        )
        complete = self.app_source[complete_start:complete_end]
        self.assertIn("battleProgressionSequenceActive", complete)
        self.assertIn("startNextBattleProgression(nowMs)", complete)
        self.assertIn("closeBattle(nowMs)", complete)

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

    def test_battle_completion_has_no_toast(self):
        start = self.app_source.index(
            "void AmoledApp::finishBattleAfterFriendship("
        )
        end = self.app_source.index(
            "void AmoledApp::resetBattleProgressionQueue(", start
        )
        self.assertNotIn("setToast(", self.app_source[start:end])

    def test_battle_bag_shows_daily_items_and_feeds_wild_monsters(self):
        bag_start = self.app_source.index("void AmoledApp::performBattleBag(")
        bag_end = self.app_source.index(
            "void AmoledApp::performBattleBagItem(", bag_start
        )
        bag = self.app_source[bag_start:bag_end]
        self.assertIn("openItemScene(AppSceneFlow::Scene::BAG);", bag)
        self.assertIn("battleBagMode = true;", bag)
        self.assertIn("homeBagItemCount(gameState)", bag)
        self.assertIn("model.exploreOnly = false;", self.app_source)
        self.assertIn("model.battleMode = battleBagMode;", self.app_source)
        self.assertIn("homeBagDailyItemCount(gameState)", self.app_source)
        self.assertIn("homeBagItemAt(gameState, index)", self.app_source)
        item_start = self.app_source.index("void AmoledApp::performBattleBagItem(")
        item_end = self.app_source.index("void AmoledApp::performBattleFlee(", item_start)
        item_action = self.app_source[item_start:item_end]
        self.assertIn("Game::foodIndexForItemId(item)", item_action)
        self.assertIn("Game::ItemInventory::remove(gameState, item)", item_action)
        self.assertIn("FriendshipSystem::classifyFoodThrow(", item_action)
        self.assertIn("FriendshipSystem::acceptsFoodThrow(", item_action)
        self.assertIn("FriendshipSystem::addFoodBond(", item_action)
        self.assertIn("BattleContinuation::ADVANCE_TURN", item_action)
        self.assertIn("BattleContinuation::WILD_TURN", item_action)
        self.assertIn("Game::ItemInventory::usableInBattle(item)",
                      self.app_source)
        self.assertIn("setToast(Ui::Amoled::CANNOT_USE, nowMs)", self.app_source)
        inventory = (ROOT / "src" / "game" / "ItemInventory.cpp").read_text()
        self.assertIn("ItemId::NORMAL_FOOD", inventory)
        self.assertIn("ItemId::SOAP_0", inventory)
        item_screen = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
                       "ui" / "ItemShopScreens.inc").read_text()
        self.assertIn("model.battleMode", item_screen)
        self.assertIn("Ui::Amoled::FEED", item_screen)

    def test_friendship_offer_uses_stick_log_sequence_and_food_bond(self):
        victory = self.app_source[
            self.app_source.index("void AmoledApp::finishBattleVictory("):
            self.app_source.index("void AmoledApp::resolveBattleFriendship(")
        ]
        recognized = victory.index("Ui::Explore::FRIEND_RECOGNIZES_FMT")
        question = victory.index("Ui::Explore::FRIEND_CONTACT_QUESTION")
        self.assertLess(recognized, question)
        self.assertIn("allowsFriendship, battleFoodBond", victory)
        self.assertNotIn("Ui::Amoled::BECOME_FRIEND", victory)

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
