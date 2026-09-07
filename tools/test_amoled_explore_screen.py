#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"


class AmoledExploreScreenTests(unittest.TestCase):
    def test_background_continues_behind_scene_selector(self):
        source = HOME_SCREEN.read_text()
        start = source.index("void renderExploreScreen(")
        end = source.index("bool exploreRouteBackAt(", start)
        render = source[start:end]

        self.assertIn("EXPLORE_SELECTOR_TOP_HEIGHT", render)
        self.assertIn("EXPLORE_SELECTOR_BUTTON_TOP", render)
        self.assertIn("EXPLORE_PREVIEW_CENTER_Y", render)
        self.assertIn("drawExploreBackgroundLayer(canvas, rowBegin, rowEnd)", render)
        # Static tint is baked into the background cache, not blended per frame.
        self.assertNotIn("PixelRenderer::fillRectAlpha(", render)
        self.assertIn("Ui::Explore::DEPART", render)
        self.assertIn("Ui::BACK", render)
        self.assertNotIn("drawBackIcon(canvas);", render)
        self.assertNotIn("drawMenuIcon(canvas);", render)

    def test_preview_background_is_cached_for_carousel_frames(self):
        source = HOME_SCREEN.read_text()
        start = source.index("bool drawExploreBackgroundLayer(")
        end = source.index("constexpr const char* EXPLORE_AREA_NAMES", start)
        cache = source[start:end]
        self.assertIn("exploreBackgroundCache", cache)
        self.assertIn("GameAssets::draw(GameAssets::Kind::EXPLORE_MENU_BACKGROUND", cache)

        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()
        self.assertIn("EXPLORE_PREVIEW_RENDER_TOP", app)
        self.assertIn("EXPLORE_PREVIEW_RENDER_BOTTOM", app)
        self.assertIn("requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT)", app)
        self.assertNotIn("if (exploreAreaAnimating) {\n            requestRenderRows(0, 224);", app)

    def test_route_animation_uses_cached_map_and_dynamic_refresh(self):
        home = HOME_SCREEN.read_text()
        start = home.index("bool drawExploreRouteMapLayer(")
        end = home.index("void drawExploreRoutePickup", start)
        route_cache = home[start:end]
        self.assertIn("drawExploreRouteWorldViewport", route_cache)
        self.assertIn("model.mapFrame", route_cache)
        self.assertNotIn("cachedExploreRouteMapFrame == model.mapFrame", route_cache)

        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()
        self.assertIn("requestExploreRouteDynamicRender", app)
        self.assertIn("EXPLORE_ROUTE_MAP_FRAME_MS", app)

    def test_route_camera_uses_prerendered_world_cache(self):
        home = HOME_SCREEN.read_text()
        assets = (ROOT / "src" / "assets" / "GameAssets.cpp").read_text()
        main = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "main.cpp").read_text()

        self.assertIn("prepareExploreRouteWorldCache", home)
        self.assertIn("drawExploreRouteWorldViewport", home)
        self.assertIn("exploreRouteWorldCache", home)
        self.assertIn("GameAssets::drawExploreTileTo(", home)
        self.assertIn("bool drawExploreTileTo(Canvas565& canvas", assets)
        self.assertIn("loopElapsedMs < targetLoopMs", main)
        self.assertNotIn(
            "vTaskDelay(pdMS_TO_TICKS(lockPhase == LockPhase::OPEN ? 20 : 16))",
            main,
        )
        self.assertIn("EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH", home)
        self.assertIn("std::memcpy(firstOutput, source", home)
        self.assertNotIn("firstOutput[column * 2] = source[column]", home)
        self.assertNotIn("exploreRouteMapCache", home)

    def test_route_preloads_and_pins_all_area_encounters(self):
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()
        start = app.index("bool AmoledApp::startExploreRoute(")
        end = app.index("bool AmoledApp::generateExploreRouteMap(", start)
        route_start = app[start:end]

        self.assertIn("encounterTableForArea(selectedExploreArea)", route_start)
        self.assertIn("setPinnedDynamicSpecies(", route_start)
        self.assertIn("preloadDynamicSpecies(", route_start)
        self.assertIn("setPinnedDynamicSpecies(nullptr, 0)", app)

    def test_route_draws_and_preloads_regional_boss(self):
        home = HOME_SCREEN.read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()

        self.assertIn("void drawExploreRouteBoss(", home)
        self.assertIn("model.bossSpeciesId", home)
        self.assertIn("drawExploreRouteBoss(canvas, model);", home)
        self.assertIn("model.bossPending = exploreRouteBossPending;", app)
        self.assertIn("routeSpecies[routeSpeciesCount++] = exploreRouteBossSpeciesId;", app)

    def test_route_completion_is_presented_as_a_modal(self):
        home = HOME_SCREEN.read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()

        self.assertIn("if (model.complete", home)
        self.assertIn("Ui::Explore::RESULT_END", home)
        self.assertIn("Ui::Explore::ANY_KEY_RETURN", home)
        completion = app[app.index("exploreRouteComplete = true;"):]
        self.assertIn("requestFullRender();", completion)

    def test_departure_fades_between_room_and_route(self):
        header = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h").read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()
        home = HOME_SCREEN.read_text()

        self.assertIn("FADE_OUT", header)
        self.assertIn("FADE_IN", header)
        self.assertIn("EXPLORE_SCENE_FADE_MS = 300", app)
        self.assertIn("ExpeditionDeparturePhase::FADE_OUT", app)
        self.assertIn("ExpeditionDeparturePhase::FADE_IN", app)
        self.assertIn("drawSceneFadeOverlay(canvas, model.fadeAlpha);", home)

    def test_home_hunger_icon_uses_asset_scaling(self):
        hud = (ROOT / "src" / "presentation" / "HudRenderer.cpp").read_text()
        self.assertIn("canvas.drawAssetPixel(", hud)

    def test_route_uses_fixed_bottom_hud_with_hp_and_status(self):
        home = HOME_SCREEN.read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()

        self.assertIn("EXPLORE_ROUTE_MAP_TOP = 0", home)
        self.assertIn("EXPLORE_ROUTE_MAP_BOTTOM = HOME_STATUS_TOP", home)
        self.assertIn("void drawExploreRouteHud(", home)
        self.assertIn("void drawExploreRouteBottomHud(", home)
        self.assertIn("std::min<uint8_t>(model.state->teamCount, 2)", home)
        self.assertIn("GameAssets::statusKind(monster.majorStatus)", home)
        self.assertIn("drawExploreBagIcon(canvas);", home)
        self.assertIn("drawExploreMenuIcon(canvas);", home)
        self.assertIn("drawExploreRouteHud(canvas, model);", home)
        self.assertIn("drawExploreRouteBottomHud(canvas, model);", home)
        self.assertNotIn("drawMenuIcon(canvas);\n}", home[home.index("void drawExploreRouteHud("):home.index("void drawExploreRouteBottomHud(")])
        self.assertIn("model.state = &gameState;", app)

    def test_route_camera_refresh_excludes_fixed_bottom_hud(self):
        home = HOME_SCREEN.read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()

        self.assertIn("EXPLORE_ROUTE_VIEW_HEIGHT = HOME_STATUS_TOP", app)
        self.assertIn("requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);", app)
        self.assertIn("constexpr int mapBottom = EXPLORE_ROUTE_VIEW_HEIGHT", app)
        self.assertIn("y < EXPLORE_ROUTE_MAP_BOTTOM", home)

    def test_route_bottom_buttons_open_bag_and_menu(self):
        home = HOME_SCREEN.read_text()
        app = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp").read_text()

        self.assertIn("bool exploreRouteBagAt(int x, int y)", home)
        self.assertIn("if (exploreRouteBagAt(x, y))", app)
        self.assertIn("openItemScene(AppSceneFlow::Scene::BAG);", app)
        self.assertIn("if (exploreRouteMenuAt(x, y))", app)

    def test_map_tiles_have_a_decoded_cache_path(self):
        assets = (ROOT / "src" / "assets" / "GameAssets.cpp").read_text()
        self.assertIn("struct DecodedTile", assets)
        self.assertIn("decodeTile", assets)
        self.assertIn("std::memset(tile.pixels, 0", assets)
        self.assertIn("std::memset(tile.opaqueMask, 0", assets)
        self.assertIn("drawMaskedAssetImage", assets)
        self.assertIn("drawDecodedTile(canvas, kind, ref", assets)
        self.assertIn("isExploreTileAnimated", assets)
        self.assertIn("drawExploreMapAnimations", HOME_SCREEN.read_text())

    def test_debug_build_profiles_exploration_scroll_stages(self):
        home = HOME_SCREEN.read_text()
        app_header = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"
        ).read_text()
        main = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "main.cpp"
        ).read_text()

        self.assertIn("[ExploreMapPerf]", home)
        self.assertIn("base(avg/max)", home)
        self.assertIn("anim(avg/max)", home)
        self.assertIn("if (model.walking)", home)
        self.assertIn("[ExplorePerf]", main)
        self.assertIn("rows(avg/max)", main)
        self.assertIn("gapMax", main)
        self.assertIn("exploreRouteMovingForDiagnostics", app_header)
        self.assertIn("#if STICKMON_ENABLE_DEBUG_FEATURES", main)


if __name__ == "__main__":
    unittest.main()
