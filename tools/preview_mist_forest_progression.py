"""Preview seven forest stages with original tiles; never change game packs."""
import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import random
from types import SimpleNamespace

from PIL import Image, ImageDraw, ImageFont

from asset_paths import essentials_dir
from generate_explore_map import regular_tile
from generate_runtime_tile_map import road_tile

from mist_forest_layouts import ROOT, W, H, TITLES, NOTES, EXTRA_ART, extra_art, design

def render(data, sheet, size=32, routes=False):
    rng = random.Random(data['seed'] + data['index'])
    @lru_cache(None)
    def tile(tid):
        return regular_tile(sheet, tid).resize((size,size), Image.Resampling.NEAREST)
    image = Image.new('RGBA',(W*size,H*size))
    for y in range(H):
        for x in range(W):
            image.alpha_composite(tile(rng.choice((385,385,385,386,387))), (x*size,y*size))
    roads = set(map(tuple, data['roads']))
    extended = roads | {(x,y) for x in (data['south_lane'],data['south_lane']+1) for y in (12,13)}
    extended |= {(x,y) for x in (data['north_lane'],data['north_lane']+1) for y in (-2,-1)}
    context = SimpleNamespace(paths=[], entry=SimpleNamespace(point=(-100,-100), edge=0))
    # All neighboring cells are explicit, so no endpoint-specific extension is needed.
    context.entry.point = (-100,-100)
    for x,y in sorted(roads):
        image.alpha_composite(tile(road_tile(context,extended,x,y)), (x*size,y*size))
    for key, tid in (('grass',390),('flowers',415),('bushes',859)):
        for x,y in data[key]:
            if key == 'flowers':
                kind = 'red_flowers' if data['index'] == 0 else 'white_flowers'
                art = extra_art(kind).resize((size,size),Image.Resampling.NEAREST)
            else:
                art = tile(990 if key == 'bushes' and (x+y)%2 else tid)
            image.alpha_composite(art,(x*size,y*size))
    for x,y in sorted(data['long_grass'],key=lambda p:(p[1],p[0])):
        for dy,tid in enumerate((391,399)):
            image.alpha_composite(tile(tid),(x*size,(y+dy)*size))
    for obj in data['decorations']:
        x,y = obj['left'], obj['top']
        kind = obj['kind']
        if kind == 'stone':
            image.alpha_composite(tile(1532),(x*size,y*size))
        else:
            art = extra_art(kind).resize((size,size*obj['height']),Image.Resampling.NEAREST)
            image.alpha_composite(art,(x*size,y*size))
    for obj in sorted(data['objects'],key=lambda o:o['top']+o['height']):
        for tid,dx,dy in obj['parts']:
            x,y=obj['left']+dx,obj['top']+dy
            if 0 <= x < W and 0 <= y < H:
                image.alpha_composite(tile(tid),(x*size,y*size))
    image = image.convert('RGB')
    if routes:
        draw = ImageDraw.Draw(image)
        for controls in [data['controls'], *data['branches']]:
            points=[((x+1)*size,(y+1)*size) for x,y in controls]
            draw.line(points,fill='#20372c',width=5)
            draw.line(points,fill='#ffe391',width=2)
        for lane,y in ((data['south_lane'],H*size-8),(data['north_lane'],8)):
            x=(lane+1)*size
            draw.ellipse((x-5,y-5,x+5,y+5),fill='#ffe391')
    return image


