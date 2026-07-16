#!/usr/bin/env python3
import argparse
from dataclasses import dataclass, field, replace
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "origin_asset" / "source_sheets" / "high"
SOURCE_ROOT = ROOT / "origin_asset" / "source_sheets"
OUT_ROOT = ROOT / "origin_asset" / "processed"

CANVAS_SIZE = 64
BOTTOM_MARGIN = 6
CONTACT_BG = (0, 120, 144, 255)

SOURCE_DIRECTIONS = ["front", "down_left", "left", "up_left", "back"]
GENERATED_DIRECTIONS = ["front", "down_left", "left", "up_left", "back", "up_right", "right", "down_right"]
LATI_SOURCE_DIRECTIONS = ["left", "down_left", "front", "up_left", "back"]
MIRRORS = {
    "up_right": "up_left",
    "right": "left",
    "down_right": "down_left",
}


@dataclass(frozen=True)
class SpeciesSpec:
    species_id: int
    slug: str
    display_name: str
    source_name: str
    scale: float
    boxes: dict
    sleeping_boxes: list
    source_group: str = "high"
    sleeping_source_name: str = None
    sleeping_source_group: str = None
    sleeping_layer_override: tuple = None
    canvas_width: int = CANVAS_SIZE
    canvas_height: int = CANVAS_SIZE
    bottom_margin: int = BOTTOM_MARGIN
    crop_padding: int = 0
    fit_to_canvas: bool = True
    source_directions: list = field(default_factory=lambda: SOURCE_DIRECTIONS)
    contact_directions: list = field(default_factory=lambda: GENERATED_DIRECTIONS)
    mirror_directions: bool = True
    mirror_map: dict = field(default_factory=lambda: MIRRORS)
    export_mirror_frames: bool = True
    frame_overrides: dict = field(default_factory=dict)
    auxiliary_boxes: dict = field(default_factory=dict)
    auxiliary_directions: dict = field(default_factory=dict)
    auxiliary_canvas_sizes: dict = field(default_factory=dict)
    background_tolerance: int = 0
    mask_to_largest_component: bool = False
    runtime_wired: bool = True
    notes: list = field(default_factory=list)


def processed_dir_name(spec):
    return f"{spec.species_id:03d}_{spec.slug}"


def processed_species_dir(spec):
    return OUT_ROOT / processed_dir_name(spec)


