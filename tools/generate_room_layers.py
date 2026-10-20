#!/usr/bin/env python3
"""Export reusable RGBA furniture and a room with independent placements."""
import argparse
import hashlib
from pathlib import Path
import json
import re
import struct
import zlib

import prepare_room_background as room
from compose_room import (load_prepared_items, image_with_opacity, item_shadow_layers,
                          item_sort_key, render_base)
from PIL import Image, ImageChops

ROOT = room.ROOT
DEFAULT_OUTPUT = ROOT / 'origin_asset/generated/room/layered'
LIBRARY = ROOT / 'tools/room_editor/generated/furniture_library/images'
STYLES = ('forest_leaf', 'starry_night', 'cloud')
FURNITURE_MAGIC = 0x4e525546
FURNITURE_HEADER = '<IHHHHIII'
PLACEMENT = '<24s32shhHHHhhHHII'


def name_bytes(name, size):
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', name) or len(name) >= size:
        raise ValueError(f'Invalid resource ID: {name}')
    return name.encode().ljust(size, b'\0')


def write_furniture(path, image):
    rgba = image.convert('RGBA').tobytes()
    colors = room.rgb565_bytes(image.convert('RGB'))
    raw = b''.join(colors[i*2:i*2+2] + rgba[i*4+3:i*4+4] for i in range(image.width*image.height))
    compressed = room.raw_deflate(raw)
    data = struct.pack(FURNITURE_HEADER, FURNITURE_MAGIC, 1, image.width,
                       image.height, 0, len(raw), len(compressed), zlib.crc32(raw)) + compressed
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)
    return path


