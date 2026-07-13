#!/usr/bin/env python3

"""Reusable tile placement rules for generated RPG Maker XP map previews."""

from math import ceil, floor


FOREST_BODY_IDS = frozenset(range(800, 804)) | frozenset(range(808, 812)) | frozenset(range(816, 820))
FOREST_CROWN_IDS = (804, 805)
ROAD_TILE_IDS = frozenset(range(537, 559))
HIGH_GRASS_TILE_ID = 391
DEEP_SEA_TILE_ID = 144
DEEP_SEA_EDGE_TILE_ID = 168
SEA_SHORE_TILE_ID = 72

LIGHTHOUSE_TILE_ROWS = (
    (3941, 3942, 3943),
    (3949, 3950, 3951),
    (3957, 3958, 3959),
    (3965, 3966, 3967),
    (3973, 3974, 3975),
    (3981, 3982, 3983),
)
LIGHTHOUSE_IDS = frozenset(tile_id for row in LIGHTHOUSE_TILE_ROWS for tile_id in row)

FENCE_TOP_LEFT = 1662
FENCE_LEFT = 1665
FENCE_TOP = 1681
FENCE_TOP_RIGHT = 1682

FOREST_FENCE_RULE = {
    "source": "Graphics/Tilesets/Outside.png",
    "ground_layer": 0,
    "fence_layer": 1,
    "crown_layer": 2,
    "forest_body_ids": tuple(sorted(FOREST_BODY_IDS)),
    "forest_crown_ids": FOREST_CROWN_IDS,
    "fence_ids": (FENCE_TOP_LEFT, FENCE_LEFT, FENCE_TOP, FENCE_TOP_RIGHT),
}

LANDMARK_RULES = {
    "source": "Graphics/Tilesets/Outside.png",
    "high_grass": {
        "tile_id": HIGH_GRASS_TILE_ID,
        "layer": 1,
        "reference": "Map028 Natural Park",
    },
    "lighthouse": {
        "tile_rows": LIGHTHOUSE_TILE_ROWS,
        "layer": 1,
        "size": (3, 6),
    },
    "left_coast": {
        "tile_ids": (DEEP_SEA_TILE_ID, DEEP_SEA_EDGE_TILE_ID, SEA_SHORE_TILE_ID),
        "reference": "Map039 Route 4",
    },
}


def _is_half_grid(value):
    doubled = round(value * 2)
    return abs(value * 2 - doubled) < 1e-6 and doubled % 2 != 0


def _route_geometry(routes, map_width, map_height):
    cells = set()
    connections = set()
    for route in routes:
        if len(route) < 2:
            raise ValueError("two-tile road route needs at least two control points")
        for first, second in zip(route, route[1:]):
            x1, y1 = first
            x2, y2 = second
            if x1 == x2:
                if not _is_half_grid(x1):
                    raise ValueError(f"vertical road centerline must use a half-grid x: {x1}")
                columns = (floor(x1), ceil(x1))
                rows = range(ceil(min(y1, y2)), floor(max(y1, y2)) + 1)
                cells.update((x, y) for x in columns for y in rows)
            elif y1 == y2:
                if not _is_half_grid(y1):
                    raise ValueError(f"horizontal road centerline must use a half-grid y: {y1}")
                rows = (floor(y1), ceil(y1))
                columns = range(ceil(min(x1, x2)), floor(max(x1, x2)) + 1)
                cells.update((x, y) for x in columns for y in rows)
            else:
                raise ValueError(f"road segment must be axis aligned: {first}->{second}")

        for x, y in (route[0], route[-1]):
            if abs(x + 0.5) < 1e-6:
                connections.update({(-1, floor(y)), (-1, ceil(y))})
            elif abs(x - (map_width - 0.5)) < 1e-6:
                connections.update({(map_width, floor(y)), (map_width, ceil(y))})
            if abs(y + 0.5) < 1e-6:
                connections.update({(floor(x), -1), (ceil(x), -1)})
            elif abs(y - (map_height - 0.5)) < 1e-6:
                connections.update({(floor(x), map_height), (ceil(x), map_height)})

    visible = {
        (x, y)
        for x, y in cells
        if 0 <= x < map_width and 0 <= y < map_height
    }
    return visible, visible | connections


