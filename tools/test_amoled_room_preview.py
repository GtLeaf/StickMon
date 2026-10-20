#!/usr/bin/env python3
"""Exercise layered room loading, real UI switching and visual fidelity."""
from pathlib import Path
import json
import shutil
import struct
import subprocess
import tempfile
import unittest

from PIL import Image, ImageChops, ImageStat
from amoled_host import build_amoled_host
import generate_room_layers as layers
import prepare_room_background as room

ROOT = Path(__file__).resolve().parents[1]


def read565(path, size):
    raw = path.read_bytes()
    values = struct.unpack('<' + 'H' * (len(raw) // 2), raw)
    image = Image.new('RGB', size)
    image.putdata([((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31) for v in values])
    return image


class RoomPreviewTests(unittest.TestCase):
    def test_real_menu_resources_cache_and_visuals(self):
        with tempfile.TemporaryDirectory() as temporary:
            temp = Path(temporary)
            resources = temp / 'data'
            shutil.copytree(ROOT / 'data', resources)
            pack = resources / 'packs/dev'
            shutil.copyfile(pack / 'rooms/standard.smonroom', pack / 'rooms/legacy.smonroom')
            paths = layers.generate(output=pack)
            self.assertEqual(len(paths), 13)  # One room, six original textures, six variants.
            self.assertFalse(list((pack / 'rooms').glob('preview_*.smonroom')))
            standard = (pack / 'rooms/standard.smonroom').read_bytes()
            header = struct.unpack_from(room.ROOM_PACK_HEADER_FORMAT, standard)
            self.assertEqual(header[1], 6)
            self.assertEqual(header[35:37], (480, 322))
            # A second room changes its background, placement and care anchor.
            alternate = room.read_layout(room.DEFAULT_LAYOUT)
            alternate['canvas']['height'] = 185
            alternate['roomGeometry']['room']['height'] = 185
            for item in alternate['furniture']:
                if item['name'] == 'forniture_bowl': item['x'] += 10
            # Absolute background references make this temporary authoring layout portable.
            base = Image.open(room.DEFAULT_LAYOUT.parent / 'room_empty.png').convert('RGB')
            base = ImageChops.multiply(base, Image.new('RGB', base.size, (190, 230, 255)))
            base_path = temp / 'alternate-base.png'; base.save(base_path)
            alternate['base']['fileName'] = str(base_path)
            for mode in ('day', 'night'): alternate['backgrounds'][mode]['fileName'] = str(base_path)
            night_base = ImageChops.multiply(base, Image.new('RGB', base.size, (110, 130, 190)))
            night_path = temp / 'alternate-night.png'; night_base.save(night_path)
            alternate['backgrounds']['night']['fileName'] = str(night_path)
            alternate['backgroundLayers'][0].update(fileName=str(base_path), visibleInNight=False)
            night_layer = dict(alternate['backgroundLayers'][0])
            night_layer.update(id='night', fileName=str(night_path), visibleInDay=False, visibleInNight=True)
            alternate['backgroundLayers'].append(night_layer)
            alt_path = temp / 'alternate.json'; alt_path.write_text(json.dumps(alternate))
            layers.generate(alt_path, pack, 'alternate', include_variants=False)
            bad = bytearray(standard)
            offset = 80 + header[6] + sum(header[9:12]) * 4 + header[12] * 6
            struct.pack_into('<H', bad, offset + 4, 65535)  # Unbounded placement count.
            (pack / 'rooms/bad_layout.smonroom').write_bytes(bad)
            (pack / 'rooms/truncated_layout.smonroom').write_bytes(standard[:-1])
            furniture = pack / 'furniture'
            bad = bytearray((furniture / 'bed_cloud.smonfurn').read_bytes()); bad[20] ^= 1
            (furniture / 'bad_crc.smonfurn').write_bytes(bad)
            bad = bytearray((furniture / 'bed_cloud.smonfurn').read_bytes()); struct.pack_into('<H', bad, 6, 65535)
            (furniture / 'oversize.smonfurn').write_bytes(bad)
            binary = temp / 'preview-test'
            build_amoled_host(binary, ['tools/amoled_room_preview_host.cpp'], debug=True)
            output = temp / 'frames'; output.mkdir()
            subprocess.run([str(binary), str(resources), str(output)], cwd=ROOT, check=True)
            alt_actual = read565(output / 'alternate-night.rgb565', (480, 370))
            alt_art = room.scale_room_layout_for_art(alternate, 2)
            alt_expected = room.compose_room_image(alt_art, night_path, room.DEFAULT_FURNITURE_DIR,
                                                   'night', alt_path.parent)
            alt_diff = ImageChops.difference(alt_actual, alt_expected)
            self.assertLessEqual(max(high for low, high in alt_diff.getextrema()), 18)
            self.assertLess(sum(ImageStat.Stat(alt_diff).mean) / 3, 3.0)
            # Compare real C++ composites against the established offline room renderer.
            for bed, rug in [(0, 0)] + [(b, r) for b in range(1, 4) for r in range(1, 4)]:
                layout = room.read_layout(room.DEFAULT_LAYOUT)
                for item in layout['furniture']:
                    style = bed if item['name'] == 'bed' else rug if item['name'] == 'carpet' else 0
                    if style:
                        kind = 'bed' if item['name'] == 'bed' else 'rug'
                        item['fileName'] = str(layers.LIBRARY / f'furniture_{kind}_{layers.STYLES[style-1]}.png')
                art = room.scale_room_layout_for_art(layout, 2)
                for mode in ('day', 'night'):
                    name = f'b{bed}_r{rug}' if bed else 'original'
                    actual = read565(output / f'{name}-{mode}.rgb565', (480, 322))
                    background = room.background_path(room.DEFAULT_LAYOUT, layout, mode)
                    expected = room.compose_room_image(art, background, room.DEFAULT_FURNITURE_DIR,
                                                       mode, room.DEFAULT_LAYOUT.parent)
                    diff = ImageChops.difference(actual, expected)
                    # RGB565 alpha compositing can differ by one display color step.
                    self.assertLessEqual(max(high for low, high in diff.getextrema()), 18, (name, mode))
                    self.assertLess(sum(ImageStat.Stat(diff).mean) / 3, 3.0, (name, mode))
                    if name == 'b1_r1' and mode == 'day':
                        keep = ROOT / 'tools/out/room-layers'
                        keep.mkdir(parents=True, exist_ok=True)
                        actual.resize((960, 644), Image.Resampling.NEAREST).save(keep / 'forest.png')
                        expected.resize((960, 644), Image.Resampling.NEAREST).save(keep / 'forest-reference.png')


if __name__ == '__main__':
    unittest.main()
