"""Render seeded bridge-cave candidates using original tiles only.

The bridge reference is the user's original Meteor Falls screenshot. This
command only writes review artifacts to .stick-local; it never updates packs.
"""

import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import shutil

from PIL import Image, ImageDraw, ImageFont

from generate_explore_map import autotile_variant, regular_tile
from generate_map_rule_preview import TILESET, load_autotiles, WATERFALL_AUTOTILE_NAMES
from valley_bridge_layouts import generate, fingerprint


ROOT = Path(__file__).resolve().parents[1]
INK, TEXT, MUTED = '#15242a', '#edf3e6', '#bdcfbf'
TILE = 32


def font(size):
    for name in ('/System/Library/Fonts/STHeiti Medium.ttc',
                 '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'):
        if Path(name).exists():
            return ImageFont.truetype(name, size)
    return ImageFont.load_default()


class Renderer:
    def __init__(self, reference):
        self.sheets = {name: Image.open(TILESET.parent / name).convert('RGBA')
                       for name in ('Caves.png', 'Outside.png')}
        self.autotiles = load_autotiles(WATERFALL_AUTOTILE_NAMES)
        image = Image.open(reference).convert('RGBA')
        self.log = image.crop((304, 288, 336, 320))

    @lru_cache(maxsize=2048)
    def tile(self, tile, frame, quarters=()):
        if 8000 <= tile < 8256:
            result = Image.new('RGBA', (32, 32))
            source = self.autotiles[0]
            for k, (q, flip_y) in enumerate(quarters):
                x, y = frame % 8 * 96 + q % 6 * 16, q // 6 * 16
                part = source.crop((x, y, x + 16, y + 16))
                if flip_y:
                    part = part.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                result.alpha_composite(part, (k % 2 * 16, k // 2 * 16))
            return result
        if 6000 <= tile < 8000:
            return regular_tile(self.sheets['Caves.png'], tile - 6000)
        if tile >= 10000:
            return regular_tile(self.sheets['Outside.png'], tile - 10000)
        assert 48 <= tile < 384, tile
        return autotile_variant(self.autotiles[tile // 48 - 1], tile % 48, frame)

    def render(self, room, frame=0, routes=False):
        image = Image.new('RGBA', (512, 384), (0, 0, 0, 255))
        for layer in room['layers']:
            for i, tile in enumerate(layer):
                if not tile:
                    continue
                quarters = room['water_quarters'].get(tile, ())
                image.alpha_composite(self.tile(tile, frame, tuple(map(tuple, quarters))),
                                      (i % 16 * TILE, i // 16 * TILE))
        for x, y in room['bridge']:
            image.alpha_composite(self.log, (x * TILE, y * TILE))
        image = image.convert('RGB')
        if routes:
            draw = ImageDraw.Draw(image)
            for path in room['paths']:
                color = '#8de5ed' if path['level'] == 1 else '#ffca71'
                points = [(x * TILE + 16, y * TILE + 16) for x, y in path['cells']]
                draw.line(points, fill=INK, width=7, joint='curve')
                draw.line(points, fill=color, width=3, joint='curve')
                for point in (points[0], points[-1]):
                    x, y = point
                    draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)
        return image


def triptych(renderer, data, frame=0, routes=False):
    page = Image.new('RGB', (1600, 492), INK)
    draw = ImageDraw.Draw(page)
    draw.text((16, 8), f"{data['name']} · 种子 {data['seed']}", font=font(24), fill=TEXT)
    titles = {'B': 'B · 盘升岩窟', 'A': 'A · 瀑潭桥厅', 'C': 'C · 落阶洞厅'}
    for i, key in enumerate('BAC'):
        x = 16 + i * 528
        draw.text((x, 44), titles[key], font=font(19), fill=MUTED)
        page.paste(renderer.render(data['rooms'][key], frame, routes), (x, 76))
    draw.text((16, 465), 'A 下层 → B 登高 → A 过桥 → C 下行；蓝色为上层，橙色为下层 / 楼梯',
              font=font(16), fill=MUTED)
    return page


def contact(renderer, samples, output, routes=False):
    page = Image.new('RGB', (1600, 86 + len(samples) * 506), INK)
    draw = ImageDraw.Draw(page)
    draw.text((16, 10), '峡谷桥洞 · 四套骨架与种子变化', font=font(30), fill=TEXT)
    draw.text((16, 52), '原素材拼接；保留两岸朝向、成对洞口与必经楼梯。设计预览，尚未接入运行时。',
              font=font(18), fill=MUTED)
    for i, data in enumerate(samples):
        page.paste(triptych(renderer, data, routes=routes), (0, 86 + i * 506))
    page.save(output)


def a_grid(renderer, samples, output, title,
           subtitle='桥位、河宽、落差与潭岸有变化；入洞后仍必须先去 B 登高。'):
    page = Image.new('RGB', (1080, 976), INK)
    draw = ImageDraw.Draw(page)
    draw.text((20, 12), title, font=font(29), fill=TEXT)
    draw.text((20, 54), subtitle, font=font(18), fill=MUTED)
    for i, data in enumerate(samples):
        x, y = 20 + i % 2 * 536, 106 + i // 2 * 430
        draw.text((x, y - 28), f"{data['name']} · 种子 {data['seed']}", font=font(20), fill=TEXT)
        page.paste(renderer.render(data['rooms']['A']), (x, y))
    page.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--bridge-reference', type=Path, required=True)
    parser.add_argument('--seeds', type=int, nargs='+', default=[6144, 105, 38, 91])
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / '.stick-local').resolve()):
        parser.error('Review outputs must be under the repository .stick-local directory')
    output.mkdir(parents=True, exist_ok=True)
    reference = args.bridge_reference.resolve()
    renderer = Renderer(reference)
    samples = [generate(seed) for seed in args.seeds]
    for data in samples:
        stem = f"seed-{data['seed']}"
        (output / f'{stem}.json').write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
        for key, room in data['rooms'].items():
            renderer.render(room).save(output / f'{stem}-{key}.png')
            renderer.render(room, routes=True).save(output / f'{stem}-{key}-routes.png')
        triptych(renderer, data).save(output / f'{stem}-group.png')
        triptych(renderer, data, routes=True).save(output / f'{stem}-routes.png')
        frames = [triptych(renderer, data, frame=i) for i in range(8)]
        frames[0].save(output / f'{stem}-water.gif', save_all=True,
                       append_images=frames[1:], duration=140, loop=0, disposal=2)
    contact(renderer, samples, output / 'four-families.png')
    contact(renderer, samples, output / 'four-families-routes.png', routes=True)
    if len(samples) == 4:
        a_grid(renderer, samples, output / 'A-four-families.png', 'A 桥厅 · 四套随机骨架')
    detail_samples = [generate(seed) for seed in (38, 42, 46, 50)]
    a_grid(renderer, detail_samples, output / 'A-same-family.png', '同一套宽桥骨架 · 四个种子',
           '桥位与河宽保留；潭岸收口、凹坑、岩石组和地面斑块随种子变化。')
    # The saved original reference is evidence, not a new runtime asset.
    copied_reference = output / 'reference-meteor-falls.png'
    if reference != copied_reference:
        shutil.copy2(reference, copied_reference)
    manifest = dict(scope='local review only; no runtime map or pack edits',
                    bridge_source=dict(file=copied_reference.name,
                                       sha256=hashlib.sha256(reference.read_bytes()).hexdigest(),
                                       crop=[304, 288, 336, 320], transform='none'),
                    materials=['Caves.png', 'Outside.png', *WATERFALL_AUTOTILE_NAMES],
                    source_sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in [TILESET.parent / 'Caves.png', TILESET]},
                    samples=[dict(seed=d['seed'], family=d['family'], name=d['name'],
                                  fingerprint=fingerprint(d), checks=d['checks']) for d in samples],
                    same_family_seeds=[d['seed'] for d in detail_samples],
                    animation_frames_per_group=8, frame_ms=140)
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    (output / 'README.md').write_text(
        '# 峡谷桥洞随机设计预览\n\n'
        '四套 A/B/C 骨架：东岸短桥、中峡高瀑、东峡宽桥、西峡绕岸。'
        '同种子整组复现；河道、桥位、楼梯与潭形随骨架改变；潭岸底角、上层绕行、'
        '岩石组、完整凹坑与地面斑块随种子变化。\n\n'
        '查看 four-families.png 与 four-families-routes.png；A-four-families.png '
        '只比较桥厅；A-same-family.png 比较同骨架不同种子。每个 seed JSON 保存完整'
        '三层地图、路线、成对洞口、水域、楼梯与装饰；GIF 包含三间房的八帧动画。\n\n'
        '全部美术来自原素材。桥取自用户提供的流星瀑布原图完整 32×32 区块，'
        '重复铺设，无改绘或变换。Sea 下岸沿用已确认的原四分块翻转组合。\n\n'
        '这是待审看的随机设计，未接入正式 C++ 地图生成器、资源包或固件；'
        '不代表运行时换层、角色遮挡及设备观感已验收。\n', encoding='utf-8')
    print(output / 'four-families.png')


if __name__ == '__main__':
    main()