SPECS = [
    SpeciesSpec(
        species_id=151,
        slug="mew",
        display_name="Mew",
        source_name="151_mew.png",
        source_group="medium",
        scale=2.0,
        boxes={},
        sleeping_boxes=[],
        notes=[
            "Custom layout: walking source row has five directions and mirrors right-side directions.",
            "Idle uses eight source directions from two rows and is not mirrored.",
        ],
    ),
    SpeciesSpec(
        species_id=1,
        slug="bulbasaur",
        display_name="Bulbasaur",
        source_name="妙蛙种子.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses walk0 for each direction.",
            "Crop boxes were derived from PSD layers walk0..walk4 and sleeping.",
        ],
        sleeping_boxes=[(353, 17, 374, 37), (375, 17, 396, 37)],
        boxes={
            "idle": [
                [(71, 18, 89, 40)],
                [(67, 41, 88, 63)],
                [(68, 67, 90, 88)],
                [(69, 89, 90, 111)],
                [(71, 114, 89, 136)],
            ],
            "walking": [
                [(71, 18, 89, 40), (92, 18, 113, 40), (115, 18, 133, 40)],
                [(67, 41, 88, 63), (90, 41, 116, 63), (118, 41, 140, 63)],
                [(68, 67, 90, 88), (92, 67, 115, 88), (118, 67, 142, 88)],
                [(69, 89, 90, 111), (92, 89, 116, 111), (118, 89, 141, 111)],
                [(71, 114, 89, 136), (91, 114, 110, 136), (112, 114, 130, 136)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=2,
        slug="ivysaur",
        display_name="Ivysaur",
        source_name="妙蛙草.psd",
        source_group="low/psd",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        sleeping_layer_override=("low/psd", "妙蛙草.psd", "sleeping"),
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses walk0 for each direction.",
            "Crop boxes were derived from PSD layers walk0..walk4 and sleeping.",
        ],
        sleeping_boxes=[(351, 22, 374, 44), (375, 22, 398, 44)],
        boxes={
            "idle": [
                [(79, 22, 99, 44)],
                [(76, 47, 99, 70)],
                [(76, 72, 100, 92)],
                [(76, 101, 97, 122)],
                [(76, 126, 96, 147)],
            ],
            "walking": [
                [(79, 22, 99, 44), (100, 22, 122, 44), (123, 22, 144, 44)],
                [(76, 47, 99, 70), (100, 47, 123, 70), (124, 47, 149, 70)],
                [(76, 72, 100, 92), (101, 72, 126, 92), (127, 72, 152, 92)],
                [(76, 101, 97, 122), (98, 101, 122, 122), (123, 101, 144, 122)],
                [(76, 126, 96, 147), (97, 126, 118, 147), (119, 126, 139, 147)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=3,
        slug="venusaur",
        display_name="Venusaur",
        source_name="妙蛙花.psd",
        source_group="low/psd",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        sleeping_layer_override=("low/psd", "妙蛙花.psd", "sleeping"),
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses walk0 for each direction.",
            "Crop boxes were derived from PSD layers walk0..walk4 and sleeping.",
        ],
        sleeping_boxes=[(350, 16, 376, 42), (376, 16, 403, 42)],
        boxes={
            "idle": [
                [(72, 20, 98, 46)],
                [(69, 48, 96, 73)],
                [(69, 78, 97, 102)],
                [(71, 106, 97, 132)],
                [(73, 134, 99, 158)],
            ],
            "walking": [
                [(72, 20, 98, 46), (98, 20, 126, 46), (126, 20, 153, 46)],
                [(69, 48, 96, 73), (96, 48, 123, 73), (123, 48, 152, 73)],
                [(69, 78, 97, 102), (97, 78, 126, 102), (126, 78, 155, 102)],
                [(71, 106, 97, 132), (97, 106, 126, 132), (126, 106, 151, 132)],
                [(73, 134, 99, 158), (99, 134, 125, 158), (125, 134, 150, 158)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=133,
        slug="eevee",
        display_name="Eevee",
        source_name="133_eevee.png",
        scale=2.0,
        sleeping_boxes=[(542, 24, 562, 43), (567, 24, 587, 43)],
        boxes={
            "idle": [
                [(1, 24, 20, 42), (25, 24, 44, 42)],
                [(1, 47, 20, 67), (25, 47, 40, 67)],
                [(1, 73, 22, 93), (27, 73, 47, 93)],
                [(1, 98, 16, 119), (21, 98, 40, 119)],
                [(1, 124, 20, 143), (25, 124, 44, 143)],
            ],
            "walking": [
                [(80, 26, 99, 47), (104, 31, 125, 47), (130, 24, 151, 47)],
                [(80, 52, 97, 73), (102, 55, 122, 73), (127, 52, 149, 73)],
                [(80, 78, 102, 99), (107, 81, 132, 99), (137, 78, 160, 99)],
                [(80, 104, 98, 126), (103, 105, 123, 126), (128, 106, 155, 126)],
                [(80, 135, 99, 155), (104, 131, 125, 155), (130, 138, 149, 155)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=4,
        slug="charmander",
        display_name="Charmander",
        source_name="小火龙.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
        ],
        sleeping_boxes=[(353, 21, 377, 44), (377, 21, 401, 44)],
        boxes={
            "idle": [
                [(74, 14, 95, 38)],
                [(77, 38, 98, 61)],
                [(75, 60, 97, 83)],
                [(77, 83, 98, 105)],
                [(81, 109, 98, 133)],
            ],
            "walking": [
                [(74, 14, 95, 38), (95, 15, 115, 38), (114, 14, 133, 38)],
                [(77, 38, 98, 61), (97, 40, 117, 61), (117, 38, 139, 61)],
                [(75, 60, 97, 83), (97, 61, 119, 83), (119, 61, 142, 83)],
                [(77, 83, 98, 105), (98, 83, 120, 105), (119, 83, 139, 105)],
                [(81, 109, 98, 133), (98, 109, 114, 133), (114, 109, 130, 133)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=5,
        slug="charmeleon",
        display_name="Charmeleon",
        source_name="火恐龙.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
        ],
        sleeping_boxes=[(351, 34, 376, 59), (375, 35, 401, 59)],
        boxes={
            "idle": [
                [(76, 18, 96, 42)],
                [(72, 42, 93, 67)],
                [(72, 68, 97, 93)],
                [(73, 94, 95, 118)],
                [(78, 122, 96, 148)],
            ],
            "walking": [
                [(76, 18, 96, 42), (95, 16, 115, 42), (115, 16, 135, 42)],
                [(72, 42, 93, 67), (92, 44, 113, 67), (112, 42, 137, 67)],
                [(72, 68, 97, 93), (97, 69, 120, 93), (120, 69, 143, 93)],
                [(73, 94, 95, 118), (95, 93, 120, 118), (120, 93, 139, 118)],
                [(78, 122, 96, 148), (96, 122, 115, 147), (116, 122, 135, 147)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=6,
        slug="charizard",
        display_name="Charizard",
        source_name="喷火龙.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
        ],
        sleeping_boxes=[(346, 19, 375, 50), (375, 20, 403, 50)],
        boxes={
            "idle": [
                [(67, 16, 96, 44)],
                [(67, 44, 94, 74)],
                [(66, 74, 97, 104)],
                [(66, 106, 97, 136)],
                [(66, 140, 95, 171)],
            ],
            "walking": [
                [(67, 16, 96, 44), (96, 13, 125, 44), (125, 13, 154, 44)],
                [(67, 44, 94, 74), (94, 44, 121, 74), (121, 43, 153, 74)],
                [(66, 74, 97, 104), (96, 76, 125, 104), (125, 74, 154, 104)],
                [(66, 106, 97, 136), (97, 106, 132, 136), (132, 105, 157, 136)],
                [(66, 140, 95, 171), (95, 140, 125, 171), (125, 140, 154, 171)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=7,
        slug="squirtle",
        display_name="Squirtle",
        source_name="007_squirtle.png",
        source_group="low",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites 15 and 16.",
        ],
        sleeping_boxes=[(274, 40, 303, 55), (305, 40, 333, 55)],
        boxes={
            "idle": [
                [(2, 4, 19, 22)],
                [(55, 3, 75, 22)],
                [(121, 3, 144, 22)],
                [(196, 2, 216, 22)],
                [(261, 3, 276, 22)],
            ],
            "walking": [
                [(2, 4, 19, 22), (21, 4, 36, 22), (38, 4, 53, 22)],
                [(55, 3, 75, 22), (77, 2, 95, 22), (97, 3, 119, 22)],
                [(121, 3, 144, 22), (146, 4, 169, 22), (171, 4, 194, 22)],
                [(196, 2, 216, 22), (218, 2, 238, 22), (240, 3, 259, 22)],
                [(261, 3, 276, 22), (278, 3, 293, 22), (295, 3, 311, 22)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=8,
        slug="wartortle",
        display_name="Wartortle",
        source_name="008_wartortle.png",
        source_group="low",
        scale=2.0,
        source_directions=["front", "down_right", "right", "up_right", "back", "up_left", "left", "down_left"],
        mirror_directions=False,
        notes=[
            "Walking uses 24 source sprites; down_left is overridden from the PSD `down_left` layer.",
            "Source direction order is front, down_right, right, up_right, back, up_left, left, down_left.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 6 and 7.",
        ],
        frame_overrides={
            ("idle", "down_left"): ("low/psd", "卡咪龟.psd", "down_left", (0,)),
            ("walking", "down_left"): ("low/psd", "卡咪龟.psd", "down_left", (0, 1, 2)),
        },
        sleeping_boxes=[(318, 35, 342, 56), (294, 33, 316, 56)],
        boxes={
            "idle": [
                [(2, 3, 24, 24)],
                [(74, 1, 92, 24)],
                [(134, 0, 153, 24)],
                [(198, 0, 220, 24)],
                [(268, 1, 288, 24)],
                [(333, 0, 351, 24)],
                [(393, 0, 413, 24)],
                [(461, 0, 484, 23)],
            ],
            "walking": [
                [(2, 3, 24, 24), (26, 3, 46, 24), (48, 3, 72, 24)],
                [(74, 1, 92, 24), (94, 1, 111, 24), (113, 2, 132, 24)],
                [(134, 0, 153, 24), (155, 2, 175, 24), (177, 2, 196, 24)],
                [(198, 0, 220, 24), (222, 2, 244, 24), (246, 0, 266, 24)],
                [(268, 1, 288, 24), (290, 0, 309, 23), (311, 1, 331, 24)],
                [(333, 0, 351, 24), (353, 0, 371, 24), (373, 1, 391, 24)],
                [(393, 0, 413, 24), (415, 2, 437, 24), (439, 2, 458, 24)],
                [(461, 0, 484, 23), (486, 2, 509, 24), (510, 1, 535, 24)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=9,
        slug="blastoise",
        display_name="Blastoise",
        source_name="水箭龟.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Walking crop boxes were derived from PSD layers walk0..walk4.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites 4 and 5 from the original split PNG source.",
        ],
        sleeping_source_name="009_blastoise.png",
        sleeping_source_group="low",
        sleeping_boxes=[(90, 33, 117, 60), (119, 33, 147, 60)],
        boxes={
            "idle": [
                [(110, 46, 136, 70)],
                [(110, 77, 136, 104)],
                [(105, 107, 132, 135)],
                [(113, 144, 136, 170)],
                [(110, 175, 136, 204)],
            ],
            "walking": [
                [(110, 46, 136, 70), (138, 44, 164, 70), (166, 44, 192, 70)],
                [(110, 77, 136, 104), (138, 80, 165, 104), (167, 75, 191, 104)],
                [(105, 107, 132, 135), (139, 107, 166, 134), (174, 107, 201, 134)],
                [(113, 144, 136, 170), (138, 144, 163, 170), (164, 144, 188, 170)],
                [(110, 175, 136, 204), (138, 176, 163, 204), (165, 176, 190, 204)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=10,
        slug="caterpie",
        display_name="Caterpie",
        source_name="010_caterpie.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the fourth-last and third-last sprites on row 1, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(514, 10, 534, 25), (536, 11, 557, 25)],
        boxes={
            "idle": [
                [(2, 6, 14, 25)],
                [(44, 10, 64, 25)],
                [(111, 8, 134, 25)],
                [(179, 4, 200, 25)],
                [(235, 5, 247, 25)],
            ],
            "walking": [
                [(2, 6, 14, 25), (16, 6, 28, 25), (30, 4, 42, 25)],
                [(44, 10, 64, 25), (66, 11, 85, 25), (87, 11, 109, 25)],
                [(111, 8, 134, 25), (136, 9, 157, 25), (159, 9, 183, 25)],
                [(179, 4, 200, 25), (199, 7, 218, 25), (215, 3, 236, 25)],
                [(235, 5, 247, 25), (249, 7, 261, 25), (263, 2, 275, 25)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=11,
        slug="metapod",
        display_name="Metapod",
        source_name="011_metapod.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 1, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(517, 9, 540, 28), (542, 9, 564, 28)],
        boxes={
            "idle": [
                [(2, 7, 19, 28)],
                [(60, 7, 81, 28)],
                [(128, 7, 152, 28)],
                [(208, 6, 228, 28)],
                [(266, 4, 283, 28)],
            ],
            "walking": [
                [(2, 7, 19, 28), (20, 6, 39, 28), (41, 6, 58, 28)],
                [(60, 7, 81, 28), (83, 9, 106, 28), (108, 6, 126, 28)],
                [(128, 7, 152, 28), (154, 9, 181, 28), (183, 5, 206, 28)],
                [(208, 6, 228, 28), (228, 8, 249, 28), (248, 5, 267, 28)],
                [(266, 4, 283, 28), (284, 2, 301, 28), (302, 6, 319, 28)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=12,
        slug="butterfree",
        display_name="Butterfree",
        source_name="012_butterfree.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses sprites 2 and 3 on row 2.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(30, 38, 57, 62), (59, 39, 86, 62)],
        boxes={
            "idle": [
                [(2, 5, 26, 28)],
                [(80, 3, 101, 28)],
                [(150, 4, 171, 28)],
                [(216, 4, 238, 28)],
                [(286, 6, 310, 28)],
            ],
            "walking": [
                [(2, 5, 26, 28), (28, 7, 56, 28), (58, 3, 78, 28)],
                [(80, 3, 101, 28), (103, 3, 127, 28), (129, 3, 149, 28)],
                [(150, 4, 171, 28), (171, 2, 192, 28), (193, 4, 215, 28)],
                [(216, 4, 238, 28), (240, 4, 261, 28), (263, 5, 284, 28)],
                [(286, 6, 310, 28), (312, 6, 342, 28), (344, 6, 364, 28)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=16,
        slug="pidgey",
        display_name="Pidgey",
        source_name="016_pidgey.png",
        source_group="low/split_by_species",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the final two sprites on row 1.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(562, 5, 578, 19), (580, 6, 596, 19)],
        boxes={
            "idle": [
                [(2, 2, 15, 19)],
                [(47, 1, 62, 19)],
                [(99, 1, 118, 19)],
                [(162, 0, 178, 19)],
                [(213, 1, 226, 19)],
            ],
            "walking": [
                [(2, 2, 15, 19), (17, 1, 30, 19), (32, 4, 45, 19)],
                [(47, 1, 62, 19), (64, 1, 80, 19), (82, 2, 97, 19)],
                [(99, 1, 118, 19), (119, 0, 139, 19), (141, 4, 161, 19)],
                [(162, 0, 178, 19), (178, 0, 195, 19), (195, 1, 211, 19)],
                [(213, 1, 226, 19), (228, 0, 241, 19), (243, 4, 256, 19)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=17,
        slug="pidgeotto",
        display_name="Pidgeotto",
        source_name="017_pidgeotto.png",
        source_group="low/split_by_species",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses sprites 6 and 7 on row 2.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(127, 39, 148, 60), (150, 40, 171, 60)],
        boxes={
            "idle": [
                [(2, 2, 19, 24)],
                [(59, 2, 80, 24)],
                [(128, 2, 152, 24)],
                [(207, 1, 229, 24)],
                [(277, 0, 292, 24)],
            ],
            "walking": [
                [(2, 2, 19, 24), (21, 2, 38, 24), (40, 2, 57, 24)],
                [(59, 2, 80, 24), (82, 3, 103, 24), (105, 2, 127, 24)],
                [(128, 2, 152, 24), (154, 3, 180, 24), (181, 3, 206, 24)],
                [(207, 1, 229, 24), (228, 1, 251, 24), (253, 2, 275, 24)],
                [(277, 0, 292, 24), (294, 0, 310, 24), (312, 0, 328, 24)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=18,
        slug="pidgeot",
        display_name="Pidgeot",
        source_name="018_pidgeot.png",
        source_group="low/split_by_species",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses sprites 6 and 7 on row 2.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(139, 40, 161, 62), (163, 42, 185, 62)],
        boxes={
            "idle": [
                [(2, 2, 19, 26)],
                [(58, 1, 80, 26)],
                [(128, 2, 154, 26)],
                [(203, 1, 225, 26)],
                [(270, 1, 285, 26)],
            ],
            "walking": [
                [(2, 2, 19, 26), (21, 2, 38, 26), (40, 2, 57, 26)],
                [(58, 1, 80, 26), (82, 3, 104, 26), (106, 1, 128, 26)],
                [(128, 2, 154, 26), (152, 3, 178, 26), (177, 3, 203, 26)],
                [(203, 1, 225, 26), (223, 0, 247, 26), (248, 2, 268, 26)],
                [(270, 1, 285, 26), (287, 1, 302, 26), (304, 1, 319, 26)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=25,
        slug="pikachu",
        display_name="Pikachu",
        source_name="皮卡丘.psd",
        source_group="low/psd",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
            "Crop boxes were derived from PSD layers walk0..walk4 and sleeping.",
        ],
        sleeping_boxes=[(350, 22, 374, 46), (377, 22, 401, 46)],
        boxes={
            "idle": [
                [(70, 15, 88, 40)],
                [(69, 42, 91, 68)],
                [(67, 70, 89, 94)],
                [(67, 97, 91, 120)],
                [(69, 123, 91, 146)],
            ],
            "walking": [
                [(70, 15, 88, 40), (91, 16, 110, 40), (114, 16, 133, 40)],
                [(69, 42, 91, 68), (93, 44, 115, 68), (116, 43, 138, 68)],
                [(67, 70, 89, 94), (91, 71, 112, 94), (114, 70, 135, 94)],
                [(67, 97, 91, 120), (94, 99, 115, 120), (117, 95, 142, 120)],
                [(69, 123, 91, 146), (94, 121, 116, 146), (118, 121, 140, 146)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=26,
        slug="raichu",
        display_name="Raichu",
        source_name="026_raichu.png",
        source_group="low",
        scale=2.0,
        bottom_margin=4,
        source_directions=["front", "down_right", "right", "up_right", "back", "up_left", "left", "down_left"],
        mirror_directions=False,
        notes=[
            "Walking uses 24 source sprites: row 1 sprites 1..21 plus row 2 sprites 1..3.",
            "Source direction order is front, down_right, right, up_right, back, up_left, left, down_left.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 3 and 2.",
        ],
        sleeping_boxes=[(507, 42, 533, 69), (535, 43, 561, 69)],
        boxes={
            "idle": [
                [(2, 4, 29, 29)],
                [(90, 0, 115, 29)],
                [(176, 3, 201, 29)],
                [(259, 2, 282, 29)],
                [(337, 2, 365, 29)],
                [(426, 3, 450, 29)],
                [(505, 3, 530, 29)],
                [(2, 40, 26, 69)],
            ],
            "walking": [
                [(2, 4, 29, 29), (32, 5, 59, 29), (61, 5, 87, 29)],
                [(90, 0, 115, 29), (118, 1, 144, 29), (147, 3, 173, 29)],
                [(176, 3, 201, 29), (202, 3, 227, 29), (230, 4, 256, 29)],
                [(259, 2, 282, 29), (285, 2, 308, 29), (311, 4, 335, 29)],
                [(337, 2, 365, 29), (365, 1, 394, 29), (396, 1, 423, 29)],
                [(426, 3, 450, 29), (452, 4, 477, 29), (479, 3, 503, 29)],
                [(505, 3, 530, 29), (532, 4, 557, 29), (560, 5, 586, 29)],
                [(2, 40, 26, 69), (27, 43, 52, 69), (54, 41, 79, 69)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=74,
        slug="geodude",
        display_name="Geodude",
        source_name="074_geodude.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the seventh-last and sixth-last sprites on row 2, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(286, 45, 311, 66), (313, 45, 338, 66)],
        boxes={
            "idle": [
                [(2, 14, 30, 29)],
                [(82, 11, 107, 29)],
                [(159, 8, 174, 29)],
                [(214, 7, 233, 29)],
                [(277, 15, 303, 29)],
            ],
            "walking": [
                [(2, 14, 30, 29), (32, 11, 55, 29), (57, 11, 81, 29)],
                [(82, 11, 107, 29), (109, 13, 136, 29), (138, 8, 157, 29)],
                [(159, 8, 174, 29), (176, 11, 197, 29), (199, 7, 212, 29)],
                [(214, 7, 233, 29), (235, 6, 249, 29), (251, 11, 275, 29)],
                [(277, 15, 303, 29), (305, 12, 328, 29), (330, 12, 353, 29)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=75,
        slug="graveler",
        display_name="Graveler",
        source_name="075_graveler.png",
        source_group="low",
        scale=2.0,
        canvas_height=68,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the seventh-last and sixth-last sprites on row 2, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 64x68 canvas preserves fixed 2x scaling for the tallest walking poses.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(341, 46, 368, 65), (370, 47, 397, 65)],
        boxes={
            "idle": [
                [(2, 6, 25, 30)],
                [(85, 3, 108, 30)],
                [(160, 2, 179, 30)],
                [(226, 3, 248, 30)],
                [(298, 5, 323, 30)],
            ],
            "walking": [
                [(2, 6, 25, 30), (27, 7, 54, 30), (56, 7, 83, 30)],
                [(85, 3, 108, 30), (108, 2, 131, 30), (133, 5, 158, 30)],
                [(160, 2, 179, 30), (181, 2, 200, 30), (202, 3, 224, 30)],
                [(226, 3, 248, 30), (250, 3, 273, 30), (275, 2, 296, 30)],
                [(298, 5, 323, 30), (325, 4, 351, 30), (353, 4, 379, 30)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=76,
        slug="golem",
        display_name="Golem",
        source_name="076_golem.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites on row 1 are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the ninth-last and eighth-last sprites on row 2, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(330, 34, 354, 58), (356, 35, 380, 58)],
        boxes={
            "idle": [
                [(2, 3, 26, 26)],
                [(86, 2, 110, 26)],
                [(165, 2, 188, 26)],
                [(239, 2, 263, 26)],
                [(317, 3, 341, 26)],
            ],
            "walking": [
                [(2, 3, 26, 26), (29, 3, 55, 26), (57, 3, 83, 26)],
                [(86, 2, 110, 26), (112, 4, 137, 26), (140, 2, 163, 26)],
                [(165, 2, 188, 26), (190, 3, 212, 26), (214, 4, 237, 26)],
                [(239, 2, 263, 26), (265, 2, 288, 26), (290, 3, 314, 26)],
                [(317, 3, 341, 26), (343, 3, 368, 26), (370, 3, 395, 26)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=92,
        slug="gastly",
        display_name="Gastly",
        source_name="092_gastly.png",
        source_group="low",
        scale=1.5,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites 7 and 8.",
        ],
        sleeping_boxes=[(177, 41, 201, 65), (204, 42, 228, 65)],
        boxes={
            "idle": [
                [(2, 2, 27, 30)],
                [(87, 3, 116, 30)],
                [(182, 5, 211, 30)],
                [(281, 2, 312, 30)],
                [(386, 2, 410, 30)],
            ],
            "walking": [
                [(2, 2, 27, 30), (31, 2, 56, 30), (59, 3, 84, 30)],
                [(87, 3, 116, 30), (119, 0, 148, 30), (151, 4, 176, 30)],
                [(182, 5, 211, 30), (214, 4, 238, 30), (247, 4, 274, 30)],
                [(281, 2, 312, 30), (316, 2, 346, 30), (351, 2, 379, 28)],
                [(386, 2, 410, 30), (417, 2, 441, 30), (444, 0, 470, 29)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=93,
        slug="haunter",
        display_name="Haunter",
        source_name="093_haunter.png",
        source_group="low",
        scale=2.0,
        bottom_margin=2,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites 10 and 9, following the source note order.",
        ],
        sleeping_boxes=[(242, 39, 268, 69), (208, 47, 240, 69)],
        boxes={
            "idle": [
                [(2, 9, 26, 32)],
                [(80, 3, 106, 32)],
                [(164, 4, 190, 32)],
                [(244, 6, 269, 32)],
                [(322, 8, 348, 32)],
            ],
            "walking": [
                [(2, 9, 26, 32), (28, 10, 52, 32), (54, 8, 78, 32)],
                [(80, 3, 106, 32), (108, 6, 134, 32), (136, 2, 162, 32)],
                [(164, 4, 190, 32), (190, 5, 216, 32), (217, 2, 242, 32)],
                [(244, 6, 269, 32), (269, 7, 294, 32), (295, 5, 320, 32)],
                [(322, 8, 348, 32), (350, 10, 376, 32), (377, 7, 403, 32)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=94,
        slug="gengar",
        display_name="Gengar",
        source_name="094_gengar.png",
        source_group="low",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 10 and 9.",
        ],
        sleeping_boxes=[(314, 34, 338, 61), (340, 35, 363, 61)],
        boxes={
            "idle": [
                [(2, 6, 27, 28)],
                [(85, 3, 107, 28)],
                [(162, 3, 183, 28)],
                [(227, 4, 246, 28)],
                [(295, 5, 320, 28)],
            ],
            "walking": [
                [(2, 6, 27, 28), (29, 6, 55, 28), (57, 6, 83, 28)],
                [(85, 3, 107, 28), (109, 5, 132, 28), (134, 4, 160, 28)],
                [(162, 3, 183, 28), (184, 3, 204, 28), (205, 4, 225, 28)],
                [(227, 4, 246, 28), (248, 3, 273, 28), (274, 4, 294, 28)],
                [(295, 5, 320, 28), (322, 4, 348, 28), (350, 4, 376, 28)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=123,
        slug="scyther",
        display_name="Scyther",
        source_name="飞天螳螂.psd",
        source_group="low/psd",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The PSD Idle/Moving section stores walk0..walk4 from top to bottom.",
            "walk0..walk4 direction order is front, down_left, left, up_left, back.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping frames are cropped from the PSD Sleeping section.",
        ],
        sleeping_boxes=[(338, 38, 367, 64), (375, 38, 403, 66)],
        boxes={
            "idle": [
                [(53, 22, 79, 47)],
                [(49, 55, 79, 84)],
                [(53, 91, 80, 120)],
                [(53, 130, 82, 156)],
                [(53, 165, 79, 191)],
            ],
            "walking": [
                [(53, 22, 79, 47), (89, 22, 115, 47), (123, 22, 149, 47)],
                [(49, 55, 79, 84), (86, 58, 114, 84), (123, 55, 154, 83)],
                [(53, 91, 80, 120), (88, 93, 115, 120), (124, 93, 153, 120)],
                [(53, 130, 82, 156), (87, 130, 117, 156), (121, 133, 149, 157)],
                [(53, 165, 79, 191), (86, 164, 111, 191), (118, 164, 143, 191)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=129,
        slug="magikarp",
        display_name="Magikarp",
        source_name="129_magikarp.png",
        scale=2.0,
        source_directions=["front", "down_right", "right", "up_right", "back", "up_left", "left", "down_left"],
        mirror_directions=False,
        notes=[
            "`helpless flopping` is stored as `walking` so the runtime movement state can reuse it.",
            "This sheet has no dedicated sleeping action; sleeping uses the two front idle frames as a non-directional fallback.",
            "Runtime movement speed is reduced for this species to make the flopping movement slow.",
        ],
        sleeping_boxes=[(0, 0, 24, 24), (27, 5, 51, 24)],
        boxes={
            "idle": [
                [(0, 0, 24, 24), (27, 5, 51, 24)],
                [(0, 27, 28, 48), (31, 28, 60, 48)],
                [(0, 51, 29, 74), (32, 52, 61, 74)],
                [(0, 79, 26, 104), (29, 77, 55, 104)],
                [(0, 107, 24, 132), (27, 107, 51, 132)],
                [(0, 135, 29, 158), (32, 135, 59, 158)],
                [(0, 161, 29, 183), (32, 162, 61, 183)],
                [(0, 186, 27, 210), (30, 188, 57, 210)],
            ],
            "walking": [
                [(116, 0, 140, 26)],
                [(116, 29, 142, 51)],
                [(116, 54, 143, 78)],
                [(116, 81, 141, 102)],
                [(116, 105, 139, 126)],
                [(116, 129, 143, 147)],
                [(116, 150, 145, 171)],
                [(116, 174, 141, 200)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=130,
        slug="gyarados",
        display_name="Gyarados",
        source_name="130_gyarados.png",
        scale=1.0,
        canvas_width=88,
        canvas_height=82,
        source_directions=["front", "down_right", "right", "up_right", "back"],
        mirror_map={"down_left": "down_right", "left": "right", "up_left": "up_right"},
        notes=[
            "The source sheet provides front, down_right, right, up_right, and back; left-side directions are generated by mirroring.",
            "The source label says Moving; project assets store it as walking for state-machine compatibility.",
            "Uses an 88x82 canvas so the large body poses are not shrunk into the default 64x64 frame.",
        ],
        sleeping_boxes=[(749, 31, 807, 89), (810, 31, 868, 85)],
        boxes={
            "idle": [
                [(0, 31, 39, 88)],
                [(0, 91, 56, 147)],
                [(0, 150, 66, 206)],
                [(0, 209, 48, 272)],
                [(0, 275, 41, 341)],
            ],
            "walking": [
                [(107, 37, 143, 88), (146, 31, 185, 88), (188, 32, 224, 88)],
                [(107, 102, 172, 153), (175, 97, 231, 153), (234, 91, 289, 153)],
                [(107, 172, 192, 225), (195, 169, 261, 225), (264, 156, 326, 225)],
                [(107, 244, 172, 299), (175, 236, 223, 299), (226, 229, 274, 299)],
                [(107, 316, 142, 375), (145, 309, 186, 375), (189, 302, 230, 375)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=134,
        slug="vaporeon",
        display_name="Vaporeon",
        source_name="134_vaporeon.png",
        scale=1.6,
        sleeping_boxes=[(456, 23, 485, 46), (490, 24, 520, 46)],
        boxes={
            "idle": [
                [(1, 23, 24, 52)],
                [(1, 57, 25, 88)],
                [(1, 93, 30, 123)],
                [(1, 128, 30, 164)],
                [(1, 169, 20, 198)],
            ],
            "walking": [
                [(63, 25, 90, 52), (95, 25, 122, 52), (127, 25, 154, 52), (159, 23, 186, 52)],
                [(63, 57, 88, 83), (93, 57, 120, 83), (125, 57, 150, 83), (155, 58, 178, 83)],
                [(63, 88, 94, 115), (99, 89, 129, 115), (134, 88, 165, 115), (170, 88, 200, 115)],
                [(63, 120, 93, 151), (98, 121, 129, 151), (134, 120, 164, 151), (169, 120, 197, 151)],
                [(63, 156, 90, 188), (95, 156, 121, 188), (126, 156, 153, 188), (158, 156, 184, 188)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=135,
        slug="jolteon",
        display_name="Jolteon",
        source_name="135_jolteon.png",
        scale=2.0,
        sleeping_boxes=[(365, 24, 391, 47), (396, 26, 421, 47)],
        boxes={
            "idle": [
                [(1, 24, 22, 50)],
                [(1, 55, 26, 83)],
                [(1, 88, 26, 115)],
                [(1, 120, 22, 147)],
                [(1, 152, 20, 177)],
            ],
            "walking": [
                [(61, 25, 82, 50), (87, 24, 108, 50), (113, 25, 134, 50), (139, 24, 161, 50)],
                [(61, 55, 83, 81), (88, 55, 111, 81), (116, 55, 138, 81), (143, 56, 166, 81)],
                [(60, 86, 87, 111), (92, 86, 119, 111), (124, 86, 151, 111), (156, 88, 182, 111)],
                [(60, 116, 83, 142), (88, 117, 111, 142), (116, 116, 139, 142), (144, 116, 168, 142)],
                [(60, 150, 79, 175), (84, 147, 103, 175), (108, 150, 127, 175), (132, 147, 151, 175)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=136,
        slug="flareon",
        display_name="Flareon",
        source_name="136_flareon.png",
        scale=2.0,
        sleeping_boxes=[(413, 24, 438, 45), (443, 25, 469, 45)],
        boxes={
            "idle": [
                [(1, 23, 22, 46), (27, 22, 48, 46), (53, 24, 74, 46)],
                [(1, 52, 23, 74), (28, 51, 51, 74), (56, 52, 80, 74)],
                [(1, 80, 26, 102), (31, 79, 57, 102), (62, 80, 87, 102)],
                [(1, 108, 24, 131), (29, 107, 51, 131), (56, 108, 76, 131)],
                [(1, 137, 21, 159), (26, 136, 46, 159), (51, 137, 71, 159)],
            ],
            "walking": [
                [(112, 27, 133, 51), (138, 24, 158, 51), (163, 27, 184, 51), (189, 26, 209, 51)],
                [(112, 57, 135, 80), (140, 56, 163, 80), (168, 57, 191, 80), (196, 58, 219, 80)],
                [(112, 86, 138, 108), (143, 85, 169, 108), (174, 86, 200, 108), (205, 85, 231, 108)],
                [(112, 113, 134, 138), (139, 114, 161, 138), (166, 114, 188, 138), (193, 114, 215, 138)],
                [(112, 143, 130, 167), (135, 144, 155, 167), (160, 143, 180, 167), (185, 144, 205, 167)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=196,
        slug="espeon",
        display_name="Espeon",
        source_name="太阳伊布.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
        ],
        sleeping_boxes=[(348, 17, 375, 41), (376, 18, 403, 41)],
        boxes={
            "idle": [
                [(64, 20, 91, 49)],
                [(67, 49, 90, 79)],
                [(63, 81, 91, 109)],
                [(69, 109, 93, 140)],
                [(71, 142, 98, 173)],
            ],
            "walking": [
                [(64, 20, 91, 49), (92, 19, 118, 49), (119, 19, 145, 49)],
                [(67, 49, 90, 79), (91, 49, 115, 79), (116, 50, 141, 79)],
                [(63, 81, 91, 109), (92, 82, 122, 109), (123, 81, 150, 109)],
                [(69, 109, 93, 140), (94, 109, 118, 140), (119, 110, 143, 140)],
                [(71, 142, 98, 173), (99, 142, 125, 173), (126, 142, 152, 173)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=197,
        slug="umbreon",
        display_name="Umbreon",
        source_name="月伊布.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
        ],
        sleeping_boxes=[(349, 18, 374, 42), (375, 19, 403, 42)],
        boxes={
            "idle": [
                [(76, 20, 95, 48)],
                [(75, 49, 95, 77)],
                [(74, 77, 100, 104)],
                [(76, 105, 97, 134)],
                [(78, 134, 97, 165)],
            ],
            "walking": [
                [(76, 20, 95, 48), (96, 19, 116, 48), (117, 19, 136, 48)],
                [(75, 49, 95, 77), (96, 50, 115, 77), (116, 49, 137, 77)],
                [(74, 77, 100, 104), (101, 78, 126, 104), (127, 78, 152, 104)],
                [(76, 105, 97, 134), (98, 105, 121, 134), (122, 104, 142, 134)],
                [(78, 134, 97, 165), (98, 134, 116, 165), (117, 134, 135, 165)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=172,
        slug="pichu",
        display_name="Pichu",
        source_name="皮丘.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
            "Crop boxes were derived from PSD layer groups walk0..walk4 and layer sleeping.",
        ],
        sleeping_boxes=[(358, 21, 376, 44), (379, 22, 397, 44)],
        boxes={
            "idle": [
                [(60, 23, 80, 45)],
                [(63, 47, 81, 71)],
                [(61, 74, 77, 98)],
                [(60, 101, 76, 127)],
                [(56, 134, 75, 160)],
            ],
            "walking": [
                [(60, 23, 80, 45), (83, 22, 102, 45), (110, 21, 130, 45)],
                [(63, 47, 81, 71), (84, 50, 100, 71), (107, 49, 128, 72)],
                [(61, 74, 77, 98), (80, 75, 96, 98), (103, 74, 119, 97)],
                [(60, 101, 76, 127), (79, 103, 94, 127), (99, 101, 119, 126)],
                [(56, 134, 75, 160), (78, 135, 98, 160), (103, 133, 123, 160)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=183,
        slug="marill",
        display_name="Marill",
        source_name="183_marill_184_azumarill.png",
        source_group="medium",
        scale=2.0,
        canvas_height=68,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Row 2 contains ten attack frames: five directions, two frames each; these are reserved for future use.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(219, 89, 246, 109), (249, 89, 277, 109)],
        boxes={
            "idle": [
                [(21, 19, 39, 42)],
                [(84, 20, 104, 42)],
                [(154, 22, 175, 42)],
                [(225, 22, 243, 42)],
                [(289, 22, 307, 42)],
            ],
            "walking": [
                [(21, 19, 39, 42), (42, 20, 60, 42), (63, 20, 81, 42)],
                [(84, 20, 104, 42), (107, 22, 128, 42), (131, 22, 151, 42)],
                [(154, 22, 175, 42), (178, 23, 199, 42), (202, 23, 222, 42)],
                [(225, 22, 243, 42), (246, 22, 263, 42), (266, 23, 286, 42)],
                [(289, 22, 307, 42), (310, 23, 328, 42), (331, 23, 349, 42)],
            ],
        },
        auxiliary_boxes={
            "attack": [
                [(69, 48, 87, 76), (90, 47, 108, 76)],
                [(111, 49, 128, 76), (131, 53, 159, 76)],
                [(162, 52, 178, 76), (181, 56, 209, 76)],
                [(212, 53, 229, 76), (232, 56, 258, 76)],
                [(261, 54, 279, 76), (282, 58, 300, 76)],
            ],
        },
        auxiliary_directions={"attack": SOURCE_DIRECTIONS},
    ),
    SpeciesSpec(
        species_id=184,
        slug="azumarill",
        display_name="Azumarill",
        source_name="183_marill_184_azumarill.png",
        source_group="medium",
        scale=2.0,
        canvas_height=68,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 4, before the shadow sprite.",
            "Row 2 contains ten attack frames: five directions, two frames each; these are reserved for future use.",
            "Row 3 contains one jump frame for each of all eight source directions; these are reserved for future use.",
            "Only the five moving and attack source directions use runtime mirroring; jump keeps all eight source directions.",
        ],
        sleeping_boxes=[(217, 233, 242, 255), (245, 236, 271, 255)],
        boxes={
            "idle": [
                [(3, 131, 27, 156)],
                [(84, 131, 108, 156)],
                [(163, 131, 181, 156)],
                [(228, 132, 246, 156)],
                [(292, 133, 316, 156)],
            ],
            "walking": [
                [(3, 131, 27, 156), (30, 130, 56, 156), (59, 132, 81, 156)],
                [(84, 131, 108, 156), (111, 131, 135, 156), (138, 130, 160, 156)],
                [(163, 131, 181, 156), (184, 132, 203, 156), (206, 131, 225, 156)],
                [(228, 132, 246, 156), (249, 131, 269, 156), (272, 132, 289, 156)],
                [(292, 133, 316, 156), (319, 131, 341, 156), (344, 131, 365, 156)],
            ],
        },
        auxiliary_boxes={
            "attack": [
                [(70, 164, 90, 189), (93, 163, 116, 189)],
                [(119, 162, 141, 189), (144, 162, 166, 189)],
                [(169, 163, 186, 189), (189, 161, 208, 189)],
                [(211, 165, 227, 189), (230, 163, 248, 189)],
                [(251, 168, 270, 189), (273, 163, 297, 189)],
            ],
            "jump": [
                [(101, 197, 122, 222)],
                [(125, 196, 143, 222)],
                [(146, 195, 161, 222)],
                [(164, 196, 182, 222)],
                [(185, 197, 206, 222)],
                [(209, 195, 226, 222)],
                [(229, 194, 243, 222)],
                [(246, 196, 266, 222)],
            ],
        },
        auxiliary_directions={
            "attack": SOURCE_DIRECTIONS,
            "jump": GENERATED_DIRECTIONS,
        },
    ),
    SpeciesSpec(
        species_id=194,
        slug="wooper",
        display_name="Wooper",
        source_name="194_wooper_195_quagsire.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites on row 1 are moving frames: five directions, three frames each; the remaining nine row sprites are not exported.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Row 2 contains ten attack frames: five directions, two frames each; these are reserved for future use.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(303, 83, 322, 101), (325, 84, 346, 101)],
        boxes={
            "idle": [
                [(3, 26, 26, 45)],
                [(81, 26, 99, 45)],
                [(145, 26, 161, 45)],
                [(200, 26, 216, 45)],
                [(258, 26, 281, 45)],
            ],
            "walking": [
                [(3, 26, 26, 45), (29, 25, 52, 45), (55, 25, 78, 45)],
                [(81, 26, 99, 45), (102, 25, 120, 45), (123, 25, 142, 45)],
                [(145, 26, 161, 45), (164, 23, 179, 45), (182, 25, 197, 45)],
                [(200, 26, 216, 45), (219, 23, 235, 45), (238, 25, 255, 45)],
                [(258, 26, 281, 45), (284, 24, 307, 45), (310, 25, 333, 45)],
            ],
        },
        auxiliary_boxes={
            "attack": [
                [(145, 52, 168, 72), (171, 55, 194, 72)],
                [(197, 52, 215, 72), (218, 54, 239, 72)],
                [(242, 50, 257, 72), (260, 53, 280, 72)],
                [(283, 52, 299, 72), (302, 53, 318, 72)],
                [(321, 52, 344, 72), (347, 54, 370, 72)],
            ],
        },
        auxiliary_directions={"attack": SOURCE_DIRECTIONS},
    ),
    SpeciesSpec(
        species_id=195,
        slug="quagsire",
        display_name="Quagsire",
        source_name="194_wooper_195_quagsire.png",
        source_group="medium",
        scale=2.0,
        canvas_width=68,
        canvas_height=72,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 2 contains ten moving frames: five directions, two frames each.",
            "Idle uses attack frame 0 from row 1 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Row 1 contains ten attack frames: five directions, two frames each; these are reserved for future use.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 68x72 canvas preserves fixed 2x scaling for the widest sleeping pose and tallest attack pose.",
        ],
        sleeping_boxes=[(293, 216, 325, 233), (328, 215, 360, 233)],
        boxes={
            "idle": [
                [(128, 133, 146, 157)],
                [(172, 132, 197, 157)],
                [(228, 131, 258, 157)],
                [(294, 129, 318, 157)],
                [(349, 127, 367, 157)],
            ],
            "walking": [
                [(130, 167, 150, 192), (153, 167, 173, 192)],
                [(176, 166, 205, 192), (208, 168, 227, 192)],
                [(230, 167, 258, 192), (261, 167, 289, 192)],
                [(292, 162, 311, 192), (314, 166, 341, 192)],
                [(344, 163, 363, 192), (366, 163, 385, 192)],
            ],
        },
        auxiliary_boxes={
            "attack": [
                [(128, 133, 146, 157), (149, 128, 169, 157)],
                [(172, 132, 197, 157), (200, 127, 225, 157)],
                [(228, 131, 258, 157), (261, 126, 291, 157)],
                [(294, 129, 318, 157), (321, 128, 346, 157)],
                [(349, 127, 367, 157), (370, 129, 388, 157)],
            ],
        },
        auxiliary_directions={"attack": SOURCE_DIRECTIONS},
    ),
    SpeciesSpec(
        species_id=285,
        slug="shroomish",
        display_name="Shroomish",
        source_name="285_shroomish_286_breloom.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 2, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(304, 54, 324, 70), (327, 54, 349, 70)],
        boxes={
            "idle": [
                [(40, 19, 60, 36)],
                [(109, 19, 129, 36)],
                [(178, 19, 198, 36)],
                [(247, 19, 267, 36)],
                [(316, 19, 336, 36)],
            ],
            "walking": [
                [(40, 19, 60, 36), (63, 19, 83, 36), (86, 19, 106, 36)],
                [(109, 19, 129, 36), (132, 20, 152, 36), (155, 19, 175, 36)],
                [(178, 19, 198, 36), (201, 20, 221, 36), (224, 20, 244, 36)],
                [(247, 19, 267, 36), (270, 19, 290, 36), (293, 20, 313, 36)],
                [(316, 19, 336, 36), (339, 18, 359, 36), (362, 18, 382, 36)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=286,
        slug="breloom",
        display_name="Breloom",
        source_name="285_shroomish_286_breloom.png",
        source_group="medium",
        scale=2.0,
        canvas_height=70,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 64x70 canvas preserves fixed 2x scaling and a transparent top margin for the tallest moving pose.",
        ],
        sleeping_boxes=[(313, 169, 338, 194), (341, 170, 368, 194)],
        boxes={
            "idle": [
                [(25, 90, 44, 116)],
                [(89, 90, 112, 116)],
                [(166, 91, 192, 116)],
                [(251, 90, 274, 116)],
                [(326, 86, 343, 116)],
            ],
            "walking": [
                [(25, 90, 44, 116), (47, 90, 65, 116), (68, 90, 86, 116)],
                [(89, 90, 112, 116), (115, 92, 141, 116), (144, 88, 163, 116)],
                [(166, 91, 192, 116), (195, 92, 222, 116), (225, 93, 248, 116)],
                [(251, 90, 274, 116), (277, 88, 295, 116), (298, 92, 323, 116)],
                [(326, 86, 343, 116), (346, 89, 369, 116), (372, 89, 395, 116)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=298,
        slug="azurill",
        display_name="Azurill",
        source_name="298_azurill.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 1 for each direction.",
            "Row 2 contains 15 jumping frames: five directions, three frames each; these are reserved for future use.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(292, 77, 314, 94), (317, 78, 340, 94)],
        boxes={
            "idle": [
                [(55, 5, 70, 25)],
                [(113, 4, 132, 25)],
                [(178, 4, 198, 25)],
                [(242, 4, 259, 25)],
                [(303, 5, 318, 25)],
            ],
            "walking": [
                [(36, 3, 52, 25), (55, 5, 70, 25), (73, 3, 89, 25)],
                [(92, 4, 110, 25), (113, 4, 132, 25), (135, 4, 154, 25)],
                [(157, 5, 175, 25), (178, 4, 198, 25), (201, 3, 219, 25)],
                [(222, 5, 239, 25), (242, 4, 259, 25), (262, 4, 280, 25)],
                [(283, 4, 300, 25), (303, 5, 318, 25), (321, 4, 338, 25)],
            ],
        },
        auxiliary_boxes={
            "jump": [
                [(3, 36, 21, 57), (24, 39, 41, 57), (44, 32, 61, 57)],
                [(64, 39, 89, 57), (92, 38, 117, 57), (120, 35, 145, 57)],
                [(148, 40, 175, 57), (178, 36, 205, 57), (208, 39, 236, 57)],
                [(239, 34, 262, 57), (265, 30, 289, 57), (292, 44, 317, 57)],
                [(320, 33, 335, 57), (338, 31, 353, 57), (356, 42, 371, 57)],
            ],
        },
        auxiliary_directions={"jump": SOURCE_DIRECTIONS},
    ),
    SpeciesSpec(
        species_id=362,
        slug="glalie",
        display_name="Glalie",
        source_name="362_glalie.png",
        source_group="medium",
        scale=2.0,
        canvas_height=72,
        fit_to_canvas=False,
        crop_padding=1,
        source_directions=["left", "up_left", "front", "down_left", "back"],
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The left three columns contain visually identical moving frames; only frame 0 is retained for each direction.",
            "Source row order is left, up_left, front, down_left, back.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the final two sprites on row 1.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 64x72 canvas preserves fixed 2x scaling for the tallest moving pose.",
            "Idle and walking remain separate logical actions, but their identical RLE payload is deduplicated when packed.",
        ],
        sleeping_boxes=[(234, 11, 260, 40), (265, 12, 291, 42)],
        boxes={
            "idle": [
                [(14, 5, 38, 35)],
                [(14, 45, 38, 73)],
                [(10, 84, 38, 110)],
                [(11, 120, 35, 148)],
                [(10, 165, 38, 190)],
            ],
            "walking": [
                [(14, 5, 38, 35)],
                [(14, 45, 38, 73)],
                [(10, 84, 38, 110)],
                [(11, 120, 35, 148)],
                [(10, 165, 38, 190)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=361,
        slug="snorunt",
        display_name="Snorunt",
        source_name="361_snorunt.png",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The left four columns contain moving frames; each source row contains four frames.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "The sheet row order is front, down_right, right, up_right, back, up_left, left, down_left.",
            "Right-side source rows are omitted; up_right, right, and down_right are mirrored from the corresponding left-side directions at runtime.",
            "Sleeping uses the two sprites immediately above the Sleeping label.",
        ],
        sleeping_boxes=[(148, 91, 167, 112), (180, 93, 200, 112)],
        boxes={
            "idle": [
                [(2, 0, 21, 22)],
                [(11, 224, 30, 246)],
                [(2, 192, 21, 214)],
                [(11, 160, 30, 181)],
                [(2, 128, 21, 149)],
            ],
            "walking": [
                [(2, 0, 21, 22), (34, 1, 53, 24), (66, 0, 85, 22), (98, 1, 117, 24)],
                [(11, 224, 30, 246), (43, 225, 61, 248), (75, 224, 94, 246), (107, 225, 126, 247)],
                [(2, 192, 21, 214), (35, 193, 53, 216), (66, 192, 85, 214), (98, 193, 117, 215)],
                [(11, 160, 30, 181), (43, 161, 62, 182), (75, 160, 94, 181), (107, 161, 126, 183)],
                [(2, 128, 21, 149), (34, 129, 53, 150), (66, 128, 85, 149), (98, 129, 117, 151)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=322,
        slug="numel",
        display_name="Numel",
        source_name="322_numel_323_camerupt.png",
        scale=2.0,
        fit_to_canvas=False,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Frames are selected from the sprite blocks immediately left of the Idle, Walking, and Sleeping labels.",
            "Idle contains two frames per direction; walking contains three frames per direction.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(431, 72, 452, 89), (457, 72, 478, 89)],
        boxes={
            "idle": [
                [(1, 21, 16, 40), (21, 21, 36, 40)],
                [(1, 45, 23, 65), (28, 46, 52, 65)],
                [(1, 70, 25, 89), (30, 71, 54, 89)],
                [(1, 94, 20, 117), (26, 94, 48, 117)],
                [(1, 124, 16, 146), (21, 122, 36, 146)],
            ],
            "walking": [
                [(82, 21, 97, 41), (102, 22, 117, 41), (122, 21, 137, 41)],
                [(82, 47, 102, 66), (107, 46, 129, 66), (134, 46, 159, 66)],
                [(82, 72, 105, 91), (110, 72, 134, 91), (139, 71, 163, 91)],
                [(82, 99, 103, 120), (108, 97, 127, 120), (132, 96, 150, 120)],
                [(82, 125, 97, 148), (102, 126, 117, 148), (122, 125, 137, 148)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=323,
        slug="camerupt",
        display_name="Camerupt",
        source_name="322_numel_323_camerupt.png",
        scale=2.0,
        canvas_width=68,
        canvas_height=72,
        fit_to_canvas=False,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Frames are selected from the sprite blocks immediately left of the Idle/Special Attack, Walking, and Sleeping labels.",
            "The source Idle/Special Attack block is not used; idle uses walking frame 1 for each direction.",
            "Walking contains three frames per direction.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 68x72 canvas preserves fixed 2x scaling and transparent margins around the widest and tallest idle poses.",
        ],
        sleeping_boxes=[(375, 193, 398, 219), (403, 194, 426, 219)],
        boxes={
            "idle": [
                [(169, 192, 188, 218)],
                [(175, 224, 198, 249)],
                [(178, 256, 206, 278)],
                [(173, 287, 198, 312)],
                [(170, 322, 189, 345)],
            ],
            "walking": [
                [(145, 191, 164, 218), (169, 192, 188, 218), (193, 191, 212, 218)],
                [(145, 224, 170, 249), (175, 224, 198, 249), (203, 224, 226, 249)],
                [(145, 256, 173, 278), (178, 256, 206, 278), (211, 256, 238, 278)],
                [(145, 287, 168, 311), (173, 287, 198, 312), (203, 287, 230, 312)],
                [(145, 322, 165, 345), (170, 322, 189, 345), (193, 322, 213, 345)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=280,
        slug="ralts",
        display_name="Ralts",
        source_name="280_ralts.png",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        source_directions=["front", "down_right", "right", "up_right", "back"],
        contact_directions=SOURCE_DIRECTIONS,
        mirror_map={"down_left": "down_right", "left": "right", "up_left": "up_right"},
        notes=[
            "Idle contains two frames per direction and walking contains three frames per direction.",
            "The first five source rows are front, down_right, right, up_right, back.",
            "Left-side preview frames are mirrored from the corresponding right-side source rows.",
            "The remaining three source rows are intentionally ignored.",
        ],
        sleeping_boxes=[(276, 1, 290, 19), (295, 1, 308, 19)],
        boxes={
            "idle": [
                [(1, 1, 14, 24), (19, 1, 32, 24)],
                [(1, 26, 14, 47), (19, 26, 33, 47)],
                [(1, 52, 20, 74), (25, 52, 43, 74)],
                [(1, 79, 14, 101), (19, 79, 33, 101)],
                [(1, 106, 14, 128), (19, 106, 32, 128)],
            ],
            "walking": [
                [(2, 228, 15, 250), (20, 228, 33, 250), (38, 228, 51, 250)],
                [(1, 253, 15, 272), (20, 253, 33, 272), (38, 253, 52, 272)],
                [(1, 277, 20, 295), (25, 277, 44, 295), (49, 277, 68, 295)],
                [(1, 300, 15, 321), (20, 300, 33, 321), (38, 300, 52, 321)],
                [(1, 326, 15, 348), (19, 326, 32, 348), (37, 326, 51, 348)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=281,
        slug="kirlia",
        display_name="Kirlia",
        source_name="281_kirlia.png",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Walking contains two rows: row 1 is frame 0 and row 2 is frame 1.",
            "The first five columns are front, down_left, left, up_left, back.",
            "The sheet labels idle as the same spinning motion as walking, so idle reuses both walking frames.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Attacking contains three frames per direction and is reserved for future use.",
        ],
        sleeping_boxes=[(371, 1, 387, 21), (392, 1, 407, 21)],
        boxes={
            "idle": [
                [(1, 1, 18, 24), (1, 26, 17, 50)],
                [(23, 1, 39, 24), (22, 26, 38, 50)],
                [(44, 1, 58, 24), (43, 26, 58, 50)],
                [(63, 1, 78, 24), (63, 26, 76, 50)],
                [(83, 1, 100, 24), (81, 26, 97, 50)],
            ],
            "walking": [
                [(1, 1, 18, 24), (1, 26, 17, 50)],
                [(23, 1, 39, 24), (22, 26, 38, 50)],
                [(44, 1, 58, 24), (43, 26, 58, 50)],
                [(63, 1, 78, 24), (63, 26, 76, 50)],
                [(83, 1, 100, 24), (81, 26, 97, 50)],
            ],
        },
        auxiliary_boxes={
            "attacking": [
                [(1, 70, 17, 92), (22, 70, 39, 92), (44, 70, 61, 92)],
                [(1, 95, 17, 118), (22, 95, 38, 118), (43, 95, 60, 118)],
                [(1, 122, 16, 145), (21, 122, 35, 145), (40, 122, 55, 145)],
                [(1, 150, 14, 172), (19, 150, 34, 172), (39, 150, 55, 172)],
                [(1, 177, 17, 198), (22, 177, 39, 198), (44, 177, 61, 198)],
            ],
        },
        auxiliary_directions={
            "attacking": ["front", "down_right", "right", "up_right", "back"],
        },
    ),
    SpeciesSpec(
        species_id=282,
        slug="gardevoir",
        display_name="Gardevoir",
        source_name="282_gardevoir.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains walking frames: five directions, three frames each.",
            "Walking is intended for a start-hold-end motion pattern like Haunter when wired into the runtime.",
            "Row 2 is the primary three-frame idle action; row 3 is a second three-frame idle action reserved for selection.",
            "The final three sprites on row 4 are sleeping frame 0, sleeping frame 1, and a non-directional fainted frame.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(259, 101, 278, 123), (280, 102, 300, 123)],
        boxes={
            "idle": [
                [(2, 34, 19, 57), (21, 33, 38, 56), (40, 34, 57, 57)],
                [(61, 34, 80, 57), (82, 33, 100, 56), (102, 34, 121, 57)],
                [(125, 34, 146, 56), (148, 33, 169, 56), (171, 34, 191, 57)],
                [(195, 32, 215, 57), (217, 31, 237, 55), (238, 32, 260, 55)],
                [(264, 32, 280, 58), (282, 31, 299, 55), (301, 32, 317, 58)],
            ],
            "walking": [
                [(2, 4, 19, 27), (21, 4, 38, 27), (40, 4, 57, 27)],
                [(61, 3, 81, 27), (83, 4, 101, 27), (103, 4, 121, 27)],
                [(123, 3, 143, 27), (145, 4, 166, 27), (168, 5, 188, 27)],
                [(192, 3, 212, 27), (214, 2, 234, 26), (236, 2, 255, 27)],
                [(259, 3, 276, 27), (278, 3, 295, 27), (297, 3, 314, 27)],
            ],
        },
        auxiliary_boxes={
            "idle_alt": [
                [(2, 63, 19, 87), (21, 63, 38, 87), (40, 64, 57, 87)],
                [(61, 63, 78, 87), (80, 64, 100, 87), (102, 64, 123, 87)],
                [(125, 64, 145, 87), (147, 64, 171, 87), (173, 64, 197, 87)],
                [(201, 63, 220, 87), (222, 63, 244, 87), (246, 63, 268, 87)],
                [(272, 62, 287, 87), (289, 63, 305, 88), (307, 62, 324, 87)],
            ],
            "faint": [
                [(304, 108, 342, 123)],
            ],
        },
        auxiliary_directions={
            "idle_alt": SOURCE_DIRECTIONS,
            "faint": ["non_directional"],
        },
        auxiliary_canvas_sizes={"faint": (84, 64)},
    ),
    SpeciesSpec(
        species_id=41,
        slug="zubat",
        display_name="Zubat",
        source_name="041_zubat.png",
        source_group="low",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the seventh-last and sixth-last sprites, kept in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
        ],
        sleeping_boxes=[(330, 15, 355, 30), (357, 17, 384, 30)],
        boxes={
            "idle": [
                [(2, 11, 27, 30)],
                [(76, 9, 98, 30)],
                [(142, 7, 160, 30)],
                [(198, 6, 219, 30)],
                [(263, 8, 288, 30)],
            ],
            "walking": [
                [(2, 11, 27, 30), (29, 10, 50, 30), (50, 11, 75, 30)],
                [(76, 9, 98, 30), (100, 7, 120, 30), (121, 9, 141, 30)],
                [(142, 7, 160, 30), (160, 5, 176, 30), (177, 9, 196, 30)],
                [(198, 6, 219, 30), (221, 6, 238, 30), (240, 9, 261, 30)],
                [(263, 8, 288, 30), (290, 8, 309, 30), (311, 11, 330, 30)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=42,
        slug="golbat",
        display_name="Golbat",
        source_name="042_golbat.png",
        source_group="low",
        scale=2.0,
        canvas_width=68,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        mask_to_largest_component=True,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the seventh-last and sixth-last sprites, kept in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "Overlapping neighboring frame bounds are isolated by keeping the target connected component.",
            "A 68x64 canvas preserves fixed 2x scaling for the widest sleeping pose.",
        ],
        sleeping_boxes=[(339, 13, 370, 29), (373, 14, 405, 29)],
        boxes={
            "idle": [
                [(2, 4, 31, 29)],
                [(78, 2, 101, 29)],
                [(145, 2, 162, 29)],
                [(204, 2, 225, 29)],
                [(262, 5, 291, 29)],
            ],
            "walking": [
                [(2, 4, 31, 29), (26, 12, 55, 29), (55, 3, 76, 29)],
                [(78, 2, 101, 29), (101, 10, 123, 29), (124, 2, 143, 29)],
                [(145, 2, 162, 29), (165, 10, 183, 29), (184, 2, 202, 29)],
                [(204, 2, 225, 29), (222, 10, 243, 29), (245, 2, 260, 29)],
                [(262, 5, 291, 29), (289, 12, 320, 29), (321, 7, 338, 29)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=169,
        slug="crobat",
        display_name="Crobat",
        source_name="169_crobat.png",
        source_group="medium",
        scale=2.0,
        canvas_width=68,
        canvas_height=68,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 walking frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses the first two sprites on row 3.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A 68x68 canvas preserves fixed 2x scaling and the full wing tips.",
        ],
        sleeping_boxes=[(5, 81, 27, 100), (29, 82, 53, 100)],
        boxes={
            "idle": [
                [(4, 19, 36, 36)],
                [(110, 7, 135, 31)],
                [(187, 8, 202, 33)],
                [(257, 9, 283, 34)],
                [(344, 15, 376, 34)],
            ],
            "walking": [
                [(4, 19, 36, 36), (41, 14, 65, 35), (74, 16, 98, 35)],
                [(110, 7, 135, 31), (136, 6, 158, 29), (163, 10, 185, 32)],
                [(187, 8, 202, 33), (204, 9, 224, 32), (231, 8, 250, 33)],
                [(257, 9, 283, 34), (288, 11, 310, 34), (314, 5, 337, 33)],
                [(344, 15, 376, 34), (380, 16, 406, 35), (414, 13, 438, 35)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=143,
        slug="snorlax",
        display_name="Snorlax",
        source_name="143_snorlax.png",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "Idle uses the second Idle/Moving frame (`walk1`) because this sheet shares idle and moving art.",
        ],
        sleeping_boxes=[(260, 18, 293, 38), (295, 19, 328, 38)],
        boxes={
            "idle": [
                [(172, 20, 200, 47)],
                [(170, 49, 192, 80)],
                [(168, 82, 189, 114)],
                [(172, 117, 194, 146)],
                [(172, 149, 200, 175)],
            ],
            "walking": [
                [(145, 20, 170, 47), (172, 20, 200, 47), (202, 20, 227, 47)],
                [(145, 50, 168, 80), (170, 49, 192, 80), (194, 51, 221, 80)],
                [(145, 83, 166, 114), (168, 82, 189, 114), (191, 83, 212, 114)],
                [(145, 116, 170, 146), (172, 117, 194, 146), (196, 119, 217, 146)],
                [(145, 148, 170, 175), (172, 149, 200, 175), (202, 148, 227, 175)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=147,
        slug="dratini",
        display_name="Dratini",
        source_name="147_dratini.png",
        scale=2.0,
        source_directions=GENERATED_DIRECTIONS,
        mirror_directions=False,
        notes=["Idle frames are sourced from walking frame 0 because this sheet has no separate idle action."],
        sleeping_boxes=[(222, 16, 241, 36), (243, 17, 262, 36)],
        boxes={
            "idle": [
                [(4, 17, 27, 39)],
                [(4, 41, 24, 64)],
                [(4, 66, 19, 89)],
                [(4, 92, 20, 115)],
                [(4, 124, 25, 146)],
                [(4, 153, 23, 179)],
                [(4, 181, 22, 208)],
                [(4, 211, 20, 235)],
            ],
            "walking": [
                [(4, 17, 27, 39), (29, 18, 53, 39), (55, 17, 75, 39)],
                [(4, 41, 24, 64), (26, 41, 45, 64), (47, 41, 68, 64)],
                [(4, 66, 19, 89), (21, 66, 40, 89), (42, 67, 69, 89)],
                [(4, 92, 20, 115), (22, 92, 43, 115), (45, 91, 71, 115)],
                [(4, 124, 25, 146), (27, 122, 49, 146), (51, 117, 72, 146)],
                [(4, 153, 23, 179), (25, 151, 41, 179), (43, 148, 63, 179)],
                [(4, 181, 22, 208), (24, 181, 44, 208), (46, 184, 74, 208)],
                [(4, 211, 20, 235), (22, 212, 43, 235), (45, 215, 72, 235)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=148,
        slug="dragonair",
        display_name="Dragonair",
        source_name="148_dragonair.png",
        scale=2.0,
        canvas_width=88,
        canvas_height=82,
        fit_to_canvas=False,
        source_directions=GENERATED_DIRECTIONS,
        mirror_directions=False,
        notes=[
            "Walking uses the sheet's single Special Attack & Moving frame for each direction.",
            "Frames use a fixed 2x source scale on an 88x82 canvas so wide moving poses are not shrunk to fit 64x64.",
        ],
        sleeping_boxes=[(335, 21, 357, 53), (359, 22, 381, 53)],
        boxes={
            "idle": [
                [(6, 21, 32, 52), (34, 21, 60, 52)],
                [(6, 54, 32, 86), (34, 54, 62, 86)],
                [(6, 89, 34, 119), (36, 88, 67, 119)],
                [(6, 123, 27, 157), (29, 121, 51, 157)],
                [(6, 160, 32, 191), (34, 159, 60, 191)],
                [(6, 195, 31, 228), (33, 193, 61, 228)],
                [(6, 231, 30, 262), (32, 230, 57, 262)],
                [(6, 264, 26, 296), (28, 264, 51, 296)],
            ],
            "walking": [
                [(99, 45, 119, 82)],
                [(99, 83, 131, 113)],
                [(99, 115, 141, 143)],
                [(99, 145, 133, 181)],
                [(99, 183, 119, 221)],
                [(99, 223, 134, 259)],
                [(99, 261, 139, 291)],
                [(99, 293, 129, 325)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=149,
        slug="dragonite",
        display_name="Dragonite",
        source_name="149_dragonite.png",
        scale=2.0,
        source_directions=GENERATED_DIRECTIONS,
        mirror_directions=False,
        notes=["The source sheet labels the movement action as Moving; project assets store it as walking for state-machine compatibility."],
        sleeping_boxes=[(359, 30, 384, 58), (386, 31, 413, 58)],
        boxes={
            "idle": [
                [(6, 34, 31, 66)],
                [(6, 68, 29, 103)],
                [(6, 105, 33, 140)],
                [(6, 142, 31, 175)],
                [(6, 177, 33, 209)],
                [(6, 211, 31, 244)],
                [(6, 246, 33, 281)],
                [(6, 283, 29, 318)],
            ],
            "walking": [
                [(59, 34, 84, 64), (86, 34, 113, 64)],
                [(59, 68, 82, 99), (84, 66, 111, 99)],
                [(59, 101, 85, 134), (87, 101, 115, 134)],
                [(59, 136, 85, 168), (87, 137, 112, 168)],
                [(59, 170, 90, 201), (92, 170, 123, 201)],
                [(59, 204, 84, 235), (86, 203, 112, 235)],
                [(59, 237, 87, 270), (89, 237, 115, 270)],
                [(59, 272, 86, 305), (88, 274, 111, 305)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=161,
        slug="sentret",
        display_name="Sentret",
        source_name="161_sentret_162_furret.png",
        scale=2.0,
        canvas_width=64,
        canvas_height=76,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The source sheet contains all eight clockwise directions starting at front.",
            "Only front, down_left, left, up_left, and back are exported; right-side directions are mirrored at runtime.",
            "A taller canvas preserves the fixed 2x scale of the upright tail without shrinking idle frames.",
        ],
        sleeping_boxes=[(366, 18, 389, 37), (391, 18, 414, 37)],
        boxes={
            "idle": [
                [(7, 20, 30, 52)],
                [(7, 54, 27, 86)],
                [(7, 88, 22, 120)],
                [(7, 122, 25, 154)],
                [(7, 156, 28, 188)],
            ],
            "walking": [
                [(178, 19, 197, 41), (199, 19, 220, 41), (222, 19, 241, 41)],
                [(178, 46, 200, 67), (202, 43, 224, 67), (226, 44, 247, 67)],
                [(178, 70, 202, 93), (204, 69, 228, 93), (230, 70, 253, 93)],
                [(178, 96, 199, 119), (201, 95, 224, 119), (226, 95, 247, 119)],
                [(178, 122, 198, 145), (200, 121, 219, 145), (221, 122, 241, 145)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=162,
        slug="furret",
        display_name="Furret",
        source_name="161_sentret_162_furret.png",
        scale=2.0,
        canvas_width=84,
        canvas_height=80,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The source sheet contains all eight clockwise directions starting at front.",
            "Only front, down_left, left, up_left, and back are exported; right-side directions are mirrored at runtime.",
            "The wider canvas preserves the fixed 2x scale of long horizontal poses without clipping or shrinking.",
        ],
        sleeping_boxes=[(441, 320, 461, 340), (463, 320, 483, 340)],
        boxes={
            "idle": [
                [(290, 320, 308, 344)],
                [(290, 346, 313, 372)],
                [(290, 374, 314, 400)],
                [(290, 402, 314, 427)],
                [(290, 429, 308, 454)],
            ],
            "walking": [
                [(69, 330, 82, 353), (84, 330, 97, 353), (99, 320, 114, 353)],
                [(69, 359, 101, 382), (103, 360, 133, 382), (135, 355, 166, 382)],
                [(69, 389, 105, 408), (107, 384, 144, 408), (146, 387, 185, 408)],
                [(69, 418, 99, 438), (101, 410, 129, 438), (131, 419, 164, 438)],
                [(69, 440, 88, 475), (90, 450, 105, 475), (107, 452, 124, 475)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=212,
        slug="scizor",
        display_name="Scizor",
        source_name="212_scizor.png",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=["Only the five left-side source directions are exported; right-side directions are mirrored at runtime."],
        sleeping_boxes=[(611, 22, 640, 52), (610, 56, 640, 84)],
        boxes={
            "idle": [
                [(34, 25, 63, 51), (66, 25, 95, 51)],
                [(34, 59, 57, 86), (68, 60, 93, 86)],
                [(32, 94, 61, 121), (66, 91, 93, 121)],
                [(30, 125, 58, 152), (66, 125, 94, 152)],
                [(32, 153, 58, 183), (59, 156, 90, 183)],
            ],
            "walking": [
                [(115, 25, 144, 52), (150, 25, 179, 51), (186, 25, 215, 52)],
                [(119, 59, 142, 86), (154, 57, 174, 86), (187, 57, 212, 87)],
                [(120, 94, 148, 121), (150, 94, 179, 121), (182, 94, 210, 121)],
                [(117, 125, 139, 152), (150, 125, 177, 152), (181, 125, 209, 152)],
                [(115, 154, 138, 183), (149, 153, 175, 183), (181, 152, 208, 183)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=261,
        slug="poochyena",
        display_name="Poochyena",
        source_name="261_poochyena_262_mightyena.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 2 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 4, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(217, 128, 239, 143), (242, 127, 264, 143)],
        boxes={
            "idle": [
                [(16, 59, 32, 77)],
                [(75, 56, 94, 77)],
                [(144, 56, 167, 77)],
                [(224, 53, 243, 77)],
                [(293, 54, 307, 77)],
            ],
            "walking": [
                [(16, 59, 32, 77), (35, 58, 51, 77), (54, 54, 72, 77)],
                [(75, 56, 94, 77), (97, 57, 118, 77), (121, 56, 141, 77)],
                [(144, 56, 167, 77), (170, 53, 193, 77), (196, 56, 221, 77)],
                [(224, 53, 243, 77), (246, 50, 265, 77), (268, 56, 290, 77)],
                [(293, 54, 307, 77), (310, 53, 324, 77), (327, 60, 341, 77)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=262,
        slug="mightyena",
        display_name="Mightyena",
        source_name="261_poochyena_262_mightyena.png",
        source_group="medium",
        scale=2.0,
        canvas_width=64,
        canvas_height=72,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 2 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 4, in left-to-right order.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A taller canvas preserves fixed 2x scaling for the 30-pixel-high moving poses.",
        ],
        sleeping_boxes=[(231, 281, 247, 305), (250, 280, 266, 305)],
        boxes={
            "idle": [
                [(3, 206, 19, 232)],
                [(60, 206, 82, 232)],
                [(137, 207, 165, 232)],
                [(232, 204, 253, 232)],
                [(306, 205, 320, 232)],
            ],
            "walking": [
                [(3, 206, 19, 232), (22, 205, 38, 232), (41, 205, 57, 232)],
                [(60, 206, 82, 232), (85, 207, 108, 232), (111, 205, 134, 232)],
                [(137, 207, 165, 232), (168, 208, 197, 232), (200, 208, 229, 232)],
                [(232, 204, 253, 232), (256, 204, 277, 232), (280, 202, 303, 232)],
                [(306, 205, 320, 232), (323, 204, 337, 232), (340, 204, 354, 232)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=276,
        slug="taillow",
        display_name="Taillow",
        source_name="276_taillow_277_swellow.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(241, 80, 257, 94), (260, 79, 278, 94)],
        boxes={
            "idle": [
                [(55, 20, 68, 36)],
                [(103, 20, 121, 36)],
                [(165, 21, 188, 36)],
                [(240, 19, 260, 36)],
                [(308, 20, 321, 36)],
            ],
            "walking": [
                [(55, 20, 68, 36), (71, 19, 84, 36), (87, 21, 100, 36)],
                [(103, 20, 121, 36), (124, 19, 143, 36), (146, 21, 162, 36)],
                [(165, 21, 188, 36), (191, 19, 214, 36), (217, 22, 237, 36)],
                [(240, 19, 260, 36), (263, 17, 283, 36), (286, 20, 305, 36)],
                [(308, 20, 321, 36), (324, 18, 337, 36), (340, 21, 353, 36)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=277,
        slug="swellow",
        display_name="Swellow",
        source_name="276_taillow_277_swellow.png",
        source_group="medium",
        scale=2.0,
        canvas_width=68,
        canvas_height=64,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 3, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A wider canvas preserves fixed 2x scaling for the 31-pixel-wide moving poses.",
        ],
        sleeping_boxes=[(329, 188, 356, 205), (359, 188, 388, 205)],
        boxes={
            "idle": [
                [(15, 120, 30, 140)],
                [(91, 120, 112, 140)],
                [(167, 119, 193, 140)],
                [(246, 118, 269, 140)],
                [(321, 117, 338, 140)],
            ],
            "walking": [
                [(15, 120, 30, 140), (33, 119, 54, 140), (57, 118, 88, 140)],
                [(91, 120, 112, 140), (115, 118, 137, 140), (140, 115, 164, 140)],
                [(167, 119, 193, 140), (196, 118, 217, 140), (220, 115, 243, 140)],
                [(246, 118, 269, 140), (272, 116, 291, 140), (294, 113, 318, 140)],
                [(321, 117, 338, 140), (341, 115, 360, 140), (363, 115, 392, 140)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=278,
        slug="wingull",
        display_name="Wingull",
        source_name="278_wingull_279_pelipper.png",
        source_group="medium",
        scale=2.0,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 2, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(334, 63, 355, 82), (358, 63, 381, 82)],
        boxes={
            "idle": [
                [(19, 30, 49, 47)],
                [(114, 27, 139, 47)],
                [(193, 23, 215, 47)],
                [(268, 27, 292, 47)],
                [(346, 29, 376, 47)],
            ],
            "walking": [
                [(19, 30, 49, 47), (52, 30, 80, 47), (83, 29, 111, 47)],
                [(114, 27, 139, 47), (142, 27, 165, 47), (168, 27, 190, 47)],
                [(193, 23, 215, 47), (218, 25, 240, 47), (243, 24, 265, 47)],
                [(268, 27, 292, 47), (295, 28, 318, 47), (321, 27, 343, 47)],
                [(346, 29, 376, 47), (379, 29, 407, 47), (410, 28, 436, 46)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=279,
        slug="pelipper",
        display_name="Pelipper",
        source_name="278_wingull_279_pelipper.png",
        source_group="medium",
        scale=2.0,
        canvas_width=68,
        canvas_height=64,
        fit_to_canvas=False,
        crop_padding=1,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Row 1 contains 15 moving frames: five directions, three frames each.",
            "This sheet has no separate idle action; idle uses moving frame 0 for each direction.",
            "Sleeping uses the third-last and second-last sprites on row 4, before the shadow sprite.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
            "A wider canvas preserves fixed 2x scaling for the 31-pixel-wide moving poses.",
        ],
        sleeping_boxes=[(260, 208, 286, 229), (289, 209, 314, 229)],
        boxes={
            "idle": [
                [(3, 110, 30, 130)],
                [(101, 109, 127, 130)],
                [(195, 109, 219, 130)],
                [(276, 107, 301, 130)],
                [(361, 107, 388, 130)],
            ],
            "walking": [
                [(3, 110, 30, 130), (33, 110, 64, 130), (67, 110, 98, 130)],
                [(101, 109, 127, 130), (130, 111, 159, 130), (162, 111, 192, 130)],
                [(195, 109, 219, 130), (222, 111, 246, 130), (249, 111, 273, 130)],
                [(276, 107, 301, 130), (304, 107, 329, 130), (332, 107, 358, 130)],
                [(361, 107, 388, 130), (391, 107, 420, 130), (423, 107, 452, 130)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=380,
        slug="latias",
        display_name="Latias",
        source_name="380_latias.png",
        scale=1.5,
        canvas_width=84,
        canvas_height=72,
        bottom_margin=10,
        crop_padding=1,
        source_directions=LATI_SOURCE_DIRECTIONS,
        contact_directions=LATI_SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The sheet is horizontal; source direction order is left, down_left, front, up_left, back.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(1, 290, 37, 312), (38, 290, 75, 311)],
        boxes={
            "idle": [
                [(1, 30, 42, 65)],
                [(43, 30, 81, 63)],
                [(82, 30, 124, 58)],
                [(125, 30, 163, 64)],
                [(164, 30, 206, 65)],
            ],
            "walking": [
                [(1, 99, 42, 131), (43, 99, 84, 131)],
                [(85, 99, 123, 130), (124, 99, 162, 130)],
                [(163, 99, 205, 128), (206, 99, 248, 128)],
                [(249, 99, 287, 131), (288, 99, 326, 131)],
                [(327, 99, 369, 132), (370, 99, 412, 132)],
            ],
        },
    ),
    SpeciesSpec(
        species_id=381,
        slug="latios",
        display_name="Latios",
        source_name="381_latios.png",
        scale=1.5,
        canvas_width=84,
        canvas_height=72,
        bottom_margin=10,
        crop_padding=1,
        source_directions=LATI_SOURCE_DIRECTIONS,
        contact_directions=LATI_SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The sheet is horizontal; source direction order is left, down_left, front, up_left, back.",
            "Only five source directions are exported; right-side directions are mirrored at runtime.",
        ],
        sleeping_boxes=[(1, 290, 42, 319), (43, 290, 84, 318)],
        boxes={
            "idle": [
                [(1, 30, 50, 69)],
                [(51, 30, 97, 68)],
                [(98, 30, 149, 61)],
                [(150, 30, 194, 65)],
                [(195, 30, 246, 67)],
            ],
            "walking": [
                [(1, 99, 50, 133), (51, 99, 100, 133)],
                [(101, 99, 147, 137), (148, 99, 194, 137)],
                [(195, 99, 246, 131), (247, 99, 298, 131)],
                [(299, 99, 343, 132), (344, 99, 388, 132)],
                [(389, 99, 440, 134), (441, 99, 492, 134)],
            ],
        },
    ),
]


def clear_background(img, tolerance=0):
    bg_pixel = img.getpixel((0, 0))
    bg = bg_pixel[:3]
    transparent_bg = bg_pixel[3] <= 16
    data = img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata()
    pixels = []
    for r, g, b, a in data:
        same_bg = not transparent_bg and (r, g, b) == bg
        near_bg = (
            not transparent_bg
            and tolerance > 0
            and max(abs(r - bg[0]), abs(g - bg[1]), abs(b - bg[2])) <= tolerance
        )
        pixels.append((0, 0, 0, 0) if a <= 16 or same_bg or near_bg else (r, g, b, a))
    out = Image.new("RGBA", img.size)
    out.putdata(pixels)
    return out


def validate_spec(spec):
    if not spec.sleeping_boxes:
        raise ValueError(f"{spec.slug} sleeping must have at least one frame")
    for action, rows in spec.boxes.items():
        if len(rows) != len(spec.source_directions):
            raise ValueError(f"{spec.slug} {action} must have one source row per direction")
        frame_count = len(rows[0])
        if frame_count == 0:
            raise ValueError(f"{spec.slug} {action} has no frames")
        for row in rows:
            if len(row) != frame_count:
                raise ValueError(f"{spec.slug} {action} rows must have a consistent frame count")
    for action, rows in spec.auxiliary_boxes.items():
        directions = spec.auxiliary_directions.get(action, spec.source_directions)
        if len(rows) != len(directions):
            raise ValueError(f"{spec.slug} {action} must have one source row per direction")
        frame_count = len(rows[0])
        if frame_count == 0:
            raise ValueError(f"{spec.slug} {action} has no frames")
        for row in rows:
            if len(row) != frame_count:
                raise ValueError(f"{spec.slug} {action} rows must have a consistent frame count")


def frame_canvas(frame, spec):
    if spec.fit_to_canvas:
        fit_scale = min(spec.scale, spec.canvas_width / frame.width, (spec.canvas_height - spec.bottom_margin) / frame.height)
    else:
        fit_scale = spec.scale
    scaled_w = max(1, int(round(frame.width * fit_scale)))
    scaled_h = max(1, int(round(frame.height * fit_scale)))
    scaled = frame.resize((scaled_w, scaled_h), Image.Resampling.NEAREST)
    canvas_w = max(spec.canvas_width, scaled.width)
    canvas_h = max(spec.canvas_height, scaled.height + spec.bottom_margin)
    canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    x = (canvas_w - scaled.width) // 2
    y = max(0, canvas_h - spec.bottom_margin - scaled.height)
    canvas.alpha_composite(scaled, (x, y))
    return canvas


def save_frame(out_dir, action, direction, index, frame):
    path = out_dir / action / f"{direction}_{index}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    frame.save(path)
    return path


def find_psd_layer(layers, name):
    for layer in layers:
        if layer.name == name:
            return layer
        if layer.is_group():
            found = find_psd_layer(layer, name)
            if found:
                return found
    return None


def psd_layer_frames(source_group, source_name, layer_name):
    from psd_tools import PSDImage

    psd = PSDImage.open(SOURCE_ROOT / source_group / source_name)
    layer = find_psd_layer(psd, layer_name)
    if layer is None:
        raise ValueError(f"PSD layer not found: {source_group}/{source_name}#{layer_name}")

    img = layer.composite().convert("RGBA")
    alpha = img.getchannel("A")
    runs = []
    start = None
    for x in range(img.width):
        opaque = alpha.crop((x, 0, x + 1, img.height)).getbbox() is not None
        if opaque and start is None:
            start = x
        if (not opaque or x == img.width - 1) and start is not None:
            end = x if not opaque else x + 1
            if end - start > 1:
                runs.append((start, end))
            start = None

    frames = []
    for x1, x2 in runs:
        strip = img.crop((x1, 0, x2, img.height))
        bbox = strip.getchannel("A").getbbox()
        if bbox:
            frames.append(strip.crop(bbox))
    return frames


def contact_rows(spec):
    rows = []
    for action in ("idle", "walking"):
        for frame_index in range(len(spec.boxes[action][0])):
            rows.append((action, frame_index))
    for frame_index in range(len(spec.sleeping_boxes)):
        rows.append(("sleeping", frame_index))
    return rows


def make_contact_sheet_for_rows(spec, out_dir, frames, rows, directions, filename=None):
    by_key = {(action, direction, index): frame for action, direction, index, frame in frames}
    cell_w = spec.canvas_width + 10
    cell_h = spec.canvas_height + 10
    left_label = 86
    top_label = 30
    width = left_label + len(directions) * cell_w + 10
    height = top_label + len(rows) * cell_h + 10
    sheet = Image.new("RGBA", (width, height), CONTACT_BG)
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    draw.text((10, 10), f"frame {spec.canvas_width}x{spec.canvas_height} px", fill=(255, 255, 255, 255), font=font)

    for col, direction in enumerate(directions):
        draw.text((left_label + col * cell_w + 2, 10), direction, fill=(255, 255, 255, 255), font=font)

    for row, (action, index) in enumerate(rows):
        y = top_label + row * cell_h
        draw.text((10, y + 26), f"{action}_{index}", fill=(255, 255, 255, 255), font=font)
        if action == "sleeping":
            x = left_label + 5
            sprite_y = y + 5
            sheet.alpha_composite(by_key[(action, None, index)], (x, sprite_y))
            draw.rectangle((x, sprite_y, x + spec.canvas_width - 1, sprite_y + spec.canvas_height - 1), outline=(44, 156, 170, 255))
            draw.text((left_label + cell_w + 2, y + 26), "non-directional", fill=(255, 255, 255, 255), font=font)
            continue
        for col, direction in enumerate(directions):
            x = left_label + col * cell_w + 5
            sprite_y = y + 5
            sheet.alpha_composite(by_key[(action, direction, index)], (x, sprite_y))
            draw.rectangle((x, sprite_y, x + spec.canvas_width - 1, sprite_y + spec.canvas_height - 1), outline=(44, 156, 170, 255))
    sheet.save(out_dir / (filename or f"{spec.slug}_idle_walking_contact.png"))


def make_contact_sheet(spec, out_dir, frames):
    make_contact_sheet_for_rows(spec, out_dir, frames, contact_rows(spec), spec.contact_directions)


def write_readme(spec, out_dir):
    idle_count = len(spec.boxes["idle"][0])
    walking_count = len(spec.boxes["walking"][0])
    sleeping_count = len(spec.sleeping_boxes)
    idle_word = "frames" if idle_count != 1 else "frame"
    walking_word = "frames" if walking_count != 1 else "frame"
    sleeping_word = "frames" if sleeping_count != 1 else "frame"
    mirror_map_text = ", ".join(f"`{direction}` from `{source}`" for direction, source in spec.mirror_map.items())
    mirror_text = (
        f"- Mirrored directions: {mirror_map_text}.\n\n"
        if spec.mirror_directions and spec.export_mirror_frames else
        "- Mirrored directions: right-side directions are drawn by runtime mirroring; only source direction PNGs are exported.\n\n"
        if spec.mirror_directions else
        "- Mirrored directions: none; all 8 directions are cropped from source frames.\n\n"
    )
    notes_text = "".join(f"- Note: {note}\n" for note in spec.notes)
    if notes_text:
        notes_text += "\n"
    auxiliary_text = ""
    auxiliary_contact_text = ""
    for action, rows in spec.auxiliary_boxes.items():
        directions = spec.auxiliary_directions.get(action, spec.source_directions)
        frame_count = len(rows[0])
        auxiliary_text += (
            f"- `{action}`: {frame_count} frame{'s' if frame_count != 1 else ''} per direction; "
            f"source directions are {', '.join(f'`{direction}`' for direction in directions)}. "
            "Extracted for future use and not wired into the current runtime state machine.\n"
        )
        auxiliary_contact_text += f"- File: `{spec.slug}_{action}_contact.png`.\n"
    if auxiliary_text:
        auxiliary_text += "\n"
    if spec.runtime_wired:
        runtime_text = (
            "## Runtime state machine\n\n"
            "- Runtime owner: `src/scenes/MainScene.cpp`.\n"
            f"- Scope: only species `{spec.species_id}` uses this {spec.display_name} state set.\n"
            "- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.\n"
            "- Direction input: the current AI velocity vector is mapped to the 8 generated directions.\n"
            "- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.\n"
            "- Sleeping behavior: sleeping frames ignore direction.\n"
            "- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.\n"
        )
    else:
        runtime_text = (
            "## Runtime state machine\n\n"
            "- Preview only: this species is not registered in `tools/generate_pokemon_sprites.py`, `Species.cpp`, or `MainScene.cpp`.\n"
            "- No LittleFS sprite pack is generated until the preview is approved.\n"
        )
    readme = out_dir / "README.md"
    readme.write_text(
        f"# {spec.species_id} {spec.display_name} PMD processing spec\n\n"
        f"This spec is intentionally scoped to {spec.display_name} only.\n\n"
        "## Source\n\n"
        f"- Input sheet: `origin_asset/source_sheets/{spec.source_group}/{spec.source_name}`\n"
        f"- Background removal: pixels matching the top-left source color"
        f"{' within tolerance ' + str(spec.background_tolerance) if spec.background_tolerance else ''} become transparent.\n"
        "- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.\n\n"
        "## Output frames\n\n"
        f"- Output root: `origin_asset/processed/{processed_dir_name(spec)}/`\n"
        "- Format: RGBA PNG with transparent background.\n"
        f"- Canvas: {spec.canvas_width}x{spec.canvas_height} px.\n"
        f"- Target scale: nearest-neighbor {spec.scale:g}x"
        f"{', capped to fit the canvas without clipping' if spec.fit_to_canvas else ', fixed without per-frame fit scaling'}.\n"
        f"- Placement: horizontally centered, bottom aligned with a {spec.bottom_margin} px bottom margin.\n"
        "- Naming: `{action}/{direction}_{frame_index}.png`.\n\n"
        "## Actions\n\n"
        f"- `idle`: {idle_count} {idle_word}, indices `0..{idle_count - 1}`.\n"
        f"- `walking`: {walking_count} {walking_word}, indices `0..{walking_count - 1}`.\n\n"
        f"- `sleeping`: {sleeping_count} non-directional {sleeping_word}, indices `0..{sleeping_count - 1}`.\n\n"
        f"{auxiliary_text}"
        "## Directions\n\n"
        f"- Source row order: {', '.join(f'`{direction}`' for direction in spec.source_directions)}.\n"
        f"- Exported direction order: {', '.join(f'`{direction}`' for direction in spec.contact_directions)}.\n"
        f"{mirror_text}"
        f"{notes_text}"
        "## Contact sheet\n\n"
        f"- File: `{spec.slug}_idle_walking_contact.png`.\n"
        f"{auxiliary_contact_text}"
        f"- Top-left label: output frame size, `frame {spec.canvas_width}x{spec.canvas_height} px`.\n"
        "- Columns: exported directions in the order above.\n"
        f"- Rows: {', '.join(f'`{a}_{i}`' for a, i in contact_rows(spec))}.\n"
        "- Sleeping rows are non-directional; the frame is shown in the first grid cell only.\n"
        "- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.\n\n"
        "## Project preview mapping\n\n"
        "- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.\n"
        "- `FRONT`: `walking/front_0.png`.\n"
        "- `BACK`: `walking/back_0.png`.\n\n"
        f"{runtime_text}",
        encoding="utf-8",
    )


def padded(box, pad=1):
    x1, y1, x2, y2 = box
    return (max(0, x1 - pad), max(0, y1 - pad), x2 + pad, y2 + pad)


def keep_largest_alpha_component(frame):
    alpha = frame.getchannel("A")
    opaque = {
        (x, y)
        for y in range(frame.height)
        for x in range(frame.width)
        if alpha.getpixel((x, y)) > 16
    }
    components = []
    while opaque:
        seed = opaque.pop()
        component = {seed}
        stack = [seed]
        while stack:
            x, y = stack.pop()
            for nx in (x - 1, x, x + 1):
                for ny in (y - 1, y, y + 1):
                    point = (nx, ny)
                    if point in opaque:
                        opaque.remove(point)
                        component.add(point)
                        stack.append(point)
        components.append(component)

    if not components:
        return frame

    largest = max(components, key=len)
    out = frame.copy()
    pixels = out.load()
    for component in components:
        if component is largest:
            continue
        for x, y in component:
            r, g, b, _ = pixels[x, y]
            pixels[x, y] = (r, g, b, 0)
    return out


def crop_source_frame(source, box, spec):
    frame = source.crop(padded(box, spec.crop_padding))
    if spec.mask_to_largest_component:
        frame = keep_largest_alpha_component(frame)
    return frame_canvas(frame, spec)


def write_mew_readme(spec, out_dir):
    readme = out_dir / "README.md"
    readme.write_text(
        f"# {spec.species_id} {spec.display_name} PMD processing spec\n\n"
        "This spec is intentionally scoped to Mew only.\n\n"
        "## Source\n\n"
        f"- Input sheet: `origin_asset/source_sheets/{spec.source_group}/{spec.source_name}`\n"
        "- Background removal: pixels matching the top-left source color become transparent.\n"
        "- Crop boxes are fixed in `tools/extract_pmd_eeveelution_frames.py`; they are not a generic PMD parser.\n\n"
        "## Output frames\n\n"
        f"- Output root: `origin_asset/processed/{processed_dir_name(spec)}/`\n"
        "- Format: RGBA PNG with transparent background.\n"
        f"- Canvas: {spec.canvas_width}x{spec.canvas_height} px.\n"
        f"- Target scale: nearest-neighbor {spec.scale:g}x, capped to fit the canvas without clipping.\n"
        "- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.\n"
        "- Naming: `{action}/{direction}_{frame_index}.png`.\n\n"
        "## Actions\n\n"
        "- `idle`: 3 frames, indices `0..2`; all 8 directions are source frames, no mirroring.\n"
        "- `walking`: 2 frames, indices `0..1`; source directions are `front`, `down_left`, `left`, `up_left`, `back`.\n"
        "- `walking` mirrored directions: `up_right` from `up_left`, `right` from `left`, `down_right` from `down_left`.\n"
        "- `sleeping`: 2 non-directional frames, indices `0..1`, sourced from the first two sprites on row 6.\n\n"
        "## Directions\n\n"
        "- Runtime direction order: `front`, `down_left`, `left`, `up_left`, `back`, `up_right`, `right`, `down_right`.\n"
        "- Idle source row 1: `front`, `down_right`, `right`, `up_right`.\n"
        "- Idle source row 2: `back`, `up_left`, `left`, `down_left`.\n\n"
        "## Contact sheet\n\n"
        f"- File: `{spec.slug}_idle_walking_contact.png`.\n"
        f"- Top-left label: output frame size, `frame {spec.canvas_width}x{spec.canvas_height} px`.\n"
        "- Rows: `walking_0`, `walking_1`, `idle_0`, `idle_1`, `idle_2`, `sleeping_0`, `sleeping_1`.\n\n"
        "## Project preview mapping\n\n"
        "- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.\n"
        "- `FRONT`: `walking/front_0.png`.\n"
        "- `BACK`: `walking/back_0.png`.\n",
        encoding="utf-8",
    )


def process_mew_spec(spec):
    source = clear_background(Image.open(SOURCE_ROOT / spec.source_group / spec.source_name).convert("RGBA"))
    out_dir = processed_species_dir(spec)
    exported = []
    contact = []

    walking = {
        "front": [(4, 3, 17, 34), (22, 3, 34, 33)],
        "down_left": [(43, 9, 69, 32), (76, 7, 102, 33)],
        "left": [(103, 13, 135, 34), (136, 11, 168, 35)],
        "up_left": [(173, 9, 201, 35), (206, 9, 234, 35)],
        "back": [(236, 4, 250, 35), (253, 4, 267, 34)],
    }

    walking_frames = {}
    for direction in SOURCE_DIRECTIONS:
        walking_frames[direction] = []
        for index, box in enumerate(walking[direction]):
            frame = frame_canvas(source.crop(padded(box)), spec)
            walking_frames[direction].append(frame)
            exported.append(save_frame(out_dir, "walking", direction, index, frame))

    for direction, source_direction in MIRRORS.items():
        for index, source_frame in enumerate(walking_frames[source_direction]):
            frame = ImageOps.mirror(source_frame)
            exported.append(save_frame(out_dir, "walking", direction, index, frame))

    idle = {
        "front": [(4, 46, 25, 68), (29, 45, 50, 67), (54, 38, 75, 65)],
        "down_right": [(79, 45, 102, 67), (105, 44, 127, 66), (132, 40, 152, 66)],
        "right": [(156, 45, 177, 66), (182, 45, 201, 65), (208, 45, 226, 67)],
        "up_right": [(231, 44, 251, 66), (256, 44, 275, 65), (280, 45, 298, 66)],
        "back": [(6, 79, 26, 101), (31, 79, 50, 100), (56, 81, 76, 101)],
        "up_left": [(79, 77, 101, 99), (105, 77, 125, 98), (132, 78, 152, 99)],
        "left": [(153, 79, 177, 100), (179, 79, 201, 99), (204, 79, 224, 101)],
        "down_left": [(228, 77, 248, 101), (253, 77, 274, 101), (282, 72, 301, 99)],
    }
    for direction in GENERATED_DIRECTIONS:
        for index, box in enumerate(idle[direction]):
            frame = frame_canvas(source.crop(padded(box)), spec)
            exported.append(save_frame(out_dir, "idle", direction, index, frame))

    sleeping = [(6, 172, 25, 197), (39, 171, 58, 198)]
    for index, box in enumerate(sleeping):
        frame = frame_canvas(source.crop(padded(box)), spec)
        path = out_dir / "sleeping" / f"frame_{index}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        frame.save(path)
        exported.append(path)

    for action, count in (("walking", 2), ("idle", 3)):
        for index in range(count):
            for direction in GENERATED_DIRECTIONS:
                contact.append((action, direction, index, Image.open(out_dir / action / f"{direction}_{index}.png")))
    for index in range(2):
        contact.append(("sleeping", None, index, Image.open(out_dir / "sleeping" / f"frame_{index}.png")))

    write_mew_readme(spec, out_dir)
    rows = [("walking", 0), ("walking", 1), ("idle", 0), ("idle", 1), ("idle", 2), ("sleeping", 0), ("sleeping", 1)]
    make_contact_sheet_for_rows(spec, out_dir, contact, rows, GENERATED_DIRECTIONS)
    print(f"{spec.slug}: exported={len(exported)} out={out_dir}")


def process_spec(spec):
    if spec.slug == "mew":
        process_mew_spec(spec)
        return
    validate_spec(spec)
    source = clear_background(Image.open(SOURCE_ROOT / spec.source_group / spec.source_name).convert("RGBA"), spec.background_tolerance)
    sleeping_source = source
    if spec.sleeping_source_name:
        sleeping_group = spec.sleeping_source_group or spec.source_group
        sleeping_source = clear_background(Image.open(SOURCE_ROOT / sleeping_group / spec.sleeping_source_name).convert("RGBA"), spec.background_tolerance)
    out_dir = processed_species_dir(spec)
    exported = []
    contact = []
    override_cache = {}

    def override_frames(action, direction):
        override = spec.frame_overrides.get((action, direction))
        if not override:
            return None
        source_group, source_name, layer_name, indices = override
        key = (source_group, source_name, layer_name)
        if key not in override_cache:
            override_cache[key] = psd_layer_frames(source_group, source_name, layer_name)
        source_frames = override_cache[key]
        return [frame_canvas(source_frames[index], spec) for index in indices]

    def sleeping_frames():
        if not spec.sleeping_layer_override:
            return [
                crop_source_frame(sleeping_source, box, spec)
                for box in spec.sleeping_boxes
            ]
        source_group, source_name, layer_name = spec.sleeping_layer_override
        return [
            frame_canvas(frame, spec)
            for frame in psd_layer_frames(source_group, source_name, layer_name)
        ]

    for action, rows in spec.boxes.items():
        source_direction_frames = {}
        for row_index, direction in enumerate(spec.source_directions):
            source_direction_frames[direction] = []
            frames = override_frames(action, direction)
            if frames is None:
                frames = [crop_source_frame(source, box, spec) for box in rows[row_index]]
            for frame_index, frame in enumerate(frames):
                source_direction_frames[direction].append(frame)
                exported.append(save_frame(out_dir, action, direction, frame_index, frame))

        if spec.mirror_directions and spec.export_mirror_frames:
            for direction, source_direction in spec.mirror_map.items():
                for frame_index, source_frame in enumerate(source_direction_frames[source_direction]):
                    frame = ImageOps.mirror(source_frame)
                    exported.append(save_frame(out_dir, action, direction, frame_index, frame))

        for frame_index in range(len(rows[0])):
            for direction in spec.contact_directions:
                contact.append((action, direction, frame_index, Image.open(out_dir / action / f"{direction}_{frame_index}.png")))

    for frame_index, frame in enumerate(sleeping_frames()):
        path = out_dir / "sleeping" / f"frame_{frame_index}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        frame.save(path)
        exported.append(path)
        contact.append(("sleeping", None, frame_index, frame))

    for action, rows in spec.auxiliary_boxes.items():
        directions = spec.auxiliary_directions.get(action, spec.source_directions)
        auxiliary_spec = spec
        if action in spec.auxiliary_canvas_sizes:
            canvas_width, canvas_height = spec.auxiliary_canvas_sizes[action]
            auxiliary_spec = replace(spec, canvas_width=canvas_width, canvas_height=canvas_height)
        auxiliary_contact = []
        for row_index, direction in enumerate(directions):
            for frame_index, box in enumerate(rows[row_index]):
                frame = crop_source_frame(source, box, auxiliary_spec)
                exported.append(save_frame(out_dir, action, direction, frame_index, frame))
                auxiliary_contact.append((action, direction, frame_index, frame))
        auxiliary_rows = [(action, frame_index) for frame_index in range(len(rows[0]))]
        make_contact_sheet_for_rows(
            auxiliary_spec,
            out_dir,
            auxiliary_contact,
            auxiliary_rows,
            directions,
            f"{spec.slug}_{action}_contact.png",
        )

    write_readme(spec, out_dir)
    make_contact_sheet(spec, out_dir, contact)
    print(f"{spec.slug}: exported={len(exported)} out={out_dir}")


def main():
    parser = argparse.ArgumentParser(description="Extract fixed PMD sprite frames.")
    parser.add_argument(
        "species",
        nargs="*",
        help="Optional species ids or slugs to process; all species are processed when omitted.",
    )
    args = parser.parse_args()

    selected = SPECS
    if args.species:
        requested = {value.lower() for value in args.species}
        selected = [
            spec
            for spec in SPECS
            if spec.slug.lower() in requested or str(spec.species_id) in requested
        ]
        found = {spec.slug.lower() for spec in selected} | {str(spec.species_id) for spec in selected}
        missing = sorted(requested - found)
        if missing:
            parser.error(f"unknown species: {', '.join(missing)}")

    for spec in selected:
        process_spec(spec)


if __name__ == "__main__":
    main()