def material_board(sheet, output):
    font=ImageFont.truetype('/System/Library/Fonts/STHeiti Medium.ttc',20)
    small=ImageFont.truetype('/System/Library/Fonts/STHeiti Medium.ttc',14)
    board=Image.new('RGB',(900,690),'#17242a')
    draw=ImageDraw.Draw(board)
    draw.text((20,14),'雾林径 · 可用原素材',font=font,fill='#f0eedc')
    draw.text((20,44),'取自 Essentials｜保留原像素与颜色｜果树和幼树为原角色图帧',font=small,fill='#afc1ba')
    tall=Image.new('RGBA',(32,64))
    tall.alpha_composite(regular_tile(sheet,391),(0,0))
    tall.alpha_composite(regular_tile(sheet,399),(0,32))
    entries=[('红色花簇','Flowers1 · 林缘小花丛',extra_art('red_flowers')),
             ('白色花簇','Flowers2 · 草甸点缀',extra_art('white_flowers')),
             ('低矮草丛','Outside:390 · 林下成片',regular_tile(sheet,390)),
             ('长草','Outside:391/399 · 树林边缘',tall),
             ('浅绿矮灌木','Outside:859 · 草丛边界',regular_tile(sheet,859)),
             ('深绿矮灌木','Outside:990 · 深林点缀',regular_tile(sheet,990)),
             ('幼树','Object tree 1 · 树群旁',extra_art('sapling')),
             ('蓝橘果树','berrytree_ORANBERRY · 少量地标',extra_art('oran')),
             ('扁平碎石','Outside:1532 · 林缘少量点缀',regular_tile(sheet,1532))]
    for i,(title,note,art) in enumerate(entries):
        x=20+(i%3)*295;y=86+(i//3)*197
        draw.rounded_rectangle((x,y,x+275,y+180),8,fill='#233a33')
        enlarged=art.resize((art.width*2,art.height*2),Image.Resampling.NEAREST)
        board.paste(enlarged,(x+105,y+8),enlarged)
        draw.text((x+12,y+135),title,font=font,fill='#e9e8ce')
        draw.text((x+12,y+160),note,font=small,fill='#afc1ba')
    board.save(output/'materials.png')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed',type=int,default=38)
    parser.add_argument('--output-dir',type=Path,
                        default=ROOT/'.stick-local/previews/mist-forest-patches-20261025')
    args=parser.parse_args()
    args.output_dir.mkdir(parents=True,exist_ok=True)
    source=essentials_dir()/'Graphics/Tilesets/Outside.png'
    sheet=Image.open(source).convert('RGBA')
    material_board(sheet,args.output_dir)
    font=ImageFont.truetype('/System/Library/Fonts/STHeiti Medium.ttc',20)
    small=ImageFont.truetype('/System/Library/Fonts/STHeiti Medium.ttc',15)
    designs=[design(i,args.seed) for i in range(7)]
    for a,b in zip(designs,designs[1:]):
        assert a['north_lane']==b['south_lane']
    row_stride=390
    page=Image.new('RGB',(1328,1268),'#17242a');draw=ImageDraw.Draw(page)
    draw.text((16,12),'雾林径 · 林缘 → 混交林 → 深林',font=font,fill='#f0eedc')
    draw.text((16,42),'成片植被版：草、花、长草连续生长，边缘不规则｜原素材｜未写入正式地图',font=small,fill='#afc1ba')
    for i,data in enumerate(designs):
        original=render(data,sheet)
        actual=render(data,sheet,26)
        original.save(args.output_dir/f'{i+1:02d}-clean.png')
        actual.save(args.output_dir/f'{i+1:02d}-26px.png')
        render(data,sheet,26,True).save(args.output_dir/f'{i+1:02d}-routes.png')
        x=16+(i%3)*440;y=82+(i//3)*row_stride
        draw.text((x,y),f'{i+1:02d}  {TITLES[i]}',font=font,fill='#e9e8ce')
        draw.text((x,y+27),NOTES[i],font=small,fill='#acbfb2')
        page.paste(actual,(x,y+51))
    # Put the two references and the transition rationale beside the final map.
    x,y=456,82+2*row_stride
    draw.text((x,y),'连续变化',font=font,fill='#e9e8ce')
    for line,text in enumerate(['01—02  圆冠树与细长树，入口开阔',
        '03—04  宽冠树从一侧逐渐扩展', '05—07  宽冠树包围林道，保留小空地',
        '草丛每片 4—9 格，花簇每片 3—6 格', '长草每片 3—5 株，保留完整上下两格',
        '同类片区留间隔，全部避让道路和树根']):
        draw.text((x,y+40+line*32),text,font=small,fill='#afc1ba')
    page.save(args.output_dir/'overview.png')
    (args.output_dir/'design.json').write_text(json.dumps(dict(seed=args.seed,
        source='Graphics/Tilesets/Outside.png',source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        extra_sources={k:dict(source=v[0],box=v[1],sha256=hashlib.sha256(
            (essentials_dir()/'Graphics'/v[0]).read_bytes()).hexdigest()) for k,v in EXTRA_ART.items()},
        preview_only=True,maps=designs),ensure_ascii=False,indent=2)+'\n')
    print(json.dumps([dict(index=d['index']+1,trees=len(d['objects']),
        wide=sum(o['kind']=='wide' for o in d['objects']),covered=len(d['canopy_on_road']))
        for d in designs]))
    print(args.output_dir/'overview.png')


if __name__=='__main__':
    main()
