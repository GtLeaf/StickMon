import base64
import io
import json
import shutil
import tempfile
import threading
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from PIL import Image

import project_scene as scene
import serve


class ProjectSceneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.current = scene.build_project_scene()

    def test_current_scene_includes_every_source_asset_and_firmware_geometry(self):
        payload = self.current
        layout = payload["layout"]
        self.assertEqual(len(payload["items"]), len(layout["furniture"]))
        self.assertEqual(len(payload["backgroundItems"]), len(layout["backgroundLayers"]))
        for asset in payload["backgroundItems"] + payload["items"]:
            data = base64.b64decode(asset["dataUrl"].split(",", 1)[1])
            with Image.open(io.BytesIO(data)) as image:
                size = (asset.get("sourceWidth", asset.get("width")),
                        asset.get("sourceHeight", asset.get("height")))
                self.assertEqual(image.size, size)
            self.assertTrue((scene.ROOT / asset["projectPath"]).is_file())
        self.assertEqual(payload["devices"]["amoled"]["roomHeight"], 296)
        self.assertEqual(payload["devices"]["amoled"]["camera"]["right"], 122)
        compiled = payload["source"]["compiledRoom"]
        self.assertEqual((compiled["width"], compiled["height"]), scene.room_pipeline.room_size(layout))
        self.assertTrue(payload["sprites"])
        self.assertTrue(all(entry["actions"] for entry in payload["sprites"]))

    def fixture(self, directory):
        root = Path(directory)
        source = root / "origin_asset/room/standar"
        source.mkdir(parents=True)
        furniture = root / "origin_asset/room/forniture"
        furniture.mkdir()
        Image.new("RGB", (240, 161), "red").save(source / "base.png")
        Image.new("RGBA", (20, 30), "blue").save(furniture / "chair.png")
        layout = {
            "canvas": {"width": 240, "height": 161},
            "backgroundLayers": [{"fileName": "base.png"}],
            "furniture": [{"id": "f1", "fileName": "chair.png", "x": 10, "y": 20,
                           "targetWidth": 10, "targetHeight": 15}],
        }
        (source / "room_layout.json").write_text(json.dumps(layout))
        pack = root / "data/packs/other"
        (pack / "rooms").mkdir(parents=True)
        (pack / "sprites").mkdir()
        original, _ = scene.active_pack(scene.ROOT)
        shutil.copy(original / "rooms/standard.smonroom", pack / "rooms/standard.smonroom")
        shutil.copy(next((original / "sprites").glob("*.smonsp")), pack / "sprites/001.smonsp")
        (pack / "manifest.json").write_text(json.dumps({
            "format": "smon-resource-pack-v1", "schema": 1, "id": "other",
            "sprites": "sprites", "room": "rooms/standard.smonroom"}))
        (root / "data/active.json").write_text('{"activePack":"other"}')
        for relative in ("src/assets/PokemonSprites.h", "src/game/Species.cpp",
                         "src/platform/api/PlatformServices.h", "src/scenes/MainScene.cpp",
                         "firmware/amoled_1_8_v1/main/AmoledGeometry.h",
                         "firmware/amoled_1_8_v1/main/ui/UiMetrics.h",
                         "firmware/amoled_1_8_v1/main/AmoledApp.cpp"):
            dest = root / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(scene.ROOT / relative, dest)
        return root, source, furniture

    def test_reload_observes_source_and_firmware_edits_without_generation(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, furniture = self.fixture(directory)
            first = scene.build_project_scene(root)
            self.assertEqual(first["source"]["packId"], "other")
            layout = json.loads((source / "room_layout.json").read_text())
            layout["furniture"][0]["x"] = 77
            (source / "room_layout.json").write_text(json.dumps(layout))
            Image.new("RGBA", (40, 60), "green").save(furniture / "chair.png")
            geometry = root / "firmware/amoled_1_8_v1/main/AmoledGeometry.h"
            geometry.write_text(geometry.read_text().replace("WIDTH = 368", "WIDTH = 400"))
            second = scene.build_project_scene(root)
            chair = second["items"][0]
            self.assertEqual(chair["x"], 77)
            self.assertEqual(chair["sourceWidth"], 40)
            self.assertEqual(chair["sourceWidth"] * chair["scaleX"], 10)
            self.assertNotEqual(chair["dataUrl"], first["items"][0]["dataUrl"])
            self.assertEqual(second["devices"]["amoled"]["width"], 400)

    def test_missing_asset_and_unknown_room_source_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root, source, furniture = self.fixture(directory)
            (furniture / "chair.png").unlink()
            with self.assertRaisesRegex(FileNotFoundError, "chair.png"):
                scene.build_project_scene(root)
            pack, manifest = scene.active_pack(root)
            manifest["room"] = "rooms/another.smonroom"
            (pack / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "--layout"):
                scene.build_project_scene(root)

    def test_active_pack_cannot_escape_resource_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _, _ = self.fixture(directory)
            (root / "data/active.json").write_text('{"packPath":"/packs/../../outside"}')
            with self.assertRaisesRegex(ValueError, "inside data/packs"):
                scene.active_pack(root)


class EditorServerTests(unittest.TestCase):
    def test_editor_and_read_only_api_share_one_origin(self):
        server = serve.create_server(0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        url = f"http://127.0.0.1:{server.server_port}"
        try:
            with urlopen(url) as response:
                self.assertIn(b"loadProjectScene", response.read())
            with urlopen(url + "/api/project-scene") as response:
                self.assertEqual(response.headers["Cache-Control"], "no-store")
                self.assertEqual(json.load(response)["type"], "stickmon-project-scene")
            for target in ("/project_scene.py", "/%2e%2e/prepare_room_background.py"):
                with self.assertRaises(HTTPError) as error:
                    urlopen(url + target)
                self.assertEqual(error.exception.code, 404)
            with self.assertRaises(HTTPError) as error:
                urlopen(Request(url + "/api/project-scene", data=b"{}", method="POST"))
            self.assertEqual(error.exception.code, 501)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == "__main__":
    unittest.main()
