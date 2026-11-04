#!/usr/bin/env python3
"""Prepare the generated transparent chat artwork for the 28px UI frame."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'origin_asset/icon/market/chat-generated.png'
OUTPUT = SOURCE.with_name('chat.png')


def main():
    with Image.open(SOURCE) as source:
        image = source.convert('RGBA')
        if image.getchannel('A').getextrema()[0] != 0:
            raise ValueError('Chat artwork must have a transparent background')
        # Keep the generated pixel edges and transparent margins intact.
        image.resize((28, 28), Image.Resampling.NEAREST).save(OUTPUT)
    print(OUTPUT)


if __name__ == '__main__':
    main()
