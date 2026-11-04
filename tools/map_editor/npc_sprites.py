"""NPC artwork decoded from the game's existing species packs."""
from functools import lru_cache
import hashlib
import io
import json
from pathlib import Path
import re
import sys

from PIL import Image
from asset_paths import essentials_dir

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/room_editor'))
from generate_sprite_previews import parse_kind_names, parse_species_idents, load_sprite_pack, decode_frame

LEGACY = [('youngster','少年','trainer_YOUNGSTER'),('lass','少女','trainer_LASS'),
          ('gentleman','绅士','trainer_GENTLEMAN'),('beauty','女士','trainer_BEAUTY'),
          ('hiker','旅行者','trainer_HIKER')]
DEFAULT_SPRITE = 'pokemon-025'


@lru_cache(None)
def species():
    idents = parse_species_idents((ROOT / 'src/game/Species.cpp').read_text())
    names = dict(re.findall(r'const char\* (\w+) = "([^"]+)";',
                            (ROOT / 'src/core/UiStrings.h').read_text().split('namespace SpeciesName {',1)[1].split('}',1)[0]))
    result = {f'pokemon-{sid:03d}': (sid, names[ident]) for sid, ident in sorted(idents.items())
              if (ROOT / f'data/packs/dev/sprites/{sid:03d}.smonsp').is_file()}
    for npc in json.loads((ROOT / 'origin_asset/market_npc_species.json').read_text())['species']:
        if (ROOT / f'data/packs/dev/sprites/{npc["id"]:03d}.smonsp').is_file():
            result[f'pokemon-{npc["id"]:03d}'] = (npc['id'], npc['name'])
    return dict(sorted(result.items()))


def sprite_path(key):
    if key in species():
        return ROOT / f'data/packs/dev/sprites/{species()[key][0]:03d}.smonsp'
    filename = next((f for k,_,f in LEGACY if k == key), None)
    if filename is None:
        raise ValueError('未知 NPC 精灵')
    return essentials_dir() / 'Graphics/Characters' / (filename+'.png')


def sprite_ids():
    return set(species()) | {k for k,_,_ in LEGACY}


@lru_cache(None)
def artwork(key):
    path = sprite_path(key)
    payload = path.read_bytes()
    meta = dict(id=key, url=f'/art/npc-{key}.png', sha256=hashlib.sha256(payload).hexdigest())
    if key not in species():
        name = next(n for k,n,_ in LEGACY if k == key)
        meta.update(name=name, legacy=True, path=f'Graphics/Characters/{path.name}', scale=32,
                    actions={action: [[dict(x=f*32,y=row*48,width=32,height=48,groundOffsetY=18)
                                       for f in (range(4) if action=='walking' else [0])]
                                      for row in [0,1,3,2]] for action in ['idle','walking']})
        return meta, payload

    sid, name = species()[key]
    kind_names = parse_kind_names((ROOT / 'src/assets/PokemonSprites.h').read_text())
    pack_sid, frames, rle, palettes = load_sprite_pack(path, kind_names)
    if pack_sid != sid:
        raise ValueError(f'精灵资源编号不一致：{key}')
    selected = []
    actions = {}
    for action in ['idle','walking']:
        directions = []
        for direction in ['FRONT','LEFT','BACK','RIGHT']:
            def matches(d):
                return sorted((f for f in frames if re.search(rf'_{action.upper()}_{d}_\d+$', f['kindName'])),
                              key=lambda f: int(f['kindName'].rsplit('_',1)[1]))
            group = matches(direction)
            mirrored = direction=='RIGHT' and not group
            if mirrored:
                group = matches('LEFT')
            if not group:
                raise ValueError(f'精灵缺少 {action}/{direction} 动画：{key}')
            indices = []
            for frame in group:
                indices.append(len(selected))
                selected.append((frame, mirrored))
            directions.append(indices)
        actions[action] = directions
    cell_w = max(f['width'] for f,_ in selected)
    cell_h = max(f['height'] for f,_ in selected)
    atlas = Image.new('RGBA', (cell_w*8,cell_h*((len(selected)+7)//8)))
    records = []
    for i,(frame,mirrored) in enumerate(selected):
        im = decode_frame(frame,rle,palettes)
        if mirrored:
            im = im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        x,y = i%8*cell_w,i//8*cell_h
        atlas.paste(im,(x,y))
        records.append(dict(x=x,y=y,width=frame['width'],height=frame['height'],
                            visibleTop=(im.getbbox() or (0,0,0,0))[1],groundOffsetY=frame['groundOffsetY'],kind=frame['kindName'],mirrored=mirrored))
    output = io.BytesIO()
    atlas.save(output,format='PNG')
    png = output.getvalue()
    meta.update(name=f'{sid:03d} · {name}', speciesId=sid, path=path.relative_to(ROOT).as_posix(),
                scale=26, atlasSha256=hashlib.sha256(png).hexdigest(),
                actions={action: [[records[i] for i in group] for group in directions]
                         for action,directions in actions.items()})
    return meta,png


def sprite_catalog(include_legacy=False):
    keys = list(species()) + ([k for k,_,_ in LEGACY] if include_legacy else [])
    return [artwork(key)[0] for key in keys]


def sprite_png(key):
    return artwork(key)[1]
