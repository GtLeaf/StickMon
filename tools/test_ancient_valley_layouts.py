"""Check authored valley routes, physical water seams and packed runtime art."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from ancient_valley_layouts import DATA, TILE_SOURCES, WATER_TILES, cpp_table
from amoled_host import ROOT, build_amoled_host


class AncientValleyTests(unittest.TestCase):
    def test_firmware_table_is_current(self):
        self.assertEqual((ROOT / 'src/game/ExploreAncientValleyLayouts.h').read_text(),
                         cpp_table())

    def test_routes_have_clear_feet_dry_approaches_and_vertical_stairs(self):
        for spec in DATA['layouts']:
            shell = set(map(tuple, spec['shell']))
            ports = {tuple(p['visual_anchor']) for p in spec['portals']}
            stairs = set(map(tuple, spec['stairs']))
            blocked = (set(map(tuple, spec['walls'])) - ports)
            for name in ('cliffs', 'water', 'decoration'):
                blocked |= set(map(tuple, spec[name]))
            for portal in spec['portals']:
                for p in map(tuple, portal['approach']):
                    self.assertIn(p, shell)
                    self.assertNotIn(p, blocked | stairs)
            for route in spec['routes']:
                points = route['points']
                self.assertEqual(len(points), len(set(map(tuple, points))))
                for a, b in zip(points, points[1:]):
                    self.assertEqual(abs(a[0]-b[0]) + abs(a[1]-b[1]), 1)
                    if tuple(a) in stairs or tuple(b) in stairs:
                        self.assertEqual(a[0], b[0])
                    for k in range(17):
                        for dx, dy in ((0, 0), (-6, 9), (0, 9), (6, 9)):
                            p = (int((26*(a[0]+.5+(b[0]-a[0])*k/16)+dx)//26),
                                 int((26*(a[1]+.5+(b[1]-a[1])*k/16)+dy)//26))
                            self.assertIn(p, shell)
                            self.assertNotIn(p, blocked, (spec['key'], a, b, p))

    def test_six_falls_join_full_upstream_and_downstream_water(self):
        count = 0
        for spec in DATA['layouts']:
            wet = set(map(tuple, spec['water']))
            cliff = set(map(tuple, spec['cliffs']))
            def tile(x, y, layer=1):
                return spec['layers'][layer][y*16+x]
            for feature in spec['features']:
                if feature['type'] != 'waterfall':
                    continue
                count += 1
                left = feature['left']; right = left + feature['width'] - 1
                crest = feature['crest']; foot = crest + feature['body_height'] + 1
                for x in range(left, right+1):
                    self.assertIn((x, crest-1), wet)
                    self.assertIn((x, foot+1), wet)
                    self.assertIn(tile(x, foot+1), WATER_TILES)
                    self.assertEqual(tile(x, foot), 336)
                for x, lip in ((left, 1144), (right, 1146)):
                    self.assertEqual(TILE_SOURCES[tile(x, crest, 2)], ('Outside.png', lip))
                for x in (left-1, right+1):
                    self.assertIn((x, crest), cliff)
                    self.assertIn((x, foot-1), cliff)
        self.assertEqual(count, 6)

    def test_packed_tiles_animation_occlusion_and_in_place_portal_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = build_amoled_host(Path(directory) / 'valley',
                                       [ROOT / 'tools/ancient_valley_host.cpp'])
            result = subprocess.run([str(binary), str(ROOT / 'data')],
                                    check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout,
                             'ancient-valley=ok layouts=4 frames=32 exits=8\n')


if __name__ == '__main__':
    unittest.main()
