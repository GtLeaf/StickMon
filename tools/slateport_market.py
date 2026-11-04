"""Original Emerald market foregrounds, reconstructed from indexed source data."""
from functools import lru_cache
from pathlib import Path
import hashlib
import struct
from PIL import Image

SOURCE = Path(__file__).resolve().parents[1] / 'origin_asset/source_maps/slateport_emerald'
SHEET = 'Slateport Emerald'
BALLOON = 836
# Each metatile is a 16px GBA tile, enlarged by nearest-neighbour to 32px.
OBJECTS = [
    ('market-pot', '空陶罐', [[524]]),
    ('market-green-basket', '青绿果篮', [[539]]),
    ('market-orange-basket', '橙黄果篮', [[555]]),
    ('market-red-basket', '红色果篮', [[547]]),
    ('market-orange-flowers', '橙色花束', [[531]]),
    ('market-white-flowers', '白花陈列篮', [[523]]),
    ('market-crate', '木货箱', [[814]]),
    ('market-label-crate', '标签货箱', [[815]]),
    ('market-board', '集市告示板', [[870]]),
    ('market-balloon', '彩色气球 · 摆动', [[836]]),
    ('market-blue-canopy', '绿宝石大蓝棚', [[640,643,526,644,642],[648,651,558,652,650],[611,0,0,0,612]]),
    ('market-umbrella-blue-full', '蓝白遮阳伞', [[738,739],[746,747],[754,0]]),
    ('market-umbrella-orange-full', '橙白遮阳伞', [[740,741],[748,749],[756,0]]),
    ('market-umbrella-green-full', '绿白遮阳伞', [[742,743],[750,751],[758,0]]),
    ('market-deckchair-green', '绿条纹躺椅', [[736],[744],[752]]),
    ('market-deckchair-blue', '蓝条纹躺椅', [[737],[745],[753]]),
]

# Read compatibility only; not offered as new stamps.
LEGACY_OBJECTS = [
    ('market-umbrella-blue', '蓝白遮阳伞（旧版缺片）', [[738,739],[754,0]]),
    ('market-umbrella-orange', '橙白遮阳伞（旧版缺片）', [[740,741],[756,0]]),
    ('market-umbrella-green', '绿白遮阳伞（旧版缺片）', [[742,743],[758,0]]),
]


@lru_cache(None)
def source_hash():
    digest = hashlib.sha256()
    for path in sorted(SOURCE.rglob('*')):
        if path.is_file() and path.suffix in ('.png','.bin','.pal','.json'):
            digest.update(path.relative_to(SOURCE).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


@lru_cache(None)
def tile_image(metatile, frame=0):
    """Use the original top-layer tile refs, flips and palette (no repainting)."""
    refs = struct.unpack_from('<8H', (SOURCE/'metatiles.bin').read_bytes(), (metatile-512)*16)
    with Image.open(SOURCE/'tiles.png') as original:
        sheet = original.copy()
    result = Image.new('RGBA', (16,16))
    for part, ref in enumerate(refs[4:]):
        index, palette = ref & 1023, ref >> 12
        if index == 0:
            continue
        if not 512 <= index < 1024:
            raise ValueError(f'Unsupported primary tile in market foreground: {index}')
        if metatile == BALLOON and 736 <= index < 740:
            i = index-736
            with Image.open(SOURCE/f'anim/balloons/{frame%4}.png') as image:
                tile = image.crop((i%2*8,i//2*8,i%2*8+8,i//2*8+8))
        else:
            i = index-512
            tile = sheet.crop((i%16*8,i//16*8,i%16*8+8,i//16*8+8))
        colors = [int(v) for line in (SOURCE/f'palettes/{palette:02}.pal').read_text().splitlines()[3:] for v in line.split()]
        assert len(colors) == 48
        tile.putpalette(colors+[0]*720)
        tile.info['transparency'] = 0
        tile = tile.convert('RGBA')
        if ref & 1024: tile = tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        if ref & 2048: tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        result.alpha_composite(tile, (part%2*8,part//2*8))
    return result.resize((32,32), Image.Resampling.NEAREST)