def two_tile_road_cells(routes, map_width, map_height):
    cells, _connections = _route_geometry(routes, map_width, map_height)
    return cells


def terrain_road_tile(cells, x, y):
    north = (x, y - 1) in cells
    east = (x + 1, y) in cells
    south = (x, y + 1) in cells
    west = (x - 1, y) in cells

    if north and east and south and west:
        if (x - 1, y - 1) not in cells:
            return 558
        if (x + 1, y - 1) not in cells:
            return 556
        if (x - 1, y + 1) not in cells:
            return 542
        if (x + 1, y + 1) not in cells:
            return 540
        return 546
    if not north and not west:
        return 537
    if not north and not east:
        return 539
    if not south and not west:
        return 553
    if not south and not east:
        return 555
    if not north:
        return 538
    if not south:
        return 554
    if not west:
        return 545
    if not east:
        return 547
    return 546


def build_two_tile_road(routes, map_width, map_height):
    """Rasterize free-moving route centerlines into an exact two-tile corridor."""

    cells, connected_cells = _route_geometry(routes, map_width, map_height)
    return {point: terrain_road_tile(connected_cells, *point) for point in cells}


def _set_tile(layers, map_width, map_height, layer, x, y, tile_id):
    if not 0 <= layer < len(layers):
        raise ValueError(f"invalid layer: {layer}")
    if not 0 <= x < map_width or not 0 <= y < map_height:
        raise ValueError(f"tile outside map: layer={layer} point=({x},{y})")
    layers[layer][y * map_width + x] = tile_id


def stamp_high_grass(
    layers,
    map_width,
    map_height,
    left,
    top,
    width,
    height,
    blocked_cells=(),
):
    """Place Map028-style high grass on layer 1 without covering routes."""

    if len(layers) != 3:
        raise ValueError("high grass composition requires exactly three layers")
    if width < 1 or height < 1:
        raise ValueError("high grass area must be non-empty")
    if left < 0 or top < 0 or left + width > map_width or top + height > map_height:
        raise ValueError("high grass area does not fit inside map")

    blocked = set(blocked_cells)
    positions = set()
    for y in range(top, top + height):
        for x in range(left, left + width):
            if (x, y) in blocked:
                continue
            _set_tile(layers, map_width, map_height, 1, x, y, HIGH_GRASS_TILE_ID)
            positions.add((x, y))
    return positions


def stamp_lighthouse(layers, map_width, map_height, left, top):
    """Place the complete 3x6 lighthouse supplied by Outside.png on layer 1."""

    if len(layers) != 3:
        raise ValueError("lighthouse composition requires exactly three layers")
    lighthouse_width = len(LIGHTHOUSE_TILE_ROWS[0])
    lighthouse_height = len(LIGHTHOUSE_TILE_ROWS)
    if (
        left < 0
        or top < 0
        or left + lighthouse_width > map_width
        or top + lighthouse_height > map_height
    ):
        raise ValueError("lighthouse does not fit inside map")

    positions = set()
    for row, tile_row in enumerate(LIGHTHOUSE_TILE_ROWS):
        for column, tile_id in enumerate(tile_row):
            x = left + column
            y = top + row
            _set_tile(layers, map_width, map_height, 1, x, y, tile_id)
            positions.add((x, y))
    return {
        "bounds": (
            left,
            top,
            left + lighthouse_width - 1,
            top + lighthouse_height - 1,
        ),
        "positions": positions,
    }


