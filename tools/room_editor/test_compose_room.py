#!/usr/bin/env python3
import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from PIL import Image

import compose_room


class ShadowLogicTests(unittest.TestCase):
    @staticmethod
    def corner_layout():
        return {
            "canvas": {"width": 64, "height": 64},
            "roomGeometry": {
                "faces": [
                    {
                        "id": "left",
                        "type": "wall",
                        "shadowSurface": "left_wall",
                        "receivesShadow": True,
                        "points": [[0, 0], [0, 32], [32, 20], [32, 0]],
                    },
                    {
                        "id": "right",
                        "type": "wall",
                        "shadowSurface": "right_wall",
                        "receivesShadow": True,
                        "points": [[64, 0], [64, 32], [32, 20], [32, 0]],
                    },
                    {
                        "id": "floor",
                        "type": "floor",
                        "shadowSurface": "floor",
                        "receivesShadow": True,
                        "points": [[32, 56], [0, 32], [32, 20], [64, 32]],
                    },
                ]
            },
        }

    def test_auto_shadow_mode_follows_light_shape(self):
        self.assertEqual(
            compose_room.active_shadow_mode({"shadowMode": "auto", "lightShape": "cone"}),
            "directional",
        )
        self.assertEqual(
            compose_room.active_shadow_mode({"shadowMode": "auto", "lightShape": "radial"}),
            "point",
        )

    def test_automatic_wall_target_chooses_one_nearest_face(self):
        left = {
            "id": "left",
            "type": "wall",
            "shadowSurface": "left_wall",
            "receivesShadow": True,
            "points": [[0, 0], [0, 64], [32, 64], [32, 0]],
        }
        right = {
            "id": "right",
            "type": "wall",
            "shadowSurface": "right_wall",
            "receivesShadow": True,
            "points": [[32, 0], [32, 64], [64, 64], [64, 0]],
        }
        layout = {"roomGeometry": {"faces": [left, right]}}
        item = {"x": 6, "y": 8, "kind": "wall_mounted", "shadowFaceIds": []}
        src = Image.new("RGBA", (16, 16), (255, 255, 255, 255))

        self.assertTrue(compose_room.item_should_cast_shadow_on_surface(item, src, "left_wall", left, layout))
        self.assertFalse(compose_room.item_should_cast_shadow_on_surface(item, src, "right_wall", right, layout))

    def test_floor_item_away_from_wall_only_casts_on_floor_face(self):
        floor = {
            "id": "floor",
            "type": "floor",
            "shadowSurface": "floor",
            "receivesShadow": True,
            "points": [[0, 0], [0, 64], [64, 64], [64, 0]],
        }
        wall = {
            "id": "wall",
            "type": "wall",
            "shadowSurface": "left_wall",
            "receivesShadow": True,
            "points": [[0, 0], [0, 20], [64, 20], [64, 0]],
        }
        layout = {"roomGeometry": {"faces": [floor, wall]}}
        item = {"x": 20, "y": 36, "kind": "floor_furniture", "shadowFaceIds": []}
        src = Image.new("RGBA", (12, 12), (255, 255, 255, 255))

        self.assertTrue(compose_room.item_should_cast_shadow_on_surface(item, src, "floor", floor, layout))
        self.assertFalse(compose_room.item_should_cast_shadow_on_surface(item, src, "left_wall", wall, layout))

    def test_projection_ray_stops_at_nearest_receiver_plane(self):
        layout = self.corner_layout()
        model = compose_room.room_projection_model(layout)
        receivers = compose_room.item_projection_faces({}, model)

        wall_hit = compose_room.nearest_projection_receiver(model, (0.5, 0.1, 0.5), (0, -0.4, -0.5), receivers)
        floor_hit = compose_room.nearest_projection_receiver(model, (0.5, 0.5, 0.2), (0, 0, -0.5), receivers)

        self.assertEqual(wall_hit["receiver"]["surface"], "left_wall")
        self.assertEqual(floor_hit["receiver"]["surface"], "floor")

    def test_explicit_wall_target_restricts_projection_receivers(self):
        model = compose_room.room_projection_model(self.corner_layout())
        receivers = compose_room.item_projection_faces({"shadowFaceIds": ["left"]}, model)
        self.assertEqual({receiver["surface"] for receiver in receivers}, {"floor", "left_wall"})

    def test_unified_floor_shadow_skips_per_face_fallback(self):
        layout = self.corner_layout()
        layout["night"] = {
            "lightShape": "radial",
            "lightStrength": 100,
            "lightX": 32,
            "lightY": 0,
            "lightDepth": 80,
            "lightRadius": 240,
            "castShadows": True,
            "shadowMode": "point",
            "shadowAlpha": 100,
            "shadowLength": 20,
            "shadowBlur": 0,
        }
        item = {
            "id": "bed",
            "x": 12,
            "y": 18,
            "kind": "floor_furniture",
            "heightPx": 24,
            "footprint": "rect",
            "shadowPolygon": [[0, 0], [1, 0], [1, 1], [0, 1]],
            "castsShadow": True,
            "shadowOpacity": 100,
            "shadowLength": 20,
            "shadowBlur": 0,
        }
        prepared = [(item, Image.new("RGBA", (20, 20), (255, 255, 255, 255)), 1)]

        with patch.object(compose_room, "draw_shadow_for_item", wraps=compose_room.draw_shadow_for_item) as fallback:
            layer = compose_room.render_shadow_layer((64, 64), layout, layout["night"], prepared, "night")

        self.assertIsNotNone(layer.getbbox())
        fallback.assert_not_called()

    def test_rect_and_none_footprints_have_distinct_results(self):
        night = compose_room.normalize_night({"shadowAlpha": 100, "shadowLength": 20}, "night")
        src = Image.new("RGBA", (20, 20), (255, 255, 255, 255))
        base_item = {"x": 16, "y": 16, "heightPx": 32, "shadowBlur": 0, "shadowOpacity": 100}

        none_layer = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        handled = compose_room.draw_floor_footprint_shadow(
            none_layer,
            night,
            {**base_item, "footprint": "none"},
            src,
            1,
            "night",
            0,
            1,
            1,
        )
        self.assertTrue(handled)
        self.assertIsNone(none_layer.getbbox())

        rect_layer = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        handled = compose_room.draw_floor_footprint_shadow(
            rect_layer,
            night,
            {**base_item, "footprint": "rect"},
            src,
            1,
            "night",
            0,
            1,
            1,
        )
        self.assertTrue(handled)
        self.assertIsNotNone(rect_layer.getbbox())

    def test_item_opacity_is_applied_once_to_shadow_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            Image.new("RGBA", (4, 4), (255, 255, 255, 255)).save(root / "item.png")
            layout = {
                "furniture": [
                    {"id": "item", "fileName": "item.png", "opacity": 0.5, "visibleInDay": True}
                ]
            }
            _item, src, opacity = compose_room.load_prepared_items(layout, root, "day")[0]

        self.assertEqual(src.getchannel("A").getextrema(), (255, 255))
        self.assertEqual(opacity, 0.5)

    def test_receiver_item_gets_other_item_shadow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base_path = root / "base.png"
            Image.new("RGBA", (64, 64), (255, 255, 255, 255)).save(base_path)
            Image.new("RGBA", (30, 20), (220, 80, 80, 255)).save(root / "receiver.png")
            Image.new("RGBA", (10, 10), (80, 120, 220, 255)).save(root / "caster.png")

            layout = {
                "canvas": {"width": 64, "height": 64},
                "roomGeometry": {
                    "faces": [
                        {
                            "id": "floor",
                            "type": "floor",
                            "shadowSurface": "floor",
                            "receivesShadow": True,
                            "points": [[0, 0], [0, 64], [64, 64], [64, 0]],
                        }
                    ]
                },
                "night": {
                    "lightShape": "radial",
                    "lightStrength": 100,
                    "lightX": 25,
                    "lightY": 0,
                    "lightDepth": 180,
                    "lightRadius": 240,
                    "castShadows": True,
                    "shadowMode": "directional",
                    "lightAngle": 90,
                    "shadowAlpha": 100,
                    "shadowLength": 15,
                    "shadowBlur": 0,
                },
                "furniture": [
                    {
                        "id": "receiver",
                        "fileName": "receiver.png",
                        "x": 10,
                        "y": 25,
                        "z": 0,
                        "sortY": 45,
                        "kind": "floor_flat",
                        "castsShadow": False,
                        "receivesShadow": True,
                        "opacity": 1,
                    },
                    {
                        "id": "caster",
                        "fileName": "caster.png",
                        "x": 20,
                        "y": 15,
                        "z": 1,
                        "sortY": 25,
                        "kind": "floor_small",
                        "heightPx": 32,
                        "footprint": "rect",
                        "castsShadow": True,
                        "receivesShadow": False,
                        "shadowOpacity": 100,
                        "shadowLength": 15,
                        "shadowBlur": 0,
                        "opacity": 1,
                    },
                ],
            }
            with_shadow = compose_room.composite(layout, base_path, root, "night", include_lighting=False)
            without_receiver = copy.deepcopy(layout)
            without_receiver["furniture"][0]["receivesShadow"] = False
            without_shadow = compose_room.composite(without_receiver, base_path, root, "night", include_lighting=False)

        self.assertLess(with_shadow.getpixel((25, 28))[0], without_shadow.getpixel((25, 28))[0])


if __name__ == "__main__":
    unittest.main()