def geometry(layout, day, destination):
    width, height = room.room_size(layout)
    walk = room.load_walk_polygon(layout, width, height)
    food = room.food_position(layout)
    bed, bx, by = room.bed_region(layout, width, height, walk)
    door, inside, outside = room.doorway_region(layout, width, height, walk)
    anchors = room.behavior_anchors_for_layout(layout, width, height, walk)
    # Reuse the stable RGB565/geometry encoder; v6 appends independent placements.
    room.write_room_pack(width, height, day.width, day.height,
        max(0, (room.DISPLAY_H - height) // 2), room.rgb565_bytes(day),
        room.raw_deflate(room.rgb565_bytes(day)), [], [], walk, *food,
        bed, bx, by, door, inside, outside, anchors, output=destination)
    data = destination.read_bytes()
    header = list(struct.unpack_from(room.ROOM_PACK_HEADER_FORMAT, data))
    metadata = data[80 + header[6]:]
    raw = room.rgb565_bytes(day)
    compressed = room.raw_deflate(raw)
    header[1], header[5], header[6] = 6, len(raw), len(compressed)
    return struct.pack(room.ROOM_PACK_HEADER_FORMAT, *header) + compressed + metadata


def generate(layout_path=room.DEFAULT_LAYOUT, output=DEFAULT_OUTPUT, room_id='standard',
             furniture_dir=room.DEFAULT_FURNITURE_DIR, include_variants=True):
    name_bytes(room_id, 32)
    output, layout_path = Path(output), Path(layout_path)
    manifest_path = output / f'{room_id}.json'
    previous = json.loads(manifest_path.read_text()).get('resources', []) if manifest_path.exists() else []
    layout = room.layout_without_preview_sprites(room.read_layout(layout_path))
    art = room.scale_room_layout_for_art(layout, room.ROOM_ART_SCALE)
    size = room.room_size(art)
    if size[0] > 480 or size[1] > 640:
        raise ValueError('Room exceeds supported art dimensions')
    bases, prepared, shadows = {}, {}, {}
    for mode in ('day', 'night'):
        base = room.background_path(layout_path, layout, mode)
        bases[mode] = render_base(art, base, mode, layout_path.parent).convert('RGB')
        prepared[mode] = load_prepared_items(art, Path(furniture_dir), mode)
        shadows[mode] = {i['id']: s for i, s in item_shadow_layers(
            size, art, art.get('night', {}), prepared[mode], mode)}
    by_id = {mode: {i['id']: (i, src, opacity) for i, src, opacity in prepared[mode]}
             for mode in prepared}
    placements, assets, slots = [], [], []
    for item in sorted(art['furniture'], key=item_sort_key):
        iid = item['id']
        modes = [m for m in prepared if iid in by_id[m]]
        if not modes:
            continue
        slot = 'rug' if item.get('name') == 'carpet' else 'bed' if item.get('name') == 'bed' else iid
        _source_item, source, opacity = by_id[modes[0]][iid]
        image = image_with_opacity(source, opacity)
        digest = hashlib.sha256(struct.pack('<HH', *image.size) + image.tobytes()).hexdigest()[:16]
        asset = f'tex_{digest}'
        assets.append(write_furniture(output / 'furniture' / f'{asset}.smonfurn', image))
        # A placement is mode-specific when an author supplied different images.
        for mode in modes[1:]:
            other = by_id[mode][iid]
            if image_with_opacity(other[1], other[2]).tobytes() != image.tobytes():
                raise ValueError('Use separate day/night placements for different textures')
        alpha = {mode: shadows[mode].get(iid, Image.new('RGBA', size)).getchannel('A')
                 for mode in ('day', 'night')}
        bounds = ImageChops.lighter(alpha['day'], alpha['night']).getbbox()
        sx, sy, sw, sh = (bounds[0], bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]) if bounds else (0, 0, 0, 0)
        day_shadow = room.raw_deflate(alpha['day'].crop(bounds).tobytes()) if bounds else b''
        night_raw = alpha['night'].crop(bounds).tobytes() if bounds else b''
        night_shadow = room.raw_deflate(night_raw) if bounds and night_raw != alpha['day'].crop(bounds).tobytes() else b''
        flags = (1 if 'day' in modes else 0) | (2 if 'night' in modes else 0) | (4 if item.get('receivesShadow', True) else 0)
        record = struct.pack(PLACEMENT, name_bytes(slot, 24), name_bytes(asset, 32),
            round(item.get('x', 0)), round(item.get('y', 0)), image.width, image.height,
            flags, sx, sy, sw, sh, len(day_shadow), len(night_shadow))
        placements.append(record + day_shadow + night_shadow)
        slots.append({'slot': slot, 'asset': asset, 'x': round(item.get('x', 0)),
                      'y': round(item.get('y', 0)), 'width': image.width, 'height': image.height})
        if include_variants and slot in ('bed', 'rug'):
            for style in STYLES:
                variant = Image.open(LIBRARY / f'furniture_{slot}_{style}.png').convert('RGBA')
                # Placements choose the target size; textures retain native pixels.
                assets.append(write_furniture(output / 'furniture' / f'{slot}_{style}.smonfurn', variant))
    if len(placements) > 32:
        raise ValueError('Room may contain at most 32 placements')
    destination = output / 'rooms' / f'{room_id}.smonroom'
    destination.parent.mkdir(parents=True, exist_ok=True)
    data = geometry(layout, bases['day'], destination)
    night_base = room.raw_deflate(room.rgb565_bytes(bases['night'])) if bases['night'].tobytes() != bases['day'].tobytes() else b''
    data += struct.pack('<IHH', len(night_base), len(placements), 0) + night_base + b''.join(placements)
    destination.write_bytes(data)
    manifest_path.write_text(json.dumps({'version': 1, 'room': room_id,
        'source': str(layout_path.relative_to(ROOT)) if layout_path.is_relative_to(ROOT) else layout_path.name,
        'placements': slots, 'resources': [p.relative_to(output).as_posix() for p in [destination, *assets]]}, indent=2)+'\n')
    # Remove only this room's obsolete exports, preserving other rooms' references.
    referenced = set()
    for manifest in output.glob('*.json'):
        referenced.update(json.loads(manifest.read_text()).get('resources', []))
    for relative in set(previous) - referenced:
        stale = output / relative
        if stale.parent in (output / 'furniture', output / 'rooms') and stale.suffix in ('.smonfurn', '.smonroom'):
            stale.unlink(missing_ok=True)
    return [destination, *assets]


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--layout', type=Path, default=room.DEFAULT_LAYOUT)
    p.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    p.add_argument('--id', default='standard')
    p.add_argument('--furniture-dir', type=Path, default=room.DEFAULT_FURNITURE_DIR)
    p.add_argument('--no-variants', action='store_true')
    args = p.parse_args()
    paths = generate(args.layout, args.output, args.id, args.furniture_dir, not args.no_variants)
    for path in paths:
        print(f'{path.name}: {path.stat().st_size} bytes')
