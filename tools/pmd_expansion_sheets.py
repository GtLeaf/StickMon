"""Read the pinned expansion sheets in the same ground space as the market NPCs."""
from dataclasses import dataclass
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image

SOURCE = Path(__file__).resolve().parents[1] / 'origin_asset/source_sheets'
EXPANSION = SOURCE / 'spritecollab-expansion'
MARKET = SOURCE / 'spritecollab-market'
MANIFEST = json.loads((EXPANSION / 'source.json').read_text())
DIRECTIONS = dict(front=0, down_left=7, left=6, up_left=5, back=4,
                  up_right=3, right=2, down_right=1)
ACTIONS = {'idle': 'Idle', 'walking': 'Walk', 'sleeping': 'Sleep'}


@dataclass(frozen=True)
class Sheet:
    name: str
    width: int
    height: int
    durations: tuple[int, ...]


def verify_sources():
    market = json.loads((MARKET / 'source.json').read_text())
    if market['revision'] != MANIFEST['spriteCollabRevision']:
        raise ValueError('Delibird and expansion sheet revisions differ')
    for root, hashes in ((EXPANSION, MANIFEST['sha256']),
                         (MARKET, {p: h for p, h in market['sha256'].items()
                                   if p.startswith('0225/')})):
        for name, expected in hashes.items():
            if hashlib.sha256((root / name).read_bytes()).hexdigest() != expected:
                raise ValueError(f'Changed sprite source: {root / name}')


def portrait(species_id, side):
    return EXPANSION / f'{species_id:04d}' / f'{side}.png'


def source_dir(species_id):
    return (MARKET if species_id == 225 else EXPANSION) / f'{species_id:04d}'


@lru_cache(maxsize=None)
def sheet_metadata(species_id):
    root = ET.parse(source_dir(species_id) / 'AnimData.xml').getroot()
    animations = {anim.findtext('Name'): anim for anim in root.findall('./Anims/Anim')}
    sheets = {}
    for action, name in ACTIONS.items():
        while animations[name].findtext('CopyOf'):
            name = animations[name].findtext('CopyOf')
        anim = animations[name]
        sheets[action] = Sheet(name, int(anim.findtext('FrameWidth')),
                               int(anim.findtext('FrameHeight')),
                               tuple(int(d.text) for d in anim.findall('./Durations/Duration')))
    shadow_color = ((0, 255, 0), (255, 0, 0), (0, 0, 255))[int(root.findtext('ShadowSize'))]
    return sheets, shadow_color


def registered_frames(species_id):
    sheets, shadow_color = sheet_metadata(species_id)
    directory = source_dir(species_id)
    frames = {}
    for action, sheet in sheets.items():
        bodies = Image.open(directory / f'{sheet.name}-Anim.png').convert('RGBA')
        shadows = Image.open(directory / f'{sheet.name}-Shadow.png').convert('RGBA')
        width, height = sheet.width, sheet.height
        rows = bodies.height // height
        if (bodies.size != shadows.size or bodies.width != len(sheet.durations) * width or
                bodies.height % height or rows not in (1, len(DIRECTIONS))):
            raise ValueError(f'Unexpected dimensions for {species_id} {sheet.name}: {bodies.size}')
        directions = {'front': 0} if action == 'sleeping' else DIRECTIONS
        for direction, row in directions.items():
            y = row * height if rows > 1 else 0
            for index in range(len(sheet.durations)):
                x = index * width
                box = (x, y, x + width, y + height)
                body, shadow = bodies.crop(box), shadows.crop(box)
                white = (255, 255, 255)
                centers, region = [], []
                for sy in range(height):
                    for sx in range(width):
                        r, g, b, a = shadow.getpixel((sx, sy))
                        if a and (r, g, b) in (shadow_color, white):
                            region.append((sx, sy))
                            if (r, g, b) == white:
                                centers.append((sx, sy))
                bounds = body.getbbox()
                if not centers or not bounds:
                    raise ValueError(f'Missing body or ground for {species_id} {action}/{direction}/{index}')
                cx = round(2 * sum(p[0] for p in centers) / len(centers))
                cy = round(2 * sum(p[1] for p in centers) / len(centers))
                rx = max(p[0] for p in region) - min(p[0] for p in region) + 1
                ry = max(p[1] for p in region) - min(p[1] for p in region) + 1
                left, top, right, bottom = bounds
                frames[action, direction, index] = (body.crop(bounds), left * 2 - cx, top * 2 - cy,
                                                    right * 2 - cx, bottom * 2 - cy, rx, ry)

    # Center horizontally and retain a common ground anchor across all actions.
    half_width = max(32, max(max(-f[1], f[3]) for f in frames.values()))
    ground_y = max(52, max(-f[2] for f in frames.values()))
    below_ground = max(12, max(f[4] for f in frames.values()))
    size = (half_width * 2, ground_y + below_ground)
    for key, (body, left, top, _, _, rx, ry) in frames.items():
        frame = Image.new('RGBA', size)
        frame.paste(body.resize((body.width * 2, body.height * 2), Image.Resampling.NEAREST),
                    (half_width + left, ground_y + top))
        yield key, frame, (half_width, ground_y, rx, ry)
