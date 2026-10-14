#!/usr/bin/env python3
"""Export all three park route types from the actual runtime preview generator."""
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import generate_runtime_tile_map as g


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path,
                        default=Path('origin_asset/generated/game/runtime_tall_grass_park_v15'))
    parser.add_argument('--font', type=Path,
                        default=Path('/System/Library/Fonts/STHeiti Medium.ttc'))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    font = lambda size: (ImageFont.truetype(str(args.font), size) if args.font.exists()
                         else ImageFont.load_default())
    names = ('蜿蜒单线', '弯曲双分支', '前后两次分岔')
    descriptions = ('1 个出口 · 无岔点', '2 个出口 · 1 个岔点', '3 个出口 · 2 个岔点')
    clean_panels, debug_panels, samples = [], [], []
    used_seeds = set()
    for index in range(9):
        route_type = index % 3
        edge = g.Edge.BOTTOM if index < 6 else g.Edge.RIGHT if route_type == 1 else g.Edge.LEFT
        for seed in range(1, 1025):
            rng = g.XorShift32(seed ^ 0xA511E9B3)
            roll = rng.bounded(100)
            selected_type = 0 if roll < 30 else 1 if roll < 80 else 2
            if selected_type != route_type or seed in used_seeds:
                continue
            m = g.generate_map(seed, edge, g.TALL_GRASS_PARK_AREA)
            turns = []
            for path in m.paths:
                directions = [(b[0]-a[0], b[1]-a[1]) for a,b in zip(path.points,path.points[1:])]
                turns.append(sum(a != b for a,b in zip(directions,directions[1:])))
            if 3 <= index < 6 and max(turns) < 4:
                continue
            used_seeds.add(seed)
            break
        else:
            raise ValueError('unable to find representative park seed')
        assert len(m.paths) == route_type + 1
        clean = g.render(m)
        debug = clean.copy()
        draw = ImageDraw.Draw(debug)
        colors = ('#74b7ff', '#ffae69', '#cd92ff')
        for path, color in reversed(list(zip(m.paths, colors))):
            draw.line([(x * 26 + 13, y * 26 + 13) for x, y in path.points], fill=color, width=3)
        forks = g.route_junctions(m.paths)
        # The two continuing routes encounter the second fork after the first.
        forks.sort(key=lambda point: (point != m.junction, point))
        for number, (x, y) in enumerate(forks, 1):
            cx, cy = x * 26 + 13, y * 26 + 13
            draw.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill='#ffdb76', outline='#18382b')
            draw.text((cx - 3, cy - 6), str(number), font=font(12), fill='#18382b')
        markers = [(m.entry.point, '入', '#68dfa3')] + [
            (path.exit.point, str(i + 1), '#ffdb76') for i, path in enumerate(m.paths)]
        for (x, y), label, color in markers:
            cx, cy = x * 26 + 13, y * 26 + 13
            draw.rectangle((cx - 11, cy - 10, cx + 11, cy + 10), fill=color)
            draw.text((cx - 7, cy - 8), label, font=font(16), fill='#18382b')
        stem = f'park_{route_type + 1}_exits_{chr(97 + index // 3)}_{seed:08x}'
        clean.save(args.output_dir / f'{stem}.png')
        debug.save(args.output_dir / f'{stem}_routes.png')
        payload = g.map_json(m)
        (args.output_dir / f'{stem}.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n')
        tree_count = sum(m.layers[1].count(tile) for tile in (800, 804, 824))
        summary = {'file': stem, 'seed': seed, 'entry': g.EDGE_NAMES[edge],
                   'exits': len(m.paths), 'forks': len(g.route_junctions(m.paths)),
                   'grassRoots': m.layers[0].count(399), 'trees': tree_count,
                   'fingerprint': payload['fingerprint']}
        samples.append(summary)
        for panels, artwork in ((clean_panels, clean), (debug_panels, debug)):
            panel = Image.new('RGB', (448, 410), '#edf3eb')
            d = ImageDraw.Draw(panel)
            d.text((16, 10), f'{names[route_type]} {chr(65 + index // 3)}',
                   font=font(23), fill='#18392b')
            d.text((16, 42), descriptions[route_type], font=font(15), fill='#536d60')
            panel.paste(artwork.convert('RGB'), (16, 70))
            d.text((16, 390), f'seed {seed:08x} · 高草 {summary["grassRoots"]} 格 · 树 {tree_count} 棵',
                   font=font(13), fill='#536d60')
            panels.append(panel)
    for panels, filename, subtitle in (
        (clean_panels, 'runtime_park_routes.png', '共享生成器 v15 · 骨架坐标、岔点与出口随机生长 · 原始 Tile 素材'),
        (debug_panels, 'runtime_park_routes_annotated.png', '绿色为入口，黄色边界标记为出口，黄色圆点为岔点'),
    ):
        board = Image.new('RGB', (1408, 1402), '#11271e')
        d = ImageDraw.Draw(board)
        d.text((24, 18), '高草园 · 已接入的三种路线', font=font(32), fill='#f0f5ea')
        d.text((24, 65), subtitle, font=font(18), fill='#b1c7b5')
        for i, panel in enumerate(panels):
            board.paste(panel, (16 + (i % 3) * 464, 110 + (i // 3) * 426))
        board.save(args.output_dir / filename)
    manifest = {'algorithmVersion': g.ALGORITHM_VERSION, 'samples': samples}
    (args.output_dir / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
