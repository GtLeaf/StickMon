#!/usr/bin/env python3
"""Export the actual market renderer at 1x without creating new artwork."""
import argparse
import json
from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/map_editor'))
from catalog import Catalog
from market_export import render_native


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,default=ROOT/'.stick-local/artifacts/market-controls-20261102/preview')
    args=parser.parse_args()
    project=json.loads((ROOT/'origin_asset/maps/market/market.stickmap.json').read_text())
    files=render_native(project,Catalog(),args.output_dir,walk=True)
    rounds=sorted(p for p in files if p.name.startswith('round-'))
    sheet=Image.new('RGB',(360*3,384*2),'#171d25');draw=ImageDraw.Draw(sheet)
    for i,path in enumerate(rounds):
        x=(i%3)*360;y=(i//3)*384
        with Image.open(path) as im:sheet.paste(im,(x,y+24))
        draw.text((x+12,y+5),f'1x / {path.stem}',fill='white')
    sheet.save(args.output_dir/'round-contact-sheet.png')
    print(args.output_dir/'round-contact-sheet.png')


if __name__=='__main__':main()
