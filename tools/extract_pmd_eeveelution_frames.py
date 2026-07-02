#!/usr/bin/env python3
from dataclasses import dataclass, field
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
    canvas_width: int = CANVAS_SIZE
    canvas_height: int = CANVAS_SIZE
    fit_to_canvas: bool = True
    source_directions: list = field(default_factory=lambda: SOURCE_DIRECTIONS)
    contact_directions: list = field(default_factory=lambda: GENERATED_DIRECTIONS)
    mirror_directions: bool = True
    mirror_map: dict = field(default_factory=lambda: MIRRORS)
    export_mirror_frames: bool = True
    background_tolerance: int = 0
    notes: list = field(default_factory=list)


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
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
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
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
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
            "Walking uses 24 source sprites: row 1 sprites 1..21 plus row 2 sprites 1..3.",
            "Source direction order is front, down_right, right, up_right, back, up_left, left, down_left.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 6 and 7.",
        ],
        sleeping_boxes=[(213, 33, 240, 56), (242, 32, 267, 56)],
        boxes={
            "idle": [
                [(2, 3, 24, 24)],
                [(74, 1, 92, 24)],
                [(134, 0, 153, 24)],
                [(198, 0, 220, 24)],
                [(268, 1, 288, 24)],
                [(333, 0, 351, 24)],
                [(393, 0, 413, 24)],
                [(2, 42, 19, 56)],
            ],
            "walking": [
                [(2, 3, 24, 24), (26, 3, 46, 24), (48, 3, 72, 24)],
                [(74, 1, 92, 24), (94, 1, 111, 24), (113, 2, 132, 24)],
                [(134, 0, 153, 24), (155, 2, 175, 24), (177, 2, 196, 24)],
                [(198, 0, 220, 24), (222, 2, 244, 24), (246, 0, 266, 24)],
                [(268, 1, 288, 24), (290, 0, 309, 23), (311, 1, 331, 24)],
                [(333, 0, 351, 24), (353, 0, 371, 24), (373, 1, 391, 24)],
                [(393, 0, 413, 24), (415, 2, 437, 24), (439, 2, 458, 24)],
                [(2, 42, 19, 56), (21, 41, 38, 56), (40, 35, 62, 56)],
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
        species_id=25,
        slug="pikachu",
        display_name="Pikachu",
        source_name="皮卡丘.psd",
        source_group="low/1",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "Only the five left-side source directions are exported; right-side directions are mirrored at runtime.",
            "This source sheet has no separate idle action; idle uses the first Idle/Moving frame (`walk0`).",
            "Crop boxes were derived from PSD layers walk0..walk4 and sleeping.",
        ],
        sleeping_boxes=[(354, 22, 377, 46), (377, 22, 401, 46)],
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
        source_directions=["front", "down_right", "right", "up_right", "back", "up_left", "left", "down_left"],
        mirror_directions=False,
        notes=[
            "Walking uses 24 source sprites: row 1 sprites 1..21 plus row 2 sprites 1..3.",
            "Source direction order is front, down_right, right, up_right, back, up_left, left, down_left.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 3 and 2.",
        ],
        sleeping_boxes=[(507, 40, 533, 67), (535, 41, 561, 67)],
        boxes={
            "idle": [
                [(2, 2, 29, 27)],
                [(90, 0, 115, 27)],
                [(176, 1, 201, 27)],
                [(259, 0, 282, 27)],
                [(337, 0, 365, 27)],
                [(426, 1, 450, 27)],
                [(505, 1, 530, 27)],
                [(2, 38, 26, 67)],
            ],
            "walking": [
                [(2, 2, 29, 27), (32, 3, 59, 27), (61, 3, 87, 27)],
                [(90, 0, 115, 27), (118, 0, 144, 27), (147, 1, 173, 27)],
                [(176, 1, 201, 27), (202, 1, 227, 27), (230, 2, 256, 27)],
                [(259, 0, 282, 27), (285, 0, 308, 27), (311, 2, 335, 27)],
                [(337, 0, 365, 27), (365, 0, 394, 27), (396, 0, 423, 27)],
                [(426, 1, 450, 27), (452, 2, 477, 27), (479, 1, 503, 27)],
                [(505, 1, 530, 27), (532, 2, 557, 27), (560, 3, 586, 27)],
                [(2, 38, 26, 67), (27, 41, 52, 67), (54, 39, 79, 67)],
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
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites 10 and 9, following the source note order.",
        ],
        sleeping_boxes=[(242, 34, 268, 64), (208, 42, 240, 64)],
        boxes={
            "idle": [
                [(2, 4, 26, 27)],
                [(80, 0, 106, 27)],
                [(164, 0, 190, 27)],
                [(244, 1, 269, 27)],
                [(322, 3, 348, 27)],
            ],
            "walking": [
                [(2, 4, 26, 27), (28, 5, 52, 27), (54, 3, 78, 27)],
                [(80, 0, 106, 27), (108, 1, 134, 27), (136, 0, 162, 27)],
                [(164, 0, 190, 27), (190, 0, 216, 27), (217, 0, 242, 27)],
                [(244, 1, 269, 27), (269, 2, 294, 27), (295, 0, 320, 27)],
                [(322, 3, 348, 27), (350, 5, 376, 27), (377, 2, 403, 27)],
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
        source_name="123_scyther.png",
        source_group="low",
        scale=2.0,
        contact_directions=SOURCE_DIRECTIONS,
        export_mirror_frames=False,
        notes=[
            "The first 15 sprites are walking frames: five directions, three frames each.",
            "This source sheet has no separate idle action; idle uses walking frame 0 for each direction.",
            "Sleeping uses row 2 sprites counted from the end: 7 and 6.",
        ],
        sleeping_boxes=[(302, 37, 332, 63), (330, 36, 359, 63)],
        boxes={
            "idle": [
                [(2, 2, 28, 28)],
                [(86, 0, 116, 28)],
                [(173, 0, 200, 28)],
                [(258, 2, 287, 28)],
                [(343, 2, 369, 28)],
            ],
            "walking": [
                [(2, 2, 28, 28), (30, 2, 56, 28), (58, 2, 84, 28)],
                [(86, 0, 116, 28), (113, 2, 141, 28), (142, 0, 173, 28)],
                [(173, 0, 200, 28), (201, 1, 228, 28), (230, 1, 259, 28)],
                [(258, 2, 287, 28), (286, 2, 316, 28), (314, 4, 342, 28)],
                [(343, 2, 369, 28), (371, 1, 396, 28), (398, 1, 423, 28)],
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
        species_id=380,
        slug="latias",
        display_name="Latias",
        source_name="380_latias.png",
        scale=1.5,
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
    bg = img.getpixel((0, 0))[:3]
    data = img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata()
    pixels = []
    for r, g, b, a in data:
        near_bg = tolerance > 0 and max(abs(r - bg[0]), abs(g - bg[1]), abs(b - bg[2])) <= tolerance
        pixels.append((0, 0, 0, 0) if a <= 16 or (r, g, b) == bg or near_bg else (r, g, b, a))
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


def frame_canvas(frame, spec):
    if spec.fit_to_canvas:
        fit_scale = min(spec.scale, spec.canvas_width / frame.width, (spec.canvas_height - BOTTOM_MARGIN) / frame.height)
    else:
        fit_scale = spec.scale
    scaled_w = max(1, int(round(frame.width * fit_scale)))
    scaled_h = max(1, int(round(frame.height * fit_scale)))
    scaled = frame.resize((scaled_w, scaled_h), Image.Resampling.NEAREST)
    canvas_w = max(spec.canvas_width, scaled.width)
    canvas_h = max(spec.canvas_height, scaled.height + BOTTOM_MARGIN)
    canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    x = (canvas_w - scaled.width) // 2
    y = max(0, canvas_h - BOTTOM_MARGIN - scaled.height)
    canvas.alpha_composite(scaled, (x, y))
    return canvas


def save_frame(out_dir, action, direction, index, frame):
    path = out_dir / action / f"{direction}_{index}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    frame.save(path)
    return path


def contact_rows(spec):
    rows = []
    for action in ("idle", "walking"):
        for frame_index in range(len(spec.boxes[action][0])):
            rows.append((action, frame_index))
    for frame_index in range(len(spec.sleeping_boxes)):
        rows.append(("sleeping", frame_index))
    return rows


def make_contact_sheet_for_rows(spec, out_dir, frames, rows, directions):
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
    sheet.save(out_dir / f"{spec.slug}_idle_walking_contact.png")


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
        f"- Output root: `origin_asset/processed/{spec.slug}/`\n"
        "- Format: RGBA PNG with transparent background.\n"
        f"- Canvas: {spec.canvas_width}x{spec.canvas_height} px.\n"
        f"- Target scale: nearest-neighbor {spec.scale:g}x"
        f"{', capped to fit the canvas without clipping' if spec.fit_to_canvas else ', fixed without per-frame fit scaling'}.\n"
        "- Placement: horizontally centered, bottom aligned with a 6 px bottom margin.\n"
        "- Naming: `{action}/{direction}_{frame_index}.png`.\n\n"
        "## Actions\n\n"
        f"- `idle`: {idle_count} {idle_word}, indices `0..{idle_count - 1}`.\n"
        f"- `walking`: {walking_count} {walking_word}, indices `0..{walking_count - 1}`.\n\n"
        f"- `sleeping`: {sleeping_count} non-directional {sleeping_word}, indices `0..{sleeping_count - 1}`.\n\n"
        "## Directions\n\n"
        f"- Source row order: {', '.join(f'`{direction}`' for direction in spec.source_directions)}.\n"
        f"- Exported direction order: {', '.join(f'`{direction}`' for direction in spec.contact_directions)}.\n"
        f"{mirror_text}"
        f"{notes_text}"
        "## Contact sheet\n\n"
        f"- File: `{spec.slug}_idle_walking_contact.png`.\n"
        f"- Top-left label: output frame size, `frame {spec.canvas_width}x{spec.canvas_height} px`.\n"
        "- Columns: exported directions in the order above.\n"
        f"- Rows: {', '.join(f'`{a}_{i}`' for a, i in contact_rows(spec))}.\n"
        "- Sleeping rows are non-directional; the frame is shown in the first grid cell only.\n"
        "- Purpose: visual QA for crop alignment, direction order, mirroring, and animation frame order.\n\n"
        "## Project preview mapping\n\n"
        "- `ICON_0`: first 64x64 cell from Pokemon Essentials icon sheet; reserved for future storage/list pages.\n"
        "- `FRONT`: `walking/front_0.png`.\n"
        "- `BACK`: `walking/back_0.png`.\n\n"
        "## Runtime state machine\n\n"
        "- Runtime owner: `src/scenes/MainScene.cpp`.\n"
        f"- Scope: only species `{spec.species_id}` uses this {spec.display_name} state set.\n"
        "- State action: `sleeping` when the monster has `STATUS_SLEEP`, `idle` when the AI velocity is near zero, otherwise `walking`.\n"
        "- Direction input: the current AI velocity vector is mapped to the 8 generated directions.\n"
        "- Idle behavior: when movement stops, the monster keeps the last movement direction and plays that direction's idle loop.\n"
        "- Sleeping behavior: sleeping frames ignore direction.\n"
        "- SpriteKind order: `tools/generate_pokemon_sprites.py` appends this species' kinds in generated direction order, with all idle frames first, walking frames second, and sleeping frames after.\n",
        encoding="utf-8",
    )


def padded(box, pad=1):
    x1, y1, x2, y2 = box
    return (max(0, x1 - pad), max(0, y1 - pad), x2 + pad, y2 + pad)


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
        f"- Output root: `origin_asset/processed/{spec.slug}/`\n"
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
    out_dir = OUT_ROOT / spec.slug
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
    out_dir = OUT_ROOT / spec.slug
    exported = []
    contact = []

    for action, rows in spec.boxes.items():
        source_direction_frames = {}
        for row_index, direction in enumerate(spec.source_directions):
            source_direction_frames[direction] = []
            for frame_index, box in enumerate(rows[row_index]):
                frame = frame_canvas(source.crop(box), spec)
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

    for frame_index, box in enumerate(spec.sleeping_boxes):
        frame = frame_canvas(sleeping_source.crop(box), spec)
        path = out_dir / "sleeping" / f"frame_{frame_index}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        frame.save(path)
        exported.append(path)
        contact.append(("sleeping", None, frame_index, frame))

    write_readme(spec, out_dir)
    make_contact_sheet(spec, out_dir, contact)
    print(f"{spec.slug}: exported={len(exported)} out={out_dir}")


def main():
    for spec in SPECS:
        process_spec(spec)


if __name__ == "__main__":
    main()
