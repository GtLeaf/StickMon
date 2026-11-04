#!/usr/bin/env python3
"""Import the pinned PMD market actors with their original ground markers.

This is an explicit offline import, not part of a firmware build. Only the
four NPCs in market_npc_species.json are touched; existing pet art is retained.
"""
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'origin_asset/source_sheets/spritecollab-market'
PROCESSED = ROOT / 'origin_asset/processed'
CATALOG = ROOT / 'origin_asset/market_npc_species.json'
METADATA = ROOT / 'origin_asset/pmd_shadow_metadata.json'
DIRECTIONS = dict(front=0, down_left=7, left=6, up_left=5, back=4)


def import_frames():
    provenance = json.loads((SOURCE / 'source.json').read_text())
    for name, expected in provenance['sha256'].items():
        if hashlib.sha256((SOURCE / name).read_bytes()).hexdigest() != expected:
            raise ValueError('Changed source: ' + name)
    metadata = json.loads(METADATA.read_text())
    if metadata['revision'] != provenance['revision']:
        raise ValueError('Shadow and body reference revisions differ')
    count = 0
    for spec in json.loads(CATALOG.read_text())['species']:
        directory = SOURCE / f'{spec["id"]:04d}'
        root = ET.parse(directory / 'AnimData.xml').getroot()
        anims = {a.findtext('Name'): a for a in root.findall('./Anims/Anim')}
        color = [(0, 255, 0), (255, 0, 0), (0, 0, 255)][int(root.findtext('ShadowSize'))]
        size = spec['canvas']
        anchor = (size // 2, size - 12)
        for action, source_action, indices in [('idle', 'Idle', [0]),
                                                ('walking', 'Walk', [0, 1, 2, 3]),
                                                ('sleeping', 'Sleep', [0, 1])]:
            name = source_action
            while anims[name].findtext('CopyOf'):
                name = anims[name].findtext('CopyOf')
            w, h = (int(anims[name].findtext(k)) for k in ('FrameWidth', 'FrameHeight'))
            bodies = Image.open(directory / f'{name}-Anim.png').convert('RGBA')
            shadows = Image.open(directory / f'{name}-Shadow.png').convert('RGBA')
            directions = dict(front=0) if action == 'sleeping' else DIRECTIONS
            for direction, row in directions.items():
                for index in indices:
                    box = (index*w, row*h, (index+1)*w, (row+1)*h)
                    body, shadow = bodies.crop(box), shadows.crop(box)
                    centers, region = [], []
                    for y in range(h):
                        for x in range(w):
                            p = shadow.getpixel((x, y))
                            if p[3] and p[:3] == (255, 255, 255):
                                centers.append((x, y))
                            if p[3] and p[:3] in (color, (255, 255, 255)):
                                region.append((x, y))
                    if not centers or not region or not body.getbbox():
                        raise ValueError(f'Missing body/ground: {spec["id"]} {name}/{row}/{index}')
                    cx = sum(p[0] for p in centers) / len(centers)
                    cy = sum(p[1] for p in centers) / len(centers)
                    rx = max(p[0] for p in region) - min(p[0] for p in region) + 1
                    ry = max(p[1] for p in region) - min(p[1] for p in region) + 1
                    offset = (round(anchor[0]-cx*2), round(anchor[1]-cy*2))
                    bounds = body.getbbox()
                    if (bounds[0]*2+offset[0] < 0 or bounds[1]*2+offset[1] < 0 or
                            bounds[2]*2+offset[0] > size or bounds[3]*2+offset[1] > size):
                        raise ValueError(f'Body clipped: {spec["id"]} {name}/{row}/{index}')
                    canvas = Image.new('RGBA', (size, size))
                    canvas.paste(body.resize((w*2, h*2), Image.Resampling.NEAREST), offset)
                    stem = f'frame_{index}' if action == 'sleeping' else f'{direction}_{index}'
                    path = PROCESSED / f'{spec["id"]:03d}_{spec["slug"]}' / action / (stem+'.png')
                    path.parent.mkdir(parents=True, exist_ok=True)
                    canvas.save(path)
                    metadata['frames'][path.relative_to(PROCESSED).as_posix()] = dict(
                        shadow=[*anchor, rx, ry], sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                        iou=1.0, reference=[name, row, index, False])
                    count += 1
    METADATA.write_text(json.dumps(metadata, indent=2)+'\n')
    print(f'Imported {count} market NPC frames with registered ground anchors')


if __name__ == '__main__':
    import_frames()