def stamp_left_coast(layers, map_width, map_height, coast_x, top=0, bottom=None):
    """Place the deep-sea to shoreline transition used on Map039's left edge."""

    if len(layers) != 3:
        raise ValueError("coast composition requires exactly three layers")
    if bottom is None:
        bottom = map_height - 1
    if coast_x < 3 or coast_x > map_width or top < 0 or bottom >= map_height or top > bottom:
        raise ValueError("left coast does not fit inside map")

    positions = set()
    for y in range(top, bottom + 1):
        for x in range(coast_x - 2):
            _set_tile(layers, map_width, map_height, 0, x, y, DEEP_SEA_TILE_ID)
            positions.add((x, y))
        _set_tile(
            layers,
            map_width,
            map_height,
            0,
            coast_x - 2,
            y,
            DEEP_SEA_EDGE_TILE_ID,
        )
        _set_tile(
            layers,
            map_width,
            map_height,
            0,
            coast_x - 1,
            y,
            SEA_SHORE_TILE_ID,
        )
        positions.update({(coast_x - 2, y), (coast_x - 1, y)})
    return positions


def stamp_forest_fence(layers, map_width, map_height, forest_x, top_y, forest_width, bottom_y=None):
    """Stamp the three-layer forest/fence composition used by Map039 Route 4.

    The forest starts one tile to the right of the fence corner and continues to
    the right edge of the stamp. Crown tips are deliberately placed above the
    fence on layer 2; the body remains on layer 0.
    """

    if len(layers) != 3:
        raise ValueError("forest fence composition requires exactly three layers")
    if forest_width < 4 or forest_width % 2 != 0:
        raise ValueError("forest_width must be an even value of at least four")
    if forest_x < 1 or forest_x + forest_width > map_width:
        raise ValueError("forest fence stamp does not fit horizontally")
    if bottom_y is None:
        bottom_y = map_height - 1
    body_height = bottom_y - top_y
    if (
        top_y < 0
        or bottom_y >= map_height
        or body_height < 6
        or body_height % 2 != 0
    ):
        raise ValueError(
            "forest fence needs one crown row and an even body height of at least six rows"
        )

    forest_right = forest_x + forest_width - 1
    fence_right = forest_right - 1

    _set_tile(layers, map_width, map_height, 1, forest_x - 1, top_y, FENCE_TOP_LEFT)
    for x in range(forest_x, fence_right):
        _set_tile(layers, map_width, map_height, 1, x, top_y, FENCE_TOP)
    _set_tile(layers, map_width, map_height, 1, fence_right, top_y, FENCE_TOP_RIGHT)
    for y in range(top_y + 1, bottom_y + 1):
        _set_tile(layers, map_width, map_height, 1, forest_x - 1, y, FENCE_LEFT)

    for offset in range(forest_width):
        x = forest_x + offset
        _set_tile(
            layers,
            map_width,
            map_height,
            2,
            x,
            top_y,
            FOREST_CROWN_IDS[offset % len(FOREST_CROWN_IDS)],
        )

        first_body = 810 if offset % 2 == 0 else 811
        _set_tile(layers, map_width, map_height, 0, x, top_y + 1, first_body)

        # Map039 repeats complete top/body pairs before the root row. Keeping an
        # even body height guarantees that roots never attach directly to tops.
        for y in range(top_y + 2, bottom_y):
            row_index = y - (top_y + 2)
            if row_index % 2 == 0:
                tile_id = 802 if offset == 0 else (801 if offset % 2 else 800)
            else:
                tile_id = 810 if offset == 0 else (809 if offset % 2 else 808)
            _set_tile(layers, map_width, map_height, 0, x, y, tile_id)

        _set_tile(
            layers,
            map_width,
            map_height,
            0,
            x,
            bottom_y,
            818 if offset % 2 == 0 else 819,
        )

    return {
        "forest_bounds": (forest_x, top_y + 1, forest_right, bottom_y),
        "fence_bounds": (forest_x - 1, top_y, fence_right, bottom_y),
        "crown_bounds": (forest_x, top_y, forest_right, top_y),
    }
